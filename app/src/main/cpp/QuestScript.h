#ifndef DND_QUEST_SCRIPT_H
#define DND_QUEST_SCRIPT_H

#include <string>

namespace dnd {

/**
 * Solo story acts — original wording only.
 * Act 1 "The Kindling" (beats 1–6; was "Ashen Lantern"). Act 2 "The Tithe" (beats 8–13; was "Millhollow's Debt").
 * Act 3 "Emberdeep" (beats 14–19; was "Emberdeep Breach"). Act 4 "Ashwake Vigil" (beats 20–25).
 * POST_QUEST (7) = procedural rooms after full story (or legacy mid-save).
 * Compatible with 5e SRD monster/rule concepts only. Not an official D&D product.
 */
enum class SoloQuestBeat : int {
    NONE = 0,          // Online host (Lanternkeeper) / client / crawl — procedural
    // Act 1 — The Kindling (was "Ashen Lantern")
    MILLHOLLOW = 1,
    THORNPATH = 2,
    CRYPT_DOORS = 3,
    BONE_GALLERY = 4,
    LANTERN_VAULT = 5,
    RESOLUTION = 6,    // Ashen Shrine
    POST_QUEST = 7,    // Procedural after full story (legacy + post–Act 4)
    // Act 2 — The Tithe
    ACT2_GREEN = 8,
    ACT2_WEIR = 9,
    ACT2_CELLAR = 10,
    ACT2_LOFT = 11,
    ACT2_HALL = 12,
    ACT2_SETTLED = 13,
    // Act 3 — Emberdeep
    ACT3_RUMOR = 14,   // Rumors by the old well
    ACT3_WELL = 15,    // Descent into the well
    ACT3_ROOTS = 16,   // Root labyrinth
    ACT3_RELIC = 17,   // Search for Ember Seal
    ACT3_WARDEN = 18,  // Breach Warden fight
    ACT3_SEALED = 19,  // Breach sealed / resolution
    // Act 4 — Ashwake Vigil
    ACT4_WATCH = 20,   // Dusk vigil on Millhollow green
    ACT4_MARSH = 21,   // Greyfen Marsh
    ACT4_CAUSEWAY = 22,// Fen Causeway
    ACT4_EMBER = 23,   // Search for Vigil Ember
    ACT4_HERALD = 24,  // Ashwake Herald fight
    ACT4_KINDLED = 25  // Vigil Kindled / resolution
};

enum class Difficulty : int {
    EASY = 0,
    MEDIUM = 1,
    HARD = 2,
    NIGHTMARE = 3
};

enum class SoloPlayMode : int {
    STORY = 0,
    CRAWL = 1,
    RAID = 2,
    CHALLENGE = 3,  // Challenge Dungeon (story-complete endgame)
    ARENA = 4,      // Ember Ring Arena (story-complete wave mode)
    ENDLESS = 5     // Endless Deep / Ashen Deep (story-complete depth crawl)
};

inline const char* difficultyName(int d) {
    switch (static_cast<Difficulty>(d)) {
        case Difficulty::EASY: return "Easy";
        case Difficulty::MEDIUM: return "Medium";
        case Difficulty::HARD: return "Hard";
        case Difficulty::NIGHTMARE: return "Nightmare";
        default: return "Easy";
    }
}

inline const char* soloQuestBeatName(int beat) {
    switch (static_cast<SoloQuestBeat>(beat)) {
        case SoloQuestBeat::MILLHOLLOW: return "Millhollow Road";
        case SoloQuestBeat::THORNPATH: return "Thornpath Woods";
        case SoloQuestBeat::CRYPT_DOORS: return "Crypt Doors";
        case SoloQuestBeat::BONE_GALLERY: return "Bone Gallery";
        case SoloQuestBeat::LANTERN_VAULT: return "Lantern Vault";
        case SoloQuestBeat::RESOLUTION: return "Ashen Shrine";
        case SoloQuestBeat::POST_QUEST: return "After the Lantern";
        case SoloQuestBeat::ACT2_GREEN: return "Millhollow Green";
        case SoloQuestBeat::ACT2_WEIR: return "Millrace Weir";
        case SoloQuestBeat::ACT2_CELLAR: return "Flooded Cellar";
        case SoloQuestBeat::ACT2_LOFT: return "Ledger Loft";
        case SoloQuestBeat::ACT2_HALL: return "Collector's Hall";
        case SoloQuestBeat::ACT2_SETTLED: return "The Tithe Broken"; // was "Debt Settled" (id unchanged)
        case SoloQuestBeat::ACT3_RUMOR: return "Wellside Rumor";
        case SoloQuestBeat::ACT3_WELL: return "Old Well Descent";
        case SoloQuestBeat::ACT3_ROOTS: return "Root Labyrinth";
        case SoloQuestBeat::ACT3_RELIC: return "Ember Seal Niche";
        case SoloQuestBeat::ACT3_WARDEN: return "Breach Threshold";
        case SoloQuestBeat::ACT3_SEALED: return "Breach Sealed";
        case SoloQuestBeat::ACT4_WATCH: return "Vigil Watch";
        case SoloQuestBeat::ACT4_MARSH: return "Greyfen Marsh";
        case SoloQuestBeat::ACT4_CAUSEWAY: return "Fen Causeway";
        case SoloQuestBeat::ACT4_EMBER: return "Vigil Niche";
        case SoloQuestBeat::ACT4_HERALD: return "Ashwake Threshold";
        case SoloQuestBeat::ACT4_KINDLED: return "Vigil Kindled";
        default: return "Wander";
    }
}

inline const char* soloQuestActTitle(int beat) {
    const auto b = static_cast<SoloQuestBeat>(beat);
    if (b >= SoloQuestBeat::ACT4_WATCH && b <= SoloQuestBeat::ACT4_KINDLED)
        return "Ashwake Vigil";
    if (b >= SoloQuestBeat::ACT3_RUMOR && b <= SoloQuestBeat::ACT3_SEALED)
        return "Emberdeep";
    if (b >= SoloQuestBeat::ACT2_GREEN && b <= SoloQuestBeat::ACT2_SETTLED)
        return "The Tithe";
    if (b >= SoloQuestBeat::MILLHOLLOW && b <= SoloQuestBeat::RESOLUTION)
        return "The Kindling";
    return "";
}

inline bool isAct1Beat(int beat) {
    return beat >= static_cast<int>(SoloQuestBeat::MILLHOLLOW)
        && beat <= static_cast<int>(SoloQuestBeat::RESOLUTION);
}

inline bool isAct2Beat(int beat) {
    return beat >= static_cast<int>(SoloQuestBeat::ACT2_GREEN)
        && beat <= static_cast<int>(SoloQuestBeat::ACT2_SETTLED);
}

inline bool isAct3Beat(int beat) {
    return beat >= static_cast<int>(SoloQuestBeat::ACT3_RUMOR)
        && beat <= static_cast<int>(SoloQuestBeat::ACT3_SEALED);
}

inline bool isAct4Beat(int beat) {
    return beat >= static_cast<int>(SoloQuestBeat::ACT4_WATCH)
        && beat <= static_cast<int>(SoloQuestBeat::ACT4_KINDLED);
}

inline bool isScriptedSoloBeat(int beat) {
    return isAct1Beat(beat) || isAct2Beat(beat) || isAct3Beat(beat) || isAct4Beat(beat);
}

/** Previous scripted beat for wipe rollback. */
inline int previousScriptedBeat(int beat) {
    if (beat == static_cast<int>(SoloQuestBeat::ACT4_WATCH))
        return static_cast<int>(SoloQuestBeat::ACT3_SEALED);
    if (isAct4Beat(beat) && beat > static_cast<int>(SoloQuestBeat::ACT4_WATCH))
        return beat - 1;
    if (beat == static_cast<int>(SoloQuestBeat::ACT3_RUMOR))
        return static_cast<int>(SoloQuestBeat::ACT2_SETTLED);
    if (isAct3Beat(beat) && beat > static_cast<int>(SoloQuestBeat::ACT3_RUMOR))
        return beat - 1;
    if (beat == static_cast<int>(SoloQuestBeat::ACT2_GREEN))
        return static_cast<int>(SoloQuestBeat::RESOLUTION);
    if (isAct2Beat(beat) && beat > static_cast<int>(SoloQuestBeat::ACT2_GREEN))
        return beat - 1;
    if (isAct1Beat(beat) && beat > static_cast<int>(SoloQuestBeat::MILLHOLLOW))
        return beat - 1;
    return beat;
}

struct QuestBeatScript {
    int beatId;
    const char* title;
    const char* roomDescription;
    const char* dmEnter;
    const char* journalOnEnter;
    /** Wren's [Lantern] line on entering the beat (after she enters the glass at beat 6). */
    const char* wrenOnEnter = nullptr;
    /** Wren's [Lantern] line when the beat's fight is cleared. */
    const char* wrenOnClear = nullptr;
};

// Act 1 — "The Kindling" (beat IDs 1–6; was "Ashen Lantern"). Original wording only.
// Hook: the protagonist's mother is kindled into the Lantern; Wickthieves steal it mid-ceremony,
// the shadows stand up, and sister Wren gives chase. First fight (a Wickthief Scout) is in beat 1,
// so it starts within ~60s. Act 1 ends with Wren relighting the Lantern and a masked watcher.
// Same beat ids / titles as before, so saves, wipe-rollback and stage backdrops keep working.
inline const char* const kKindlingTitleCard =
    "In Millhollow, we do not bury our dead. We burn them into the light.";

inline const QuestBeatScript kAshenLanternBeats[] = {
    {
        1,
        "Millhollow Road",
        "The green at the edge of Millhollow Road, night of the kindling. A ring of black ribbons, a pyre "
        "burned down to embers, and the Ashen Lantern on its iron post, flaring as your mother's soul "
        "enters the glass. Then hooded figures with candle stubs burning on their heads tear the Lantern "
        "from its hook and run. The light goes with them. Every shadow on the green stands up on its own. "
        "Your sister Wren is already sprinting into the dark after the thieves. One Wickthief turns back "
        "to stop you.",
        "Tonight is your mother's kindling. Her soul has barely touched the glass when the Lantern is "
        "stolen. Wren gives chase. Strike down the Wickthief Scout and follow her.",
        "Mother's kindling. Wickthieves stole the Ashen Lantern mid-ceremony; the shadows rose and Wren "
        "chased the thieves toward Hollowbarrow."
    },
    {
        2,
        "Thornpath Woods",
        "Thornpath claws at your cloak. Wren's bootprints run ahead between the roots. Lantern-glass "
        "glints in the mud, and when you lift a shard it whispers in your mother's voice: "
        "\"Don't let them light me again.\" Ahead, something made of smoke hunts by sound.",
        "The woods remember light. A Gloamhound remembers hunger: smoke for a body, two teal eyes, "
        "a jaw too long. Stay sharp.",
        "Thornpath Woods: a shard of the Lantern whispered in Mother's voice: \"Don't let them light me again.\""
    },
    {
        3,
        "Crypt Doors",
        "Stone doors seal Hollowbarrow Crypt. Ash-stained runes spiral around a cold keyhole, and Wren's "
        "knife is wedged in the rune-lock, snapped at the hilt. She got here first. A draft smells of old "
        "bone and extinguished oil.",
        "Wren is ahead of you, alone, inside the crypt. Search for a rune-key to open the wards cleanly, "
        "or force a way after her.",
        "Wren's broken knife was jammed in Hollowbarrow's rune-lock. She went in alone."
    },
    {
        4,
        "Bone Gallery",
        "A long gallery of burial niches. The Ashbound sit in them too neatly: tall, thin, cracked like "
        "burnt paper, ember light leaking between their ribs. Some still wear Millhollow funeral ribbons.",
        "Steel yourselves. These dead were kindled once. What is left of them does not sleep kindly.",
        "The Bone Gallery: Ashbound in the niches, some still wearing Millhollow funeral ribbons."
    },
    {
        5,
        "Lantern Vault",
        "The vault opens on a stone plinth. The Ashen Lantern rests there, cold iron and grey glass, "
        "guttering. Wren lies collapsed beside it, breathing shallow. Beyond the plinth, the vault's "
        "undying custodian unfolds from the dark.",
        "There: the Lantern, and Wren. The Lantern Sexton guards them both. Watch for one brutal surge, "
        "then claim the light.",
        "The Lantern Vault: the stolen Lantern on its plinth, Wren collapsed beside it, the Lantern Sexton on guard."
    },
    {
        6,
        "Ashen Shrine",
        "A shallow shrine niche faces the vault path. Soot rings the altar. The Lantern gutters in your "
        "hands; it will not burn again without a soul. Wren, wounded and fading into the Gloam, presses "
        "her palm to the glass before you can stop her. The light flares brighter than it has in decades, "
        "and it speaks with her voice.",
        "The Lantern is guttering. It will not burn again without a soul to feed it.",
        "Wren gave herself to the Lantern at the Ashen Shrine. It burns again, in her voice. A grey-masked "
        "watcher wrote in a ledger and vanished."
    }
};

// Act 2 — The Tithe (beat IDs 8–13; was "Millhollow's Debt"). Original wording only.
// The grey collectors were never after grain: Millhollow owes the Grey Tithe one kindling per
// generation, the elders have been choosing who, and the protagonist's name is next. Same beat
// ids, enemy ids, Search flags and gold as before, so saves and stage backdrops keep working.
inline const QuestBeatScript kTitheBeats[] = {
    {
        8,
        "Millhollow Green",
        "Dawn after Wren went into the glass. Grey coats stand on Millhollow's green, wet with dew, "
        "in a line too straight for honest work. Elder Hesk will not meet your eye. A Tithe Enforcer "
        "reads from a ribboned scroll in the slow voice of a funeral: a tithe overdue, a name owed, "
        "the light still hungry.",
        "Act 2 — The Tithe. These collectors are not here for grain. Find out what Millhollow owes, "
        "and who has been paying it.",
        "Grey Tithe collectors came to Millhollow Green. They talk about the debt like a funeral.",
        "Hesk is sweating through his collar. I don't like that. I don't like any of this."
    },
    {
        9,
        "Millrace Weir",
        "The millrace foams over a timber weir. Under the dripping boards a smugglers' run leads to the "
        "mill cellar, and the sacks stacked along it are not grain: stoppered jars of ash, each tied "
        "with a black funeral ribbon.",
        "The weir path is slick, and something quick and mean is guarding the jars.",
        "The millrace hides a smugglers' run: jars of ash in black ribbons, not grain.",
        "They used to hide sugar under here. Now it's people."
    },
    {
        10,
        "Flooded Cellar",
        "Knee-deep water fills the mill cellar. Crates float among funeral ribbons and wax-stoppered ash. "
        "A rusted grate leads up toward the dry loft. One of the Ashbound on the steps still wears a "
        "Millhollow ribbon, tied the way only one family ever tied them.",
        "The cellar is not empty. Clear a path up to the loft where the Tithe keeps its books.",
        "The flooded cellar is full of tithed ash. One of the Ashbound wore a Millhollow ribbon.",
        "That ribbon. That's Aunt Lise's knot.",
        "Aunt Lise. They took her the winter I turned nine. They said she was unwell. She was only sad."
    },
    {
        11,
        "Ledger Loft",
        "A dry loft above the mill. Shelves of wax-sealed books. One lies open, the ink still wet: not "
        "sums, but souls. Mother's name, dated last night, marked PAID. Below it, your name, the next "
        "entry, in a hand that is not Hesk's. Wren's name is there too, struck through, with a note: "
        "already kindled — surplus.",
        "Search the loft for the soul-ledger. Soft open — no fight unless you leave it behind.",
        "The Ledger Loft: a soul-ledger. Mother marked PAID. Your name is next. Wren: \"surplus\".",
        "Surplus. They wrote surplus."
    },
    {
        12,
        "Collector's Hall",
        "The mill's upper hall, hung with grey coats. Collector Sallow Venn waits with the ledger-seal on "
        "a chain. He was born in Millhollow and sent away as a boy, the year the elders chose his father. "
        "He looks tired, and almost honest.",
        "Sallow Venn will not hand over the ledger-seal. He collects only what the village already agreed to.",
        "Collector Sallow Venn waited in the mill hall with the ledger-seal.",
        "He's from here. I hate that I feel sorry for him.",
        "He's still breathing. He's on his knees. Don't decide yet — take him out to the green."
    },
    {
        13,
        "The Tithe Broken",
        "Millhollow's elders gather on the green. The soul-ledger burns in a clay bowl and the local "
        "contract breaks with it; ash drifts like snow. Hesk, finally looking at you, admits the elders "
        "have been choosing who goes into the light. Sallow Venn kneels in the wet grass. \"The Lantern "
        "was never yours,\" he says. \"Ask what's under the well.\"",
        "The Tithe is broken here, for now. Rest if you need — then Onward for Act 3: Emberdeep.",
        "The Tithe Broken — the soul-ledger burned; Hesk confessed; Venn pointed at the well.",
        "Hesk held the quill. All those years. He held the quill."
    }
};

// Act 3 — Emberdeep (beat IDs 14–19; was "Emberdeep Breach"). Under the well is where Millhollow
// has dumped its spent souls for three hundred years; the Ashbound are the village's own dead, and
// the Breach Warden is the First Lamplighter, bound as a gate. He names Maren Vey.
inline const QuestBeatScript kEmberdeepBeats[] = {
    {
        14,
        "Wellside Rumor",
        "Ash and warm steam rise from Millhollow's old well. Claw marks score the curb, and a child "
        "points at them without a word. When the light is done with someone, an old woman mutters, "
        "they go down. A Tithe scavenger is already picking through what crawled back up.",
        "Act 3 — Emberdeep. The well is not a well. Go down and see what Millhollow has been burying.",
        "Ash and steam at the old well. When the light is done with someone, they go down.",
        "That's our well. That's where they put people when the light was done with them."
    },
    {
        15,
        "Old Well Descent",
        "The shaft opens into a gallery of roots and old brick. Ember veins pulse in the stone. Wet "
        "footprints spiral down, not all of them adult, and a small shoe hangs caught in the rope-run.",
        "The descent is watched. Clear the scavengers before the roots close in.",
        "Down the old well: ember veins, wet footprints, a child's shoe in the rope-run.",
        nullptr,
        "A rope-boy. He hauled the ash-buckets and he fell. I don't even know his name, and that's worse."
    },
    {
        16,
        "Root Labyrinth",
        "A maze of living roots and collapsed niches, lit by pale fungus. The Ashbound here do not wait "
        "in rows. They come out of the walls, and every one of them is wearing a Millhollow face.",
        "These are Millhollow's dead. Keep together, and do not look away.",
        "The Root Labyrinth: the Ashbound are the village's own ancestors.",
        "I know them. Old Pell. The baker. Don't you dare look away.",
        "The baker. I stole the heels off his loaves every morning. His hands were always flour. They're ash now."
    },
    {
        17,
        "Ember Seal Niche",
        "A quiet niche cut into warm stone. The Ember Seal rests in a soot-black socket: an iron signet, "
        "still warm, the First Lamplighter's own. Words run around the rim. No foes here unless you "
        "leave without it.",
        "Search for the Ember Seal — the only key that closes the wound kindly. Then press Onward.",
        "The Ember Seal Niche: the First Lamplighter's signet, still warm."
    },
    {
        18,
        "Breach Threshold",
        "A cracked arch frames a vertical wound of ember-light. Heat warps the air. Bound across it like "
        "a bar across a door stands the First Lamplighter: your ancestor, made into a gate. He does not "
        "want to fight you. He cannot do anything else.",
        "The First Lamplighter is bound as a gate. Free him the only way left, then seal the wound.",
        "The First Lamplighter, bound as the gate of the ember wound.",
        "He has our grandmother's eyes. Oh, no. No.",
        "He's family. We just… we just had to."
    },
    {
        19,
        "Breach Sealed",
        "The Ember Seal waits over the wound. It will close the well either way. Below, the ash-dead of "
        "three hundred years are still down there, and the Lantern in your hands feels heavier for it.",
        "The seal will close the wound. What happens to the souls below is yours to choose.",
        "Breach Sealed — the First Lamplighter named Maren Vey before he went still."
    }
};

// Act 4 — Ashwake Vigil (beat IDs 20–25; title kept). The Vigil Spire is a Tithe siphon, Greyfen
// was a town, and at the Spire the Chandler unmasks as Maren Vey, takes a shard of the Lantern with
// part of Wren in it, and walks down into the Ashen Deep. Last line: "Follow the ash."
inline const QuestBeatScript kAshwakeVigilBeats[] = {
    {
        20,
        "Vigil Watch",
        "Dusk on Millhollow's green. The Lantern burns thinner every night now, as if something far off "
        "is drinking it, and Wren's voice is thinner with it. Hesk points west, toward Greyfen and its "
        "abandoned fen-tower. A Tithe Watcher is already on the green, checking the draw.",
        "Act 4 — Ashwake Vigil. Something in the west is drinking the Lantern. Cross Greyfen and find out what.",
        "The Lantern thins at dusk, and Wren with it. Something west in Greyfen is drinking the light.",
        "I'm getting quieter. Something is drinking me."
    },
    {
        21,
        "Greyfen Marsh",
        "Greyfen sucks at boots and cloak. Drowned huts lean in black water, and a lantern-post stands in "
        "the middle of it all, its glass cold. This was a town. Something quick and wet scavenges among "
        "the willows.",
        "This was a town once. Clear the scavengers and keep to the causeway.",
        "Greyfen Marsh: drowned huts and a cold lantern-post. This was a town.",
        nullptr,
        "They had a lantern. It went out. That's all it takes."
    },
    {
        22,
        "Fen Causeway",
        "A raised timber path crosses the deeper fen. Watch-crypts rise from the mist beside it. The "
        "Ashbound on the posts wear ribbons, but not Millhollow's: Greyfen's dead, knotted in a way no "
        "one alive remembers.",
        "Greyfen's own dead hold the causeway. Press through — the Vigil Niche waits beyond.",
        "The Fen Causeway: Ashbound in Greyfen ribbons, a second town's dead.",
        "I don't know these people. That's worse."
    },
    {
        23,
        "Vigil Niche",
        "A quiet alcove under the fen-tower. The Vigil Ember sits in a soot cup, an iron cinder, and it is "
        "not a flame. It is a mouth. A scrap of Tithe rite in an old, careful hand is tucked behind the "
        "cup. No foes here unless you leave without it.",
        "Search for the Vigil Ember — not a light to kindle, but the valve. Recover it to reverse the draw.",
        "The Vigil Niche: the Vigil Ember is a valve, not a flame.",
        "This isn't a light. It's a throat."
    },
    {
        24,
        "Ashwake Threshold",
        "The Vigil Spire's threshold yawns, ash-mist pouring from cracked glass. On the stair the "
        "Bell-Herald of the Tithe lifts its bell, and each time it rings, the Lantern goes quiet in your hands.",
        "The Bell-Herald guards the siphon. Silence it, then turn the Vigil Ember in the Spire.",
        "The Bell-Herald of the Tithe at the Vigil Spire's threshold.",
        "That bell — I can't hear myself when it —",
        "It's quiet. Good. I hate that bell."
    },
    {
        25,
        "Vigil Kindled",
        "You seat the Vigil Ember and turn it, and the draw reverses. Light slams back east toward "
        "Millhollow. In the flare the Chandler's mask fails: Maren Vey, old as the village, burning from "
        "the inside, stands on the stair. She does not fight. She snaps a shard from the Lantern, and "
        "Wren cries out inside the glass. Then Maren turns and walks down, into a dark that is the Ashen Deep.",
        "Follow the ash. Rest, return to the menu, or press Onward: Lamplighter's Rounds, the Ashen Deep, "
        "the Ember Ring, the Cinder Crucible and the Three Drowned Lanterns are open.",
        "Ashwake Vigil — the siphon reversed. Maren Vey unmasked, took a shard of the Lantern with part of "
        "Wren in it, and walked down into the Ashen Deep.",
        "She's walking down. I can still see her. Follow the ash."
    }
};

inline const QuestBeatScript* findQuestBeat(int beatId) {
    for (const auto& b : kAshenLanternBeats) {
        if (b.beatId == beatId) return &b;
    }
    for (const auto& b : kTitheBeats) {
        if (b.beatId == beatId) return &b;
    }
    for (const auto& b : kEmberdeepBeats) {
        if (b.beatId == beatId) return &b;
    }
    for (const auto& b : kAshwakeVigilBeats) {
        if (b.beatId == beatId) return &b;
    }
    return nullptr;
}

} // namespace dnd

#endif // DND_QUEST_SCRIPT_H
