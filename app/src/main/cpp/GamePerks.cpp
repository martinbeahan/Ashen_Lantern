// v2.32 Kindled Path (combat revamp PR 4): Wards, Lantern Arts, Steady Flames and Re-temper.
// Solo only — every entry point checks Character::perksOn() / universalStatsActive(), so online
// tables and Host/Join never see a perk. Numbers live in Perks.h; see docs/PERKS.md.
#include "Game.h"
#include "EnemyAi.h"
#include "CombatCurve.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace dnd {

namespace {
bool up(const Character* c) { return c && !c->isDead && !c->isDowned && c->currentHp > 0; }
int classOrd(const Character& c) { return static_cast<int>(c.characterClass); }
bool hasHarmful(const Character& c) {
    const auto& s = c.status;
    return s.burn > 0 || s.marked > 0 || s.blind > 0 || s.slowed > 0 || s.shackled > 0 || c.nextAttackPenalty > 0;
}
} // namespace

// --- Battle bookkeeping -------------------------------------------------------------------------
void Game::startBattlePerks() {
    encoreUid_.clear();
    encoreFromUid_.clear();
    for (auto& p : players_) {
        if (!p) continue;
        p->resetBattlePerks();
        if (p->wardEquipped(perks::EMBER_WARD) && !p->isDead) {
            p->status.shield = std::max(1, static_cast<int>(p->maxHp * perks::kEmberWardShield));
            p->spendWard(perks::EMBER_WARD);
        }
    }
}

void Game::tickPerkStatuses(Character& c) {
    auto& st = c.status;
    auto down = [](int& v) { if (v > 0) --v; };
    if (!c.isFoe()) {
        down(st.braced); down(st.rallied); down(st.spite); down(st.smoke); down(st.resist); down(st.exhausted);
        if (c.perksOn())
            for (int& cd : c.artCd) if (cd > 0 && cd < perks::kOncePerBattle) --cd;
        return;
    }
    if (st.sundered > 0 && --st.sundered == 0) c.armorClass += perks::kSunderAc;
    down(st.stunned);
    if (st.bleed > 0) {
        if (up(&c) && st.bleedDmg > 0) {
            c.takeDamage(st.bleedDmg);
            noteDailyQuestDamage(st.bleedDmg);
            addChatMessage("Combat", c.name + " bleeds for " + std::to_string(st.bleedDmg) + ".");
            if (c.currentHp <= 0) bleedKill_ = true;
        }
        if (--st.bleed <= 0) { st.bleed = 0; st.bleedDmg = 0; }
    }
}

int Game::bracedHeroIndex() const {
    for (size_t i = 0; i < players_.size(); ++i)
        if (up(players_[i].get()) && players_[i]->status.braced > 0) return static_cast<int>(i);
    return -1;
}

bool Game::heroHidden(int i) const {
    if (i < 0 || static_cast<size_t>(i) >= players_.size()) return false;
    const Character* h = players_[static_cast<size_t>(i)].get();
    if (!up(h) || h->status.smoke <= 0) return false;
    for (const auto& p : players_) if (up(p.get()) && p->status.smoke <= 0) return true; // someone else to hit
    return false;
}

int Game::perkRallyBonus(const Character& c) const {
    return (c.status.rallied > 0 ? perks::kRallyBonus : 0) + (c.status.spite > 0 ? perks::kSpiteBonus : 0);
}

int Game::perkOutgoingDamage(Character& actor, int dmg, bool special, std::stringstream* note) {
    if (!actor.perksOn() || dmg <= 0) return dmg;
    double m = actor.damageMult();
    if (special && specialMult_ > 1.0) {
        m *= specialMult_;
        if (note) *note << "(Overkindle x2) ";
    }
    if (actor.wardReady(perks::KINDLE_RUSH)) {
        actor.spendWard(perks::KINDLE_RUSH);
        m *= perks::kKindleRush;
        if (note) *note << "(Kindle Rush +50%) ";
        else addChatMessage("Combat", "Kindle Rush! " + actor.name + "'s opening blow burns +50%.");
    }
    return std::max(1, static_cast<int>(std::lround(dmg * m)));
}

// --- Wards on incoming hits -----------------------------------------------------------------------
std::string Game::wardsBeforeHit(Character& hero, int& dmg) {
    if (!hero.perksOn() || dmg <= 0) return "";
    std::string note;
    if (hero.wardReady(perks::ASH_VEIL)) {
        hero.spendWard(perks::ASH_VEIL);
        const int after = (dmg + 1) / 2;
        note += " (Ash Veil: -" + std::to_string(dmg - after) + ")";
        dmg = after;
    }
    if (hero.status.shield > 0) {
        const int soak = std::min(hero.status.shield, dmg);
        hero.status.shield -= soak;
        dmg -= soak;
        note += " (shield soaks " + std::to_string(soak) + ")";
    }
    if (dmg >= hero.currentHp && hero.currentHp > 0 && hero.wardReady(perks::LAST_LIGHT)) {
        hero.spendWard(perks::LAST_LIGHT);
        dmg = hero.currentHp - 1;
        note += " (Last Light — " + hero.name + " holds on at 1 HP!)";
    }
    return note;
}

void Game::wardsAfterHit(Character& hero, bool wasDowned) {
    if (up(&hero) && hero.wardReady(perks::SECOND_WICK)
        && hero.currentHp < static_cast<int>(hero.maxHp * perks::kSecondWickBelow)) {
        hero.spendWard(perks::SECOND_WICK);
        const int amt = std::max(1, static_cast<int>(hero.maxHp * perks::kSecondWickHeal));
        hero.currentHp = std::min(hero.maxHp, hero.currentHp + amt);
        addChatMessage("Combat", "Second Wick! " + hero.name + " flares back for " + std::to_string(amt) + " HP.");
    }
    if (!wasDowned && hero.isDowned) {
        for (auto& p : players_) {
            if (!up(p.get()) || p.get() == &hero || !p->wardReady(perks::SPITE_SPARK)) continue;
            p->spendWard(perks::SPITE_SPARK);
            p->status.spite = perks::kSpiteRounds + 1; // ticks at the end of its own turn
            addChatMessage("Combat", "Spite Spark! " + p->name + " burns hotter (+3 to hit and damage).");
        }
    }
}

void Game::clearHarmful(Character& c) {
    auto& s = c.status;
    s.burn = 0; s.burnDmg = 0; s.marked = 0; s.blind = 0; s.shackled = 0;
    c.nextAttackPenalty = 0;
    if (s.slowed > 0) {
        s.slowed = 0;
        c.initiative = s.slowedFromInit;
        s.slowedFromInit = 0;
        rebuildTurnOrder();
    }
}

void Game::wardsAfterFoeMove() {
    for (auto& p : players_) {
        if (!up(p.get()) || !p->perksOn() || !hasHarmful(*p)) continue;
        if (p->status.resist > 0) {
            clearHarmful(*p);
            addChatMessage("Combat", p->name + " is Sanctified — the curse slides off.");
        } else if (p->wardReady(perks::CLEANSE)) {
            p->spendWard(perks::CLEANSE);
            clearHarmful(*p);
            addChatMessage("Combat", "Cleanse! " + p->name + " burns the affliction away.");
        }
    }
}

bool Game::perkFoeTurnTaken(Character& foe, int foeIdx) {
    auto& st = foe.status;
    if (st.stunned > 0) {
        lastEvent_ = foe.name + " is bound in ash and loses its turn.";
        addChatMessage("Combat", lastEvent_);
        foe.ai.move = 0;
        return true;
    }
    if (st.asleep > 0) {
        lastEvent_ = foe.name + " sleeps on, lulled by the song.";
        addChatMessage("Combat", lastEvent_);
        return true;
    }
    if (st.discord > 0) {
        st.discord = 0;
        Character* other = nullptr;
        for (auto& e : enemies_) if (e.get() != &foe && up(e.get())) { other = e.get(); break; }
        if (!other) return false;
        pushVisualEvent(VisualEventType::ENEMY_ATTACK, foeIdx);
        const int avg = foe.curveAvgDmgX10 > 0 ? foe.curveAvgDmgX10 : 50;
        const int dmg = std::max(1, curve::rollFoeDamage(avg, std::max(1, foe.curveEL), false, false));
        other->takeDamage(dmg);
        lastEvent_ = "Discord! " + foe.name + " turns on " + other->name + " for " + std::to_string(dmg) + ".";
        addChatMessage("Combat", lastEvent_);
        if (other->currentHp <= 0) bleedKill_ = true; // removed after this turn
        return true;
    }
    return false;
}

// --- Lantern Arts -----------------------------------------------------------------------------------
bool Game::foeSavesVsArt(const Character& foe, const Character& actor) {
    const int dc = 8 + CombatSystem::proficiencyBonusForLevel(actor.level) + CombatSystem::getPrimaryModifier(actor);
    const bool tough = foe.curveRole == static_cast<int>(curve::Role::BOSS) || foe.curveRole == static_cast<int>(curve::Role::ELITE);
    const int bonus = 2 + std::max(0, foe.curveEL) / 4 + (tough ? 2 : 0);
    return getRandomInt(1, 20) + bonus >= dc;
}

void Game::artDamage(Character& actor, Character& foe, int foeIdx, int dmg) {
    dmg = perkOutgoingDamage(actor, std::max(1, dmg), false, nullptr);
    foe.takeDamage(dmg);
    noteDailyQuestDamage(dmg);
    noteFoeDamaged(foe, dmg, &actor, false);
    pushVisualEvent(VisualEventType::ENEMY_DAMAGE, foeIdx);
}

bool Game::runArt(Character& actor, int artIdx, int foeIdx) {
    const int pi = heroIndexOf(&actor);
    const int cls = classOrd(actor);
    Character* foe = (foeIdx >= 0 && static_cast<size_t>(foeIdx) < enemies_.size()) ? enemies_[static_cast<size_t>(foeIdx)].get() : nullptr;
    const std::string an = perks::art(cls, artIdx).name;
    const int mainMod = CombatSystem::getPrimaryModifier(actor);
    auto say = [&](const std::string& line) { lastEvent_ = line; addChatMessage("Combat", line); };
    auto needFoe = [&]() { return up(foe); };
    auto otherFoes = [&]() {
        std::vector<int> v;
        for (size_t i = 0; i < enemies_.size(); ++i) if (up(enemies_[i].get())) v.push_back(static_cast<int>(i));
        return v;
    };
    auto party = [&](bool includeDowned) {
        std::vector<Character*> v;
        for (auto& p : players_) if (p && !p->isDead && (includeDowned || up(p.get()))) v.push_back(p.get());
        return v;
    };
    pushVisualEvent(VisualEventType::PLAYER_ATTACK, pi);
    switch (cls * 10 + artIdx) {
        case 0: { // Fighter Cleave
            if (!needFoe()) return false;
            dmSay(actor.name + " sweeps wide — Cleave!");
            performWeaponAttack(&actor, *foe, pi, true, foeIdx, false, false, 0, perks::kCleaveMult);
            for (int i : otherFoes()) {
                if (i == foeIdx) continue;
                performWeaponAttack(&actor, *enemies_[static_cast<size_t>(i)], pi, true, i, false, false, 0, perks::kCleaveMult);
                break;
            }
            return true;
        }
        case 1: actor.status.braced = 3; say(actor.name + " plants their feet — Iron Brace! (+4 AC, foes must face them)"); return true;
        case 2: { // Sunder
            if (!needFoe()) return false;
            const int before = foe->currentHp;
            performWeaponAttack(&actor, *foe, pi, true, foeIdx, false, false);
            if (foe->currentHp < before && foe->currentHp > 0) {
                if (foe->status.sundered <= 0) foe->armorClass -= perks::kSunderAc;
                foe->status.sundered = 3;
                addChatMessage("Combat", foe->name + "'s armor cracks (-3 AC, 3 rounds).");
            }
            return true;
        }
        case 3: for (Character* p : party(false)) p->status.rallied = 3;
            say(actor.name + " roars a Warcry! The party gets +2 to hit and damage."); return true;
        case 10: { // Wizard Cinder Burst
            std::ostringstream ss;
            ss << actor.name << " — Cinder Burst!";
            for (int i : otherFoes()) {
                Character& f = *enemies_[static_cast<size_t>(i)];
                int dmg = getRandomInt(1, 6) + getRandomInt(1, 6) + std::max(0, mainMod);
                const bool saved = foeSavesVsArt(f, actor);
                if (saved) dmg = std::max(1, dmg / 2);
                const int hp0 = f.currentHp;
                artDamage(actor, f, i, dmg);
                ss << " " << f.name << " " << (hp0 - std::max(0, f.currentHp)) << (saved ? " (saved)" : "") << ".";
            }
            say(ss.str());
            return true;
        }
        case 11: { // Ash Bind
            if (!needFoe()) return false;
            if (foe->curveRole == static_cast<int>(curve::Role::BOSS)) {
                foe->ai.move = 0;
                foe->ai.cooldown = std::max(foe->ai.cooldown, 2);
                say("Ash Bind! " + foe->name + " shrugs off the chains but loses its planned move.");
            } else if (foeSavesVsArt(*foe, actor)) {
                say("Ash Bind — " + foe->name + " tears free (saved).");
            } else {
                foe->status.stunned = 1;
                foe->ai.move = 0;
                say("Ash Bind! " + foe->name + " is stunned for a round.");
            }
            return true;
        }
        case 12: { // Wick Ward
            Character* best = nullptr;
            for (Character* p : party(false))
                if (!best || p->currentHp * best->maxHp < best->currentHp * p->maxHp) best = p;
            if (!best) return false;
            const int amt = 10 + 2 * std::max(0, mainMod);
            best->status.shield += amt;
            say(actor.name + " weaves a Wick Ward around " + best->name + " (shield " + std::to_string(amt) + ").");
            return true;
        }
        case 13: actor.status.overkindle = 1; say(actor.name + " stokes the flame — Overkindle! The next special hits double."); return true;
        case 20: actor.status.smoke = 2; actor.status.nextCrit = 1;
            say(actor.name + " vanishes in a Smoke Step — the next hit will be a critical."); return true;
        case 21: { // Bleeding Cut
            if (!needFoe()) return false;
            const int before = foe->currentHp;
            performWeaponAttack(&actor, *foe, pi, true, foeIdx, false, false);
            if (foe->currentHp < before && foe->currentHp > 0) {
                foe->status.bleed = 3;
                foe->status.bleedDmg = std::max(2, (std::max(0, mainMod) + actor.level / 2) / 2);
                addChatMessage("Combat", foe->name + " is bleeding (" + std::to_string(foe->status.bleedDmg) + " a round, 3 rounds).");
            }
            return true;
        }
        case 22: { // Lift Purse
            if (!needFoe()) return false;
            const int gold = 10 + 3 * std::max(1, foe->curveEL);
            actor.gold += gold;
            foe->ai.move = static_cast<int>(ai::Move::STRIKE);
            say(actor.name + " lifts " + std::to_string(gold) + " gold from " + foe->name + " — its plan falls apart.");
            return true;
        }
        case 23: { // Fan of Knives
            dmSay(actor.name + " fans a spray of knives!");
            for (int i : otherFoes())
                performWeaponAttack(&actor, *enemies_[static_cast<size_t>(i)], pi, true, i, false, false, 0, perks::kFanMult);
            return true;
        }
        case 30: { // Mending Circle
            int total = 0;
            for (Character* p : party(true)) total += p->heal(getRandomInt(1, 6) + std::max(0, mainMod) + actor.level);
            actor.status.healedRecently = 2;
            say(actor.name + " draws a Mending Circle — the party heals " + std::to_string(total) + " HP in all.");
            return true;
        }
        case 31: { // Relight
            Character* t = nullptr;
            for (auto& p : players_) if (p && !p->isDead && (p->isDowned || p->currentHp <= 0)) { t = p.get(); break; }
            if (!t) { lastEvent_ = "Relight needs a downed ally."; return false; }
            t->heal(std::max(1, static_cast<int>(t->maxHp * perks::kRelightHp)));
            actor.status.healedRecently = 2;
            say(actor.name + " Relights " + t->name + " — back on their feet at " + std::to_string(t->currentHp) + " HP!");
            return true;
        }
        case 32: for (Character* p : party(false)) { clearHarmful(*p); p->status.resist = 3; }
            say(actor.name + " Sanctifies the party — afflictions burn away."); return true;
        case 33: { // Dawnstrike
            if (!needFoe()) return false;
            const bool bane = foe->enemyId.find("ashbound") != std::string::npos || foe->enemyId.find("crier") != std::string::npos;
            dmSay(actor.name + " calls the dawn — Dawnstrike!" + std::string(bane ? " The undead light burns double!" : ""));
            performWeaponAttack(&actor, *foe, pi, true, foeIdx, false, false, getRandomInt(1, 8), bane ? 2.0 : 1.0);
            return true;
        }
        case 40: for (Character* p : party(false)) p->status.rallied = 4;
            say(actor.name + " sings a Rallying Verse! +2 to hit and damage for the party."); return true;
        case 41: { // Discord
            if (!needFoe()) return false;
            if (otherFoes().size() < 2) { lastEvent_ = "Discord needs a second foe to turn on."; return false; }
            if (foeSavesVsArt(*foe, actor)) say("Discord — " + foe->name + " shakes off the whisper (saved).");
            else { foe->status.discord = 1; say("Discord! " + foe->name + " will turn on its own side."); }
            return true;
        }
        case 42: { // Encore
            Character* ally = nullptr;
            for (Character* p : party(false)) if (p != &actor) { ally = p; break; }
            if (!ally) { lastEvent_ = "Encore needs an ally standing."; return false; }
            encoreUid_ = ally->uid;
            say(actor.name + " calls for an Encore — " + ally->name + " acts again!");
            return true;
        }
        case 43: { // Ashen Lullaby
            if (!needFoe()) return false;
            const int r = foe->curveRole;
            if (r != static_cast<int>(curve::Role::MINION) && r != static_cast<int>(curve::Role::SKIRMISHER)) {
                lastEvent_ = "Ashen Lullaby only works on minions and skirmishers.";
                return false;
            }
            foe->status.asleep = 1;
            foe->ai.move = 0;
            say(actor.name + " hums an Ashen Lullaby — " + foe->name + " falls asleep.");
            return true;
        }
        default: return false;
    }
}

bool Game::playerUseArt(int slot, int targetEnemyIndex) {
    if (gameOver_ || turnOrder_.empty() || enemies_.empty() || isMerchantRoom_) return false;
    Character* actor = getCurrentActor();
    if (!actor || !isPartyMember(actor) || !up(actor)) return false;
    if (!actor->perksOn()) { lastEvent_ = "Lantern Arts are a solo feature."; return false; }
    if (slot < 0 || slot >= actor->artSlots()) return false;
    const int a = actor->artEq[static_cast<size_t>(slot)];
    if (!actor->artEquipped(a)) { lastEvent_ = "No Art in that slot."; return false; }
    const perks::ArtInfo& info = perks::art(classOrd(*actor), a);
    if (actor->artCd[static_cast<size_t>(a)] > 0) {
        lastEvent_ = std::string(info.name) + (actor->artCd[static_cast<size_t>(a)] >= perks::kOncePerBattle
            ? " is spent for this battle." : " recharges in " + std::to_string(actor->artCd[static_cast<size_t>(a)]) + " rounds.");
        addChatMessage("Combat", lastEvent_);
        return false;
    }
    if (actor->status.shackled > 0 || actor->status.exhausted > 0) {
        lastEvent_ = actor->name + (actor->status.shackled > 0 ? " is shackled" : " is burnt out") + " — no Arts this turn.";
        addChatMessage("Combat", lastEvent_);
        return false;
    }
    int foeIdx = targetEnemyIndex;
    if (foeIdx < 0 || static_cast<size_t>(foeIdx) >= enemies_.size() || !up(enemies_[static_cast<size_t>(foeIdx)].get())) {
        foeIdx = -1;
        for (size_t i = 0; i < enemies_.size(); ++i) if (up(enemies_[i].get())) { foeIdx = static_cast<int>(i); break; }
    }
    const std::string uid = actor->uid;
    const int roomBefore = roomCount_;
    if (!runArt(*actor, a, foeIdx)) {
        if (!lastEvent_.empty()) addChatMessage("Combat", lastEvent_);
        return false;
    }
    actor->artCd[static_cast<size_t>(a)] = perks::cooldownFor(info.cooldown, actor->signatureTier()) + (info.cooldown >= perks::kOncePerBattle ? 0 : 1);
    if (roomCount_ != roomBefore) return true;
    bool anyDead = false;
    for (const auto& e : enemies_) if (e && e->currentHp <= 0) anyDead = true;
    Character* now = getCurrentActor();
    if (!now || now->uid != uid) return true;
    if (anyDead) resolveEnemyDefeated(actor, foeIdx);
    else advanceTurn();
    return true;
}

bool Game::allyTryArt(Character& actor) {
    if (!actor.perksOn() || actor.status.shackled > 0 || actor.status.exhausted > 0) return false;
    int foes = 0, bossIdx = -1, bigIdx = -1, lowIdx = -1, smallIdx = -1;
    for (size_t i = 0; i < enemies_.size(); ++i) {
        const Character* e = enemies_[i].get();
        if (!up(e)) continue;
        ++foes;
        const int ii = static_cast<int>(i);
        if (e->curveRole == static_cast<int>(curve::Role::BOSS)) bossIdx = ii;
        if (bigIdx < 0 || e->currentHp > enemies_[static_cast<size_t>(bigIdx)]->currentHp) bigIdx = ii;
        if (lowIdx < 0 || e->currentHp < enemies_[static_cast<size_t>(lowIdx)]->currentHp) lowIdx = ii;
        if ((e->curveRole == static_cast<int>(curve::Role::MINION) || e->curveRole == static_cast<int>(curve::Role::SKIRMISHER))
            && e->status.asleep <= 0 && smallIdx < 0) smallIdx = ii;
    }
    if (foes == 0) return false;
    int downed = 0, hurt = 0, harmful = 0, standing = 0;
    bool squishyLow = false, critical = false;
    for (auto& p : players_) {
        if (!p || p->isDead) continue;
        if (!up(p.get())) { ++downed; continue; }
        ++standing;
        if (p->currentHp * 2 < p->maxHp) ++hurt;
        if (hasHarmful(*p)) ++harmful;
        if (p.get() != &actor && p->currentHp * 5 < p->maxHp * 2) squishyLow = true;
        if (p->currentHp * 3 < p->maxHp) critical = true;
    }
    const bool rallied = actor.status.rallied > 0;
    for (int slot = 0; slot < actor.artSlots(); ++slot) {
        const int a = actor.artEq[static_cast<size_t>(slot)];
        if (!actor.artReady(a)) continue;
        int target = lowIdx;
        bool want = false;
        switch (classOrd(actor) * 10 + a) {
            case 0: want = foes >= 2; break;
            case 1: want = squishyLow || (bossIdx >= 0 && standing >= 2); break;
            case 2: target = bossIdx >= 0 ? bossIdx : bigIdx; want = enemies_[static_cast<size_t>(target)]->status.sundered <= 0; break;
            case 3: case 40: want = !rallied && (foes >= 2 || bossIdx >= 0); break;
            case 10: want = foes >= 2; break;
            case 11: target = bossIdx >= 0 ? bossIdx : bigIdx; want = true; break;
            case 12: want = hurt > 0; break;
            case 13: want = specialAvailable(actor) && (bossIdx >= 0 || foes >= 2); break;
            case 20: want = actor.currentHp * 3 < actor.maxHp || bossIdx >= 0; break;
            case 21: target = bossIdx >= 0 ? bossIdx : bigIdx;
                want = !specialAvailable(actor) && enemies_[static_cast<size_t>(target)]->status.bleed <= 0; break; // Sneak Attack first
            case 22: target = bossIdx >= 0 ? bossIdx : bigIdx;
                want = ai::moveInfo(ai::moveFromInt(enemies_[static_cast<size_t>(target)]->ai.move)).intent == ai::Intent::HEAVY; break;
            case 23: want = foes >= 3 || (foes >= 2 && !specialAvailable(actor)); break;
            case 30: want = hurt >= 2 || critical; break;
            case 31: want = downed > 0; break;
            case 32: want = harmful > 0; break;
            case 33: want = true; break;
            case 41: target = bigIdx; want = foes >= 2; break;
            case 42: want = standing >= 2; break;
            case 43: target = smallIdx; want = smallIdx >= 0 && foes >= 2; break;
            default: break;
        }
        if (!want || target < 0) continue;
        if (playerUseArt(slot, target)) return true;
    }
    return false;
}

// --- Kindled Path: learn, equip, Re-temper (solo, outside combat) --------------------------------------
Character* Game::perkHero(int heroIdx, std::string* why) {
    if (heroIdx < 0 || static_cast<size_t>(heroIdx) >= players_.size() || !players_[static_cast<size_t>(heroIdx)]) {
        if (why) *why = "No such hero.";
        return nullptr;
    }
    syncStatRules();
    Character* p = players_[static_cast<size_t>(heroIdx)].get();
    if (!p->perksOn()) { if (why) *why = "The Kindled Path is solo only."; return nullptr; }
    if (isInCombat()) { if (why) *why = "Not during a fight."; return nullptr; }
    return p;
}

bool Game::learnPerk(int heroIdx, int kind, int id, std::string* why) {
    std::string tmp;
    if (!why) why = &tmp;
    Character* p = perkHero(heroIdx, why);
    if (!p) return false;
    const int free = p->embersFree();
    auto refuse = [&](const std::string& s) { *why = s; return false; };
    if (kind == 0) {
        if (id < 0 || id >= perks::kFlameCount) return refuse("No such flame.");
        const int cap = p->flameCap(); // 6 from Rekindling rank II
        if (p->flames[static_cast<size_t>(id)] >= cap) return refuse("Already at rank " + std::to_string(cap) + ".");
        if (free < 1) return refuse("Need 1 Ember.");
        p->flames[static_cast<size_t>(id)]++;
        const int hp0 = p->maxHp;
        p->calculateAC();
        p->applyStatsForLevel();
        if (p->maxHp > hp0) p->currentHp += p->maxHp - hp0;
        p->currentHp = std::min(p->currentHp, p->maxHp);
        addChatMessage("System", p->name + " kindles " + perks::flameName(id) + " (rank " + std::to_string(p->flames[static_cast<size_t>(id)]) + ").");
        return true;
    }
    if (kind == 1) {
        if (id < 0 || id >= perks::kWardCount) return refuse("No such Ward.");
        if (p->wardsLearned & (1 << id)) return refuse("Already learned.");
        if (free < perks::kWardCost) return refuse("Need 2 Embers.");
        p->wardsLearned |= (1 << id);
        for (int k = 0; k < p->wardSlots(); ++k) { int& s = p->wardEq[static_cast<size_t>(k)]; if (s < 0) { s = id; break; } }
        addChatMessage("System", p->name + " learns the Ward " + perks::wardName(id) + ".");
        return true;
    }
    if (kind == 2) {
        if (id < 0 || id >= perks::kArtsPerClass) return refuse("No such Art.");
        if (p->artsLearned & (1 << id)) return refuse("Already learned.");
        if (p->level < perks::kArtUnlock[id]) return refuse("Unlocks at level " + std::to_string(perks::kArtUnlock[id]) + ".");
        if (free < perks::kArtCost) return refuse("Need 3 Embers.");
        p->artsLearned |= (1 << id);
        for (int k = 0; k < p->artSlots(); ++k) { int& s = p->artEq[static_cast<size_t>(k)]; if (s < 0) { s = id; break; } }
        addChatMessage("System", p->name + " learns the Art " + perks::art(classOrd(*p), id).name + ".");
        return true;
    }
    return refuse("Unknown perk.");
}

bool Game::equipPerk(int heroIdx, int kind, int slot, int id, std::string* why) {
    std::string tmp;
    if (!why) why = &tmp;
    Character* p = perkHero(heroIdx, why);
    if (!p) return false;
    // v2.33: a 3rd Ward slot from Rekindling rank III, a 3rd Art slot from rank V.
    auto place = [&](auto& eq, int slots) {
        if (slot < 0 || slot >= slots) { *why = slots > 2 ? "No such slot." : "No such slot (a 3rd slot comes with Rekindling)."; return false; }
        if (id >= 0) // already in another slot: swap the two
            for (int k = 0; k < slots; ++k)
                if (k != slot && eq[static_cast<size_t>(k)] == id) eq[static_cast<size_t>(k)] = eq[static_cast<size_t>(slot)];
        eq[static_cast<size_t>(slot)] = id < 0 ? -1 : id;
        return true;
    };
    if (kind == 1) {
        if (id >= 0 && (id >= perks::kWardCount || !(p->wardsLearned & (1 << id)))) { *why = "Learn it first."; return false; }
        return place(p->wardEq, p->wardSlots());
    }
    if (kind == 2) {
        if (id >= 0 && (id >= perks::kArtsPerClass || !(p->artsLearned & (1 << id)))) { *why = "Learn it first."; return false; }
        return place(p->artEq, p->artSlots());
    }
    *why = "Unknown perk.";
    return false;
}

int Game::retemperCostFor(const Character& p) const {
    return p.freeRespecs > 0 ? 0 : perks::retemperCost(p.level);
}

bool Game::retemper(int heroIdx, std::string* why) {
    std::string tmp;
    if (!why) why = &tmp;
    Character* p = perkHero(heroIdx, why);
    if (!p) return false;
    const int cost = retemperCostFor(*p);
    Character* payer = nullptr;
    if (cost > 0) {
        if (p->gold >= cost) payer = p;
        else if (!players_.empty() && players_[0] && players_[0]->gold >= cost) payer = players_[0].get();
        if (!payer) { *why = "Re-temper costs " + std::to_string(cost) + " gold."; return false; }
    }
    // Stat points back: every point above the class's starting array returns to the pool.
    const Character fresh("", p->characterClass, "retemper");
    int* cur[6] = {&p->attributes.strength, &p->attributes.dexterity, &p->attributes.constitution,
                   &p->attributes.intelligence, &p->attributes.wisdom, &p->attributes.charisma};
    const int base[6] = {fresh.attributes.strength, fresh.attributes.dexterity, fresh.attributes.constitution,
                         fresh.attributes.intelligence, fresh.attributes.wisdom, fresh.attributes.charisma};
    int refunded = 0;
    for (int i = 0; i < 6; ++i) {
        const int spent = std::max(0, *cur[i] - base[i]);
        if (i == 2) {
            // Odd CON points stored +level HP in bonusMaxHp (increaseAttribute); take that back,
            // never below the story +2 padding.
            int odd = 0;
            for (int v = base[i] + 1; v <= *cur[i]; ++v) if (v % 2 != 0) ++odd;
            const int floorHp = p->bonusMaxResources > 0 ? 2 : 0;
            p->bonusMaxHp = std::max(std::min(p->bonusMaxHp, floorHp), p->bonusMaxHp - odd * p->level);
        }
        refunded += spent;
        *cur[i] -= spent;
    }
    p->pendingStatPoints += refunded;
    p->flames.fill(0);
    p->wardsLearned = 0;
    p->wardEq = {{-1, -1, -1}};
    p->artsLearned = 0;
    p->artEq = {{-1, -1, -1}};
    p->resetBattlePerks();
    if (cost > 0) payer->gold -= cost;
    else p->freeRespecs = std::max(0, p->freeRespecs - 1);
    p->calculateAC();
    p->applyStatsForLevel();
    p->currentHp = std::min(p->currentHp, p->maxHp);
    addChatMessage("System", p->name + " is Re-tempered: " + std::to_string(refunded) + " stat points and "
                   + std::to_string(p->embersEarned()) + " Embers to spend again"
                   + (cost > 0 ? " (" + std::to_string(cost) + " gold)." : " (free)."));
    return true;
}

std::string Game::getPerkState(int heroIdx) {
    if (heroIdx < 0 || static_cast<size_t>(heroIdx) >= players_.size() || !players_[static_cast<size_t>(heroIdx)]) return "";
    syncStatRules();
    const Character& p = *players_[static_cast<size_t>(heroIdx)];
    std::string why;
    const bool canEdit = p.perksOn() && !isInCombat();
    if (!p.perksOn()) why = "The Kindled Path is solo only.";
    else if (isInCombat()) why = "Not during a fight.";
    std::ostringstream ss;
    ss << "H|" << p.name << "|" << classOrd(p) << "|" << p.level << "|" << perks::kLevelCap << "|" << p.embersEarned()
       << "|" << p.embersSpent() << "|" << p.embersFree() << "|" << p.freeRespecs << "|" << retemperCostFor(p)
       << "|" << (canEdit ? 1 : 0) << "|" << why << "|" << p.gold << "|" << p.signatureTier()
       // v2.33 (appended): 14 Ward slots, 15 Art slots, 16 Rekindling rank
       << "|" << p.wardSlots() << "|" << p.artSlots() << "|" << p.prestigeRank << "\n";
    auto slotOf = [](const auto& eq, int slots, int i) {
        for (int k = 0; k < slots; ++k) if (eq[static_cast<size_t>(k)] == i) return k;
        return -1;
    };
    for (int i = 0; i < perks::kFlameCount; ++i)
        ss << "F|" << i << "|" << perks::flameName(i) << "|" << perks::flameDesc(i) << "|" << p.flames[static_cast<size_t>(i)] << "|" << p.flameCap() << "\n";
    for (int i = 0; i < perks::kWardCount; ++i) {
        const int slot = slotOf(p.wardEq, p.wardSlots(), i);
        ss << "W|" << i << "|" << perks::wardName(i) << "|" << perks::wardDesc(i) << "|" << ((p.wardsLearned >> i) & 1) << "|" << slot << "\n";
    }
    for (int i = 0; i < perks::kArtsPerClass; ++i) {
        const perks::ArtInfo& a = perks::art(classOrd(p), i);
        const int slot = slotOf(p.artEq, p.artSlots(), i);
        ss << "A|" << i << "|" << a.name << "|" << a.desc << "|" << perks::cooldownFor(a.cooldown, p.signatureTier())
           << "|" << perks::kArtUnlock[i] << "|" << ((p.artsLearned >> i) & 1) << "|" << slot << "|" << static_cast<int>(a.target) << "\n";
    }
    return ss.str();
}

std::string Game::getArtsBar() const {
    const Character* a = getCurrentActor();
    if (!a || !isPartyMember(a) || !a->perksOn() || enemies_.empty()) return "";
    std::ostringstream ss;
    bool first = true;
    for (int slot = 0; slot < a->artSlots(); ++slot) {
        const int i = a->artEq[static_cast<size_t>(slot)];
        if (!a->artEquipped(i)) continue;
        const perks::ArtInfo& info = perks::art(classOrd(*a), i);
        const int cd = a->artCd[static_cast<size_t>(i)];
        if (!first) ss << ";";
        first = false;
        ss << slot << "|" << i << "|" << info.name << "|" << cd << "|" << (cd <= 0 && a->status.shackled <= 0 && a->status.exhausted <= 0 ? 1 : 0)
           << "|" << static_cast<int>(info.target) << "|" << perks::cooldownFor(info.cooldown, a->signatureTier());
    }
    return ss.str();
}

// --- v2.33 Rekindling (prestige, PR 5; docs/PRESTIGE.md) ---------------------------------------------
namespace {
/** Kindled Path back to empty (every Ember returns). Re-temper tokens are kept. */
void clearKindledPath(Character& p) {
    p.flames.fill(0);
    p.wardsLearned = 0;
    p.wardEq = {{-1, -1, -1}};
    p.artsLearned = 0;
    p.artEq = {{-1, -1, -1}};
    p.resetBattlePerks();
}
} // namespace

bool Game::rekindleReady() const {
    if (!universalStatsActive() || players_.empty() || !players_[0]) return false;
    const Character& h = *players_[0];
    return questAct4Complete_ && h.level >= prestige::kEligibleLevel && h.prestigeRank < prestige::kMaxRank;
}

std::string Game::rekindleBlockReason() const {
    if (!universalStatsActive()) return "Rekindling is solo only.";
    if (players_.empty() || !players_[0]) return "No hero.";
    const Character& h = *players_[0];
    if (h.prestigeRank >= prestige::kMaxRank) return "Everflame (rank X) is the highest rank.";
    if (!questAct4Complete_) return "Finish the story (Acts 1-4) first.";
    if (h.level < prestige::kEligibleLevel) return "Reach level 50 first.";
    if (soloPlayMode_ != static_cast<int>(SoloPlayMode::STORY) && soloPlayMode_ != static_cast<int>(SoloPlayMode::CRAWL))
        return "Finish or leave this endgame run first.";
    if (gameOver_) return "The party has fallen.";
    if (isInCombat()) return "Not during a fight.";
    return "";
}

bool Game::rekindle(std::string* why) {
    std::string tmp;
    if (!why) why = &tmp;
    syncStatRules();
    *why = rekindleBlockReason();
    if (!why->empty()) return false;
    const int newRank = std::min(prestige::kMaxRank, players_[0]->prestigeRank + 1);
    for (size_t i = 0; i < players_.size(); ++i) {
        if (!players_[i] || players_[i]->isFoe()) continue;
        Character& p = *players_[i];
        // Reset: level 1, the class's starting attributes, Embers back (perks relock until 21).
        const Character fresh("", p.characterClass, "rekindle");
        p.attributes = fresh.attributes;
        p.pendingStatPoints = 0;
        // Odd-CON HP goes with the points; the story +2 HP padding stays (it pairs with the +1 use).
        p.bonusMaxHp = p.bonusMaxResources > 0 ? std::min(std::max(0, p.bonusMaxHp), 2) : 0;
        clearKindledPath(p);
        p.freeRespecs = std::min(9, p.freeRespecs + 1); // a free Re-temper after every Rekindling
        p.level = 1;
        p.xp = 0;
        p.peakLevel = std::max(p.peakLevel, prestige::kEligibleLevel);
        p.prestigeRank = newRank;
        p.prestigeHalf = (i != 0); // the companion Rekindles with you at half strength
        p.rekindleOffered = false;
        p.status = Character::Statuses{};
        p.signatureUses = 0;
        p.specialUsedThisCombat = false;
        p.nextAttackPenalty = 0;
        p.isDowned = p.isDead = p.isStable = false;
        p.deathSaveSuccesses = p.deathSaveFailures = 0;
        p.calculateAC();
        p.applyStatsForLevel();
        p.calculateAC();
        p.currentHp = p.maxHp;
        p.resources = p.maxResources;
    }
    pressure_ = 0;
    rekindleRooms_ = 0;
    lowHpStreak_ = 0;
    const std::string t = prestige::title(newRank);
    lastEvent_ = players_[0]->name + " is " + t + "! Level 1 again, with every piece of gear, gold and the story kept.";
    addChatMessage("System", lastEvent_);
    dmSay("The Lantern gutters, then catches again, brighter. " + players_[0]->name + " walks the road once more as "
          + t + ".");
    addJournalEntry("Rekindled — rank " + std::string(prestige::roman(newRank)) + ".");
    return true;
}

std::string Game::getRekindleState() const {
    std::ostringstream ss;
    if (players_.empty() || !players_[0]) return "";
    const Character& h = *players_[0];
    const int rank = prestige::clampRank(h.prestigeRank);
    const int next = std::min(prestige::kMaxRank, rank + 1);
    const std::string why = rekindleBlockReason();
    const int attune = h.attuned() ? prestige::attuneCap(h.level) : -1;
    ss << "R|" << (rekindleReady() ? 1 : 0) << "|" << (why.empty() ? 1 : 0) << "|" << (h.rekindleOffered ? 1 : 0)
       << "|" << rank << "|" << next << "|" << why << "|" << h.level << "|" << difficulty_ << "|" << attune
       << "|" << prestige::title(rank) << "|" << prestige::title(next)
       << "|" << prestige::wardSlots(rank) << "|" << prestige::artSlots(rank) << "\n";
    for (int r = 1; r <= prestige::kMaxRank; ++r) ss << "P|" << r << "|" << prestige::rankPerk(r) << "\n";
    return ss.str();
}

void Game::markRekindleOffered() {
    if (!players_.empty() && players_[0]) players_[0]->rekindleOffered = true;
}

void Game::clearPrestigeForNightmare() {
    for (auto& up : players_) {
        if (!up || up->isFoe()) continue;
        Character& p = *up;
        if (p.prestigeRank <= 0 && !p.prestigeHalf) continue;
        p.prestigeRank = 0;
        p.prestigeHalf = false;
        // Trim what the rank paid for: flame rank 6, the 3rd slots, prestige Embers, +caps.
        for (int& f : p.flames) f = std::min(f, perks::kFlameMaxRank);
        p.wardEq[2] = -1;
        p.artEq[2] = -1;
        if (p.embersSpent() > p.embersEarned()) {
            clearKindledPath(p);
            p.freeRespecs = std::min(9, p.freeRespecs + 1);
        }
        p.clampAttributesToCaps();
        const int hp = p.currentHp;
        p.calculateAC();
        p.applyStatsForLevel();
        p.currentHp = std::min(p.maxHp, hp);
        p.resources = std::min(p.resources, p.maxResources);
    }
}

std::string Game::clearPrestigeInSave(const std::string& data) {
    if (data.empty() || data.find('|') == std::string::npos) return "";
    try {
        Game tmp{LoadOnlyTag{}};
        tmp.parseSave(data);
        if (tmp.players_.empty()) return "";
        for (auto& p : tmp.players_) if (p) p->statRules = true; // a solo save: count perks as in play
        tmp.clearPrestigeForNightmare();
        return tmp.serialize();
    } catch (...) {
        return "";
    }
}

#ifndef NDEBUG
// DEBUG BUILDS ONLY. Compiled out of release (NDEBUG): the symbol does not exist in the shipped .so.
bool Game::debugJumpToRekindleReady(bool buildLikeAPlayer) {
    syncStatRules();
    if (!universalStatsActive() || players_.empty() || isInCombat()) return false;
    questComplete_ = true;
    questAct2Complete_ = questAct2LedgerFound_ = true;
    questAct3Complete_ = questAct3SealFound_ = true;
    questAct4Complete_ = questAct4EmberFound_ = true;
    if (soloPlayMode_ == static_cast<int>(SoloPlayMode::STORY)) questBeat_ = static_cast<int>(SoloQuestBeat::POST_QUEST);
    for (size_t i = 0; i < players_.size(); ++i) {
        Character* p = players_[i].get();
        if (!p || p->isFoe()) continue;
        while (p->level < Character::kMaxLevel) p->addXpLevels(perks::xpToNext(p->level));
        p->rekindleOffered = false;
        if (buildLikeAPlayer) debugBuildLikeAPlayer(static_cast<int>(i));
    }
    addChatMessage("System", "[debug] Party set to level 50 with the story complete.");
    return true;
}

/**
 * v2.33 rebalance: make a debug level-50 hero look like a real one — every stat point spent
 * like the model hero (half main stat, 30% CON, 20% DEX, overflow when capped), gear at least the
 * normal bonus for the level (the old pieces go to the bag), and every Ember spent on two Arts,
 * Wards and Steady Flames (the same loadout tools/combat_sim uses).
 */
void Game::debugBuildLikeAPlayer(int heroIdx) {
    if (heroIdx < 0 || static_cast<size_t>(heroIdx) >= players_.size() || !players_[static_cast<size_t>(heroIdx)]) return;
    Character& p = *players_[static_cast<size_t>(heroIdx)];
    const int main = p.mainStatIndex();
    static const int kPattern[10] = {-1, -1, -1, -1, -1, 2, 2, 2, 1, 1}; // -1 = main stat
    for (int k = 0, guard = 0; p.pendingStatPoints > 0 && guard < 1000; ++k, ++guard) {
        const int want = kPattern[k % 10] < 0 ? main : kPattern[k % 10];
        if (p.increaseAttribute(want)) continue;
        bool any = false;
        for (int idx : {main, 2, 1, 0, 3, 4, 5}) if (p.increaseAttribute(idx)) { any = true; break; }
        if (!any) break; // every stat at its cap: the rest stay to spend later
    }
    const int cls = static_cast<int>(p.characterClass);
    const int gear = p.level / 2;
    if (!p.equippedWeapon || p.equippedWeapon->type != ItemType::WEAPON || p.equippedWeapon->bonus < gear) {
        if (p.equippedWeapon) p.inventory.push_back(p.equippedWeapon);
        p.equippedWeapon = Item::make("Lampwright's Blade", ItemType::WEAPON, gear, ItemRarity::RARE, cls);
    }
    const int armor = curve::starterArmorBase(p.characterClass) + gear;
    if (!p.equippedArmor || p.equippedArmor->type != ItemType::ARMOR || p.equippedArmor->bonus < armor) {
        if (p.equippedArmor) p.inventory.push_back(p.equippedArmor);
        p.equippedArmor = Item::make("Lampwright's Mail", ItemType::ARMOR, armor, ItemRarity::RARE, cls);
    }
    static const int kArtsPick[5][3] = {{0, 2, 3}, {0, 3, 1}, {1, 3, 0}, {0, 1, 2}, {0, 2, 3}};
    static const int kWardPick[5][3] = {{perks::ASH_VEIL, perks::LAST_LIGHT, perks::SECOND_WICK},
                                        {perks::EMBER_WARD, perks::LAST_LIGHT, perks::ASH_VEIL},
                                        {perks::KINDLE_RUSH, perks::STEADY_HAND, perks::LAST_LIGHT},
                                        {perks::SECOND_WICK, perks::LAST_LIGHT, perks::ASH_VEIL},
                                        {perks::LAST_LIGHT, perks::SPITE_SPARK, perks::ASH_VEIL}};
    const int c = std::clamp(cls, 0, 4);
    std::string why;
    learnPerk(heroIdx, 2, kArtsPick[c][0], &why);
    learnPerk(heroIdx, 1, kWardPick[c][0], &why);
    learnPerk(heroIdx, 1, kWardPick[c][1], &why);
    learnPerk(heroIdx, 2, kArtsPick[c][1], &why);
    if (p.artSlots() > 2) learnPerk(heroIdx, 2, kArtsPick[c][2], &why);
    if (p.wardSlots() > 2) learnPerk(heroIdx, 1, kWardPick[c][2], &why);
    // Steady Flames: the main stat's flame (flame ids 0-5 follow the stats), CON, Bright Edge, Thick Wax, DEX.
    for (int id : {main, static_cast<int>(perks::DEEP_TALLOW), static_cast<int>(perks::BRIGHT_EDGE),
                   static_cast<int>(perks::THICK_WAX), static_cast<int>(perks::QUICKWICK)}) {
        while (p.embersFree() >= 1 && p.flames[static_cast<size_t>(id)] < p.flameCap())
            if (!learnPerk(heroIdx, 0, id, &why)) break;
    }
    p.calculateAC();
    p.applyStatsForLevel();
    p.currentHp = p.maxHp;
    p.resources = p.maxResources;
}
#endif

} // namespace dnd
