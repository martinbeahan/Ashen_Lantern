package com.fintrack.dndbeginnerremote

import androidx.annotation.DrawableRes

/**
 * Gear glyphs (v2.27). Items have no art id in the save, so the item NAME is the identity:
 * name -> glyph, falling back by item type (Weapon -> sword, Armor -> mail, Potion -> potion).
 * Rarity stays in the socket frame and row pip; Legendary adds an amber inlay on the glyph,
 * plus static glow circles unless reduce-flash is on (calm keeps the inlay, drops the glow).
 * No save-format change. Art: docs/art/ash_ember/gear, generator tools/gear_icons.
 * GearIconsTest parses the C++ item tables so every item in the game has an explicit glyph.
 */
object GearIcons {
    const val SWORD = "sword"
    const val SHORT_BLADE = "short_blade"
    const val RAPIER = "rapier"
    const val STAFF = "staff"
    const val WAND = "wand"
    const val MACE = "mace"
    const val HAMMER = "hammer"
    const val MAUL = "maul"
    const val ASHEN_LANTERN = "ashen_lantern"
    const val LEATHER = "leather"
    const val CLOTH = "cloth"
    const val MAIL = "mail"
    const val PLATE = "plate"
    const val COAT = "coat"
    const val POTION = "potion"
    const val TONIC = "tonic"
    const val RAID_KEY = "raid_key"
    const val GOLD = "gold"

    private fun names(glyph: String, vararg n: String) = n.map { it to glyph }

    /** Every item name the native game can create (see GearIconsTest). Names are untouched here. */
    val BY_NAME: Map<String, String> = linkedMapOf(
        *(names(SWORD, "Longsword", "Champion's Longsword", "Fighter's Arming Sword", "Steel Blade",
            "Runed Blade", "Traveler Blade", "Traveler's Blade", "Crownbreaker", "Emberdeep Relic Blade") +
        names(SHORT_BLADE, "Shortsword", "Silent Shortsword", "Rogue's Stiletto", "Shadow Dirk", "Nightwhisper") +
        names(RAPIER, "Rapier", "Stage Rapier", "Bard's Stage Rapier", "Songsteel Rapier") +
        names(STAFF, "Quarterstaff", "Focus Staff", "Wizard's Focus Rod", "Starfall Focus") +
        names(WAND, "Arcane Wand") +
        names(MACE, "Mace", "Hallowed Mace", "Dawnward Mace") +
        names(HAMMER, "Cleric's Warhammer") +
        names(MAUL, "War Maul") +
        names(ASHEN_LANTERN, "Ashen Lantern") +
        names(LEATHER, "Leather Armor", "Padded Vest", "Shadow Leathers", "Rogue's Leathers",
            "Performer's Leathers", "Veil of Shadows", "Veilwalker Hide") +
        names(CLOTH, "Traveler Clothes", "Scholar Robes", "Wizard's Robe", "Temple Vestments",
            "Archsage Mantle", "Sturdy Cloak") +
        names(MAIL, "Chain Shirt", "Scale Mail", "Reinforced Mail", "Bulwark Mail", "Fighter's Mail",
            "Reliquary Mail", "Emberdeep Relic Mail", "Cleric's Scale") +
        names(PLATE, "Knight Plate", "Sanctum Plate", "Aegis of Millhollow") +
        names(COAT, "Troubadour Coat") +
        names(POTION, "Healing Draught", "Greater Potion") +
        names(TONIC, "Wayfarer's Tonic")).toTypedArray()
    )

    /** Glyphs that have a Legendary inlay variant (Ashen Lantern, consumables and meta never do). */
    val LEGENDARY_GLYPHS = setOf(SWORD, SHORT_BLADE, RAPIER, STAFF, WAND, MACE, HAMMER, MAUL,
        LEATHER, CLOTH, MAIL, PLATE, COAT)

    val ALL_GLYPHS = listOf(SWORD, SHORT_BLADE, RAPIER, STAFF, WAND, MACE, HAMMER, MAUL, ASHEN_LANTERN,
        LEATHER, CLOTH, MAIL, PLATE, COAT, POTION, TONIC, RAID_KEY, GOLD)

    fun fallbackFor(type: String): String = when (type.trim().lowercase()) {
        "armor", "armour" -> MAIL
        "potion" -> POTION
        else -> SWORD
    }

    fun hasExplicitGlyph(name: String): Boolean = BY_NAME.containsKey(name.trim())

    fun glyphFor(name: String, type: String): String = BY_NAME[name.trim()] ?: fallbackFor(type)

    /** Drawable stem, e.g. ic_gear_mace, ic_gear_mace_legendary, ic_gear_mace_legendary_calm. */
    fun stemFor(glyph: String, rarity: String, calm: Boolean): String {
        val legendary = rarity.trim().equals("legendary", ignoreCase = true) && glyph in LEGENDARY_GLYPHS
        return when {
            !legendary -> "ic_gear_$glyph"
            calm -> "ic_gear_${glyph}_legendary_calm"
            else -> "ic_gear_${glyph}_legendary"
        }
    }

    private val DRAWABLES: Map<String, Int> = mapOf(
        "ic_gear_sword" to R.drawable.ic_gear_sword,
        "ic_gear_short_blade" to R.drawable.ic_gear_short_blade,
        "ic_gear_rapier" to R.drawable.ic_gear_rapier,
        "ic_gear_staff" to R.drawable.ic_gear_staff,
        "ic_gear_wand" to R.drawable.ic_gear_wand,
        "ic_gear_mace" to R.drawable.ic_gear_mace,
        "ic_gear_hammer" to R.drawable.ic_gear_hammer,
        "ic_gear_maul" to R.drawable.ic_gear_maul,
        "ic_gear_ashen_lantern" to R.drawable.ic_gear_ashen_lantern,
        "ic_gear_leather" to R.drawable.ic_gear_leather,
        "ic_gear_cloth" to R.drawable.ic_gear_cloth,
        "ic_gear_mail" to R.drawable.ic_gear_mail,
        "ic_gear_plate" to R.drawable.ic_gear_plate,
        "ic_gear_coat" to R.drawable.ic_gear_coat,
        "ic_gear_potion" to R.drawable.ic_gear_potion,
        "ic_gear_tonic" to R.drawable.ic_gear_tonic,
        "ic_gear_raid_key" to R.drawable.ic_gear_raid_key,
        "ic_gear_gold" to R.drawable.ic_gear_gold,
        "ic_gear_sword_legendary" to R.drawable.ic_gear_sword_legendary,
        "ic_gear_sword_legendary_calm" to R.drawable.ic_gear_sword_legendary_calm,
        "ic_gear_short_blade_legendary" to R.drawable.ic_gear_short_blade_legendary,
        "ic_gear_short_blade_legendary_calm" to R.drawable.ic_gear_short_blade_legendary_calm,
        "ic_gear_rapier_legendary" to R.drawable.ic_gear_rapier_legendary,
        "ic_gear_rapier_legendary_calm" to R.drawable.ic_gear_rapier_legendary_calm,
        "ic_gear_staff_legendary" to R.drawable.ic_gear_staff_legendary,
        "ic_gear_staff_legendary_calm" to R.drawable.ic_gear_staff_legendary_calm,
        "ic_gear_wand_legendary" to R.drawable.ic_gear_wand_legendary,
        "ic_gear_wand_legendary_calm" to R.drawable.ic_gear_wand_legendary_calm,
        "ic_gear_mace_legendary" to R.drawable.ic_gear_mace_legendary,
        "ic_gear_mace_legendary_calm" to R.drawable.ic_gear_mace_legendary_calm,
        "ic_gear_hammer_legendary" to R.drawable.ic_gear_hammer_legendary,
        "ic_gear_hammer_legendary_calm" to R.drawable.ic_gear_hammer_legendary_calm,
        "ic_gear_maul_legendary" to R.drawable.ic_gear_maul_legendary,
        "ic_gear_maul_legendary_calm" to R.drawable.ic_gear_maul_legendary_calm,
        "ic_gear_leather_legendary" to R.drawable.ic_gear_leather_legendary,
        "ic_gear_leather_legendary_calm" to R.drawable.ic_gear_leather_legendary_calm,
        "ic_gear_cloth_legendary" to R.drawable.ic_gear_cloth_legendary,
        "ic_gear_cloth_legendary_calm" to R.drawable.ic_gear_cloth_legendary_calm,
        "ic_gear_mail_legendary" to R.drawable.ic_gear_mail_legendary,
        "ic_gear_mail_legendary_calm" to R.drawable.ic_gear_mail_legendary_calm,
        "ic_gear_plate_legendary" to R.drawable.ic_gear_plate_legendary,
        "ic_gear_plate_legendary_calm" to R.drawable.ic_gear_plate_legendary_calm,
        "ic_gear_coat_legendary" to R.drawable.ic_gear_coat_legendary,
        "ic_gear_coat_legendary_calm" to R.drawable.ic_gear_coat_legendary_calm,
    )

    fun hasDrawable(stem: String): Boolean = DRAWABLES.containsKey(stem)

    @DrawableRes
    fun drawable(glyph: String, rarity: String = "Common", calm: Boolean = false): Int =
        DRAWABLES[stemFor(glyph, rarity, calm)] ?: R.drawable.ic_gear_sword

    @DrawableRes
    fun forItem(name: String, type: String, rarity: String, calm: Boolean): Int =
        drawable(glyphFor(name, type), rarity, calm)

    /** Socket frame (fill + rarity stroke + rim hairline); the glyph sits on top as the image. */
    @DrawableRes
    fun socketFor(rarity: String): Int = when (rarity.trim().lowercase()) {
        "uncommon" -> R.drawable.bg_gear_socket_uncommon
        "rare" -> R.drawable.bg_gear_socket_rare
        "epic" -> R.drawable.bg_gear_socket_epic
        "legendary" -> R.drawable.bg_gear_socket_legendary
        else -> R.drawable.bg_gear_socket_common
    }
}
