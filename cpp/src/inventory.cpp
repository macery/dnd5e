#include "dnd/inventory.hpp"

#include <algorithm>
#include <stdexcept>

namespace dnd {

const char* item_category_name(ItemCategory category) {
    switch (category) {
        case ItemCategory::Gear: return "Gear";
        case ItemCategory::Consumable: return "Consumable";
    }
    throw std::invalid_argument("unknown item category");
}

ItemCategory item_category_from_name(const std::string& name) {
    if (name == "Gear") return ItemCategory::Gear;
    if (name == "Consumable") return ItemCategory::Consumable;
    throw std::invalid_argument("unknown item category: " + name);
}

bool is_healing_item(const Item& item) {
    return item.category == ItemCategory::Consumable && item.heal_dice_count > 0;
}

int use_healing_item(Character& character, const Dice& dice, const Item& item) {
    if (!is_healing_item(item)) throw std::invalid_argument(item.name + " doesn't restore hit points");

    int healing = Dice::sum(dice.roll(item.heal_dice_sides, item.heal_dice_count)) + item.heal_bonus;
    int before = character.current_hp;
    character.current_hp = std::min(character.max_hp, character.current_hp + std::max(0, healing));
    return character.current_hp - before;
}

double total_weight(const std::vector<InventoryEntry>& inventory) {
    double total = 0.0;
    for (const auto& entry : inventory) total += entry.item.weight_lb * entry.quantity;
    return total;
}

std::string format_cost(int cost_cp) {
    int gp = cost_cp / 100;
    int sp = (cost_cp % 100) / 10;
    int cp = cost_cp % 10;

    std::string text;
    auto append = [&text](int amount, const char* unit) {
        if (amount == 0) return;
        if (!text.empty()) text += ' ';
        text += std::to_string(amount) + ' ' + unit;
    };
    append(gp, "gp");
    append(sp, "sp");
    append(cp, "cp");
    return text.empty() ? "0 cp" : text;
}

}  // namespace dnd
