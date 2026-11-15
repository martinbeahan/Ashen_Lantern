// Deterministic non-Mythical EL sweep (mythic_test): 19200 parties x modes; the hash was taken on master d0af1b2 (v2.39).
#include <cstdint>
#include <string>
namespace elsweep {
using namespace dnd;
inline Character hero(int cls, int level, int rank, int extra, int rar, int up) {
    Character p = curve::makeReferenceHero(static_cast<CharacterClass>(cls), level);
    p.prestigeRank = rank; p.statRules = true;
    const auto r = static_cast<ItemRarity>(rar);
    auto w = Item::make("W", ItemType::WEAPON, std::max(0, p.equippedWeapon->bonus + extra), ItemRarity::COMMON, cls);
    auto a = Item::make("A", ItemType::ARMOR, std::max(0, p.equippedArmor->bonus + extra), ItemRarity::COMMON, cls);
    for (auto* it : {&w, &a}) {
        (*it)->rarity = r; (*it)->upgradeLevel = up;
        (*it)->legStat0 = cls % 6; (*it)->legStat1 = 2; (*it)->legStat2 = (cls + 1) % 6; (*it)->subEffect = 1;
    }
    p.equippedWeapon = w; p.equippedArmor = a;
    p.applyStatsForLevel(); p.recomputeMaxHp(); p.calculateAC();
    return p;
}
/** FNV-1a over "el,off,def;" for every case; count in n. */
inline uint64_t sweep(int& n) {
    uint64_t h = 1469598103934665603ull; n = 0;
    auto mix = [&](int v) { std::string s = std::to_string(v) + ","; for (char ch : s) { h ^= static_cast<unsigned char>(ch); h *= 1099511628211ull; } };
    for (int cls = 0; cls < 5; ++cls)
    for (int level : {1, 8, 20, 35, 50})
    for (int rank : {0, 1, 5, 10})
    for (int extra : {-3, 0, 6, 20})
    for (int rar : {0, 3, 4})
    for (int up : {0, 15, 20, 25})
    for (int measured = 0; measured < 2; ++measured)
    for (int mode : {0, 4}) {
        std::vector<curve::PartyMember> pm;
        pm.push_back(curve::memberOf(hero(cls, level, rank, extra, rar, up), measured != 0));
        if (cls != 1) pm.push_back(curve::memberOf(hero(1, level, rank / 2, extra, rar, up), measured != 0));
        const auto e = curve::encounterLevels(pm, mode);
        mix(e.el); mix(e.offEl); mix(e.defEl); ++n;
    }
    return h;
}
} // namespace elsweep
