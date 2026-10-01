#pragma once

#include <string>
#include <vector>

namespace dnd {

// Damage types, resistance, vulnerability, and immunity (2014 PHB ch. 9);
// see docs/monsters-and-encounters.md. Types are lowercase PHB names:
// "bludgeoning", "piercing", "slashing", "fire", "poison", "radiant", ...

struct DamageTraits {
    std::vector<std::string> resistances;
    std::vector<std::string> vulnerabilities;
    std::vector<std::string> immunities;

    bool empty() const { return resistances.empty() && vulnerabilities.empty() && immunities.empty(); }
};

enum class DamageModifier { Normal, Resisted, Vulnerable, Immune };

// How `traits` take damage of `type` (case-insensitive). Immunity wins;
// resistance and vulnerability together cancel out. An empty type (untyped
// damage) is always Normal.
DamageModifier damage_modifier(const DamageTraits& traits, const std::string& type);

// Resistance halves (rounded down), vulnerability doubles, immunity is 0.
int adjust_damage(int amount, DamageModifier modifier);

// For the log: "halved: resistant to slashing", "doubled: vulnerable to
// bludgeoning", "immune to poison"; "" for Normal.
std::string damage_modifier_note(DamageModifier modifier, const std::string& type);

// "bludgeoning,piercing" <-> {"bludgeoning", "piercing"} (trimmed, lowercased
// for damage types). Empty text gives an empty list.
std::vector<std::string> split_list(const std::string& text);
std::string join_list(const std::vector<std::string>& items, const std::string& separator = ", ");

// Rage: resistance to bludgeoning, piercing, and slashing.
const std::vector<std::string>& physical_damage_types();

}  // namespace dnd
