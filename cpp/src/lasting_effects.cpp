#include "dnd/lasting_effects.hpp"

#include <algorithm>

#include "dnd/equipment.hpp"

namespace dnd {

bool has_lasting_effect(const Character& character, const std::string& name) {
    return character.lasting_effects.count(name) > 0;
}

int aid_amount(int slot_level) { return 5 * std::max(1, slot_level - 1); }

int aid_bonus(const Character& character) {
    auto found = character.lasting_effects.find(kAid);
    return found == character.lasting_effects.end() ? 0 : found->second;
}

std::string lasting_summary(const Spell& spell) {
    if (spell.name == kMageArmor) return "until a long rest: AC 13 + DEX while unarmored (1 creature)";
    if (spell.name == kAid) {
        return "until a long rest: +5 max and current HP, +5 per slot level above 2nd (up to " +
               std::to_string(spell.max_targets) + " creatures)";
    }
    return "lasts until a long rest";
}

std::string lasting_refusal(const Spell& spell, const Character& target, const std::vector<InventoryEntry>& inventory,
                            int slot_level) {
    if (target.dead) return target.character_name + " is dead.";
    if (spell.name == kMageArmor) {
        if (equipped_armor(inventory)) return target.character_name + " is wearing armor.";
        if (has_lasting_effect(target, kMageArmor)) return target.character_name + " already has Mage Armor.";
    } else if (spell.name == kAid) {
        if (aid_bonus(target) >= aid_amount(slot_level)) return target.character_name + " already has Aid.";
    }
    return "";
}

std::string apply_lasting_spell(const Spell& spell, Character& target, int slot_level) {
    if (spell.name == kMageArmor) {
        target.lasting_effects[kMageArmor] = 0;
        return "Mage Armor until a long rest";
    }
    if (spell.name == kAid) {
        // A stronger Aid replaces the weaker one: only the difference is added.
        int gain = aid_amount(slot_level) - aid_bonus(target);
        target.lasting_effects[kAid] = aid_amount(slot_level);
        target.max_hp += gain;
        target.current_hp += gain;
        return "Aid: +" + std::to_string(gain) + " max HP";
    }
    target.lasting_effects[spell.name] = 0;
    return spell.name + " until a long rest";
}

void end_lasting_effects(Character& character) {
    int aid = aid_bonus(character);
    character.max_hp -= aid;
    character.current_hp = std::min(character.current_hp, character.max_hp);
    character.lasting_effects.clear();
}

bool end_mage_armor_if_armored(Character& character, const std::vector<InventoryEntry>& inventory) {
    if (!has_lasting_effect(character, kMageArmor) || !equipped_armor(inventory)) return false;
    character.lasting_effects.erase(kMageArmor);
    return true;
}

std::string describe_lasting_effects(const Character& character) {
    std::string text;
    for (const auto& [name, amount] : character.lasting_effects) {
        if (!text.empty()) text += ", ";
        text += name;
        if (name == kAid) text += " (+" + std::to_string(amount) + " HP)";
    }
    return text;
}

std::string format_lasting_effects(const std::map<std::string, int>& effects) {
    std::string text;
    for (const auto& [name, amount] : effects) {
        if (!text.empty()) text += ";";
        text += name + "=" + std::to_string(amount);
    }
    return text;
}

std::map<std::string, int> parse_lasting_effects(const std::string& text) {
    std::map<std::string, int> effects;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find(';', start);
        if (end == std::string::npos) end = text.size();
        std::string part = text.substr(start, end - start);
        size_t equals = part.find('=');
        if (equals != std::string::npos && equals > 0) {
            try {
                effects[part.substr(0, equals)] = std::stoi(part.substr(equals + 1));
            } catch (const std::exception&) {
                effects[part.substr(0, equals)] = 0;
            }
        }
        start = end + 1;
    }
    return effects;
}

}  // namespace dnd
