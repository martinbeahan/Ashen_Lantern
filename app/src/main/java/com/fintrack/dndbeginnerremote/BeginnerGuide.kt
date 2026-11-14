package com.fintrack.dndbeginnerremote

/**
 * Beginner-friendly copy for tutorial + solo DM coaching.
 * Keeps explanations short and practical (5e SRD-flavored).
 * Stat blurbs describe how *this* game uses attributes in C++ combat/search.
 */
object BeginnerGuide {

    data class Page(val title: String, val body: String)

    val tutorialPages = listOf(
        Page(
            "Welcome, adventurer",
            "This is a simple tabletop fantasy dungeon crawl, compatible with 5e SRD concepts.\n\n" +
                "You play a hero. The Lanternkeeper (your narrator) describes the world, " +
                "runs monsters, and helps you learn the buttons.\n\n" +
                "You do not need to know the rulebooks to start."
        ),
        Page(
            "Your turn — Attack",
            "Attack = swing your weapon.\n\n" +
                "The game rolls a d20, adds your attack bonus, and compares it to the enemy's Armor Class (AC).\n\n" +
                "• Equal or higher than AC → Hit, then roll damage\n" +
                "• Natural 20 → Critical hit (extra damage)\n" +
                "• Natural 1 → Automatic miss"
        ),
        Page(
            "Special (your class power)",
            "Special uses a limited resource (like a spell slot or feature use):\n\n" +
                "• Fighter — Action Surge: attack twice\n" +
                "• Rogue — Sneak Attack: extra damage dice\n" +
                "• Wizard — Magic Missile: auto-hit force darts\n" +
                "• Cleric — Healing Word: heal the most hurt ally\n\n" +
                "If Special is greyed out or fails, you may be out of uses — try a Short Rest."
        ),
        Page(
            "Potion, Rest, Search",
            "• Potion — drink a Potion of Healing (2d4+2 HP). Costs one supply/resource.\n\n" +
                "• Short Rest — catch your breath: recover some HP and some special uses (once per clear). " +
                "• Long Rest — full HP and special/supply restore (every 5 rooms in Story, Crawl, Arena, Endless, Raid & Challenge; also once per clear). " +
                "Not a full night's sleep.\n\n" +
                "• Search (Srch) — look for treasure or danger (skill check). Can find gold… or a trap."
        ),
        Page(
            "Stats & leveling",
            "Clear rooms to earn XP (see the gold XP bar under your turn banner).\n\n" +
                "When you level up you get 2 attribute points — open Sheet to spend them.\n\n" +
                "Your class cares most about one attack/heal stat plus Constitution for hit points. " +
                "Tap Help → What do stats do? anytime."
        ),
        Page(
            "Playing solo",
            "Story adventure runs the original four-act story, The Kindling:\n\n" +
                "• Act 1 The Kindling: recover the stolen Ashen Lantern from Hollowbarrow Crypt\n" +
                "• Act 2 The Tithe: find out what Millhollow really owes\n" +
                "• Act 3 Emberdeep: go down the old well\n" +
                "• Act 4 Ashwake Vigil: follow the draw west into Greyfen\n" +
                "• Wren speaks from the Lantern in chat as [Lantern]\n" +
                "• Souls you free settle in the glass: read them in the Memory Ledger (menu or Settings)\n" +
                "• Dungeon Crawl skips story for procedural rooms from the start\n\n" +
                "Pick Difficulty on the main menu (default Easy). On a full party wipe:\n" +
                "Easy/Medium keep gear; Hard strips gold/gear; Nightmare ends the run.\n\n" +
                "Continue mid-quest works — your beat is saved.\n" +
                "Compatible with 5e SRD concepts only. Not affiliated with, sponsored by, endorsed by, or approved by Wizards of the Coast LLC."
        )
    )

    fun actionReference(): String = """
        |Quick reference
        |
        |Attack — Weapon strike vs Armor Class (d20 + bonuses).
        |Special — Your class feature (uses a resource).
        |Potion — Heal 2d4+2 HP (uses a resource).
        |Short Rest — Recover some HP and resources (once per clear).
        |Long Rest — Full HP and resources (every 5 rooms; once per clear).
        |Search — Investigate the room (risk/reward).
        |Sheet — Your stats, HP, AC, XP, gear.
        |Log — Adventure journal.
        |
        |Tip: AC is how hard you are to hit. Higher is better.
        |Tip: Resources power Specials and combat Potions — rest to regain some.
        |Wayfarer's Tonic (daily quest) — open Inventory and tap Use.
        |Tip: XP fills as you clear rooms; level-ups grant attribute points.
    """.trimMargin()

    /** Plain-English blurbs matching this build's C++ rules (SRD-safe wording). Classic = online tables. */
    fun statBlurb(statIndex: Int): String = when (statIndex) {
        0 -> "Strength — attack rolls & weapon damage for Fighters (and most melee)."
        1 -> "Dexterity — Rogue attacks/damage; everyone's Armor Class and initiative."
        2 -> "Constitution — hit points each level; helps Short Rest recovery. Spending CON also adds HP now."
        3 -> "Intelligence — Wizard weapon attacks/damage; Search checks for loot (or traps)."
        4 -> "Wisdom — Cleric weapon attacks/damage; boosts Healing Word amount."
        5 -> "Charisma — Bard weapon attacks/damage and Cutting Quip (psychic sting + rattle)."
        else -> "Attribute score — higher is usually better."
    }

    /** v2.31 solo rules: every stat helps every class (docs/STATS.md). */
    fun universalStatBlurb(statIndex: Int): String = when (statIndex) {
        0 -> "Strength — Fighter's main stat. Everyone: Heft (+1 weapon damage per 4 STR over 10), break shield-walls at STR 16, STR saves resist Shackle."
        1 -> "Dexterity — Rogue's main stat. Everyone: AC (max +6 from DEX), initiative, Evade (up to 24% off telegraphed heavies and area attacks), DEX saves."
        2 -> "Constitution — no class's main stat. Everyone: HP each level and on Short Rest, CON saves vs Burn, a bonus on death saves."
        3 -> "Intelligence — Wizard's main stat (Magic Missile darts grow with it). Everyone: Search, Weak point (crit on 19–20 at INT 18, 18–20 at INT 26), exact foe intent numbers at INT 14."
        4 -> "Wisdom — Cleric's main stat. Everyone: Mending (up to +40% healing received), WIS saves vs Rattled, Marked and Webs, +1 use on Short Rest at WIS 18."
        5 -> "Charisma — Bard's main stat. Everyone: Barter (up to 20% off, better sell price up to 75%), Luck (up to +30% gold), Inspire (companions hit more), Intimidate (foes flee sooner)."
        else -> "Attribute score — higher is usually better."
    }

    fun statShortHint(statIndex: Int, universal: Boolean = false): String = if (universal) when (statIndex) {
        0 -> "Heft damage, break guards, STR saves"
        1 -> "AC (max +6), initiative, Evade"
        2 -> "HP, rests, CON & death saves"
        3 -> "Search, crits, read foe intents"
        4 -> "Healing received, WIS saves, rests"
        5 -> "Prices, gold, Inspire, Intimidate"
        else -> ""
    } else when (statIndex) {
        0 -> "Melee hits & damage (Fighter)"
        1 -> "AC, initiative, Rogue hits"
        2 -> "More HP; better rests"
        3 -> "Wizard hits; Search checks"
        4 -> "Cleric hits; Healing Word"
        5 -> "Bard hits; Cutting Quip"
        else -> ""
    }

    fun classStatTip(classId: Int, universal: Boolean = false): String = if (universal) when (classId) {
        0 -> "Fighter: Strength is your main stat (hits, damage, Action Surge; cap +2). Every other stat helps too — CON for HP, DEX for AC."
        1 -> "Wizard: Intelligence is your main stat (hits, damage, Magic Missile; cap +2). CON and DEX keep you alive; WIS makes healing land harder."
        2 -> "Rogue: Dexterity is your main stat (hits, damage, Sneak Attack; cap +2). STR adds Heft, CON adds HP."
        3 -> "Cleric: Wisdom is your main stat (hits, damage, Healing Word; cap +2). CON and DEX keep you standing."
        4 -> "Bard: Charisma is your main stat (hits, damage, Cutting Quip; cap +2) and also lowers prices and inspires allies."
        else -> "Your main stat drives hits, damage and your special; every stat helps."
    } + " Main stat 20 / 24 / 28 / 32 unlocks Signature bonuses." else when (classId) {
        0 -> "Fighter tip: prioritize Strength, then Constitution. Dexterity still helps AC."
        1 -> "Wizard tip: prioritize Intelligence, then Constitution. Dexterity helps AC (light armor)."
        2 -> "Rogue tip: prioritize Dexterity (hits, damage, AC), then Constitution."
        3 -> "Cleric tip: prioritize Wisdom (attacks + Healing Word), then Constitution."
        4 -> "Bard tip: prioritize Charisma (attacks + Cutting Quip), then Constitution. Dexterity still helps AC."
        else -> "Boost your class's main attack stat and Constitution for survivability."
    }

    /**
     * Help → Stats. Solo (universal) shows the v2.31 rules and, when the native guide is
     * available, what every stat does for the current hero right now.
     */
    fun statsHelpMessage(universal: Boolean = false, forHero: StatGuide? = null, heroName: String = ""): String = buildString {
        if (!universal) {
            appendLine("How stats work in this game (online tables use the classic rules)")
            appendLine()
            for (i in 0..5) appendLine("• ${statBlurb(i)}")
            appendLine()
            appendLine("By class")
            for (c in 0..4) appendLine("• ${classStatTip(c)}")
            appendLine()
            append("Level-up: clear rooms → earn XP → open Sheet when you (or an ally) level up and spend points.")
            return@buildString
        }
        appendLine("How stats work (solo)")
        appendLine("Every stat helps every class. Your class's main stat still drives hits, damage and your special.")
        appendLine()
        if (forHero != null && forHero.active) {
            appendLine(forHero.helpText(heroName))
            appendLine()
            appendLine("All stats")
        }
        for (i in 0..5) appendLine("• ${universalStatBlurb(i)}")
        appendLine()
        appendLine("Caps: 18 + half your level (max 30 at level 24); your main stat can go 2 higher. Gear can push past a cap.")
        appendLine("Signature (main stat): 20 = one free special per battle · 24 = special +25% · 28 = special crits on 19–20 · 32 = Lantern Arts cooldown −1 (coming soon).")
        appendLine()
        append("Level-up: clear rooms → earn XP → open Sheet when you (or an ally) level up and spend points.")
    }

    fun specialBlurb(classId: Int): String = when (classId) {
        0 -> "Action Surge: take an extra burst and attack twice this turn."
        1 -> "Magic Missile: three darts that always hit — great when you keep missing."
        2 -> "Sneak Attack: add extra damage dice when you catch a foe off-guard."
        3 -> "Healing Word: heal the ally with the lowest HP (includes you)."
        4 -> "Cutting Quip: CHA attack vs AC — psychic sting and rattle (-2 on their next attack)."
        else -> "Your Special is a limited class power. Use it when it matters."
    }

    /**
     * Solo DM coach line for the current combat status string from native code.
     * Returns null if no new tip is needed.
     */
    fun turnCoachTip(
        status: String,
        localPlayerName: String,
        isShop: Boolean,
        specialName: String,
        classId: Int
    ): String? {
        if (isShop) {
            return "Keeper: Safe room. Tap Shop to buy gear, or Leave/Short Rest area controls to move on."
        }
        if (status.contains("Game Over", ignoreCase = true)) {
            return "Keeper: The party has fallen. Tap Menu, then Solo adventure to begin again (this save is cleared)."
        }
        val turnMarker = "Turn: "
        val turnLine = status.lineSequence().firstOrNull { it.contains(turnMarker) } ?: return null
        val whose = turnLine.substringAfter(turnMarker).trim()
        if (whose.equals("Safe", true) || whose.equals("None", true)) return null

        val companionLine = status.lineSequence().firstOrNull { it.startsWith("Companion:") }
        val companionPlayer = companionLine != null &&
            companionLine.contains("[Player]", ignoreCase = true) &&
            companionLine.contains(whose)
        val myTurn = whose.equals(localPlayerName, true) || whose.equals("You", true) || companionPlayer
        if (!myTurn) {
            return "Keeper: It's $whose's turn. Watch the log — enemies and Auto allies act automatically."
        }
        if (companionPlayer && !whose.equals(localPlayerName, true)) {
            return "Keeper: $whose's turn (you control them). Use Attack / Special / Potion just like your hero."
        }

        val hurt = status.lineSequence().any { line ->
            line.startsWith(localPlayerName) && line.contains("HP:") && run {
                val hp = line.substringAfter("HP:").substringBefore(" ").trim()
                val parts = hp.split("/")
                if (parts.size == 2) {
                    val cur = parts[0].toIntOrNull() ?: return@run false
                    val max = parts[1].toIntOrNull() ?: return@run false
                    max > 0 && cur * 2 <= max
                } else false
            }
        }

        return buildString {
            append("Keeper: Your turn, $localPlayerName. ")
            append("Attack = weapon swing vs their AC. ")
            append("Special ($specialName) = ${specialBlurb(classId)} ")
            if (hurt) append("You're bloodied — consider Potion or Healing Word if you have it. ")
            append("Rest when the room is clear — Short or Long, once per clear.")
        }
    }

    /** Plain-English meaning of a weapon/armor bonus for inventory & shop. */
    fun gearBonusFlavor(type: String, bonus: Int): String {
        val b = bonus.coerceAtLeast(0)
        return when {
            type.equals("Weapon", ignoreCase = true) || type.equals("W", ignoreCase = true) ->
                "Weapon +$b: adds +$b to your attack roll and +$b to damage when you hit."
            type.equals("Armor", ignoreCase = true) || type.equals("A", ignoreCase = true) ->
                "Armor +$b: raises your Armor Class (AC) by $b. Higher AC = harder to hit."
            type.equals("Potion", ignoreCase = true) || type.equals("P", ignoreCase = true) ->
                "Potion: drink from Inventory (Use). Consumed when you drink it."
            else ->
                if (b > 0) "Bonus +$b improves this item's main combat effect." else "No bonus yet — upgrade or find stronger gear."
        }
    }

    fun gearStatsHelpBlurb(): String =
        "Weapon bonus → attack & damage. Armor bonus → Armor Class (AC). " +
            "Higher rarity usually means stronger bonuses. Upgrade at merchants when you can afford it."

}
