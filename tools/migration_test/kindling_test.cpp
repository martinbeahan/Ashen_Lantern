// v2.25 Kindling Acts 2–4 rewrite: old (v2.24 / pre-v2.23) Continue saves mid-Act 2/3/4 keep their
// beat, pick up the new room text on refresh, and get safe defaults for the new story choices
// (Venn "ended" / well "sealed in" once the act is already complete). Run tools/migration_test/run.sh.
#include "Game.h"
#include <iostream>
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

/** A v2.24 (E2-marked, 35-field) header, or a pre-v2.23 one (no marker) when marker == false. */
static std::string header(int beat, bool a2, bool a3, bool a4, bool marker = true, bool ledger = false) {
    std::vector<std::string> h(35, "0");
    h[0] = "solo-sid"; h[1] = std::to_string(beat); h[6] = "";
    h[8] = std::to_string(beat);
    h[9] = "1"; h[10] = "1"; h[11] = "1";
    h[12] = a2 ? "1" : "0"; h[13] = (a2 || ledger) ? "1" : "0";
    h[14] = "0"; // STORY
    h[19] = a3 ? "1" : "0"; h[20] = a3 ? "1" : "0";
    h[29] = a4 ? "1" : "0"; h[30] = a4 ? "1" : "0";
    h[33] = std::to_string(beat);
    h[34] = "E2";
    if (!marker) h.resize(34);
    std::string out;
    for (size_t i = 0; i < h.size(); ++i) { if (i) out += ","; out += h[i]; }
    return out;
}

static std::string oldSave(int beat, bool a2, bool a3, bool a4, const std::string& room,
                           bool marker = true, bool ledger = false) {
    // room/journal/chat contain no reserved characters, so they read the same encoded or not.
    return header(beat, a2, a3, a4, marker, ledger) + "|" + room + "~Old event|" + kParty + "||old chat line~|old journal line~";
}

static std::vector<std::string> headerFields(const std::string& save) {
    std::vector<std::string> f; std::string cur;
    for (char c : save.substr(0, save.find('|'))) { if (c == ',') { f.push_back(cur); cur.clear(); } else cur.push_back(c); }
    f.push_back(cur);
    return f;
}

static bool contains(const std::string& s, const std::string& n) { return s.find(n) != std::string::npos; }

int main() {
    // 1) Mid-Act 2 (Ledger Loft, beat 11) v2.24 save: beat kept, choices undecided, room text refreshed,
    //    journal / chat / lastEvent left as history.
    {
        Game g;
        CHECK(g.deserialize(oldSave(11, false, false, false,
            "A dry loft above the mill. Shelves of wax-sealed ledgers; one book lies open with fresh ink")));
        CHECK(g.getQuestBeat() == 11);
        CHECK(g.getStoryChoices() == "0,0");
        CHECK(g.pendingStoryChoice() == Game::PROMPT_NONE);
        const std::string journal = g.getJournal(), chat = g.getChatHistory(), ev = g.getLastEvent();
        CHECK(g.refreshStoryRoomAfterLoad());
        CHECK(contains(g.getRoomDescription(), "surplus"));
        CHECK(!contains(g.getRoomDescription(), "wax-sealed ledgers;"));
        CHECK(g.getJournal() == journal && g.getChatHistory() == chat && g.getLastEvent() == ev);
        CHECK(!g.refreshStoryRoomAfterLoad()); // idempotent
        std::cout << "MID ACT 2: beat 11 kept, room text refreshed, history untouched\n";
    }
    // 2) Post-Act-2 save sitting on beat 13 (old script already killed Venn): defaults to ended,
    //    never prompted; beat 13 shows its new label.
    {
        Game g;
        CHECK(g.deserialize(oldSave(13, true, false, false, "Millhollow's elders gather on the green.")));
        CHECK(g.getVennChoice() == Game::VENN_ENDED);
        CHECK(g.pendingStoryChoice() == Game::PROMPT_NONE);
        CHECK(std::string(soloQuestBeatName(13)) == "The Tithe Broken");
        CHECK(std::string(soloQuestActTitle(13)) == "The Tithe");
        CHECK(std::string(soloQuestActTitle(16)) == "Emberdeep");
        CHECK(std::string(soloQuestActTitle(22)) == "Ashwake Vigil");
    }
    // 3) Mid-Act 3 (Root Labyrinth) from a pre-v2.23 (unmarked) save: Venn ended, well undecided.
    {
        Game g;
        CHECK(g.deserialize(oldSave(16, true, false, false, "A maze of living roots", /*marker=*/false)));
        CHECK(g.getQuestBeat() == 16);
        CHECK(g.getStoryChoices() == "2,0");
        CHECK(g.refreshStoryRoomAfterLoad());
        CHECK(contains(g.getRoomDescription(), "Millhollow face"));
    }
    // 4) Act 3 already complete on beat 19: well defaults to "sealed in", no prompt.
    {
        Game g;
        CHECK(g.deserialize(oldSave(19, true, true, false, "The Ember Seal locks into the arch.")));
        CHECK(g.getStoryChoices() == "2,1");
        CHECK(g.pendingStoryChoice() == Game::PROMPT_NONE);
    }
    // 5) Mid-Act 4 (Fen Causeway) after the fight: cleared suffix survives the refresh; enemies none.
    {
        Game g;
        const std::string suffix = " The foes lie still. You may Search, take a Short Rest, or press Onward.";
        CHECK(g.deserialize(oldSave(22, true, true, false, "A raised timber path crosses deeper fen." + suffix)));
        CHECK(g.getStoryChoices() == "2,1");
        CHECK(g.refreshStoryRoomAfterLoad());
        const std::string d = g.getRoomDescription();
        CHECK(contains(d, "Greyfen's dead"));
        CHECK(d.size() > suffix.size() && d.compare(d.size() - suffix.size(), suffix.size(), suffix) == 0);
    }
    // 6) New play: beat 12 cleared -> Onward to beat 13 prompts for Venn; sparing round-trips; the
    //    v2.25 header keeps the E2 marker at index 34 and the choices after it.
    {
        Game g;
        CHECK(g.deserialize(oldSave(12, false, false, false, "The mill's upper hall.", true, /*ledger=*/true)));
        g.playerAdvanceFromCleared();
        CHECK(g.getQuestBeat() == 13);
        CHECK(g.pendingStoryChoice() == Game::PROMPT_VENN);
        CHECK(contains(g.getChatHistory(), "[Lantern]: Hesk held the quill."));
        CHECK(!g.chooseStoryOption(Game::PROMPT_WELL, 1)); // wrong prompt
        CHECK(!g.chooseStoryOption(Game::PROMPT_VENN, 3)); // bad option
        // Return to Menu before answering: the undecided state is saved and asks again.
        Game mid; CHECK(mid.deserialize(g.serialize()));
        CHECK(mid.pendingStoryChoice() == Game::PROMPT_VENN);
        CHECK(g.chooseStoryOption(Game::PROMPT_VENN, Game::VENN_SPARED));
        CHECK(g.pendingStoryChoice() == Game::PROMPT_NONE);
        const std::string s = g.serialize();
        const auto h = headerFields(s);
        CHECK(h.size() == 37 && h[34] == "E2" && h[35] == "1" && h[36] == "0");
        Game g2; CHECK(g2.deserialize(s));
        CHECK(g2.getStoryChoices() == "1,0");
        CHECK(g2.serialize() == s);
        g2.playerAdvanceFromCleared();
        CHECK(g2.getQuestBeat() == 14 && g2.getVennChoice() == Game::VENN_SPARED);
    }
    // 7) Beat 18 cleared -> 19 prompts for the well; Onward without answering keeps the old ending
    //    (sealed in) and moves on to Act 4.
    {
        Game g;
        CHECK(g.deserialize(oldSave(18, true, false, false, "A cracked arch frames a vertical wound.")));
        g.playerAdvanceFromCleared();
        CHECK(g.getQuestBeat() == 19);
        CHECK(g.pendingStoryChoice() == Game::PROMPT_WELL);
        CHECK(contains(g.getChatHistory(), "Maren Vey"));
        g.playerAdvanceFromCleared();
        CHECK(g.getQuestBeat() == 20 && g.getWellChoice() == Game::WELL_SEALED_IN);
        CHECK(contains(g.getChatHistory(), "[Lantern]: I'm getting quieter."));
    }
    // 8) "Let them rest" colours the Act 4 epilogue; the story still ends on "Follow the ash."
    {
        Game g;
        CHECK(g.deserialize(oldSave(18, true, false, false, "arch")));
        g.playerAdvanceFromCleared();
        CHECK(g.chooseStoryOption(Game::PROMPT_WELL, Game::WELL_LET_REST));
        CHECK(contains(g.getChatHistory(), "[Lantern]: Thank you. They were so tired."));
        Game end;
        std::string s = g.serialize();
        // Jump the same hero to beat 24 cleared, then Onward into Vigil Kindled.
        auto h = headerFields(s);
        h[1] = h[8] = "24"; h[29] = "0";
        std::string head; for (size_t i = 0; i < h.size(); ++i) { if (i) head += ","; head += h[i]; }
        CHECK(end.deserialize(head + s.substr(s.find('|'))));
        end.playerAdvanceFromCleared();
        CHECK(end.getQuestBeat() == 25);
        const std::string chat = end.getChatHistory();
        CHECK(contains(chat, "ash-dead below Millhollow are resting"));
        CHECK(contains(chat, "Follow the ash"));
        CHECK(contains(end.getRoomDescription(), "Maren Vey"));
    }
    // 9) No D&D / WotC names crept into the rewritten script.
    {
        for (int b = 8; b <= 25; ++b) {
            const QuestBeatScript* q = findQuestBeat(b);
            CHECK(q != nullptr);
            if (!q) continue;
            const std::string all = std::string(q->title) + q->roomDescription + q->dmEnter + q->journalOnEnter
                + (q->wrenOnEnter ? q->wrenOnEnter : "") + (q->wrenOnClear ? q->wrenOnClear : "");
            for (const char* bad : {"Dungeon Master", "Melf", "Wizards of the Coast", "D&D", "grain debt"})
                CHECK(!contains(all, bad));
        }
    }

    if (failures) { std::cerr << failures << " check(s) failed\n"; return 1; }
    std::cout << "kindling_test OK\n";
    return 0;
}
