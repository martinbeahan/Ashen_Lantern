#ifndef DND_MEMORY_CATALOG_H
#define DND_MEMORY_CATALOG_H

#include <string>

/**
 * The Memory Ledger (v2.26) — Ember Memories: named soul-fragments that lodge in the Lantern.
 * Lore only: no stats, no items, no set bonus. Original wording only.
 *
 * `id` is sacred (the Kotlin meta-pref store keys off it); everything else is display text and may
 * be re-worded freely. Grants are decided in Game.cpp; the per-hero store lives in Kotlin meta prefs
 * (MemoryLedger.kt) and never in save_state or a Host/Join snapshot.
 */
namespace dnd {
namespace memory {

enum Source : int { SRC_STORY = 0, SRC_CRAWL = 1, SRC_DEEP = 2, SRC_ARENA = 3, SRC_RAID = 4 };
enum Ribbon : int { RIB_WREN = 0, RIB_MAREN = 1, RIB_LANTERN = 2 };
/** How a story memory is earned (used by live grants and by the progress / migration pass). */
enum Trigger : int {
    T_NONE = 0,         // not a story memory
    T_ENTER = 1,        // entering the beat (guaranteed)
    T_CLEAR = 2,        // clearing the beat's fight (guaranteed)
    T_SEARCH_FLAG = 3,  // Search; the beat's item flag is safety-set at the act resolution (guaranteed)
    T_SEARCH_ONLY = 4,  // Search only (missable)
    T_WELL_CHOICE = 5,  // the Act 3 choice is made (or defaulted)
    T_CLEAR_OR_SEARCH = 6
};
/** Which story choice colours a body: 0 none, 1 Venn (alt1 = spared, alt2 = ended), 2 well
 *  (alt1 = sealed in, alt2 = let rest). Undecided shows `body` alone. */
enum Variant : int { VAR_NONE = 0, VAR_VENN = 1, VAR_WELL = 2 };

struct MemoryDef {
    const char* id;
    const char* title;
    const char* speaker;
    const char* place;
    const char* body;
    const char* wren;     // optional "Wren, from the glass" remark (nullptr = none)
    int source;
    int ribbon;
    int beat;             // story beat (0 for other sources)
    int trigger;
    int variant = VAR_NONE;
    const char* alt1 = nullptr; // appended (Venn) / replaces body (well) — see bodyFor()
    const char* alt2 = nullptr;
};

inline const MemoryDef kMemories[] = {
    // ---- Act 1 — The Kindling (6) ----
    {"mem_kindling_night", "The Kindling Night", "The green, that night", "Millhollow Road",
     "Black ribbons in a ring. Mother's hand going still in yours, and the glass taking the warmth out of "
     "her like a breath drawn in. Then the candle-headed thieves, and every shadow on the green standing up.",
     "I ran. You stayed with her. Both of those were right.", SRC_STORY, RIB_LANTERN, 1, T_CLEAR_OR_SEARCH},
    {"mem_mothers_whisper", "Don't Let Them Light Me", "Mother, in a shard", "Thornpath Woods",
     "A sliver of grey glass in the mud, warm as a cheek. It speaks once, in her voice, very tired: "
     "\"Don't let them light me again.\" Then it is only glass.",
     "She knew what it was. She knew, and she went in anyway.", SRC_STORY, RIB_WREN, 2, T_ENTER},
    {"mem_wrens_knife", "The Broken Knife", "Wren's knife", "Crypt Doors",
     "Snapped at the hilt and jammed in the rune-lock. She bought it with sugar money when she was nine, "
     "named it Fang, and never cut anything with it but string.",
     "Don't laugh. It was a good knife.", SRC_STORY, RIB_WREN, 3, T_SEARCH_FLAG},
    {"mem_funeral_ribbon", "Ribbon in the Niche", "A Millhollow funeral ribbon", "Bone Gallery",
     "Black wool in the village knot, around the wrist of something kindled before you were born. "
     "Whoever tied it loved them. Whatever wears it now does not remember.",
     "We tie them so the light knows who we sent. Did it ever care?", SRC_STORY, RIB_LANTERN, 4, T_CLEAR},
    {"mem_sextons_watch", "The Sexton's Watch", "The Lantern Sexton", "Lantern Vault",
     "\"I was set to keep the light from thieves. No one told me what to do when the light was the thief. "
     "I kept it anyway. Keeping was all I had left.\"",
     nullptr, SRC_STORY, RIB_LANTERN, 5, T_CLEAR},
    {"mem_wrens_palm", "Wren's Palm", "Wren", "Ashen Shrine",
     "Her palm flat on the glass before you can stop her. Cold, then bright, then her voice coming from "
     "the wrong place: inside the light, a hand's width from your face.",
     "Don't you dare cry. I mean it. I can see you.", SRC_STORY, RIB_WREN, 6, T_ENTER},

    // ---- Act 2 — The Tithe (5) ----
    {"mem_tithe_sermon", "The Tithe Sermon", "A grey collector", "Millhollow Green",
     "\"Every family owes the light one soul a generation. It isn't cruelty, it's arithmetic. The village "
     "agreed. The village has always agreed. You simply weren't told.\"",
     "Arithmetic. He said it like a prayer.", SRC_STORY, RIB_LANTERN, 8, T_SEARCH_ONLY},
    {"mem_lises_ribbon", "Aunt Lise", "Wren's great-aunt", "Flooded Cellar",
     "\"They said I was unwell. I was only sad, and sad is cheap to take. I tied my own ribbon. I wanted "
     "to get the knot right, for the girls.\"",
     "She did get it right. I'd know that knot anywhere.", SRC_STORY, RIB_WREN, 10, T_CLEAR},
    {"mem_your_name", "The Next Entry", "The soul-ledger", "Ledger Loft",
     "The page is still wet. Your name, in a hand that is not Hesk's. Below it, Wren's name, struck "
     "through, and a note: already kindled — surplus.",
     "Surplus. They wrote surplus.", SRC_STORY, RIB_WREN, 11, T_SEARCH_FLAG},
    {"mem_venn_father", "The Exile's Father", "Sallow Venn", "The Tithe Broken",
     "\"I was ten when they chose my father. Hesk held the quill. My mother packed one bag and walked us "
     "out of Millhollow before dawn, and I swore I'd come back and collect.\"",
     "I hate that I understand him.", SRC_STORY, RIB_LANTERN, 13, T_ENTER, VAR_VENN,
     "He walked out of Millhollow a second time, alive, because you let him.",
     "He came back. He collected. He did not leave again."},
    {"mem_hesk_lie", "Hesk's Lie", "Elder Hesk", "The Tithe Broken",
     "\"We told the families the Lantern chose. It never chose. Four of us met in the mill loft and picked "
     "the sick and the poor and the ones nobody would ask after. I wrote your name myself.\"",
     "He didn't write it. That isn't his hand. He's still lying.", SRC_STORY, RIB_LANTERN, 13, T_ENTER},

    // ---- Act 3 — Emberdeep (6) ----
    {"mem_well_dumping", "Down the Well", "An old well-keeper", "Wellside Rumor",
     "\"Ash that wouldn't settle in the glass, we tipped down the well. Buckets of it, every winter. My "
     "father did it, and his. Nobody asked what ash wants.\"",
     "It wants to stop. That's all it wants.", SRC_STORY, RIB_LANTERN, 14, T_SEARCH_ONLY},
    {"mem_rope_boy", "The Rope-Boy", "A boy who hauled buckets", "Old Well Descent",
     "\"I worked the rope for the ash-buckets. Six pennies a night. The rope was wet and I was nine. "
     "Nobody came down for me. I kept waiting to be heavy enough to matter.\"",
     "I don't know him. I should know him. Someone should.", SRC_STORY, RIB_LANTERN, 15, T_CLEAR},
    {"mem_baker_hands", "The Baker's Hands", "The baker on Mill Lane", "Root Labyrinth",
     "\"Little thief, every morning, the heel off the end loaf. I always left one on the sill for you. "
     "You never knew that, did you?\"",
     "His hands were always flour. They're ash now. Don't you dare look away.", SRC_STORY, RIB_WREN, 16, T_CLEAR},
    {"mem_first_oath", "The First Oath", "The First Lamplighter", "Ember Seal Niche",
     "Scratched inside the seal's rim: \"I swear to carry the light and never to drink it. Maren Vey swore "
     "the same beside me. One of us kept it.\"",
     "Your blood. Our blood. He kept it.", SRC_STORY, RIB_MAREN, 17, T_SEARCH_FLAG},
    {"mem_maren_named", "She Called Herself Founder", "The First Lamplighter", "Breach Threshold",
     "\"She named the village, laid the green, hung the first Lantern. Founder, she called herself. "
     "Abbess. She was thirsty even then, and she was so kind about it.\"",
     "The watcher in the crypt. Maren Vey. Say it so I don't forget.", SRC_STORY, RIB_MAREN, 18, T_CLEAR},
    {"mem_well_judgment", "The Well's Judgment", "The ash-dead below Millhollow", "Breach Sealed",
     "The seal is in your hand. Under the well, the ash-dead wait to hear what you will do.",
     "I felt every one of them. I'll keep feeling them.", SRC_STORY, RIB_LANTERN, 19, T_WELL_CHOICE, VAR_WELL,
     "The seal locked hard. Below, a sound like many people sitting down in the dark, and then nothing. "
     "The Lantern burns bright. Millhollow sleeps.",
     "You turned the seal gently. Below, the Ashbound lay down at last and the well breathed out. The "
     "Lantern is dimmer now, and lighter to carry."},

    // ---- Act 4 — Ashwake Vigil (6) ----
    {"mem_thinning", "I'm Getting Quieter", "Wren", "Vigil Watch",
     "\"Something's pulling. Not hard. Like a thread in a sleeve. Every night I'm a little further from the "
     "glass, and I can't hear the green the way I could.\"",
     nullptr, SRC_STORY, RIB_WREN, 20, T_ENTER},
    {"mem_greyfen_post", "The Last Lamp of Greyfen", "Greyfen's lamplighter", "Greyfen Marsh",
     "\"Greyfen had a Lantern too. I lit it forty years. One autumn it drank the whole town in a week, and "
     "I stood my post because nobody told me to stop.\"",
     "A whole town. Like ours.", SRC_STORY, RIB_LANTERN, 21, T_CLEAR},
    {"mem_strangers", "I Don't Know These People", "Wren", "Fen Causeway",
     "\"Greyfen ribbons. A different knot. I don't know their names or their faces, and they were kindled "
     "into the same thirst as Mother.\"",
     nullptr, SRC_STORY, RIB_WREN, 22, T_CLEAR},
    {"mem_siphon_prayer", "The Siphon Prayer", "A scrap in Maren's hand", "Vigil Niche",
     "\"Draw gently, that they do not wake. Draw from many, that none is missed. Draw at dusk, when the "
     "green is busy with its grief.\"",
     "She wrote a prayer for it. A careful one.", SRC_STORY, RIB_MAREN, 23, T_SEARCH_FLAG},
    {"mem_bell_hushed", "The Bell That Hushed Her", "Wren", "Ashwake Threshold",
     "\"Every time it tolled I went under. Not asleep. Put away, like a cup on a shelf. I could hear you "
     "fighting and I couldn't say your name.\"",
     nullptr, SRC_STORY, RIB_WREN, 24, T_CLEAR},
    {"mem_follow_the_ash", "Follow the Ash", "Wren", "Vigil Kindled",
     "\"She took a piece of me down there. I can feel it, like a cold tooth. Don't leave it with her. "
     "Follow the ash.\"",
     nullptr, SRC_STORY, RIB_WREN, 25, T_ENTER},

    // ---- Lamplighter's Rounds — Millhollow Voices (8, crawl pool) ----
    {"mem_tamsin_patience", "The Miller's Patience", "Tamsin the miller", "Lamplighter's Rounds",
     "\"Grain doesn't hurry and neither did I. Forty years at the stones. I'd have waited for the light "
     "too, if it had ever asked nicely.\"",
     "Tamsin gave us flour the bad winter. She never said a word about paying.", SRC_CRAWL, RIB_LANTERN, 0, T_NONE},
    {"mem_oake_hymn", "Brother Oake's Hymn", "Brother Oake", "Lamplighter's Rounds",
     "\"I sang the kindlings for thirty years. I knew every verse and never once asked what came after "
     "the last one. I'm asking now.\"",
     "He sang at Mother's. His voice cracked on the high part.", SRC_CRAWL, RIB_LANTERN, 0, T_NONE},
    {"mem_dunmore_spark", "Dunmore's Spark", "Dunmore the smith", "Lamplighter's Rounds",
     "\"Iron remembers heat. So do I. Strike me and see. There's a spark left in this old ash yet.\"",
     "He let me work the bellows once. I was terrible.", SRC_CRAWL, RIB_LANTERN, 0, T_NONE},
    {"mem_wake_verse", "The Wake-Singer's Last Verse", "A wake-singer", "Lamplighter's Rounds",
     "\"The last verse is for the one who stays behind. Nobody sang it for me. Sing it for whoever "
     "carries you.\"",
     nullptr, SRC_CRAWL, RIB_LANTERN, 0, T_NONE},
    {"mem_candle_girl", "The Candle Girl", "A girl who sold candles", "Lamplighter's Rounds",
     "\"Two for a penny, by the green. I sold the candles they lit me with. I hope somebody got a good "
     "price.\"",
     "She used to give me the broken ones for free.", SRC_CRAWL, RIB_LANTERN, 0, T_NONE},
    {"mem_millrace_drowner", "Under the Weir", "A man under the weir", "Lamplighter's Rounds",
     "\"They said I drowned. I didn't drown. I was carried, and the water was the kindest part of it.\"",
     nullptr, SRC_CRAWL, RIB_LANTERN, 0, T_NONE},
    {"mem_widow_green", "Windows on the Green", "A widow on the green", "Lamplighter's Rounds",
     "\"I watched every kindling from my window and I counted them. Nobody ever asked me for the count, "
     "so I kept it. Now I'm in it.\"",
     nullptr, SRC_CRAWL, RIB_LANTERN, 0, T_NONE},
    {"mem_unpaid_name", "The Unpaid Name", "A name the ledger missed", "Lamplighter's Rounds",
     "\"They wrote me down and never came. I lived my whole life owed. It's a strange thing, to be a debt "
     "nobody collected.\"",
     "Lucky. Or forgotten. I can't tell which is worse.", SRC_CRAWL, RIB_LANTERN, 0, T_NONE},

    // ---- Ashen Deep — Maren, in reverse (4; depths 5 / 10 / 20 / 30) ----
    {"mem_maren_drink", "I Will Not Go Out", "Maren Vey", "Ashen Deep · depth 5",
     "\"Every lantern tastes of its village. Millhollow is bread and smoke. I will drink until the stars "
     "are cold, and I will not go out.\"",
     "She's talking to me. She knows I'm listening.", SRC_DEEP, RIB_MAREN, 5, T_NONE},
    {"mem_maren_drown", "The First Drowning", "Maren Vey", "Ashen Deep · depth 10",
     "\"Corran's Rest was the first. I only meant to sip. The whole town came into the glass at once, and "
     "it was the warmest I have ever been.\"",
     nullptr, SRC_DEEP, RIB_MAREN, 10, T_NONE},
    {"mem_maren_abbess", "Abbess", "Maren Vey", "Ashen Deep · depth 20",
     "\"They called me Abbess because I tended the dying. I sat with them and held their hands. I noticed "
     "what left them, and where it went.\"",
     nullptr, SRC_DEEP, RIB_MAREN, 20, T_NONE},
    {"mem_maren_pact", "The Pact", "Maren Vey", "Ashen Deep · depth 30",
     "\"I was twenty and afraid of the dark. Something in the Gloam offered me a light that would never "
     "fail. I said yes before it finished speaking.\"",
     "She was scared. That doesn't make it better.", SRC_DEEP, RIB_MAREN, 30, T_NONE},

    // ---- Ember Ring — caged-soul rares (3; Wave 5 apex, first time per memory) ----
    {"mem_old_sallow", "Old Sallow's Lullaby", "Old Sallow, ringmaster", "Ember Ring",
     "\"I sang this to my boy in the cart the night we left Millhollow. Hush now, the light's not hungry, "
     "the light's not hungry tonight.\"",
     "That's Venn's mother.", SRC_ARENA, RIB_LANTERN, 0, T_NONE, VAR_VENN,
     "\"You let him walk away. I won't forget your face.\"",
     "\"You were the last face he saw. I won't forget yours either.\""},
    {"mem_kiln_singer", "The Kiln-Singer", "A caged soul", "Ember Ring",
     "\"I sang to the kilns so they'd fire even. They caged me for the voice. Let me out and I'll sing you "
     "something that isn't for the fire.\"",
     nullptr, SRC_ARENA, RIB_LANTERN, 0, T_NONE},
    {"mem_unfinished_bolt", "The Unfinished Bolt", "A weaver in the pit", "Ember Ring",
     "\"Half a bolt of blue on the loom when they took me. Somebody finish it. Somebody wear it to a "
     "wedding.\"",
     nullptr, SRC_ARENA, RIB_LANTERN, 0, T_NONE},

    // ---- Three Drowned Lanterns — raid lore (9; all three on a town's first clear) ----
    {"mem_corran_proud_lord", "The Proud Lord", "A steward of Corran's Rest", "Corran's Rest",
     "\"Our lord bought the Lantern from a kind abbess with a soft voice. He hung it over his own gate, so "
     "the light would fall on him first.\"",
     nullptr, SRC_RAID, RIB_LANTERN, 0, T_NONE},
    {"mem_corran_crown_drowned", "The Night the Crown Drowned", "A steward of Corran's Rest", "Corran's Rest",
     "\"It drank the gate, then the hall, then the lord in his crown. We watched the light walk down the "
     "street, house by house.\"",
     nullptr, SRC_RAID, RIB_LANTERN, 0, T_NONE},
    {"mem_corran_hole_king", "A Hole in the Shape of a King", "A steward of Corran's Rest", "Corran's Rest",
     "\"What wears the crown now isn't him. It's the place he was, kept warm. Put it out, and let the place "
     "be empty.\"",
     "Corran's Rest was her first. She told us so.", SRC_RAID, RIB_LANTERN, 0, T_NONE},
    {"mem_kiln_many_hearths", "Many Hearths", "A Kilnmouth potter", "Kilnmouth",
     "\"Kilnmouth had a hearth in every house and a kiln on every corner. We thought a town that warm "
     "could never go cold.\"",
     nullptr, SRC_RAID, RIB_LANTERN, 0, T_NONE},
    {"mem_kiln_smiths", "The Smiths Who Would Not Cool", "A Kilnmouth potter", "Kilnmouth",
     "\"When the light came the smiths stood at their forges and refused to cool. They are standing there "
     "still. They are what you fought.\"",
     nullptr, SRC_RAID, RIB_LANTERN, 0, T_NONE},
    {"mem_kiln_heads_of_coal", "Heads of Coal", "A Kilnmouth potter", "Kilnmouth",
     "\"Every head on it was someone's neighbour. Cut one off and another remembers a name. Thank you for "
     "not stopping.\"",
     nullptr, SRC_RAID, RIB_LANTERN, 0, T_NONE},
    {"mem_weft_the_weft", "The Weft", "A Weftholme weaver", "Weftholme",
     "\"In Weftholme every child learned the loom before their letters. The town was one long cloth, and "
     "every house was a thread.\"",
     nullptr, SRC_RAID, RIB_LANTERN, 0, T_NONE},
    {"mem_weft_web_of_light", "A Web of Light", "A Weftholme weaver", "Weftholme",
     "\"The Lantern hung in the square and the threads caught its glow. We were so proud. We wove it into "
     "our shawls.\"",
     nullptr, SRC_RAID, RIB_LANTERN, 0, T_NONE},
    {"mem_weft_matriarch_sewing", "The Matriarch Who Kept Sewing", "A Weftholme weaver", "Weftholme",
     "\"Our eldest kept sewing while the light took us, stitch by stitch, to keep the cloth whole. She is "
     "sewing still. She is so tired.\"",
     nullptr, SRC_RAID, RIB_LANTERN, 0, T_NONE},
};

inline constexpr int kMemoryCount = static_cast<int>(sizeof(kMemories) / sizeof(kMemories[0]));

inline const MemoryDef* findMemory(const std::string& id) {
    for (const auto& m : kMemories) if (id == m.id) return &m;
    return nullptr;
}

/** Body text coloured by the saved story choices (venn / well as in Game::getStoryChoices()). */
inline std::string bodyFor(const MemoryDef& m, int venn, int well) {
    std::string b = m.body;
    if (m.variant == VAR_VENN && (venn == 1 || venn == 2)) {
        const char* extra = (venn == 1) ? m.alt1 : m.alt2;
        if (extra) { b += " "; b += extra; }
    } else if (m.variant == VAR_WELL && (well == 1 || well == 2)) {
        const char* repl = (well == 1) ? m.alt1 : m.alt2;
        if (repl) b = repl;
    }
    return b;
}

/** Category line shown on an uncollected card — never a title or a twist. */
inline std::string lockedHintFor(const MemoryDef& m) {
    switch (m.source) {
        case SRC_STORY:
            if (m.beat <= 6) return "Found in story · The Kindling";
            if (m.beat <= 13) return "Found in story · The Tithe";
            if (m.beat <= 19) return "Found in story · Emberdeep";
            return "Found in story · Ashwake Vigil";
        case SRC_CRAWL: return "Found on Lamplighter's Rounds";
        case SRC_DEEP: return "Found in the Ashen Deep";
        case SRC_ARENA: return "Found in the Ember Ring";
        default: return "Found in a Drowned Lantern";
    }
}

/** The three lore memories freed by a raid boss's first clear (nullptrs if not a raid boss). */
inline void raidLoreFor(const std::string& bossId, const char* out[3]) {
    out[0] = out[1] = out[2] = nullptr;
    if (bossId == "boss_hollow_crown") {
        out[0] = "mem_corran_proud_lord"; out[1] = "mem_corran_crown_drowned"; out[2] = "mem_corran_hole_king";
    } else if (bossId == "boss_ember_hydra") {
        out[0] = "mem_kiln_many_hearths"; out[1] = "mem_kiln_smiths"; out[2] = "mem_kiln_heads_of_coal";
    } else if (bossId == "boss_nightfang") {
        out[0] = "mem_weft_the_weft"; out[1] = "mem_weft_web_of_light"; out[2] = "mem_weft_matriarch_sewing";
    }
}

} // namespace memory
} // namespace dnd

#endif // DND_MEMORY_CATALOG_H
