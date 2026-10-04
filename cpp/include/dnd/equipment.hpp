#pragma once

#include <string>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/character_creation.hpp"
#include "dnd/item.hpp"

namespace dnd {

// What a class is trained with (2014 PHB ch. 3). Rules data, like hit dice;
// kept here rather than in ClassInfo so the class table stays readable.
struct ArmorProficiencies {
    bool light = false;
    bool medium = false;
    bool heavy = false;
    bool shields = false;
};

struct WeaponProficiencies {
    bool simple = false;
    bool martial = false;
    std::vector<std::string> weapons;  // specific weapons (item names), e.g. a Bard's Rapier
};

struct ClassEquipment {
    ArmorProficiencies armor;
    WeaponProficiencies weapons;
    std::vector<StartingItem> starting_kit;  // the PHB default kit (docs/equipment-system.md)
};

// nullptr for an unknown class.
const ClassEquipment* class_equipment(const std::string& class_name);
// Weapons a race is trained with (the base Dwarf: Battleaxe, Handaxe, ...).
std::vector<std::string> race_weapon_proficiencies(const std::string& race_name);

bool is_proficient_with_weapon(const std::string& class_name, const std::string& race_name, const Item& weapon);
// Armor or a shield. Unknown classes are proficient with nothing.
bool is_proficient_with_armor(const std::string& class_name, const Item& armor, const std::string& subclass = "");

// The equipped entry of each kind in an inventory, or nullptr.
const InventoryEntry* equipped_weapon(const std::vector<InventoryEntry>& inventory);
const InventoryEntry* equipped_armor(const std::vector<InventoryEntry>& inventory);
const InventoryEntry* equipped_shield(const std::vector<InventoryEntry>& inventory);

// --- Armor class -------------------------------------------------------------

struct ArmorClassPart {
    std::string label;  // "Chain Mail", "DEX", "Shield", "Unarmored Defense (CON)"
    int value = 0;
};

struct ArmorClassBreakdown {
    int total = 10;
    std::vector<ArmorClassPart> parts;
    // "Chain Mail 16 + Shield 2 = 18"; "Unarmored 10 + DEX 3 = 13".
    std::string text() const;
};

// AC from worn armor, DEX, a shield, and class features (PHB ch. 5):
//  - no armor: 10 + DEX, or Unarmored Defense if higher -- Barbarian + CON
//    (a shield is allowed), Monk + WIS (only without a shield);
//  - light armor: base + DEX; medium: base + DEX (max 2); heavy: base;
//  - a shield: +2;
//  - the Defense Fighting Style: +1 while wearing armor.
// Effect bonuses (Shield of Faith) are added on top in combat.
ArmorClassBreakdown calculate_armor_class(const Character& character, const std::string& class_name,
                                          const std::vector<InventoryEntry>& inventory);

// --- Weapon attacks ------------------------------------------------------------

struct WeaponAttack {
    std::string name;  // the weapon's name, or "Unarmed Strike"
    bool proficient = true;
    int attack_bonus = 0;       // ability modifier + proficiency (if proficient)
    int damage_dice_count = 1;  // 0 for an unarmed strike (flat damage)
    int damage_dice_sides = 4;
    int damage_bonus = 0;       // ability modifier (unarmed: 1 + STR)
    std::string damage_type;
    bool melee = true;
    bool versatile_two_handed = false;  // using a Versatile weapon's larger die
    bool uses_strength = true;       // the attack uses STR (Rage's damage bonus needs this)
    bool finesse_or_ranged = false;  // eligible for Sneak Attack
    bool unarmed = false;
    bool martial_arts = false;       // a Monk's unarmed strike or monk weapon (Martial Arts applies)
    bool great_weapon_fighting = false;  // reroll 1s and 2s on the damage dice (Fighting Style)
};

// The attack with the wielded weapon, or an unarmed strike (1 + STR
// bludgeoning, always proficient; PHB ch. 9) when nothing is wielded.
// Finesse weapons use the better of STR and DEX; ranged weapons use DEX;
// Versatile weapons use their larger die when no shield is worn. A Monk
// without armor or a shield gets Martial Arts (PHB ch. 3) with unarmed
// strikes and monk weapons (shortswords, and simple melee weapons that
// aren't two-handed or heavy): the better of STR and DEX, and the Martial
// Arts die if it's bigger than the weapon's. A Fighter's Fighting Style
// applies too: Archery (+2 to hit, ranged), Dueling (+2 damage, a melee
// weapon in one hand), Great Weapon Fighting (a melee weapon in two hands).
WeaponAttack weapon_attack(const Character& character, const std::string& class_name, const std::string& race_name,
                           const std::vector<InventoryEntry>& inventory);

// --- Wearing armor ---------------------------------------------------------------

// True if the worn armor or shield is one the class isn't proficient with
// (PHB ch. 5: disadvantage on STR/DEX attacks, checks, and saves; no spellcasting).
bool wearing_armor_without_proficiency(const std::string& class_name, const std::vector<InventoryEntry>& inventory,
                                       const std::string& subclass = "");

// The race's speed, minus 10 ft in heavy armor whose STR requirement the
// character doesn't meet.
int walking_speed(const Character& character, const std::string& race_name,
                  const std::vector<InventoryEntry>& inventory);

// --- Equipping -------------------------------------------------------------------

struct EquipResult {
    bool ok = false;
    std::string error;                      // why not, when !ok
    std::vector<int> unequipped;            // item ids taken off to make room
    std::vector<std::string> warnings;      // e.g. not proficient, too weak for heavy armor
};

// What's wrong with using `item` (a weapon, armor, or shield): not
// proficient, or too weak for heavy armor. Empty for anything else.
std::vector<std::string> equipment_warnings(const Character& character, const std::string& class_name,
                                            const std::string& race_name, const Item& item);

// Equips `item_id` in `inventory`, taking off whatever it replaces: one armor,
// one shield, and one weapon at a time, and a two-handed weapon can't be
// wielded with a shield. Items that aren't weapons, armor, or shields can't
// be equipped. The caller persists the changed flags (Repository::set_equipped).
EquipResult equip(std::vector<InventoryEntry>& inventory, int item_id, const Character& character,
                  const std::string& class_name, const std::string& race_name);
// Returns false if the item isn't in the inventory or wasn't equipped.
bool unequip(std::vector<InventoryEntry>& inventory, int item_id);

}  // namespace dnd
