#include "dnd/damage.hpp"

#include <algorithm>
#include <cctype>

namespace dnd {

namespace {

std::string lowercase(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

bool contains(const std::vector<std::string>& types, const std::string& type) {
    return std::any_of(types.begin(), types.end(), [&](const std::string& t) { return lowercase(t) == type; });
}

}  // namespace

DamageModifier damage_modifier(const DamageTraits& traits, const std::string& type) {
    if (type.empty()) return DamageModifier::Normal;
    std::string key = lowercase(type);
    if (contains(traits.immunities, key)) return DamageModifier::Immune;
    bool resisted = contains(traits.resistances, key);
    bool vulnerable = contains(traits.vulnerabilities, key);
    if (resisted && !vulnerable) return DamageModifier::Resisted;
    if (vulnerable && !resisted) return DamageModifier::Vulnerable;
    return DamageModifier::Normal;
}

int adjust_damage(int amount, DamageModifier modifier) {
    switch (modifier) {
        case DamageModifier::Normal: return amount;
        case DamageModifier::Resisted: return amount / 2;
        case DamageModifier::Vulnerable: return amount * 2;
        case DamageModifier::Immune: return 0;
    }
    return amount;
}

std::string damage_modifier_note(DamageModifier modifier, const std::string& type) {
    switch (modifier) {
        case DamageModifier::Normal: return "";
        case DamageModifier::Resisted: return "halved: resistant to " + type;
        case DamageModifier::Vulnerable: return "doubled: vulnerable to " + type;
        case DamageModifier::Immune: return "immune to " + type;
    }
    return "";
}

std::vector<std::string> split_list(const std::string& text) {
    std::vector<std::string> items;
    std::string current;
    auto flush = [&] {
        auto begin = current.find_first_not_of(" \t");
        auto end = current.find_last_not_of(" \t");
        if (begin != std::string::npos) items.push_back(current.substr(begin, end - begin + 1));
        current.clear();
    };
    for (char c : text) {
        if (c == ',') {
            flush();
        } else {
            current += c;
        }
    }
    flush();
    return items;
}

std::string join_list(const std::vector<std::string>& items, const std::string& separator) {
    std::string text;
    for (const auto& item : items) text += (text.empty() ? "" : separator) + item;
    return text;
}

const std::vector<std::string>& physical_damage_types() {
    static const std::vector<std::string> types = {"bludgeoning", "piercing", "slashing"};
    return types;
}

}  // namespace dnd
