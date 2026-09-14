#pragma once
/**
 * Authored set-piece encounter templates (AA step 2).
 * Original names/flavor only — SRD-compatible monster concepts (goblin/skeleton/wolf/ogre).
 * Not an official D&D product; no WotC module text.
 */
#include <cstddef>
#include <string>

namespace dnd {
namespace EncounterAuthorship {

enum class TemplateId : int {
    NONE = 0,
    SHADOW_AMBUSH = 1,      // Ambush — goblin flankers
    BONEBOUND_DUO = 2,      // Elite pair — tough skeletons
    RELIC_SNATCH = 3,       // Mini-objective — wolves + goblin guarding a cache
    KINGS_HERALD = 4,       // Boss-adjacent tease — herald of Goblin/Skeleton King
    WOLF_PACK = 5,          // Pack pressure
    OGRE_ROADBLOCK = 6,     // Single bruiser mid-crawl
    COUNT = 7
};

struct TemplateInfo {
    TemplateId id;
    const char* key;           // stable id
    const char* displayName;   // UI / toast
    const char* telegraph;     // DM line when the set-piece starts
    const char* roomFlavor;    // replaces / sets room description flavor
    const char* clearFeed;     // combat feed when cleared
    int minRoomDepth;          // inclusive gate
    int xpBonus;               // added on clear (on top of normal room XP)
    int goldBonus;             // party gold on clear
    bool preferPostStory;      // extra weight after story progress
};

inline const TemplateInfo* table(size_t& outCount) {
    static const TemplateInfo kTable[] = {
        { TemplateId::SHADOW_AMBUSH, "shadow_ambush", "Shadow Ambush",
          "SET-PIECE — Shadow Ambush: scrap-iron blades flash from the dark. Watch your flanks!",
          "Torchlight catches scrap shields — goblin blades wait in the shadows.",
          "Shadow Ambush cleared! Cache coin and a gear scrap for your trouble.",
          /*min*/ 3, /*xp*/ 35, /*gold*/ 18, /*post*/ false },

        { TemplateId::BONEBOUND_DUO, "bonebound_duo", "Bonebound Duo",
          "SET-PIECE — Bonebound Duo: two linked skeletons rise as one. Break the pair!",
          "Twin bone piles rattle in sync. An elite pair claims this ossuary niche.",
          "Bonebound Duo shattered! Heavier purse and a solid find.",
          5, 45, 22, false },

        { TemplateId::RELIC_SNATCH, "relic_snatch", "Relic Snatch",
          "SET-PIECE — Relic Snatch: wolves and a goblin pack-leader guard a sealed cache. Clear them to claim it!",
          "A wax-sealed cache sits on a cracked plinth — guardians pace the chamber.",
          "Relic Snatch won! The cache yields coin, XP, and a guaranteed Common or Uncommon.",
          4, 40, 28, true },

        { TemplateId::KINGS_HERALD, "kings_herald", "King's Herald",
          "SET-PIECE — King's Herald: a crowned runner bellows for their liege. This is a tease — the true king may wait deeper.",
          "A spiked banner and a scrap-iron messenger stand ready. Someone's court is near.",
          "King's Herald silenced! Spoils for blunting the court's advance scout.",
          6, 50, 25, true },

        { TemplateId::WOLF_PACK, "wolf_pack", "Howl Corridor",
          "SET-PIECE — Howl Corridor: a wolf pack closes from both ends. Hold the line!",
          "Wet pawprints and a low chorus of howls fill the corridor.",
          "Howl Corridor survived! Pack trophies and a better haul.",
          4, 38, 20, false },

        { TemplateId::OGRE_ROADBLOCK, "ogre_roadblock", "Ogre Roadblock",
          "SET-PIECE — Ogre Roadblock: a hulking ogre bars the arch. One hard fight, clear reward.",
          "A massive figure blocks the archway, club scraping stone.",
          "Ogre Roadblock cleared! The bruiser's pouch and a solid Common/Uncommon.",
          7, 55, 30, true },
    };
    outCount = sizeof(kTable) / sizeof(kTable[0]);
    return kTable;
}

inline const TemplateInfo* findById(TemplateId id) {
    if (id == TemplateId::NONE) return nullptr;
    size_t n = 0;
    const TemplateInfo* t = table(n);
    for (size_t i = 0; i < n; ++i) {
        if (t[i].id == id) return &t[i];
    }
    return nullptr;
}

inline const char* displayName(TemplateId id) {
    const TemplateInfo* info = findById(id);
    return info ? info->displayName : "";
}

} // namespace EncounterAuthorship
} // namespace dnd
