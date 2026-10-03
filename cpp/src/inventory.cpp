#include "dnd/inventory.hpp"

#include <algorithm>
#include <stdexcept>

namespace dnd {

const char* item_category_name(ItemCategory category) {
    switch (category) {
        case ItemCategory::Gear: return "Gear";
        case ItemCategory::Consumable: return "Consumable";
        case ItemCategory::Weapon: return "Weapon";
        case ItemCategory::Armor: return "Armor";
        case ItemCategory::Shield: return "Shield";
        case ItemCategory::Wondrous: return "Wondrous";
        case ItemCategory::Scroll: return "Scroll";
    }
    throw std::invalid_argument("unknown item category");
}

ItemCategory item_category_from_name(const std::string& name) {
    if (name == "Gear") return ItemCategory::Gear;
    if (name == "Consumable") return ItemCategory::Consumable;
    if (name == "Weapon") return ItemCategory::Weapon;
    if (name == "Armor") return ItemCategory::Armor;
    if (name == "Shield") return ItemCategory::Shield;
    if (name == "Wondrous") return ItemCategory::Wondrous;
    if (name == "Scroll") return ItemCategory::Scroll;
    throw std::invalid_argument("unknown item category: " + name);
}

const char* rarity_name(Rarity rarity) {
    switch (rarity) {
        case Rarity::None: return "";
        case Rarity::Common: return "Common";
        case Rarity::Uncommon: return "Uncommon";
        case Rarity::Rare: return "Rare";
        case Rarity::VeryRare: return "Very Rare";
        case Rarity::Legendary: return "Legendary";
    }
    throw std::invalid_argument("unknown rarity");
}

Rarity rarity_from_name(const std::string& name) {
    if (name.empty()) return Rarity::None;
    if (name == "Common") return Rarity::Common;
    if (name == "Uncommon") return Rarity::Uncommon;
    if (name == "Rare") return Rarity::Rare;
    if (name == "VeryRare" || name == "Very Rare") return Rarity::VeryRare;
    if (name == "Legendary") return Rarity::Legendary;
    throw std::invalid_argument("unknown rarity: " + name);
}

const char* weapon_category_name(WeaponCategory category) {
    return category == WeaponCategory::Simple ? "Simple" : "Martial";
}

WeaponCategory weapon_category_from_name(const std::string& name) {
    if (name == "Simple") return WeaponCategory::Simple;
    if (name == "Martial") return WeaponCategory::Martial;
    throw std::invalid_argument("unknown weapon category: " + name);
}

const char* armor_type_name(ArmorType type) {
    switch (type) {
        case ArmorType::Light: return "Light";
        case ArmorType::Medium: return "Medium";
        case ArmorType::Heavy: return "Heavy";
        case ArmorType::Shield: return "Shield";
    }
    throw std::invalid_argument("unknown armor type");
}

ArmorType armor_type_from_name(const std::string& name) {
    for (auto type : {ArmorType::Light, ArmorType::Medium, ArmorType::Heavy, ArmorType::Shield}) {
        if (name == armor_type_name(type)) return type;
    }
    throw std::invalid_argument("unknown armor type: " + name);
}

bool is_healing_item(const Item& item) {
    return item.category == ItemCategory::Consumable && item.heal_dice_count > 0;
}

int roll_healing(const Dice& dice, const Item& item) {
    if (!is_healing_item(item)) throw std::invalid_argument(item.name + " doesn't restore hit points");
    return std::max(0, Dice::sum(dice.roll(item.heal_dice_sides, item.heal_dice_count)) + item.heal_bonus);
}

int use_healing_item(Character& character, const Dice& dice, const Item& item) {
    int healing = roll_healing(dice, item);
    int before = character.current_hp;
    character.current_hp = std::min(character.max_hp, character.current_hp + healing);
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
