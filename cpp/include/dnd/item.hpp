#pragma once

#include <string>

namespace dnd {

// Stored as text in Items.Category. Weapons and armor still live in their
// own tables for now; they're expected to become categories here later.
enum class ItemCategory { Gear, Consumable };

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
};

// One stack of an item a character carries.
struct InventoryEntry {
    Item item;
    int quantity = 0;
};

}  // namespace dnd
