// v2.35 #89: turn-order pointer safety. Game::setCompanionClass used to free the old companion while
// turnOrder_ still pointed at it; rebuildTurnOrder() then read the freed Character's uid (use-after-free).
// This test replaces operator new / delete so freed memory is poisoned (0xDD) and never reused
// (quarantined), the same way the audit reproduced it: any read through a stale turn-order pointer
// shows up as a garbage uid / crash instead of silently reading recycled memory.
// Run tools/migration_test/run.sh.
#include "Game.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <string>
std::ostream aout(std::cout.rdbuf());
using namespace dnd;

// ---- poisoning allocator (test binary only) ----
static constexpr size_t kHdr = 16;
void* operator new(size_t n) {
    auto* b = static_cast<unsigned char*>(std::malloc(n + kHdr));
    if (!b) throw std::bad_alloc();
    std::memcpy(b, &n, sizeof n);
    return b + kHdr;
}
void* operator new[](size_t n) { return ::operator new(n); }
void operator delete(void* q) noexcept {
    if (!q) return;
    auto* b = static_cast<unsigned char*>(q) - kHdr;
    size_t n; std::memcpy(&n, b, sizeof n);
    std::memset(q, 0xDD, n); // poison, and keep the block (quarantine) so nothing reuses it
}
void operator delete[](void* q) noexcept { ::operator delete(q); }
void operator delete(void* q, size_t) noexcept { ::operator delete(q); }
void operator delete[](void* q, size_t) noexcept { ::operator delete(q); }

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; ++failures; } } while (0)

namespace dnd {
struct GameTestAccess {
    static Character* comp(Game& g) { return g.findNpcCompanion(); }
    static int slotOf(Game& g, const Character* c) {
        for (size_t i = 0; i < g.turnOrder_.size(); ++i) if (g.turnOrder_[i] == c) return static_cast<int>(i);
        return -1;
    }
    static void setTurn(Game& g, int i) { g.currentTurnIndex_ = i; }
    static void clearRoom(Game& g) { g.clearEnemies(); g.enterClearedRoom(g.players_[0].get()); }
    static void clearEnemies(Game& g) { g.clearEnemies(); }
    static bool owns(const Game& g, const Character* c) { return g.ownsActor(c); }
    static void rebuild(Game& g) { g.rebuildTurnOrder(); }
};
} // namespace dnd
using T = GameTestAccess;

static const CharacterClass kAll[] = {CharacterClass::FIGHTER, CharacterClass::WIZARD, CharacterClass::ROGUE,
                                      CharacterClass::BARD, CharacterClass::CLERIC};

int main() {
    int reclasses = 0;
    // ---- The audit's repro: reload a cleared room where the companion holds the current slot, reclass.
    for (unsigned seed = 1; seed <= 12; ++seed) {
        for (CharacterClass compCl : kAll) {
            Game g;
            g.seedForTesting(seed);
            g.startNewGame(CharacterClass::FIGHTER, "Mira", 1, 1, compCl, true);
            g.setAsHost(true);
            T::clearRoom(g);
            Game r;
            CHECK(r.deserialize(g.serialize()));
            CHECK(!r.isInCombat());
            CHECK(r.turnOrderIsLive());
            Character* old = T::comp(r);
            CHECK(old != nullptr);
            if (!old) continue;
            const int slot = T::slotOf(r, old);
            CHECK(slot >= 0); // after a load the turn order holds the party even outside a fight
            if (slot < 0) continue;
            T::setTurn(r, slot);
            const std::string uid = old->uid;
            const size_t n = r.turnOrderSize();
            for (CharacterClass to : kAll) {
                if (to == T::comp(r)->characterClass) continue;
                CHECK(r.setCompanionClass(to));
                ++reclasses;
                CHECK(r.turnOrderIsLive());
                CHECK(r.turnOrderSize() == n);
                Character* cur = r.getCurrentActor();
                CHECK(cur != nullptr && T::owns(r, cur));
                // The new companion keeps the slot (same uid, new class).
                CHECK(cur && cur->uid == uid && cur->characterClass == to && cur == T::comp(r));
            }
            // And the game keeps working: save / load round trip.
            Game r2;
            CHECK(r2.deserialize(r.serialize()));
            CHECK(r2.turnOrderIsLive());
        }
    }

    // ---- Clearing foes never leaves their turn slots behind (every enemies_.clear() site).
    {
        Game g;
        g.seedForTesting(3);
        g.startNewGame(CharacterClass::WIZARD, "Mira", 1, 1, CharacterClass::FIGHTER, true);
        g.setAsHost(true);
        CHECK(g.isInCombat() && !g.getEnemies().empty());
        const Character* foe = g.getEnemies().front().get();
        T::setTurn(g, T::slotOf(g, foe)); // a foe holds the current slot
        T::clearEnemies(g);
        CHECK(g.getEnemies().empty() && g.turnOrderIsLive());
        CHECK(g.turnOrderSize() == 2);
        T::rebuild(g); // used to read the freed foe's uid
        CHECK(g.turnOrderIsLive() && g.turnOrderSize() == 2);
        CHECK(!T::owns(g, nullptr));
    }

    // ---- Walking through rooms (each one clears the last room's foes) keeps the turn order live.
    {
        Game g;
        g.seedForTesting(5);
        g.startNewGame(CharacterClass::ROGUE, "Mira", 2, 1, CharacterClass::CLERIC, true);
        g.setAsHost(true);
        for (int i = 0; i < 25; ++i) {
            T::clearRoom(g);
            CHECK(g.turnOrderIsLive());
            g.dmAdvanceRoom();
            CHECK(g.turnOrderIsLive());
        }
    }

    if (failures) { std::cerr << failures << " turnorder_test failure(s)\n"; return 1; }
    std::cout << "TURN ORDER: " << reclasses << " reclasses after a reload with the companion on the current slot; no stale pointers\n";
    std::cout << "turnorder_test OK\n";
    return 0;
}
