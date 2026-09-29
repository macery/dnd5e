#pragma once

#include <string>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/dice.hpp"
#include "dnd/item.hpp"

namespace dnd {

const char* item_category_name(ItemCategory category);
// Inverse of item_category_name. Throws std::invalid_argument for unknown text.
ItemCategory item_category_from_name(const std::string& name);

// True for a consumable that restores HP (e.g. a Potion of Healing).
bool is_healing_item(const Item& item);

// Rolls the item's healing and applies it, capped at max HP. Returns the HP
// actually regained. Only changes the character -- removing the used item
// from the inventory is the caller's job (see Repository). Throws
// std::invalid_argument if the item doesn't heal.
int use_healing_item(Character& character, const Dice& dice, const Item& item);

// Sum of weight_lb * quantity across the inventory.
double total_weight(const std::vector<InventoryEntry>& inventory);

// e.g. 5000 -> "50 gp", 250 -> "2 gp 5 sp", 1 -> "1 cp", 0 -> "0 cp".
std::string format_cost(int cost_cp);

}  // namespace dnd
