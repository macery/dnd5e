#pragma once

#include <optional>
#include <string>

namespace dnd {

// Stored as text in Items.Category. Weapons, armor, and shields carry their
// rules data in `weapon` / `armor` (the WeaponStats / ArmorStats tables).
// Wondrous items are worn magic items (a Cloak of Protection); scrolls cast
// a spell (docs/magic-items.md).
enum class ItemCategory { Gear, Consumable, Weapon, Armor, Shield, Wondrous, Scroll };

// A magic item's rarity (DMG ch. 7); None for mundane items.
enum class Rarity { None, Common, Uncommon, Rare, VeryRare, Legendary };

enum class WeaponCategory { Simple, Martial };

// A weapon's PHB table entry (ch. 5).
struct WeaponStats {
    WeaponCategory category = WeaponCategory::Simple;
    int damage_dice_count = 1;
    int damage_dice_sides = 4;
    std::string damage_type;      // "slashing", "piercing", "bludgeoning"
    int versatile_dice_sides = 0; // two-handed damage die for Versatile weapons; 0 if not versatile
    bool finesse = false;
    bool light = false;
    bool heavy = false;
    bool two_handed = false;
    bool ranged = false;
    bool thrown = false;
    bool reach = false;
    bool loading = false;
    bool ammunition = false;
};

enum class ArmorType { Light, Medium, Heavy, Shield };

// An armor's PHB table entry (ch. 5). A shield is ArmorType::Shield with
// base_ac 2 (a bonus rather than a base).
struct ArmorStats {
    ArmorType type = ArmorType::Light;
    int base_ac = 10;
    int max_dex_bonus = -1;  // -1 = no cap (light); 2 for medium; 0 for heavy
    int strength_requirement = 0;
    bool stealth_disadvantage = false;
};

// Seeded, read-only item data (like Monster and Weapon). A consumable with
// heal_dice_count > 0 restores (heal_dice_count)d(heal_dice_sides) +
// heal_bonus HP when used, e.g. a Potion of Healing is 2d4+2.
struct Item {
    int item_id = 0;
    std::string name;
    ItemCategory category = ItemCategory::Gear;
    std::string description;
    double weight_lb = 0.0;
    int cost_cp = 0;  // copper pieces: 1 gp = 100 cp, 1 sp = 10 cp
    int heal_dice_count = 0;
    int heal_dice_sides = 0;
    int heal_bonus = 0;
    std::optional<WeaponStats> weapon;  // category Weapon
    std::optional<ArmorStats> armor;    // category Armor or Shield

    // Magic items (docs/magic-items.md). All zero/empty for mundane items.
    Rarity rarity = Rarity::None;
    std::string base_item;        // a +N weapon's or armor's mundane item ("Longsword" for "Longsword +1")
    int magic_bonus = 0;          // +N to attack and damage (weapon) or to AC (armor, shield)
    bool requires_attunement = false;
    int ac_bonus = 0;             // while attuned (Cloak and Ring of Protection)
    int save_bonus = 0;           // to every saving throw, while attuned
    int unarmored_ac_bonus = 0;   // while attuned, with no armor and no shield (Bracers of Defense)
    std::string sets_ability;     // "Strength" or "Constitution": raised to sets_ability_to while attuned
    int sets_ability_to = 0;
    int scroll_spell_id = 0;      // category Scroll: the spell it casts
    int uses = 0;                 // limited-use gear (a Healer's Kit has 10); 0 = not tracked

    bool magical() const { return rarity != Rarity::None; }
};

// One stack of an item a character carries.
struct InventoryEntry {
    Item item;
    int quantity = 0;
    bool equipped = false;  // worn armor/shield, or the wielded weapon
    // Attuned to (DMG): needed for items that require attunement to work. An
    // attuned wondrous item counts as worn (docs/magic-items.md).
    bool attuned = false;
    // Limited-use items: uses gone from the stack's current item. When it
    // reaches Item::uses, that item is used up and the count starts over.
    int uses_spent = 0;
};

}  // namespace dnd
