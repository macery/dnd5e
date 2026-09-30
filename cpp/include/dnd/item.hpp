#pragma once

#include <optional>
#include <string>

namespace dnd {

// Stored as text in Items.Category. Weapons, armor, and shields carry their
// rules data in `weapon` / `armor` (the WeaponStats / ArmorStats tables).
enum class ItemCategory { Gear, Consumable, Weapon, Armor, Shield };

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
};

// One stack of an item a character carries.
struct InventoryEntry {
    Item item;
    int quantity = 0;
    bool equipped = false;  // worn armor/shield, or the wielded weapon
};

}  // namespace dnd
