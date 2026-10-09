#include "dnd/equipment.hpp"

#include <algorithm>
#include <functional>
#include <map>

#include "dnd/class_features.hpp"
#include "dnd/class_options.hpp"
#include "dnd/lasting_effects.hpp"
#include "dnd/magic_items.hpp"
#include "dnd/rules.hpp"

namespace dnd {

namespace {

const std::map<std::string, ClassEquipment>& class_equipment_table() {
    // 2014 PHB ch. 3. Kits list the main weapon first: that's the one wielded.
    static const std::map<std::string, ClassEquipment> table = {
        {"Barbarian", {{true, true, false, true}, {true, true, {}}, {{"Greataxe", 1}, {"Handaxe", 2}}}},
        {"Bard", {{true, false, false, false}, {true, false, {"Hand Crossbow", "Longsword", "Rapier", "Shortsword"}},
                  {{"Leather Armor", 1}, {"Rapier", 1}, {"Dagger", 1}}}},
        {"Cleric", {{true, true, false, true}, {true, false, {}},
                    {{"Scale Mail", 1}, {"Shield", 1}, {"Mace", 1}, {"Light Crossbow", 1}}}},
        // Druids won't wear metal armor or shields; the item data doesn't say
        // what things are made of yet, so that's left to the player.
        {"Druid", {{true, true, false, true},
                   {false, false,
                    {"Club", "Dagger", "Dart", "Javelin", "Mace", "Quarterstaff", "Scimitar", "Sickle", "Sling", "Spear"}},
                   {{"Leather Armor", 1}, {"Shield", 1}, {"Quarterstaff", 1}}}},
        {"Fighter", {{true, true, true, true}, {true, true, {}},
                     {{"Chain Mail", 1}, {"Shield", 1}, {"Longsword", 1}, {"Light Crossbow", 1}}}},
        {"Monk", {{false, false, false, false}, {true, false, {"Shortsword"}}, {{"Shortsword", 1}}}},
        {"Paladin", {{true, true, true, true}, {true, true, {}},
                     {{"Chain Mail", 1}, {"Shield", 1}, {"Longsword", 1}}}},
        {"Ranger", {{true, true, false, true}, {true, true, {}},
                    {{"Scale Mail", 1}, {"Longbow", 1}, {"Shortsword", 2}}}},
        {"Rogue", {{true, false, false, false}, {true, false, {"Hand Crossbow", "Longsword", "Rapier", "Shortsword"}},
                   {{"Leather Armor", 1}, {"Rapier", 1}, {"Shortbow", 1}, {"Dagger", 2}}}},
        {"Sorcerer", {{false, false, false, false},
                      {false, false, {"Dagger", "Dart", "Sling", "Quarterstaff", "Light Crossbow"}},
                      {{"Light Crossbow", 1}, {"Dagger", 2}}}},
        {"Warlock", {{true, false, false, false}, {true, false, {}},
                     {{"Leather Armor", 1}, {"Light Crossbow", 1}, {"Dagger", 2}}}},
        {"Wizard", {{false, false, false, false},
                    {false, false, {"Dagger", "Dart", "Sling", "Quarterstaff", "Light Crossbow"}},
                    {{"Quarterstaff", 1}}}},
    };
    return table;
}

const InventoryEntry* equipped_of(const std::vector<InventoryEntry>& inventory, ItemCategory category) {
    for (const auto& entry : inventory) {
        if (entry.equipped && entry.item.category == category) return &entry;
    }
    return nullptr;
}

bool contains(const std::vector<std::string>& names, const std::string& name) {
    return std::find(names.begin(), names.end(), name) != names.end();
}

}  // namespace

const ClassEquipment* class_equipment(const std::string& class_name) {
    const auto& table = class_equipment_table();
    auto found = table.find(class_name);
    return found == table.end() ? nullptr : &found->second;
}

std::vector<std::string> race_weapon_proficiencies(const std::string& race_name) {
    // Dwarven Combat Training (PHB ch. 2). The Elf's weapon training belongs
    // to its subraces, which aren't modeled yet.
    if (race_name == "Dwarf") return {"Battleaxe", "Handaxe", "Light Hammer", "Warhammer"};
    return {};
}

bool is_proficient_with_weapon(const std::string& class_name, const std::string& race_name, const Item& weapon) {
    if (!weapon.weapon) return false;
    // A +1 rapier is still a rapier.
    const std::string& name = weapon.base_item.empty() ? weapon.name : weapon.base_item;
    if (contains(race_weapon_proficiencies(race_name), name)) return true;
    const ClassEquipment* info = class_equipment(class_name);
    if (!info) return false;
    if (weapon.weapon->category == WeaponCategory::Simple && info->weapons.simple) return true;
    if (weapon.weapon->category == WeaponCategory::Martial && info->weapons.martial) return true;
    return contains(info->weapons.weapons, name);
}

bool is_proficient_with_armor(const std::string& class_name, const Item& armor, const std::string& subclass) {
    if (!armor.armor) return false;
    // Life Domain clerics also train with heavy armor.
    if (subclass == "Life Domain" && armor.armor->type == ArmorType::Heavy) return true;
    const ClassEquipment* info = class_equipment(class_name);
    if (!info) return false;
    switch (armor.armor->type) {
        case ArmorType::Light: return info->armor.light;
        case ArmorType::Medium: return info->armor.medium;
        case ArmorType::Heavy: return info->armor.heavy;
        case ArmorType::Shield: return info->armor.shields;
    }
    return false;
}

const InventoryEntry* equipped_weapon(const std::vector<InventoryEntry>& inventory) {
    return equipped_of(inventory, ItemCategory::Weapon);
}

const InventoryEntry* equipped_armor(const std::vector<InventoryEntry>& inventory) {
    return equipped_of(inventory, ItemCategory::Armor);
}

const InventoryEntry* equipped_shield(const std::vector<InventoryEntry>& inventory) {
    return equipped_of(inventory, ItemCategory::Shield);
}

// --- Armor class -------------------------------------------------------------

std::string ArmorClassBreakdown::text() const {
    std::string result;
    for (const auto& part : parts) {
        if (result.empty()) {
            result = part.label + " " + std::to_string(part.value);
        } else {
            result += " + " + part.label + " " + std::to_string(part.value);
        }
    }
    return result + " = " + std::to_string(total);
}

ArmorClassBreakdown calculate_armor_class(const Character& stored, const std::string& class_name,
                                          const std::vector<InventoryEntry>& inventory) {
    const Character character = with_item_scores(stored, inventory);  // an Amulet of Health's CON
    ArmorClassBreakdown ac;
    int dex = ability_modifier(character.dexterity);
    const InventoryEntry* armor = equipped_armor(inventory);
    const InventoryEntry* shield = equipped_shield(inventory);

    if (armor && armor->item.armor) {
        const ArmorStats& stats = *armor->item.armor;
        ac.parts.push_back({armor->item.name, stats.base_ac + armor->item.magic_bonus});
        switch (stats.type) {
            case ArmorType::Light:
                ac.parts.push_back({"DEX", dex});
                break;
            case ArmorType::Medium: {
                int capped = stats.max_dex_bonus >= 0 ? std::min(dex, stats.max_dex_bonus) : dex;
                ac.parts.push_back({capped < dex ? "DEX (max " + std::to_string(stats.max_dex_bonus) + ")" : "DEX",
                                    capped});
                break;
            }
            case ArmorType::Heavy:
            case ArmorType::Shield:
                break;
        }
    } else {
        // Unarmored: 10 + DEX, or Unarmored Defense when it's better.
        std::vector<ArmorClassPart> best = {{"Unarmored", 10}, {"DEX", dex}};
        int best_total = 10 + dex;
        if (class_name == "Barbarian") {
            int con = ability_modifier(character.constitution);
            if (10 + dex + con > best_total) {
                best = {{"Unarmored Defense", 10}, {"DEX", dex}, {"CON", con}};
                best_total = 10 + dex + con;
            }
        } else if (class_name == "Warlock" && has_armor_of_shadows(character)) {
            // Armor of Shadows: Mage Armor at will, 13 + DEX.
            if (13 + dex > best_total) {
                best = {{"Armor of Shadows", 13}, {"DEX", dex}};
                best_total = 13 + dex;
            }
        } else if (class_name == "Sorcerer" && character.subclass == "Draconic Bloodline") {
            // Draconic Resilience: 13 + DEX unarmored (a shield is fine).
            if (13 + dex > best_total) {
                best = {{"Draconic Resilience", 13}, {"DEX", dex}};
                best_total = 13 + dex;
            }
        } else if (class_name == "Monk" && !shield) {
            int wis = ability_modifier(character.wisdom);
            if (10 + dex + wis > best_total) {
                best = {{"Unarmored Defense", 10}, {"DEX", dex}, {"WIS", wis}};
                best_total = 10 + dex + wis;
            }
        }
        // Mage Armor is one more unarmored base; only the best one counts.
        if (has_lasting_effect(character, kMageArmor) && 13 + dex > best_total) {
            best = {{kMageArmor, 13}, {"DEX", dex}};
            best_total = 13 + dex;
        }
        ac.parts = best;
    }
    if (shield && shield->item.armor) {
        ac.parts.push_back({shield->item.name, shield->item.armor->base_ac + shield->item.magic_bonus});
    }
    // Worn wondrous items: Cloak and Ring of Protection always; Bracers of
    // Defense only with no armor and no shield.
    for (const auto& entry : inventory) {
        if (!item_active(entry)) continue;
        if (entry.item.ac_bonus != 0) ac.parts.push_back({entry.item.name, entry.item.ac_bonus});
        if (entry.item.unarmored_ac_bonus != 0 && !armor && !shield) {
            ac.parts.push_back({entry.item.name, entry.item.unarmored_ac_bonus});
        }
    }
    if (armor && armor->item.armor && fighting_style(character, class_name) == FightingStyle::Defense) {
        ac.parts.push_back({"Defense", 1});
    }

    ac.total = 0;
    for (const auto& part : ac.parts) ac.total += part.value;
    return ac;
}

// --- Weapon attacks ------------------------------------------------------------

WeaponAttack weapon_attack(const Character& stored, const std::string& class_name, const std::string& race_name,
                           const std::vector<InventoryEntry>& inventory) {
    const Character character = with_item_scores(stored, inventory);  // Gauntlets of Ogre Power's STR
    WeaponAttack attack;
    int str = ability_modifier(character.strength);
    int dex = ability_modifier(character.dexterity);
    int proficiency = proficiency_bonus(character.level);

    const InventoryEntry* wielded = equipped_weapon(inventory);
    bool monk_unarmored = class_name == "Monk" && !equipped_armor(inventory) && !equipped_shield(inventory);
    if (!wielded || !wielded->item.weapon) {
        attack.name = "Unarmed Strike";
        attack.proficient = true;
        attack.unarmed = true;
        attack.damage_type = "bludgeoning";
        attack.melee = true;
        attack.magical = class_name == "Monk" && character.level >= 6;  // Ki-Empowered Strikes
        if (monk_unarmored) {  // Martial Arts: the better of STR and DEX, and the Martial Arts die
            int ability = std::max(str, dex);
            attack.martial_arts = true;
            attack.uses_strength = str >= dex;
            attack.attack_bonus = ability + proficiency;
            attack.damage_dice_count = 1;
            attack.damage_dice_sides = martial_arts_die(character.level);
            attack.damage_bonus = ability;
        } else {
            attack.attack_bonus = str + proficiency;
            attack.damage_dice_count = 0;
            attack.damage_dice_sides = 1;
            attack.damage_bonus = 1 + str;
        }
        return attack;
    }

    const WeaponStats& stats = *wielded->item.weapon;
    bool monk_weapon = monk_unarmored && !stats.ranged && !stats.two_handed && !stats.heavy &&
                       (stats.category == WeaponCategory::Simple || wielded->item.name == "Shortsword" ||
                        wielded->item.base_item == "Shortsword");
    int ability = stats.ranged ? dex : (stats.finesse || monk_weapon) ? std::max(str, dex) : str;
    attack.uses_strength = !stats.ranged && ability == str;
    attack.finesse_or_ranged = stats.finesse || stats.ranged;
    attack.martial_arts = monk_weapon;
    attack.name = wielded->item.name;
    attack.proficient = is_proficient_with_weapon(class_name, race_name, wielded->item) ||
                        (class_name == "Warlock" && has_pact_blade(character) && !stats.ranged);  // pact weapon
    attack.attack_bonus = ability + (attack.proficient ? proficiency : 0);
    attack.damage_dice_count = stats.damage_dice_count;
    attack.damage_dice_sides = stats.damage_dice_sides;
    if (stats.versatile_dice_sides > 0 && !equipped_shield(inventory)) {
        attack.damage_dice_sides = stats.versatile_dice_sides;
        attack.versatile_two_handed = true;
    }
    if (monk_weapon && martial_arts_die(character.level) > attack.damage_dice_sides &&
        attack.damage_dice_count == 1) {
        attack.damage_dice_sides = martial_arts_die(character.level);
        attack.versatile_two_handed = false;
    }
    attack.damage_bonus = ability;
    attack.damage_type = stats.damage_type;
    attack.melee = !stats.ranged;

    switch (fighting_style(character, class_name)) {
        case FightingStyle::Archery:
            if (stats.ranged) attack.attack_bonus += 2;
            break;
        case FightingStyle::Dueling:
            if (attack.melee && !stats.two_handed && !attack.versatile_two_handed) attack.damage_bonus += 2;
            break;
        case FightingStyle::GreatWeaponFighting:
            attack.great_weapon_fighting = attack.melee && (stats.two_handed || attack.versatile_two_handed);
            break;
        case FightingStyle::Defense:
        case FightingStyle::None:
            break;
    }
    if (item_active(*wielded)) {  // a +N weapon
        attack.attack_bonus += wielded->item.magic_bonus;
        attack.damage_bonus += wielded->item.magic_bonus;
    }
    attack.magical = wielded->item.magic_bonus > 0 || (class_name == "Warlock" && has_pact_blade(character) && attack.melee);
    return attack;
}

// --- Wearing armor ---------------------------------------------------------------

bool wearing_armor_without_proficiency(const std::string& class_name, const std::vector<InventoryEntry>& inventory,
                                       const std::string& subclass) {
    for (const InventoryEntry* worn : {equipped_armor(inventory), equipped_shield(inventory)}) {
        if (worn && !is_proficient_with_armor(class_name, worn->item, subclass)) return true;
    }
    return false;
}

int walking_speed(const Character& character, const std::string& race_name,
                  const std::vector<InventoryEntry>& inventory) {
    const RaceInfo* race = find_race_info(race_name);
    int speed = race ? race->speed : 30;
    const InventoryEntry* armor = equipped_armor(inventory);
    if (armor && armor->item.armor && armor->item.armor->type == ArmorType::Heavy &&
        with_item_scores(character, inventory).strength < armor->item.armor->strength_requirement) {
        speed -= 10;
    }
    return speed;
}

// --- Equipping -------------------------------------------------------------------

std::vector<std::string> equipment_warnings(const Character& character, const std::string& class_name,
                                            const std::string& race_name, const Item& item) {
    std::vector<std::string> warnings;
    ItemCategory category = item.category;
    if (category == ItemCategory::Weapon && !is_proficient_with_weapon(class_name, race_name, item)) {
        warnings.push_back("Not proficient with " + item.name +
                           ": attacks with it won't add the proficiency bonus.");
    }
    if ((category == ItemCategory::Armor || category == ItemCategory::Shield) &&
        !is_proficient_with_armor(class_name, item, character.subclass)) {
        warnings.push_back("Not proficient with " + item.name +
                           ": while wearing it, attacks, ability checks, and saves using STR or DEX have "
                           "disadvantage, and no spells can be cast.");
    }
    if (item.armor && item.armor->type == ArmorType::Heavy && character.strength < item.armor->strength_requirement) {
        warnings.push_back(item.name + " needs STR " + std::to_string(item.armor->strength_requirement) +
                           ": speed drops by 10 ft.");
    }
    return warnings;
}

EquipResult equip(std::vector<InventoryEntry>& inventory, int item_id, const Character& character,
                  const std::string& class_name, const std::string& race_name) {
    EquipResult result;
    auto target = std::find_if(inventory.begin(), inventory.end(),
                               [item_id](const InventoryEntry& e) { return e.item.item_id == item_id; });
    if (target == inventory.end()) {
        result.error = "That item isn't in the inventory.";
        return result;
    }
    const Item& item = target->item;
    ItemCategory category = item.category;
    if (category == ItemCategory::Wondrous) {
        result.error = item.name + " is worn by attuning to it, which takes a short rest.";
        return result;
    }
    if (category != ItemCategory::Weapon && category != ItemCategory::Armor && category != ItemCategory::Shield) {
        result.error = item.name + " can't be equipped.";
        return result;
    }

    auto take_off = [&](const std::function<bool(const InventoryEntry&)>& matches) {
        for (auto& entry : inventory) {
            if (entry.equipped && entry.item.item_id != item_id && matches(entry)) {
                entry.equipped = false;
                result.unequipped.push_back(entry.item.item_id);
            }
        }
    };
    auto is = [](ItemCategory c) { return [c](const InventoryEntry& e) { return e.item.category == c; }; };

    take_off(is(category));  // one armor, one shield, one weapon
    if (category == ItemCategory::Shield) {
        take_off([](const InventoryEntry& e) { return e.item.weapon && e.item.weapon->two_handed; });
    }
    if (category == ItemCategory::Weapon && item.weapon && item.weapon->two_handed) {
        take_off(is(ItemCategory::Shield));
    }
    target->equipped = true;
    result.ok = true;

    result.warnings = equipment_warnings(character, class_name, race_name, item);
    return result;
}

bool unequip(std::vector<InventoryEntry>& inventory, int item_id) {
    for (auto& entry : inventory) {
        if (entry.item.item_id == item_id && entry.equipped) {
            entry.equipped = false;
            return true;
        }
    }
    return false;
}

}  // namespace dnd
