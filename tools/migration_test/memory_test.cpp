// v2.26 Memory Ledger: which story memories an existing (v2.23 / v2.24 / v2.25 / pre-v2.23) Continue
// save is granted on migration, choice-coloured bodies, live grants, and that Host / Join tables and
// the save format never carry the Ledger. The per-slot meta-pref store itself (slot-1 keys, New Game /
// Abandon clears, Host / Join untouched) is covered by app/src/test/.../MemoryLedgerStoreTest.kt.
// Run tools/migration_test/run.sh.
#include "Game.h"
#include "MemoryCatalog.h"
#include <algorithm>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
std::ostream aout(std::cout.rdbuf());
using namespace dnd;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; ++failures; } } while (0)

static const char* kParty =
    "Hero,0,5,10,30,30,1,1,0,16,12,14,8,10,10,50,3,local-player,None:0:W:0:-1:0,None:0:A:0:-1:0,0,0,0,0,0,0,;"
    "Corvin (NPC),1,5,10,20,20,3,3,0,8,14,12,16,10,10,0,2,ally-1,None:0:W:0:-1:0,None:0:A:0:-1:0,0,0,0,0,0,1,;";
static const char* kFoe =
    "Ashbound,0,1,10,12,12,0,0,0,12,10,12,6,8,6,0,0,enemy_ashbound#1,None:0:W:0:-1:0,None:0:A:0:-1:0,0,0,0,0,0,0,;";

enum Ver { PRE_V223, V223, V224, V225 };
struct Flags {
    bool a1 = true, lantern = true, crypt = true, a2 = false, ledger = false, a3 = false, seal = false,
         a4 = false, ember = false, searched = false;
    int mode = 0; // STORY
    int venn = 0, well = 0; // v2.25 fields
};

static std::string header(int beat, const Flags& f, Ver v) {
    std::vector<std::string> h(35, "0");
    h[0] = "solo-sid"; h[1] = std::to_string(beat); h[6] = "";
    h[8] = std::to_string(beat);
    h[9] = f.lantern ? "1" : "0"; h[10] = f.a1 ? "1" : "0"; h[11] = f.crypt ? "1" : "0";
    h[12] = f.a2 ? "1" : "0"; h[13] = f.ledger ? "1" : "0";
    h[14] = std::to_string(f.mode);
    h[19] = f.a3 ? "1" : "0"; h[20] = f.seal ? "1" : "0";
    h[29] = f.a4 ? "1" : "0"; h[30] = f.ember ? "1" : "0";
    h[33] = std::to_string(beat);
    h[34] = "E2";
    if (v == PRE_V223) h.resize(34);
    if (v == V225) { h.push_back(std::to_string(f.venn)); h.push_back(std::to_string(f.well)); }
    std::string out;
    for (size_t i = 0; i < h.size(); ++i) { if (i) out += ","; out += h[i]; }
    return out;
}

static std::string save(int beat, const Flags& f, Ver v, bool foes = false) {
    return header(beat, f, v) + "|Room~Event|" + kParty + "|" + (foes ? kFoe : "") + "|chat~|journal~";
}

static std::set<std::string> idsOf(const std::string& progress) {
    std::set<std::string> out;
    const auto bar = progress.find('|');
    if (bar == std::string::npos) return out;
    std::string cur;
    for (char c : progress.substr(bar + 1) + ",") {
        if (c == ',') { if (!cur.empty()) out.insert(cur); cur.clear(); } else cur.push_back(c);
    }
    return out;
}
static std::string choicesOf(const std::string& p) { return p.substr(0, p.find('|')); }
static bool has(const std::set<std::string>& s, const char* id) { return s.count(id) > 0; }
static bool contains(const std::string& s, const std::string& n) { return s.find(n) != std::string::npos; }
static const std::vector<std::string> kAct1 = {"mem_kindling_night", "mem_mothers_whisper", "mem_wrens_knife",
    "mem_funeral_ribbon", "mem_sextons_watch", "mem_wrens_palm"};
static bool hasAct1(const std::set<std::string>& s) {
    for (const auto& id : kAct1) if (!s.count(id)) return false;
    return true;
}
static std::vector<std::string> drained(Game& g) {
    std::string d = g.drainMemoryGrants();
    std::vector<std::string> out; std::string cur;
    for (char c : d.substr(d.find('|') + 1) + ",") {
        if (c == ',') { if (!cur.empty()) out.push_back(cur); cur.clear(); } else cur.push_back(c);
    }
    return out;
}
static bool in(const std::vector<std::string>& v, const char* id) {
    for (const auto& x : v) if (x == id) return true;
    return false;
}

int main() {
    // 0) Catalogue: unique stable ids, 23 story memories, every raid boss has three, no spoilers in
    //    the locked hints, no D&D / WotC names anywhere.
    {
        std::set<std::string> ids; int story = 0;
        for (const auto& m : memory::kMemories) {
            CHECK(ids.insert(m.id).second);
            CHECK(std::string(m.id).rfind("mem_", 0) == 0);
            if (m.source == memory::SRC_STORY) ++story;
            const std::string hint = memory::lockedHintFor(m);
            CHECK(!contains(hint, m.title));
            std::string all = std::string(m.title) + m.speaker + m.place + m.body + (m.wren ? m.wren : "")
                + (m.alt1 ? m.alt1 : "") + (m.alt2 ? m.alt2 : "");
            for (const char* bad : {"Dungeon Master", "Melf", "Wizards of the Coast", "D&D", "Forgotten Realms"})
                CHECK(!contains(all, bad));
        }
        CHECK(memory::kMemoryCount == 47 && story == 23);
        for (const char* boss : {"boss_hollow_crown", "boss_ember_hydra", "boss_nightfang"}) {
            const char* r[3]; memory::raidLoreFor(boss, r);
            for (const char* id : r) CHECK(id && memory::findMemory(id));
        }
    }
    // 1) Pre-v2.23 legacy post–Act 1 save (beat 7): exactly the six Act 1 memories, even without
    //    the rune-key flag (approved: grant all six Act 1 cards).
    {
        Flags f; f.crypt = false;
        const std::string p = Game::previewStoryMemories(save(7, f, PRE_V223));
        const auto s = idsOf(p);
        CHECK(hasAct1(s) && s.size() == 6);
        CHECK(choicesOf(p) == "0,0");
    }
    // 2) v2.23 save mid-Act 2 in the Ledger Loft, not yet searched: Act 1 + Aunt Lise only.
    {
        Flags f;
        const auto s = idsOf(Game::previewStoryMemories(save(11, f, V223)));
        CHECK(hasAct1(s) && has(s, "mem_lises_ribbon") && s.size() == 7);
        CHECK(!has(s, "mem_your_name") && !has(s, "mem_tithe_sermon") && !has(s, "mem_venn_father"));
        // …and once the ledger is taken (flag set), The Next Entry counts.
        f.ledger = true; f.searched = true;
        CHECK(has(idsOf(Game::previewStoryMemories(save(11, f, V223))), "mem_your_name"));
    }
    // 3) v2.24 save sitting on The Tithe Broken with Act 2 done: Venn defaults to ended (old kill),
    //    the Act 2 guaranteed set is granted, the missable sermon is not; Venn's card says "ended".
    {
        Flags f; f.a2 = true; f.ledger = true;
        const std::string p = Game::previewStoryMemories(save(13, f, V224));
        const auto s = idsOf(p);
        CHECK(choicesOf(p) == "2,0");
        for (const char* id : {"mem_lises_ribbon", "mem_your_name", "mem_venn_father", "mem_hesk_lie"}) CHECK(has(s, id));
        CHECK(!has(s, "mem_tithe_sermon") && s.size() == 10);
        const auto* venn = memory::findMemory("mem_venn_father");
        CHECK(contains(memory::bodyFor(*venn, 2, 0), "He did not leave again"));
        CHECK(contains(memory::bodyFor(*venn, 1, 0), "because you let him"));
        CHECK(!contains(memory::bodyFor(*venn, 0, 0), "He did not leave again"));
    }
    // 4) v2.24 mid-fight in the Root Labyrinth (foe alive): Rope-Boy yes, Baker's Hands not until cleared.
    {
        Flags f; f.a2 = true; f.ledger = true;
        const auto mid = idsOf(Game::previewStoryMemories(save(16, f, V224, /*foes=*/true)));
        CHECK(has(mid, "mem_rope_boy") && !has(mid, "mem_baker_hands") && !has(mid, "mem_well_dumping"));
        const auto clr = idsOf(Game::previewStoryMemories(save(16, f, V224)));
        CHECK(has(clr, "mem_baker_hands"));
    }
    // 5) v2.24 Act 3 already complete, now on Vigil Watch mid-fight: well defaults to sealed in →
    //    The Well's Judgment is granted with the "sealed" body; Act 3 guaranteed + seal memory.
    {
        Flags f; f.a2 = f.ledger = f.a3 = f.seal = true;
        const std::string p = Game::previewStoryMemories(save(20, f, V224, true));
        const auto s = idsOf(p);
        CHECK(choicesOf(p) == "2,1");
        for (const char* id : {"mem_rope_boy", "mem_baker_hands", "mem_first_oath", "mem_maren_named",
                               "mem_well_judgment", "mem_thinning"}) CHECK(has(s, id));
        CHECK(!has(s, "mem_greyfen_post") && !has(s, "mem_well_dumping"));
        const auto* w = memory::findMemory("mem_well_judgment");
        CHECK(contains(memory::bodyFor(*w, 2, 1), "burns bright"));
        CHECK(contains(memory::bodyFor(*w, 2, 2), "dimmer"));
    }
    // 6) v2.25 story-complete save (spared Venn, let them rest): all 21 non-missable story memories;
    //    choices come through for the choice-coloured cards (Old Sallow knows you spared him).
    {
        Flags f; f.a2 = f.ledger = f.a3 = f.seal = f.a4 = f.ember = true; f.venn = 1; f.well = 2;
        const std::string p = Game::previewStoryMemories(save(25, f, V225));
        const auto s = idsOf(p);
        CHECK(choicesOf(p) == "1,2");
        CHECK(s.size() == 21 && has(s, "mem_follow_the_ash") && has(s, "mem_siphon_prayer"));
        CHECK(contains(memory::bodyFor(*memory::findMemory("mem_old_sallow"), 1, 2), "let him walk away"));
        // Same hero as a pre-v2.23 save (no choice fields): defaults ended / sealed in.
        Flags g = f; g.venn = g.well = 0;
        CHECK(choicesOf(Game::previewStoryMemories(save(7, g, PRE_V223))) == "2,1");
        CHECK(idsOf(Game::previewStoryMemories(save(7, g, PRE_V223))).size() == 21);
    }
    // 7) Dungeon Crawl hero (never played the story): nothing to migrate. Broken save: "".
    {
        Flags f; f.a1 = f.lantern = f.crypt = false; f.mode = 1;
        CHECK(idsOf(Game::previewStoryMemories(save(0, f, V224))).empty());
        CHECK(Game::previewStoryMemories("not|a|save").empty());
    }
    // 8) Live: Onward into The Tithe Broken grants Venn's and Hesk's testimony plus the ledger
    //    safety memory; owned ids (pushed by Kotlin) are never queued twice; the save carries none.
    {
        Game g;
        Flags f; f.ledger = false;
        CHECK(g.deserialize(save(12, f, V224)));
        CHECK(g.needsMemoryContext());
        g.setMemoryLedgerContext("mem_hesk_lie,mem_unknown_id", 0);
        CHECK(!g.needsMemoryContext());
        g.playerAdvanceFromCleared();
        CHECK(g.getQuestBeat() == 13);
        auto d = drained(g);
        CHECK(in(d, "mem_venn_father") && in(d, "mem_your_name") && !in(d, "mem_hesk_lie"));
        CHECK(drained(g).empty()); // drained once
        CHECK(contains(g.getChatHistory(), "A soul settles in the glass — The Exile's Father."));
        const std::string s = g.serialize();
        CHECK(!contains(s, "mem_"));
        const auto h = s.substr(0, s.find('|'));
        CHECK(std::count(h.begin(), h.end(), ',') == 36); // still the v2.25 37-field header
        Game again; CHECK(again.deserialize(s));
        CHECK(again.needsMemoryContext() && drained(again).empty()); // load never replays grants
    }
    // 9) Live: Ledger Loft Search grants The Next Entry; the Well's Judgment arrives with the choice.
    {
        Game g;
        Flags f; f.a2 = false;
        CHECK(g.deserialize(save(11, f, V224)));
        g.playerInteract("Hero");
        CHECK(in(drained(g), "mem_your_name"));
        Game w;
        Flags f3; f3.a2 = f3.ledger = true;
        CHECK(w.deserialize(save(18, f3, V224)));
        w.playerAdvanceFromCleared(); // beat 19
        auto d = drained(w);
        CHECK(in(d, "mem_maren_named") && in(d, "mem_first_oath") && !in(d, "mem_well_judgment"));
        CHECK(w.chooseStoryOption(Game::PROMPT_WELL, Game::WELL_LET_REST));
        CHECK(in(drained(w), "mem_well_judgment"));
    }
    // 10) Lamplighter's Rounds pity: post-story procedural room, 8 dry rooms → the next Search frees a
    //     Millhollow voice and resets the counter; with no context pushed, pool grants wait.
    {
        Flags f; f.a2 = f.ledger = f.a3 = f.seal = f.a4 = f.ember = true;
        Game noCtx; CHECK(noCtx.deserialize(save(7, f, V225)));
        noCtx.playerInteract("Hero");
        CHECK(drained(noCtx).empty());
        Game g; CHECK(g.deserialize(save(7, f, V225)));
        g.setMemoryLedgerContext("", 8);
        g.playerInteract("Hero");
        const std::string raw = g.drainMemoryGrants();
        CHECK(raw.rfind("0|mem_", 0) == 0);
        const auto* m = memory::findMemory(raw.substr(2));
        CHECK(m && m->source == memory::SRC_CRAWL);
    }
    // 11) Host / Join tables never grant, and a Join snapshot load leaves nothing queued.
    {
        Game host;
        host.startDmSession("Keeper");
        host.setMemoryLedgerContext("", 8);
        host.dmBeginDungeon();
        host.dmAdvanceRoom();
        CHECK(drained(host).empty());
        Game client;
        client.prepareClientJoin();
        CHECK(client.needsMemoryContext() && drained(client).empty());
        Flags f; f.a2 = true; f.ledger = true;
        CHECK(client.deserialize(save(13, f, V224)));
        CHECK(drained(client).empty());
    }

    if (failures) { std::cerr << failures << " check(s) failed\n"; return 1; }
    std::cout << "memory_test OK\n";
    return 0;
}
