// v2.28 save slots: Game::describeSave() summarises a slot's blob for the Adventures picker and
// the crash-guard recovery check WITHOUT adopting it. Old v2.23–v2.27 saves (and pre-v2.23
// unencoded ones) must describe correctly so they land in slot 1 intact; broken blobs must be
// reported unreadable (never "fixed" or adopted). Run tools/migration_test/run.sh.
#include "Game.h"
#include <iostream>
#include <string>
#include <vector>
std::ostream aout(std::cout.rdbuf());
using namespace dnd;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; ++failures; } } while (0)

static const char* kParty =
    "Mira,0,7,10,30,30,1,1,0,16,12,14,8,10,10,50,3,local-player,None:0:W:0:-1:0,None:0:A:0:-1:0,0,0,0,0,0,0,;"
    "Corvin (NPC),1,5,10,20,20,3,3,0,8,14,12,16,10,10,0,2,ally-1,None:0:W:0:-1:0,None:0:A:0:-1:0,0,0,0,0,0,1,;";

/** fields: 34 = pre-v2.23 (no marker), 35 = v2.23/24 (E2), 37 = v2.25+ (E2 + Venn/well choices). */
static std::string header(int beat, int fields, bool gameOver = false, bool a4 = false, int mode = 0, int difficulty = 1) {
    std::vector<std::string> h(37, "0");
    h[0] = "solo-sid"; h[1] = std::to_string(beat); h[2] = gameOver ? "1" : "0"; h[6] = "";
    h[8] = std::to_string(beat);
    h[9] = "1"; h[10] = "1"; h[11] = "1";
    h[12] = beat > 13 ? "1" : "0"; h[13] = h[12];
    h[14] = std::to_string(mode);
    h[15] = std::to_string(difficulty);
    h[19] = beat > 19 ? "1" : "0"; h[20] = h[19];
    h[29] = a4 ? "1" : "0"; h[30] = h[29];
    h[34] = "E2"; h[35] = "1"; h[36] = "2";
    h.resize(fields);
    std::string out;
    for (size_t i = 0; i < h.size(); ++i) { if (i) out += ","; out += h[i]; }
    return out;
}

static std::string save(int beat, int fields, bool gameOver = false, bool a4 = false, int mode = 0, int difficulty = 1) {
    return header(beat, fields, gameOver, a4, mode, difficulty) + "|Room text~Old event|" + kParty + "||chat~|journal~";
}

int main() {
    // 1) v2.25–v2.27 save mid-Act 2 (Ledger Loft): summary for the slot card.
    {
        Game::SaveSummary s;
        CHECK(Game::describeSave(save(11, 37), &s));
        CHECK(s.heroName == "Mira");
        CHECK(s.classId == 0);
        CHECK(s.level == 7);
        CHECK(s.questBeat == 11);
        CHECK(std::string(soloQuestActTitle(11)) == "The Tithe");
        CHECK(!s.gameOver && !s.storyComplete);
        CHECK(s.difficulty == 1);
        std::cout << "V2.27 SAVE: Mira Lv 7 Fighter, beat 11 (The Tithe) described\n";
    }
    // 2) v2.23/24 (35 fields, no choices) and pre-v2.23 (34 fields, no E2 marker) still describe.
    {
        Game::SaveSummary a, b;
        CHECK(Game::describeSave(save(16, 35), &a));
        CHECK(a.questBeat == 16 && a.heroName == "Mira");
        CHECK(Game::describeSave(save(3, 34), &b));
        CHECK(b.questBeat == 3 && b.level == 7);
        std::cout << "V2.23/24 + PRE-V2.23 SAVES: described\n";
    }
    // 3) Game Over (Easy) keeps its flag so Continue still opens the wipe dialog; a finished
    //    story reports storyComplete (slot 1's per-slot story_fully_complete is derived from it).
    {
        Game::SaveSummary over, done;
        CHECK(Game::describeSave(save(9, 37, true, false, 0, 0), &over));
        CHECK(over.gameOver && over.difficulty == 0);
        CHECK(Game::describeSave(save(25, 37, false, true), &done));
        CHECK(done.storyComplete);
        Game::SaveSummary crawl;
        CHECK(Game::describeSave(save(0, 37, false, false, 1), &crawl));
        CHECK(crawl.soloPlayMode == 1);
        std::cout << "GAME OVER / STORY COMPLETE / CRAWL: flags kept\n";
    }
    // 4) Broken blobs are unreadable, and describing never touches a live game.
    {
        Game::SaveSummary s;
        s.heroName = "untouched";
        CHECK(!Game::describeSave("", &s));
        CHECK(!Game::describeSave("garbage-without-sections", &s));
        CHECK(!Game::describeSave(header(11, 37) + "|Room~Ev|Mira,notanumber,;||~|~", &s));
        CHECK(s.heroName == "untouched");

        Game live;
        CHECK(live.deserialize(save(11, 37)));
        const std::string before = live.serialize();
        Game::SaveSummary other;
        CHECK(Game::describeSave(save(20, 37), &other));
        CHECK(!Game::describeSave("x|y", &other));
        CHECK(live.serialize() == before);
        CHECK(live.getQuestBeat() == 11);
        std::cout << "BROKEN BLOBS: rejected; live game untouched\n";
    }
    if (failures) { std::cerr << failures << " failure(s)\n"; return 1; }
    std::cout << "slots_test OK\n";
    return 0;
}
