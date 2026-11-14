#ifndef DND_GAME_LOGIC_H
#define DND_GAME_LOGIC_H

#include <string>
#include <random>
#include <vector>
#include <memory>
#include <sstream>
#include <algorithm>
#include "Bestiary.h"
#include "StatRules.h"
#include "Perks.h"
#include <array>
#include "SaveCodec.h"
#include "AndroidOut.h"

namespace dnd {

/**
 * Seeded RNG for every combat / loot / saving-throw roll (Code Auditor M5). Replaces the
 * unseeded C rand(), which replayed the same roll sequence on every launch. thread_local so
 * the UI-thread JNI calls and the native render thread never share engine state.
 */
namespace rng {
inline std::mt19937& engine() {
    thread_local std::mt19937 eng{[] {
        std::random_device rd;
        std::seed_seq seq{rd(), rd(), rd(), rd()};
        return std::mt19937(seq);
    }()};
    return eng;
}
/** Uniform integer in [0, n-1]; 0 when n <= 1. Drop-in for the old "rand() % n". */
inline int below(int n) {
    if (n <= 1) return 0;
    std::uniform_int_distribution<int> dist(0, n - 1);
    return dist(engine());
}
} // namespace rng

enum class CharacterClass {
    FIGHTER,
    WIZARD,
    ROGUE,
    CLERIC,
    BARD  // ordinal 4 — append-only so old saves (0–3) stay valid
};

struct Attributes {
    int strength = 10;
    int dexterity = 10;
    int constitution = 10;
    int intelligence = 10;
    int wisdom = 10;
    int charisma = 10;

    /** 5e ability modifier: floor((score - 10) / 2), so 9 -> -1 and 8 -> -1 (L6: was truncating). */
    static int getModifier(int score) {
        const int d = score - 10;
        return (d >= 0) ? d / 2 : -((1 - d) / 2);
    }
};

enum class ItemType {
    WEAPON,
    ARMOR,
    POTION
};

enum class ItemRarity {
    COMMON = 0,
    UNCOMMON = 1,
    RARE = 2,
    EPIC = 3,
    LEGENDARY = 4
};

/** Legendary sub-effect (unlocks at upgrade 20+). Persist as int id. */
enum class LegendarySubEffect : int {
    NONE = 0,
    LIFESTEAL = 1,      // heal 1 + upgrade/10 on hit (weapon)
    ON_HIT_HEAL = 2,    // flat +2 HP on hit (weapon)
    FIGHT_SHIELD = 3    // once-per-fight absorb ~5 HP (armor)
};

/** classTag: -1 = any class; else CharacterClass ordinal.
 *  Legendary: legStat0/1/2 = attribute indices (0=STR..5=CHA), -1 unused.
 *  subEffect unlocks visually/mechanically at upgradeLevel >= 20.
 */
struct Item {
    std::string name;
    ItemType type;
    int bonus = 0;
    ItemRarity rarity = ItemRarity::COMMON;
    int classTag = -1;
    int upgradeLevel = 0;
    int legStat0 = -1;
    int legStat1 = -1;
    int legStat2 = -1;
    int subEffect = 0; // LegendarySubEffect

    static const char* rarityName(ItemRarity r) {
        switch (r) {
            case ItemRarity::COMMON: return "Common";
            case ItemRarity::UNCOMMON: return "Uncommon";
            case ItemRarity::RARE: return "Rare";
            case ItemRarity::EPIC: return "Epic";
            case ItemRarity::LEGENDARY: return "Legendary";
        }
        return "Common";
    }

    static const char* attrShort(int idx) {
        switch (idx) {
            case 0: return "STR";
            case 1: return "DEX";
            case 2: return "CON";
            case 3: return "INT";
            case 4: return "WIS";
            case 5: return "CHA";
            default: return "?";
        }
    }

    static const char* subEffectName(int id) {
        switch (static_cast<LegendarySubEffect>(id)) {
            case LegendarySubEffect::LIFESTEAL: return "Lifesteal (on hit)";
            case LegendarySubEffect::ON_HIT_HEAL: return "On-hit heal";
            case LegendarySubEffect::FIGHT_SHIELD: return "Once-per-fight shield";
            default: return "";
        }
    }

    /** Upgrade level cap by rarity — Legendary reaches 20+ for full power. */
    int maxUpgradeLevel() const {
        switch (rarity) {
            case ItemRarity::COMMON: return 5;
            case ItemRarity::UNCOMMON: return 8;
            case ItemRarity::RARE: return 12;
            case ItemRarity::EPIC: return 16;
            case ItemRarity::LEGENDARY: return 25;
        }
        return 5;
    }

    /** How many legendary bonus stats are active at current upgrade. */
    int activeLegStatCount() const {
        if (rarity != ItemRarity::LEGENDARY) return 0;
        if (upgradeLevel >= 20) return 3;
        if (upgradeLevel >= 15) return 2;
        return 1;
    }

    bool subEffectUnlocked() const {
        return rarity == ItemRarity::LEGENDARY && upgradeLevel >= 20 && subEffect > 0;
    }

    /** +1 per active legendary bonus stat matching this attribute index. */
    int legendaryAttrBonus(int attrIndex) const {
        if (rarity != ItemRarity::LEGENDARY) return 0;
        int n = activeLegStatCount();
        int add = 0;
        if (n >= 1 && legStat0 == attrIndex) add++;
        if (n >= 2 && legStat1 == attrIndex) add++;
        if (n >= 3 && legStat2 == attrIndex) add++;
        return add;
    }

    std::string legendaryBonusText() const {
        if (rarity != ItemRarity::LEGENDARY) return "";
        std::stringstream ss;
        int n = activeLegStatCount();
        ss << "Bonus stats:";
        if (n >= 1 && legStat0 >= 0) ss << " +" << attrShort(legStat0);
        if (n >= 2 && legStat1 >= 0) ss << " +" << attrShort(legStat1);
        if (n >= 3 && legStat2 >= 0) ss << " +" << attrShort(legStat2);
        if (n < 3) {
            ss << " (next at +" << (n == 1 ? 15 : 20) << ")";
        }
        if (subEffectUnlocked()) {
            ss << " | " << subEffectName(subEffect);
        } else if (rarity == ItemRarity::LEGENDARY && subEffect > 0) {
            ss << " | " << subEffectName(subEffect) << " (unlocks at +20)";
        }
        return ss.str();
    }

    static const char* classTagName(int tag) {
        switch (tag) {
            case 0: return "Fighter";
            case 1: return "Wizard";
            case 2: return "Rogue";
            case 3: return "Cleric";
            case 4: return "Bard";
            default: return "Any";
        }
    }

    std::string rarityLabel() const { return rarityName(rarity); }
    /** Plain-English potion effect for UI (empty for non-potions). */
    std::string potionEffectText() const {
        if (type != ItemType::POTION) return "";
        if (name == "Wayfarer's Tonic") {
            return "Drink to restore " + std::to_string(bonus) + " HP and refresh 1 special/supply use. Consumed on use.";
        }
        return "Drink to restore " + std::to_string(std::max(1, bonus)) + " HP. Consumed on use.";
    }
    std::string classLabel() const { return classTagName(classTag); }

    bool canEquip(CharacterClass c) const {
        if (type == ItemType::POTION) return false;
        if (classTag < 0) return true;
        return classTag == static_cast<int>(c);
    }

    int shopCost() const { return shopCostAtBonus(bonus); }
    /** Shop value as if the item had bonus b (b = bonus - upgradeLevel is its value before upgrades). */
    int shopCostAtBonus(int b) const {
        int rarityMult = 1 + static_cast<int>(rarity);
        int base = (std::max(0, b) + 1) * 20 * rarityMult;
        if (type == ItemType::POTION) return std::max(15, b * 3);
        if (classTag >= 0) base += 10; // class-tagged premium
        return std::max(15, base);
    }

    /** Gold to raise this item one upgrade tier (+1 bonus). */
    int upgradeCost() const {
        int rarityBase = 15 * (1 + static_cast<int>(rarity));
        return rarityBase * (upgradeLevel + 1) * (1 + bonus / 2);
    }

    /**
     * Sell-back gold: fair fraction of shop value, scaled by rarity + upgrades (~40–60%).
     * v2.31 Barter: +1% per CHA mod on top (solo only; callers pass 0 online), so the old 65%
     * cap can reach 75% (stats::kSellCapMax). The CHA extra is paid on the item's value before
     * forge upgrades, so an upgrade can never be resold at a profit; always below the buy price.
     */
    int sellPrice(int chaMod = 0) const {
        int buy = shopCost();
        // Common 40% … Legendary 60% base; +2% per upgrade tier, cap 65%.
        int pct = 40 + static_cast<int>(rarity) * 5 + upgradeLevel * 2;
        if (pct > stats::kSellCapBase) pct = stats::kSellCapBase;
        int price = (buy * pct) / 100;
        const int extraPct = std::min(stats::kSellCapMax - pct, stats::sellBonusPct(chaMod));
        if (extraPct > 0) price += (shopCostAtBonus(bonus - upgradeLevel) * extraPct) / 100;
        return std::max(1, price);
    }

    std::string getDescription() const {
        std::stringstream ss;
        ss << name;
        if (bonus != 0) ss << " (+" << bonus << ")";
        if (upgradeLevel > 0) ss << " [+" << upgradeLevel << "]";
        if (rarity == ItemRarity::LEGENDARY) ss << " ★";
        return ss.str();
    }

    /** Persist: name:bonus:W|A|P:rarity:classTag:upgrade:leg0:leg1:leg2:subEffect */
    std::string toToken() const {
        char t = 'W';
        if (type == ItemType::ARMOR) t = 'A';
        else if (type == ItemType::POTION) t = 'P';
        std::stringstream ss;
        // Name is percent-encoded (L8): a ':' in a name would otherwise shift every field.
        ss << savecodec::encode(name) << ":" << bonus << ":" << t << ":"
           << static_cast<int>(rarity) << ":" << classTag << ":" << upgradeLevel
           << ":" << legStat0 << ":" << legStat1 << ":" << legStat2 << ":" << subEffect;
        return ss.str();
    }

    static std::shared_ptr<Item> fromToken(const std::string& tok) {
        if (tok.empty() || tok == "None" || tok.rfind("None:", 0) == 0) return nullptr;
        std::vector<std::string> parts;
        std::string cur;
        for (char c : tok) {
            if (c == ':') { parts.push_back(cur); cur.clear(); }
            else cur.push_back(c);
        }
        parts.push_back(cur);
        if (parts.size() < 3) return nullptr;
        auto item = std::make_shared<Item>();
        item->name = savecodec::decode(parts[0]); // no-op for names without '%'

        // v2.36 #91 (rest of audit L8): no std::stoi / catch(...) — strict parse, default, clamp.
        namespace lim = savecodec::limits;
        item->bonus = savecodec::clampInt(savecodec::parseIntOr(parts[1], 0), lim::kMinItemBonus, lim::kMaxItemBonus);
        char t = parts[2].empty() ? 'W' : parts[2][0];
        if (t == 'A') item->type = ItemType::ARMOR;
        else if (t == 'P') item->type = ItemType::POTION;
        else item->type = ItemType::WEAPON;
        if (parts.size() >= 4) {
            int r = 0;
            if (savecodec::tryParseInt(parts[3], r)) {
                item->rarity = static_cast<ItemRarity>(savecodec::clampInt(r, 0, 4));
            } else {
                // L8: was an empty catch(...) — keep Common, but say so instead of hiding it.
                aout << "Item::fromToken: bad rarity '" << parts[3] << "' for " << item->name << ", using Common" << std::endl;
            }
        }
        if (parts.size() >= 5) {
            item->classTag = savecodec::parseIntOr(parts[4], -1);
            if (item->classTag < -1 || item->classTag > 4) item->classTag = -1; // unknown class -> any class
        }
        if (parts.size() >= 6) {
            item->upgradeLevel = std::max(0, savecodec::parseIntOr(parts[5], 0)); // clamped below
        }
        if (parts.size() >= 7) {
            item->legStat0 = savecodec::parseIntOr(parts[6], -1);
            if (item->legStat0 < -1 || item->legStat0 > 5) item->legStat0 = -1;
        }
        if (parts.size() >= 8) {
            item->legStat1 = savecodec::parseIntOr(parts[7], -1);
            if (item->legStat1 < -1 || item->legStat1 > 5) item->legStat1 = -1;
        }
        if (parts.size() >= 9) {
            item->legStat2 = savecodec::parseIntOr(parts[8], -1);
            if (item->legStat2 < -1 || item->legStat2 > 5) item->legStat2 = -1;
        }
        if (parts.size() >= 10) {
            item->subEffect = savecodec::parseIntOr(parts[9], 0);
            if (item->subEffect < 0 || item->subEffect > lim::kMaxSubEffect) item->subEffect = 0;
        }
        if (static_cast<int>(item->rarity) < 0 || static_cast<int>(item->rarity) > 4)
            item->rarity = ItemRarity::COMMON;
        // Highest cap any rarity has (Legendary +25): never rewrites a real item, only absurd values.
        item->upgradeLevel = savecodec::clampInt(item->upgradeLevel, 0, savecodec::limits::kMaxUpgradeLevel);
        return item;
    }

    static void rollLegendaryExtras(Item& i) {
        if (i.rarity != ItemRarity::LEGENDARY) return;
        // Three distinct bonus stats
        int pool[6] = {0,1,2,3,4,5};
        for (int k = 5; k > 0; --k) {
            int j = rng::below(k + 1);
            int tmp = pool[k]; pool[k] = pool[j]; pool[j] = tmp;
        }
        i.legStat0 = pool[0];
        i.legStat1 = pool[1];
        i.legStat2 = pool[2];
        if (i.type == ItemType::ARMOR) {
            i.subEffect = static_cast<int>(LegendarySubEffect::FIGHT_SHIELD);
        } else {
            i.subEffect = (rng::below(2) == 0)
                ? static_cast<int>(LegendarySubEffect::LIFESTEAL)
                : static_cast<int>(LegendarySubEffect::ON_HIT_HEAL);
        }
    }

    static std::shared_ptr<Item> make(const std::string& n, ItemType t, int b,
                                      ItemRarity r = ItemRarity::COMMON, int cls = -1) {
        auto i = std::make_shared<Item>();
        i->name = n; i->type = t; i->bonus = b; i->rarity = r; i->classTag = cls;
        if (r == ItemRarity::LEGENDARY) rollLegendaryExtras(*i);
        return i;
    }
};

struct Spell {
    std::string name;
    int damageDice;
    int damageSides;
    int cost; // Resource cost
    std::string description;
};

struct Character {
    std::string name;
    std::string uid;
    /** Stable foe archetype id (Bestiary.h), e.g. "enemy_wickthief". Empty for heroes. */
    std::string enemyId;
    CharacterClass characterClass = CharacterClass::FIGHTER;
    int level = 1;
    int xp = 0;
    int maxHp = 1;
    int currentHp = 1;
    Attributes attributes;
    int armorClass = 10;

    int resources = 0;
    int maxResources = 0;
    int pendingStatPoints = 0;
    int gold = 0;

    int initiative = 0;
    bool isDowned = false;
    bool isStable = false;   // 3 death-save successes: skip further saves until heal/damage
    bool isDead = false;     // 3 death-save failures: out of the fight (not party Game Over alone)
    int deathSaveSuccesses = 0;
    int deathSaveFailures = 0;
    /** Solo NPC companion: true = existing ally AI; false = human chooses actions on their turn. */
    bool aiControlled = false;
    /** Fighter Action Surge (and similar): at most once per combat; cleared on initiative. */
    bool specialUsedThisCombat = false;

    // --- v2.31 (universal stats, PR 3) ------------------------------------------------------
    /**
     * Solo rules switch (not saved; Game::syncStatRules sets it for the party in solo play):
     * attribute caps, DEX AC cap, Heft, crit range, Mending, Signature tiers. Online heroes and
     * foes keep the old rules (false).
     */
    bool statRules = false;
    /** Signature tier 1 (main 20+): free class-special uses left this battle. Player field 31. */
    int signatureUses = 0;

    // --- v2.32 (Kindled Path, PR 4; Perks.h) --------------------------------------------------
    /** Steady Flame ranks 0..5 (player field 32). Count only under the solo rules. */
    std::array<int, perks::kFlameCount> flames{};
    /** Wards: learned bit mask, equipped ids (-1 empty), used this battle (player field 33). */
    int wardsLearned = 0;
    std::array<int, perks::kWardSlotsMax> wardEq{{-1, -1, -1}}; // slot 3 (field 39) from rank III
    int wardUsed = 0;
    /** Lantern Arts of this hero's class: learned mask, equipped (-1 empty), cooldowns (field 34). */
    int artsLearned = 0;
    std::array<int, perks::kArtSlotsMax> artEq{{-1, -1, -1}};   // slot 3 (field 40) from rank V
    std::array<int, perks::kArtsPerClass> artCd{};
    /** Free Re-temper (respec) tokens: 1 for every hero saved before v2.32 (field 35). */
    int freeRespecs = 0;
    /** Highest level this hero has reached (field 36; Rekindling catch-up in PR 5). */
    int peakLevel = 1;

    /** Perks work only for solo heroes (the stat-rules switch); online heroes and foes: never. */
    bool perksOn() const { return statRules && !isFoe(); }
    int embersEarned() const { return perks::embersEarned(level, prestigeShare()); }
    int embersSpent() const {
        int s = 0;
        for (int r : flames) s += std::clamp(r, 0, perks::kFlameMaxRankRekindled);
        for (int i = 0; i < perks::kWardCount; ++i) if (wardsLearned & (1 << i)) s += perks::kWardCost;
        for (int i = 0; i < perks::kArtsPerClass; ++i) if (artsLearned & (1 << i)) s += perks::kArtCost;
        return s;
    }
    int embersFree() const { return std::max(0, embersEarned() - embersSpent()); }
    bool wardEquipped(int id) const {
        if (!perksOn() || id < 0) return false;
        for (int k = 0; k < wardSlots(); ++k)
            if (wardEq[static_cast<size_t>(k)] == id) return (wardsLearned & (1 << id)) != 0;
        return false;
    }
    /** An equipped Ward that has not fired this battle. */
    bool wardReady(int id) const { return wardEquipped(id) && !(wardUsed & (1 << id)); }
    void spendWard(int id) { if (id >= 0) wardUsed |= (1 << id); }
    bool artEquipped(int i) const {
        if (!perksOn() || i < 0 || i >= perks::kArtsPerClass) return false;
        for (int k = 0; k < artSlots(); ++k)
            if (artEq[static_cast<size_t>(k)] == i) return (artsLearned & (1 << i)) != 0;
        return false;
    }
    bool artReady(int i) const { return artEquipped(i) && artCd[static_cast<size_t>(i)] <= 0; }
    /** Bright Edge: +2% damage per rank (solo). */
    double damageMult() const {
        return perksOn() ? 1.0 + perks::kBrightEdgePerRank * flames[perks::BRIGHT_EDGE] : 1.0;
    }
    /** New battle: Wards re-arm, Arts are ready. */
    void resetBattlePerks() {
        wardUsed = 0;
        artCd.fill(0);
    }

    // --- v2.29 (combat curve, PR 1) ---------------------------------------------------------
    /**
     * Max HP earned outside the level formula, kept through every level-up (audit bug b):
     * the story's +2 HP padding and the "+level HP" a CON point grants beyond what the formula
     * gives (odd CON points). Saved as player field 28; legacy saves derive it on load.
     */
    int bonusMaxHp = 0;
    /** Rekindling (prestige) rank 0..10, per save slot; saved as player field 27. EL counts 25%. */
    int prestigeRank = 0;
    /** v2.33: the companion Rekindles with the hero at half strength (field 37). */
    bool prestigeHalf = false;
    /** v2.33: the "Ready to Rekindle" explainer was shown for this climb (hero, field 38). */
    bool rekindleOffered = false;
    /** Rank that counts for attributes, caps, HP, Embers and milestones (companion: half). */
    int prestigeShare() const { return prestige::share(prestigeRank, prestigeHalf); }
    /** Same, but only while the solo rules are on (0 online / for foes). */
    int prestigeActive() const { return perksOn() ? prestigeShare() : 0; }
    int flameCap() const { return prestige::flameCap(prestigeActive()); }
    int wardSlots() const { return prestige::wardSlots(prestigeActive()); }
    int artSlots() const { return prestige::artSlots(prestigeActive()); }
    /** XP multiplier from Rekindling (+5% per rank; rank IV+ x1.5 below peak level). */
    double prestigeXpMult() const { return perksOn() ? prestige::xpMult(prestigeRank, level, peakLevel) : 1.0; }
    /** Rekindled at least once (rank >= 1) with the solo rules on; the companion counts too, even
     *  when its half share rounds to 0 (it climbs from level 1 with the same gear). */
    bool attuned() const { return perksOn() && prestige::clampRank(prestigeRank) > 0; }
    /** Attunement: gear counts up to floor(level / 2) + 3 once Rekindled (solo); rank 0: uncapped. */
    int weaponBonusEff() const {
        const int b = (equippedWeapon && equippedWeapon->type == ItemType::WEAPON) ? equippedWeapon->bonus : 0;
        return prestige::attunedBonus(b, level, attuned());
    }
    int armorBonusEff() const {
        const int b = (equippedArmor && equippedArmor->type == ItemType::ARMOR) ? equippedArmor->bonus : 0;
        return prestige::attunedBonus(b, level, attuned());
    }
    /**
     * Encounter-curve foe stats (CombatCurve.h). curveEL > 0 means this foe was built from the
     * Reference Lamplighter: attacks use curveToHit and curveAvgDmgX10 (tenths) instead of the
     * class/stat formulas. 0 = legacy foe (old saves, online Keeper tables) — old rules apply.
     * Saved as enemy fields 20..23 (19 is AC).
     */
    int curveEL = 0;
    int curveRole = 0;          // curve::Role
    int curveToHit = 0;
    int curveAvgDmgX10 = 0;
    /** Telegraph: turns this foe spends winding up before it acts (surprise boss = 1). Field 25. */
    int curveWarnTurns = 0;
    /** Early surprise boss downgraded to the Elite role (field 26). */
    bool curveSurprise = false;
    /** Not saved: freshly spawned, the encounter curve has not been applied yet. */
    bool curvePending = false;
    bool isFoe() const { return !enemyId.empty(); }

    // --- v2.30 (enemy AI, PR 2) -------------------------------------------------------------
    /**
     * Extra max resources outside the class formula (the story's +1 potion/feature padding),
     * kept through every level-up like bonusMaxHp. Saved as player field 29; legacy saves derive
     * it on load (saved max - formula).
     */
    int bonusMaxResources = 0;
    /**
     * Short combat statuses (EnemyAi.h). Rounds left; each ticks down at the end of the bearer's
     * own turn. Saved as one '.'-joined field (player 30 / enemy 27). Rattled stays the existing
     * nextAttackPenalty.
     */
    struct Statuses {
        int burn = 0, burnDmg = 0;   // Burn: burnDmg at the end of each of the bearer's turns
        int marked = 0;              // Marked hero: foes +2 to hit it
        int guarded = 0;             // Guarded foe: +3 AC (already added to armorClass)
        int blind = 0;               // Blinded hero: -4 to hit
        int slowed = 0, slowedFromInit = 0; // Slowed hero: acts last; initiative restored after
        int shackled = 0;            // Shackled hero: no special (class feature) this turn
        int inspired = 0;            // Inspired foe (Rally cry): +2 to hit
        int healedRecently = 0;      // hero healed an ally lately (AI focus; not shown)
        // v2.32 Lantern Arts / Wards (appended to the codec; all end with the battle)
        int shield = 0;              // hero: HP absorbed before damage (Ember Ward, Wick Ward)
        int braced = 0;              // hero: Iron Brace rounds — foes must target it, +4 AC (in armorClass)
        int rallied = 0;             // hero: Warcry / Rallying Verse rounds: +2 to hit and damage
        int spite = 0;               // hero: Spite Spark rounds: +3 to hit and damage
        int smoke = 0;               // hero: Smoke Step: untargetable this many rounds
        int nextCrit = 0;            // hero: the next weapon hit is a crit (Smoke Step)
        int resist = 0;              // hero: Sanctify rounds: new statuses slide off
        int overkindle = 0;          // hero: the next special deals double
        int exhausted = 0;           // hero: after Overkindle, no special or Art for a round
        int sundered = 0;            // foe: -3 AC rounds (already taken off armorClass)
        int stunned = 0;             // foe: skips its turns
        int asleep = 0;              // foe: skips turns until hit
        int bleed = 0, bleedDmg = 0; // foe: damage at the end of each of its turns
        int discord = 0;             // foe: its next turn attacks another foe
        bool any() const { return burn || marked || guarded || blind || slowed || shackled || inspired
                                  || shield || braced || rallied || spite || smoke || resist || exhausted
                                  || sundered || stunned || asleep || bleed || discord; }
    } status;
    /**
     * Foe AI state (solo only; EnemyAi.h). The planned move is the intent chip the UI shows.
     * Saved as one '.'-joined field (enemy 28); the boss phase is enemy field 24.
     */
    struct FoeAi {
        int move = 0;         // ai::Move planned for this foe's next turn (0 = plan on its turn)
        int target = -1;      // hero index, or 100 + foe index for ally moves, -1 none
        int cooldown = 0;     // turns before its signature move is ready again
        int phase = 0;        // bosses: 1..3 (0 = not set yet)
        int flags = 0;        // once-per-fight moves already used (ai::Flag bits)
        int stolenGold = 0;   // Snatch: gold carried (returned if it falls, lost if it flees)
        int stagger = 0;      // damage taken since a telegraphed Slam was planned
        int lastHitBy = -1;   // hero index that hit it since it last planned
        int alt = 0;          // alternating patterns (Warden, Drake, Hydra)
        int lastTarget = -1;  // Hollow Crown: never the same hero twice in a row
        bool critThisRound = false; // Hydra: a crit since its last turn stops the regrowth
    } ai;

    std::shared_ptr<Item> equippedWeapon;
    std::shared_ptr<Item> equippedArmor;
    std::vector<std::shared_ptr<Item>> inventory;

    /** Out-of-range class ordinals (bad save / remote input) fall back to Fighter (H2). */
    static CharacterClass validClass(CharacterClass c) {
        const int v = static_cast<int>(c);
        return (v >= 0 && v <= 4) ? c : CharacterClass::FIGHTER;
    }

    Character(std::string n, CharacterClass c, std::string id = "local")
        : name(std::move(n)), uid(std::move(id)), characterClass(validClass(c)), level(1), xp(0) {
        // Base class arrays once; never re-applied on level-up (spent points must stick).
        switch (characterClass) {
            case CharacterClass::FIGHTER: attributes = {16, 12, 14, 8, 10, 10}; break;
            case CharacterClass::WIZARD:  attributes = {8, 14, 12, 16, 10, 10}; break;
            case CharacterClass::ROGUE:   attributes = {10, 16, 12, 12, 10, 14}; break;
            case CharacterClass::CLERIC:  attributes = {14, 10, 14, 10, 16, 12}; break;
            case CharacterClass::BARD:    attributes = {10, 14, 12, 10, 10, 16}; break;
            default:                      attributes = {16, 12, 14, 8, 10, 10}; break; // Fighter
        }
        applyStatsForLevel();
        currentHp = maxHp;
        resources = maxResources;
        giveStarterGear();
        calculateAC();
    }

    void giveStarterGear() {
        inventory.clear();
        const int cls = static_cast<int>(characterClass);
        if (characterClass == CharacterClass::FIGHTER) {
            equippedWeapon = Item::make("Longsword", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
            equippedArmor = Item::make("Chain Shirt", ItemType::ARMOR, 3, ItemRarity::COMMON, cls);
        } else if (characterClass == CharacterClass::ROGUE) {
            equippedWeapon = Item::make("Shortsword", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
            equippedArmor = Item::make("Leather Armor", ItemType::ARMOR, 1, ItemRarity::COMMON, cls);
        } else if (characterClass == CharacterClass::WIZARD) {
            equippedWeapon = Item::make("Quarterstaff", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
            equippedArmor = Item::make("Traveler Clothes", ItemType::ARMOR, 0, ItemRarity::COMMON, -1);
        } else if (characterClass == CharacterClass::BARD) {
            equippedWeapon = Item::make("Rapier", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
            equippedArmor = Item::make("Leather Armor", ItemType::ARMOR, 1, ItemRarity::COMMON, cls);
        } else {
            equippedWeapon = Item::make("Mace", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
            equippedArmor = Item::make("Scale Mail", ItemType::ARMOR, 4, ItemRarity::COMMON, cls);
        }
    }

    void calculateAC() {
        int dexMod = Attributes::getModifier(effectiveAttr(1));
        if (statRules) dexMod = stats::acFromDex(dexMod); // v2.31: DEX gives at most +6 AC
        const int armorBonus = armorBonusEff(); // v2.33: attuned to level / 2 + 3 when Rekindled
        armorClass = 10 + dexMod + armorBonus;
    }

    /**
     * Level formula HP (no bonusMaxHp): level 1 = hit die + CON mod (heroes at least 12 —
     * v2.29 level-1 floor), later levels hit die/2 + 1 + CON mod (min 1). Uses EFFECTIVE CON so
     * Legendary CON bonuses count (audit bug d).
     */
    int formulaMaxHp() const {
        const int conMod = Attributes::getModifier(effectiveAttr(2));
        const int hd = hitDie();
        int hp = hd + conMod;
        if (!isFoe()) hp = std::max(kHeroLevel1HpFloor, hp);
        if (hp < 1) hp = 1;
        for (int lvl = 2; lvl <= level; ++lvl) {
            int gain = (hd / 2 + 1) + conMod; if (gain < 1) gain = 1; hp += gain;
        }
        return hp;
    }
    static constexpr int kHeroLevel1HpFloor = 12;

    /** Max HP = formula + kept bonus. Never touches currentHp beyond clamping. */
    void recomputeMaxHp() {
        maxHp = std::max(1, formulaMaxHp() + std::max(0, bonusMaxHp));
        if (perksOn() && flames[perks::THICK_WAX] > 0) // v2.32 Thick Wax: +3% max HP per rank
            maxHp = static_cast<int>(maxHp * (1.0 + perks::kThickWaxPerRank * flames[perks::THICK_WAX]));
        if (prestigeActive() > 0) // v2.33 Rekindling: +2% max HP per rank (companion: half)
            maxHp = static_cast<int>(maxHp * prestige::hpMult(prestigeActive()));
        if (currentHp > maxHp) currentHp = maxHp;
    }

    /**
     * Gear changed (equip / unequip / upgrade / transfer): refresh AC and HP from effective
     * attributes. A higher max heals by the difference (you gained it); a lower max clamps.
     */
    void refreshDerivedStats() {
        const int oldMax = maxHp;
        calculateAC();
        recomputeMaxHp();
        if (maxHp > oldMax && !isDowned && !isDead && currentHp > 0) {
            currentHp = std::min(maxHp, currentHp + (maxHp - oldMax));
        }
    }

    /** Initiative modifier: effective DEX (Legendary DEX counts — audit bug d). */
    int initiativeModifier() const { return Attributes::getModifier(effectiveAttr(1)); }
    /** Search (INT check) score: effective INT (audit bug d). */
    int searchScore() const { return effectiveAttr(3); }
    /** Healing Word bonus: effective WIS modifier (audit bug d). */
    int healingWordModifier() const { return Attributes::getModifier(effectiveAttr(4)); }

    /** Class formula max resources for the current level (no bonusMaxResources). */
    int formulaMaxResources() const {
        switch (characterClass) {
            case CharacterClass::ROGUE: return 1 + (level / 3);
            case CharacterClass::WIZARD:
            case CharacterClass::CLERIC:
            case CharacterClass::BARD: return 2 + level;
            case CharacterClass::FIGHTER:
            default: return 1 + (level >= 17 ? 1 : 0);
        }
    }

    /** Recompute HP/resources from current level + attributes. Does NOT reset attributes. */
    void applyStatsForLevel() {
        switch (characterClass) {
            case CharacterClass::ROGUE:
                maxResources = 1 + (level / 3);
                break;
            case CharacterClass::WIZARD:  // spell slots (simplified)
            case CharacterClass::CLERIC:
            case CharacterClass::BARD:    // Cutting Quip / performance uses
                maxResources = 2 + level;
                break;
            case CharacterClass::FIGHTER:
            default:
                maxResources = 1 + (level >= 17 ? 1 : 0); // Action Surge once (twice at 17+), shared with potions
                break;
        }
        maxResources += std::max(0, bonusMaxResources); // v2.30: story +1 kept through level-ups
        maxResources += prestige::extraSpecialUses(prestigeActive()); // v2.33 rank VI: +1 use
        // Level 1: max hit die + CON; later levels: average hit die + CON (+ kept bonus HP).
        recomputeMaxHp();
    }

    /** Ceiling for model / legacy levels (the Reference Lamplighter can sit above the cap). */
    static constexpr int kMaxLevelSafety = 100;
    /** v2.32: heroes stop at level 50 (perks::kLevelCap). */
    static constexpr int kMaxLevel = perks::kLevelCap;

    /**
     * Award XP. One award can now give SEVERAL levels (audit bug c): +2 stat points per level,
     * HP/resources recomputed once and refilled. Returns the number of levels gained.
     */
    int addXpLevels(int amount) {
        // v2.36 #91: saturate (an XP total near INT_MAX used to wrap negative).
        if (amount > 0) xp = savecodec::saturatingAdd(xp, amount, 0, savecodec::limits::kMaxXp);
        int gained = 0;
        // v2.32: the cap is 50; XP for the next level follows perks::xpToNext (unchanged below 20).
        // Stat points: +2 per level to 20, then +2 at 25, 30 ... 50; levels 21-50 give 1 Ember each.
        while (level < kMaxLevel && xp >= perks::xpToNext(level)) {
            xp -= perks::xpToNext(level);
            level++;
            pendingStatPoints += perks::statPointsAt(level);
            ++gained;
        }
        clampLevelToCap();
        peakLevel = std::max(peakLevel, level);
        if (gained > 0) {
            applyStatsForLevel();
            currentHp = maxHp;
            resources = maxResources;
        }
        return gained;
    }
    bool addXp(int amount) { return addXpLevels(amount) > 0; }
    /** v2.36 #91: add gold without overflow (0 .. savecodec::limits::kMaxGold). */
    void gainGold(long long amount) { gold = savecodec::saturatingAdd(gold, amount, 0, savecodec::limits::kMaxGold); }

    /** v2.31: index of the class's main stat (0 STR .. 5 CHA). */
    int mainStatIndex() const { return stats::mainIndex(static_cast<int>(characterClass)); }
    /** Spent (base) score, no gear. */
    int baseAttr(int which) const {
        switch (which) {
            case 0: return attributes.strength;
            case 1: return attributes.dexterity;
            case 2: return attributes.constitution;
            case 3: return attributes.intelligence;
            case 4: return attributes.wisdom;
            case 5: return attributes.charisma;
            default: return 10;
        }
    }
    void setBaseAttr(int which, int v) {
        switch (which) {
            case 0: attributes.strength = v; break;
            case 1: attributes.dexterity = v; break;
            case 2: attributes.constitution = v; break;
            case 3: attributes.intelligence = v; break;
            case 4: attributes.wisdom = v; break;
            case 5: attributes.charisma = v; break;
            default: break;
        }
    }
    /** v2.31 cap on the spent score for this stat at the current level (gear may exceed it). */
    int attrCapFor(int which) const { return stats::attrCap(level, which == mainStatIndex(), prestigeShare()); }
    bool canRaise(int which) const { return !statRules || baseAttr(which) < attrCapFor(which); }
    /** Effective modifier (gear included). */
    int effMod(int which) const { return Attributes::getModifier(effectiveAttr(which)); }
    /** Signature tier from the effective main score (0 when the solo rules are off). */
    int signatureTier() const { return statRules ? stats::signatureTier(effectiveAttr(mainStatIndex())) : 0; }

    /**
     * v2.31 save migration: clamp every spent score above its cap and refund the points.
     * Returns the refunded points; `note` lists the changes ("STR 25 → 22"). HP and AC refresh.
     */
    int clampAttributesToCaps(std::string* note = nullptr) {
        int refunded = 0;
        for (int i = 0; i < 6; ++i) {
            const int cap = attrCapFor(i);
            const int v = baseAttr(i);
            if (v > cap) {
                refunded += v - cap;
                setBaseAttr(i, cap);
                if (note) {
                    if (!note->empty()) *note += ", ";
                    *note += std::string("STRDEXCONINTWISCHA").substr(static_cast<size_t>(i) * 3, 3) + " " + std::to_string(v) + " → " + std::to_string(cap);
                }
            }
        }
        if (refunded > 0) {
            pendingStatPoints += refunded;
            calculateAC();
            recomputeMaxHp();
        }
        return refunded;
    }

    /** Spend one point. v2.31: refused (false) when the stat is at its cap in solo play. */
    bool increaseAttribute(int index) {
        if (pendingStatPoints <= 0) return false;
        if (index < 0 || index > 5) return false;
        if (!canRaise(index)) return false;
        const int formulaBefore = formulaMaxHp();
        switch(index) {
            case 0: attributes.strength++; break;
            case 1: attributes.dexterity++; break;
            case 2: attributes.constitution++; break;
            case 3: attributes.intelligence++; break;
            case 4: attributes.wisdom++; break;
            case 5: attributes.charisma++; break;
        }
        pendingStatPoints--;
        calculateAC();
        if (index == 2) {
            // A CON point is worth +level HP right away (as before). Whatever the formula does
            // not cover (odd points) goes into bonusMaxHp so the next level-up keeps it (bug b).
            const int formulaGain = formulaMaxHp() - formulaBefore;
            bonusMaxHp += std::max(0, level - formulaGain);
            recomputeMaxHp();
            currentHp = std::min(maxHp, currentHp + level);
        }
        return true;
    }

    bool performSavingThrow(int attributeScore, int dc) {
        return (rng::below(20) + 1 + Attributes::getModifier(attributeScore)) >= dc;
    }

    // amountCritical: treat as a critical hit while dying (2 failures) — 5e style.
    void takeDamage(int amount, bool amountCritical = false) {
        if (amount <= 0) return;
        // Already dead: ignore further damage.
        if (isDead) return;
        // While dying (or stable at 0 HP): damage adds death-save failures, does not reset counters.
        if (isDowned || (currentHp <= 0 && isStable)) {
            if (isStable) {
                isStable = false; // damage knocks you unconscious / unstable again
                isDowned = true;
            }
            deathSaveFailures += amountCritical ? 2 : 1;
            if (deathSaveFailures >= 3) {
                isDead = true;
                isDowned = true;
                isStable = false;
                deathSaveFailures = 3;
            }
            currentHp = 0;
            return;
        }
        currentHp = std::max(0, currentHp - amount);
        if (currentHp == 0) {
            isDowned = true;
            isStable = false;
            isDead = false;
            deathSaveSuccesses = 0;
            deathSaveFailures = 0;
        }
    }

    /**
     * Heal. v2.31 Mending: a hero under the solo rules receives +4% per WIS mod (cap +40%) from
     * every source. Returns the amount after Mending (before clamping to max HP).
     */
    int heal(int amount) {
        if (amount <= 0) return 0;
        if (isDead) return 0; // permanently dead — needs magic not modeled here
        if (statRules && !isFoe()) {
            const double m = stats::mending(effMod(4));
            if (m > 0) amount = static_cast<int>(amount * (1.0 + m) + 0.5);
        }
        isDowned = false;
        isStable = false;
        deathSaveSuccesses = 0;
        deathSaveFailures = 0;
        if (currentHp <= 0) {
            currentHp = std::min(maxHp, amount);
        } else {
            currentHp = std::min(maxHp, currentHp + amount);
        }
        return amount;
    }

    /** Party-wipe continue: clear death locks and set HP (difficulty-tuned by caller). */
    void reviveAfterWipe(int hp) {
        isDead = false;
        isDowned = false;
        isStable = false;
        deathSaveSuccesses = 0;
        deathSaveFailures = 0;
        currentHp = std::max(1, std::min(maxHp, hp));
    }

    /** Hard difficulty: strip gold + equipped gear back to class starters. Stats/level kept. */
    void stripGearAndGoldToStarters() {
        gold = 0;
        inventory.clear();
        giveStarterGear();
        calculateAC();
    }

    void addToInventory(const std::shared_ptr<Item>& item) {
        if (item) inventory.push_back(item);
    }

    bool upgradeOwnedItem(std::shared_ptr<Item>& item, std::string& err) {
        if (!item || item->type == ItemType::POTION) {
            err = "Cannot upgrade that.";
            return false;
        }
        if (item->upgradeLevel >= item->maxUpgradeLevel()) {
            err = "Max upgrade (+" + std::to_string(item->maxUpgradeLevel()) + ") for "
                + item->rarityLabel() + ".";
            return false;
        }
        int cost = item->upgradeCost();
        if (gold < cost) {
            err = "Not enough gold! Need " + std::to_string(cost) + "g.";
            return false;
        }
        gold -= cost;
        item->bonus += 1;
        item->upgradeLevel += 1;
        // Legendary stat bonuses unlock at +15/+20 — refresh AC and HP (bug d).
        refreshDerivedStats();
        err.clear();
        return true;
    }

    /** Sum of legendary attr bonuses from equipped weapon+armor. */
    int legendaryAttrBonus(int attrIndex) const {
        int add = 0;
        if (equippedWeapon) add += equippedWeapon->legendaryAttrBonus(attrIndex);
        if (equippedArmor) add += equippedArmor->legendaryAttrBonus(attrIndex);
        return add;
    }

    int effectiveAttr(int which) const {
        int base = 0;
        switch (which) {
            case 0: base = attributes.strength; break;
            case 1: base = attributes.dexterity; break;
            case 2: base = attributes.constitution; break;
            case 3: base = attributes.intelligence; break;
            case 4: base = attributes.wisdom; break;
            case 5: base = attributes.charisma; break;
            default: return 10;
        }
        // v2.32 Steady Flames: +1 per rank on top (like gear, it can pass the cap). Solo only.
        const int flame = perksOn() ? std::clamp(flames[static_cast<size_t>(which)], 0, flameCap()) : 0;
        // v2.33 Rekindling: +1 to every attribute per rank (companion: half). Solo only.
        return base + legendaryAttrBonus(which) + flame + prestigeActive();
    }

    /** Once-per-fight shield charges (armor Legendary sub-effect). Not persisted mid-fight across saves. */
    mutable bool fightShieldUsed = false;
    /** Temporary attack-roll penalty from Bard Cutting Quip (applied then cleared on next attack). */
    int nextAttackPenalty = 0;

    std::string getDetailedSheet() const {
        std::stringstream ss;
        ss << "--- " << name << " ---\n";
        ss << "Lvl " << level << " " << getClassName() << " | Gold: " << gold << "\n";
        if (pendingStatPoints > 0) ss << "POINTS TO SPEND: " << pendingStatPoints << "\n";
        ss << "HP: " << currentHp << "/" << maxHp << " | AC: " << armorClass << "\n";
        if (level >= kMaxLevel && !isFoe()) ss << "XP: " << xp << "/" << xpToNextLevel() << " (level cap " << kMaxLevel << ")\n";
        else ss << "XP: " << xp << "/" << xpToNextLevel() << " (to Lvl " << (level + 1) << ")\n";
        ss << "Resources: " << resources << "/" << maxResources << "\n";
        ss << "STR: " << attributes.strength << " (" << showMod(attributes.strength) << ")\n";
        ss << "DEX: " << attributes.dexterity << " (" << showMod(attributes.dexterity) << ")\n";
        ss << "CON: " << attributes.constitution << " (" << showMod(attributes.constitution) << ")\n";
        ss << "INT: " << attributes.intelligence << " (" << showMod(attributes.intelligence) << ")\n";
        ss << "WIS: " << attributes.wisdom << " (" << showMod(attributes.wisdom) << ")\n";
        ss << "CHA: " << attributes.charisma << " (" << showMod(attributes.charisma) << ")\n";
        ss << "Weapon: " << (equippedWeapon ? equippedWeapon->getDescription() + " [" + equippedWeapon->rarityLabel() + "/" + equippedWeapon->classLabel() + "]" : "None") << "\n";
        ss << "Armor: " << (equippedArmor ? equippedArmor->getDescription() + " [" + equippedArmor->rarityLabel() + "/" + equippedArmor->classLabel() + "]" : "None") << "\n";
        ss << "Bag: " << inventory.size() << " item(s)\n";
        return ss.str();
    }

    /** XP required to leave the current level (matches addXp threshold). */
    /** XP needed for the next level; at the cap the bar is shown full (never 0, the UI divides). */
    int xpToNextLevel() const { return level >= kMaxLevel ? perks::xpToNext(kMaxLevel - 1) : perks::xpToNext(level); }
    /** Level 50 is the top: higher (old, uncapped saves) clamps down, extra XP is a full bar. */
    void clampLevelToCap() {
        if (isFoe()) return;
        if (level > kMaxLevel) level = kMaxLevel;
        if (level >= kMaxLevel) xp = std::min(std::max(0, xp), perks::xpToNext(kMaxLevel - 1));
    }

    std::string getClassName() const {
        switch(characterClass) {
            case CharacterClass::FIGHTER: return "Fighter";
            case CharacterClass::WIZARD: return "Wizard";
            case CharacterClass::ROGUE: return "Rogue";
            case CharacterClass::CLERIC: return "Cleric";
            case CharacterClass::BARD: return "Bard";
        }
        return "Unknown";
    }

    static std::string showMod(int score) {
        int mod = Attributes::getModifier(score);
        return (mod >= 0 ? "+" : "") + std::to_string(mod);
    }

    static std::string getPrimaryStatName(CharacterClass c) {
        if (c == CharacterClass::WIZARD) return "Intelligence";
        if (c == CharacterClass::ROGUE) return "Dexterity";
        if (c == CharacterClass::CLERIC) return "Wisdom";
        if (c == CharacterClass::BARD) return "Charisma";
        return "Strength";
    }

    std::string getSpecialAbilityName() const {
        if (characterClass == CharacterClass::FIGHTER) return "Action Surge";
        if (characterClass == CharacterClass::WIZARD) return "Magic Missile";
        if (characterClass == CharacterClass::ROGUE) return "Sneak Attack";
        if (characterClass == CharacterClass::BARD) return "Cutting Quip";
        return "Healing Word";
    }

    int hitDie() const {
        if (characterClass == CharacterClass::FIGHTER) return 10;
        if (characterClass == CharacterClass::WIZARD) return 6;
        if (characterClass == CharacterClass::ROGUE) return 8;
        if (characterClass == CharacterClass::BARD) return 8;
        return 8; // cleric
    }

    int weaponDamageDie() const {
        // SRD-flavored starter weapons
        if (characterClass == CharacterClass::FIGHTER) return 8;  // longsword
        if (characterClass == CharacterClass::ROGUE) return 6;    // shortsword
        if (characterClass == CharacterClass::WIZARD) return 6;   // quarterstaff
        if (characterClass == CharacterClass::BARD) return 8;    // rapier
        return 6; // mace
    }

    int sneakAttackDice() const {
        // 5e: 1d6 at 1st, +1d6 every odd level
        return 1 + (level - 1) / 2;
    }
};

struct RollResult {
    int total;
    int dieRoll;
    bool isCriticalHit;
    bool isCriticalFail;
};

class CombatSystem {
public:
    static int getPrimaryModifier(const Character& c) {
        if (c.characterClass == CharacterClass::WIZARD) return Attributes::getModifier(c.effectiveAttr(3));
        if (c.characterClass == CharacterClass::ROGUE) return Attributes::getModifier(c.effectiveAttr(1));
        if (c.characterClass == CharacterClass::CLERIC) return Attributes::getModifier(c.effectiveAttr(4));
        if (c.characterClass == CharacterClass::BARD) return Attributes::getModifier(c.effectiveAttr(5));
        return Attributes::getModifier(c.effectiveAttr(0));
    }

    static int proficiencyBonusForLevel(int level) {
        // 2 at 1-4, 3 at 5-8, ... +6 at 17-20; v2.32: frozen at level 20 (power past 20 is perks).
        return perks::proficiency(level);
    }

    /**
     * v2.31 crit range for a hero under the solo rules: INT Weak point (19+ at mod +4, 18+ at +8)
     * and, for a class special at Signature tier 3, one more. Foes and online heroes: 20.
     */
    static int critMin(const Character& attacker, bool special = false) {
        if (!attacker.statRules || attacker.isFoe()) return 20;
        return stats::critMin(attacker.effMod(3), special && attacker.signatureTier() >= 3);
    }

    static RollResult performAttackRoll(const Character& attacker, bool special = false) {
        RollResult r = performAttackRollRaw(attacker);
        if (r.dieRoll >= critMin(attacker, special)) r.isCriticalHit = true; // only counts on a hit
        return r;
    }

    static RollResult performAttackRollRaw(const Character& attacker) {
        int die = rng::below(20) + 1;
        if (attacker.curveEL > 0) {
            // v2.29 encounter-curve foe: to-hit comes from the Reference Lamplighter (CombatCurve.h).
            return { die + attacker.curveToHit, die, (die == 20), (die == 1) };
        }
        int mod = getPrimaryModifier(attacker);
        int pb = proficiencyBonusForLevel(attacker.level);
        int weaponBonus = (attacker.equippedWeapon && attacker.equippedWeapon->type == ItemType::WEAPON) ? attacker.weaponBonusEff() : 0;
        return { die + mod + pb + weaponBonus, die, (die == 20), (die == 1) };
    }

    static int calculateDamage(const Character& attacker, bool isCritical, int extraDice = 0, int extraSides = 6) {
        int sides = attacker.weaponDamageDie();
        int damage = (rng::below(sides) + 1);
        if (isCritical) damage += (rng::below(sides) + 1); // 5e: double damage dice only
        for (int i = 0; i < extraDice; ++i) {
            damage += (rng::below(extraSides) + 1);
            if (isCritical) damage += (rng::below(extraSides) + 1);
        }
        int mod = getPrimaryModifier(attacker);
        int weaponBonus = (attacker.equippedWeapon && attacker.equippedWeapon->type == ItemType::WEAPON) ? attacker.weaponBonusEff() : 0;
        int total = damage + mod + weaponBonus;
        if (attacker.statRules && !attacker.isFoe()) total += stats::heft(attacker.effMod(0)); // v2.31 Heft
        return total < 1 ? 1 : total;
    }
};

class LootSystem {
public:
    /**
     * luckBonus: 0 normal; bosses add mild luck (see noteBossDefeat).
     * Drop chance and rarity use separate rolls (#46 hotfix): trash stays nerfed;
     * bosses better than trash but not BiS-guaranteed.
     * allowLegendary: true ONLY for endgame boss / Boss Raid / Challenge Dungeon / Arena Wave-5 defeat loot.
     *   Never from trash, Search, shop, early GK/SK/Ashen Drake, or pity.
     * legendaryChancePercent: used when allowLegendary (normal endgame/raid = 7;
     *   Challenge Dungeon uses a dynamic chance from prefs — see docs/CHALLENGE_DUNGEON.md;
     *   Arena Wave 5 uses flat 7% like Boss Raid — see docs/ARENA.md).
     * preferClass / preferClass2: party class tags (-1 = unused).
     */
    /**
     * Loot tuning (Code Auditor L10: named instead of magic numbers). Values are unchanged
     * from the #46 hotfix; percentages are out of 100 on rng::below(100).
     */
    struct Tuning {
        static constexpr int kDropGate = 58;            // drop if roll + luck >= 58 (~42% at luck 0)
        static constexpr int kRarityLuckDivisor = 3;    // rarity roll only gets luck/3
        static constexpr int kEpicAt = 98;              // #46: was shared-roll >= 95
        static constexpr int kRareAt = 90;              // #46: was >= 82
        static constexpr int kUncommonAt = 70;          // #46: was >= 68
        static constexpr int kDepthPerBonus = 5;        // +1 bonus per 5 rooms of depth
        static constexpr int kLegendaryExtraBonus = 2;
        static constexpr int kPartyClassPct = 62;       // with a party: 62% party class,
        static constexpr int kNeutralUntilPct = 92;     //   30% unclassed, 8% off-party class
        static constexpr int kSoloClassPct = 40;        // no party info: 40% random class
    };

    /** Per-class item tables, indexed by class id 0..4 (Fighter, Wizard, Rogue, Cleric, Bard). */
    struct NamedItem { const char* name; int armorExtra; };
    static constexpr int kClassCount = 5;
    static constexpr NamedItem kLegendaryWeapons[kClassCount] = {
        {"Crownbreaker", 0}, {"Starfall Focus", 0}, {"Nightwhisper", 0}, {"Dawnward Mace", 0}, {"Songsteel Rapier", 0}};
    static constexpr NamedItem kLegendaryArmor[kClassCount] = {
        {"Aegis of Millhollow", 3}, {"Archsage Mantle", 1}, {"Veilwalker Hide", 2}, {"Sanctum Plate", 3}, {"Troubadour Coat", 1}};
    static constexpr NamedItem kLegendaryWeaponAny = {"Emberdeep Relic Blade", 0};
    static constexpr NamedItem kLegendaryArmorAny = {"Emberdeep Relic Mail", 2};
    static constexpr NamedItem kClassWeapons[kClassCount] = {
        {"Champion's Longsword", 0}, {"Focus Staff", 0}, {"Silent Shortsword", 0}, {"Hallowed Mace", 0}, {"Stage Rapier", 0}};
    static constexpr NamedItem kClassArmor[kClassCount] = {
        {"Bulwark Mail", 2}, {"Scholar Robes", 0}, {"Shadow Leathers", 1}, {"Temple Vestments", 2}, {"Performer's Leathers", 1}};
    static constexpr const char* kGenericWeapons[kClassCount] = {
        "Steel Blade", "Runed Blade", "Shadow Dirk", "War Maul", "Arcane Wand"};
    static constexpr NamedItem kGenericArmor = {"Reinforced Mail", 1};

    /**
     * luckBonus: 0 normal; bosses add mild luck (see noteBossDefeat).
     * Drop chance and rarity use separate rolls (#46 hotfix): trash stays nerfed;
     * bosses better than trash but not BiS-guaranteed.
     * allowLegendary: true ONLY for endgame boss / Boss Raid / Challenge Dungeon / Arena Wave-5 defeat loot.
     *   Never from trash, Search, shop, early GK/SK/Ashen Drake, or pity.
     * legendaryChancePercent: used when allowLegendary (normal endgame/raid = 7;
     *   Challenge Dungeon uses a dynamic chance from prefs — see docs/CHALLENGE_DUNGEON.md;
     *   Arena Wave 5 uses flat 7% like Boss Raid — see docs/ARENA.md).
     * preferClass / preferClass2: party class tags (-1 = unused).
     */
    static std::shared_ptr<Item> generateLoot(int roomDepth, int luckBonus = 0,
                                              int preferClass = -1, int preferClass2 = -1,
                                              bool allowLegendary = false,
                                              int legendaryChancePercent = 7) {
        using T = Tuning;
        if (rng::below(100) + luckBonus < T::kDropGate) return nullptr;

        // Rarity: luck only adds a fraction so bosses stay better than trash, not BiS flood.
        const int rarityRoll = rng::below(100) + (luckBonus / T::kRarityLuckDivisor);
        const int legChance = std::clamp(legendaryChancePercent, 0, 100);
        ItemRarity rarity = ItemRarity::COMMON;
        if (allowLegendary && rng::below(100) < legChance) rarity = ItemRarity::LEGENDARY;
        else if (rarityRoll >= T::kEpicAt) rarity = ItemRarity::EPIC;
        else if (rarityRoll >= T::kRareAt) rarity = ItemRarity::RARE;
        else if (rarityRoll >= T::kUncommonAt) rarity = ItemRarity::UNCOMMON;

        int bonus = (roomDepth / T::kDepthPerBonus) + static_cast<int>(rarity);
        if (rarity == ItemRarity::LEGENDARY) bonus += T::kLegendaryExtraBonus;
        if (bonus < 0) bonus = 0;
        const bool weapon = rng::below(2) == 0;
        const int cls = pickLootClass(preferClass, preferClass2);

        auto make = [&](const NamedItem& it, int classTag) {
            return weapon ? Item::make(it.name, ItemType::WEAPON, bonus, rarity, classTag)
                          : Item::make(it.name, ItemType::ARMOR, bonus + it.armorExtra, rarity, classTag);
        };
        const bool classed = cls >= 0 && cls < kClassCount;
        if (rarity == ItemRarity::LEGENDARY) {
            if (classed) return make(weapon ? kLegendaryWeapons[cls] : kLegendaryArmor[cls], cls);
            return make(weapon ? kLegendaryWeaponAny : kLegendaryArmorAny, -1);
        }
        if (weapon) {
            const int ni = rng::below(kClassCount); // drawn before the class check, as before
            if (classed) return make(kClassWeapons[cls], cls);
            return make(NamedItem{kGenericWeapons[ni], 0}, -1);
        }
        if (classed) return make(kClassArmor[cls], cls);
        return make(kGenericArmor, -1);
    }

    /** Class tag for a drop: biased toward the party's classes (-1 = usable by anyone). */
    static int pickLootClass(int preferClass, int preferClass2) {
        using T = Tuning;
        int party[kClassCount];
        int partyN = 0;
        auto pushParty = [&](int c) {
            if (c < 0 || c >= kClassCount) return;
            for (int i = 0; i < partyN; ++i) if (party[i] == c) return;
            party[partyN++] = c;
        };
        pushParty(preferClass);
        pushParty(preferClass2);

        const int pick = rng::below(100);
        if (partyN == 0) return (pick < T::kSoloClassPct) ? rng::below(kClassCount) : -1;
        if (pick < T::kPartyClassPct) return party[rng::below(partyN)];
        if (pick < T::kNeutralUntilPct) return -1;
        int off[kClassCount];
        int offN = 0;
        for (int c = 0; c < kClassCount; ++c) {
            bool in = false;
            for (int i = 0; i < partyN; ++i) if (party[i] == c) { in = true; break; }
            if (!in) off[offN++] = c;
        }
        return (offN > 0) ? off[rng::below(offN)] : -1;
    }

    // Boss logic keys off the stable enemy ID (Bestiary.h), never the display name.
    static bool isBossId(const std::string& enemyId) { return bestiary::isBoss(enemyId); }
    static int bossTier(const std::string& enemyId) { return bestiary::bossTier(enemyId); }
    static bool isEndgameBossId(const std::string& enemyId) { return bestiary::isEndgameBoss(enemyId); }
};

class SkillSystem {
public:
    static bool performCheck(const Character& /*c*/, int attributeScore, int dc) {
        return (rng::below(20) + 1 + Attributes::getModifier(attributeScore)) >= dc;
    }
};

} // namespace dnd

#endif // DND_GAME_LOGIC_H
