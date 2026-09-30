#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "dnd/equipment.hpp"
#include "dnd/repository.hpp"

namespace {

// Seeded items by name, from an in-memory repository.
struct Items {
    dnd::Repository repo{":memory:"};
    std::vector<dnd::Item> all = repo.get_items();

    dnd::Item operator()(const std::string& name) const {
        for (const auto& item : all) {
            if (item.name == name) return item;
        }
        FAIL("no item " << name);
        return {};
    }

    // An inventory holding these items, all equipped.
    std::vector<dnd::InventoryEntry> wearing(std::initializer_list<const char*> names) const {
        std::vector<dnd::InventoryEntry> inventory;
        for (const char* name : names) inventory.push_back({(*this)(name), 1, true});
        return inventory;
    }
};

dnd::Character stats(int str, int dex, int con = 10, int wis = 10, int level = 1) {
    dnd::Character c;
    c.strength = str;
    c.dexterity = dex;
    c.constitution = con;
    c.wisdom = wis;
    c.level = level;
    return c;
}

}  // namespace

TEST_CASE("every class has proficiencies and a kit made of seeded items", "[equipment]") {
    Items items;
    for (const auto& info : dnd::class_infos()) {
        INFO(info.name);
        const dnd::ClassEquipment* equipment = dnd::class_equipment(info.name);
        REQUIRE(equipment != nullptr);
        REQUIRE_FALSE(equipment->starting_kit.empty());
        for (const auto& starting : equipment->starting_kit) {
            INFO(starting.item_name);
            auto item = items(starting.item_name);
            // Every class is trained with its own kit.
            if (item.weapon) REQUIRE(dnd::is_proficient_with_weapon(info.name, "Human", item));
            if (item.armor) REQUIRE(dnd::is_proficient_with_armor(info.name, item));
        }
    }
    REQUIRE(dnd::class_equipment("Artificer") == nullptr);
}

TEST_CASE("weapon and armor proficiency", "[equipment]") {
    Items items;
    REQUIRE(dnd::is_proficient_with_weapon("Fighter", "Human", items("Greatsword")));
    REQUIRE(dnd::is_proficient_with_weapon("Rogue", "Human", items("Rapier")));        // named
    REQUIRE(dnd::is_proficient_with_weapon("Rogue", "Human", items("Dagger")));        // simple
    REQUIRE_FALSE(dnd::is_proficient_with_weapon("Rogue", "Human", items("Greataxe")));
    REQUIRE_FALSE(dnd::is_proficient_with_weapon("Wizard", "Human", items("Longsword")));
    REQUIRE(dnd::is_proficient_with_weapon("Wizard", "Human", items("Quarterstaff")));
    REQUIRE_FALSE(dnd::is_proficient_with_weapon("Druid", "Human", items("Longsword")));
    REQUIRE(dnd::is_proficient_with_weapon("Druid", "Human", items("Scimitar")));
    REQUIRE(dnd::is_proficient_with_weapon("Wizard", "Dwarf", items("Warhammer")));  // Dwarven Combat Training

    REQUIRE(dnd::is_proficient_with_armor("Fighter", items("Plate Armor")));
    REQUIRE(dnd::is_proficient_with_armor("Cleric", items("Half Plate")));
    REQUIRE_FALSE(dnd::is_proficient_with_armor("Cleric", items("Plate Armor")));
    REQUIRE_FALSE(dnd::is_proficient_with_armor("Rogue", items("Shield")));
    REQUIRE_FALSE(dnd::is_proficient_with_armor("Wizard", items("Leather Armor")));
    REQUIRE_FALSE(dnd::is_proficient_with_armor("Fighter", items("Longsword")));  // not armor
}

TEST_CASE("armor class from armor, DEX, shields, and class features", "[equipment]") {
    Items items;

    SECTION("unarmored") {
        auto ac = dnd::calculate_armor_class(stats(10, 16), "Wizard", {});
        REQUIRE(ac.total == 13);
        REQUIRE(ac.text() == "Unarmored 10 + DEX 3 = 13");
    }

    SECTION("Barbarian Unarmored Defense, shield allowed") {
        auto barbarian = stats(16, 14, 16);
        REQUIRE(dnd::calculate_armor_class(barbarian, "Barbarian", {}).text() ==
                "Unarmored Defense 10 + DEX 2 + CON 3 = 15");
        REQUIRE(dnd::calculate_armor_class(barbarian, "Barbarian", items.wearing({"Shield"})).total == 17);
        // A low CON never makes Unarmored Defense worse than plain 10 + DEX.
        REQUIRE(dnd::calculate_armor_class(stats(16, 14, 6), "Barbarian", {}).text() == "Unarmored 10 + DEX 2 = 12");
    }

    SECTION("Monk Unarmored Defense, lost with a shield") {
        auto monk = stats(10, 16, 10, 14);
        REQUIRE(dnd::calculate_armor_class(monk, "Monk", {}).total == 15);                          // 10 + 3 + 2
        REQUIRE(dnd::calculate_armor_class(monk, "Monk", items.wearing({"Shield"})).total == 15);  // 10 + 3 + shield 2
        REQUIRE(dnd::calculate_armor_class(monk, "Monk", items.wearing({"Shield"})).text() ==
                "Unarmored 10 + DEX 3 + Shield 2 = 15");
    }

    SECTION("light armor adds all of DEX") {
        auto ac = dnd::calculate_armor_class(stats(10, 18), "Rogue", items.wearing({"Studded Leather Armor"}));
        REQUIRE(ac.total == 16);  // 12 + 4
    }

    SECTION("medium armor caps DEX at +2") {
        auto ac = dnd::calculate_armor_class(stats(10, 18), "Ranger", items.wearing({"Scale Mail"}));
        REQUIRE(ac.total == 16);  // 14 + 2
        REQUIRE(ac.text() == "Scale Mail 14 + DEX (max 2) 2 = 16");
        REQUIRE(dnd::calculate_armor_class(stats(10, 8), "Ranger", items.wearing({"Scale Mail"})).total == 13);
    }

    SECTION("heavy armor ignores DEX; a shield adds 2") {
        auto ac = dnd::calculate_armor_class(stats(16, 8), "Fighter", items.wearing({"Chain Mail", "Shield"}));
        REQUIRE(ac.total == 18);
        REQUIRE(ac.text() == "Chain Mail 16 + Shield 2 = 18");
    }

    SECTION("carried but unequipped armor doesn't count") {
        auto inventory = items.wearing({"Plate Armor"});
        inventory[0].equipped = false;
        REQUIRE(dnd::calculate_armor_class(stats(10, 14), "Fighter", inventory).total == 12);
    }
}

TEST_CASE("weapon attacks from the wielded weapon", "[equipment]") {
    Items items;
    auto fighter = stats(16, 14);  // STR +3, DEX +2, proficiency +2

    SECTION("strength weapon") {
        auto attack = dnd::weapon_attack(fighter, "Fighter", "Human", items.wearing({"Greataxe"}));
        REQUIRE(attack.name == "Greataxe");
        REQUIRE(attack.attack_bonus == 5);
        REQUIRE(attack.damage_dice_sides == 12);
        REQUIRE(attack.damage_bonus == 3);
        REQUIRE(attack.damage_type == "slashing");
        REQUIRE(attack.melee);
    }

    SECTION("finesse uses the better of STR and DEX; ranged uses DEX") {
        auto rogue = stats(8, 18);  // STR -1, DEX +4
        REQUIRE(dnd::weapon_attack(rogue, "Rogue", "Human", items.wearing({"Rapier"})).attack_bonus == 6);
        auto bow = dnd::weapon_attack(fighter, "Fighter", "Human", items.wearing({"Longbow"}));
        REQUIRE(bow.attack_bonus == 4);  // DEX +2 + 2
        REQUIRE_FALSE(bow.melee);
    }

    SECTION("versatile: the bigger die without a shield") {
        auto two_handed = dnd::weapon_attack(fighter, "Fighter", "Human", items.wearing({"Longsword"}));
        REQUIRE(two_handed.damage_dice_sides == 10);
        REQUIRE(two_handed.versatile_two_handed);
        auto with_shield = dnd::weapon_attack(fighter, "Fighter", "Human", items.wearing({"Longsword", "Shield"}));
        REQUIRE(with_shield.damage_dice_sides == 8);
        REQUIRE_FALSE(with_shield.versatile_two_handed);
    }

    SECTION("no proficiency bonus with an untrained weapon") {
        auto wizard = stats(10, 14);
        auto attack = dnd::weapon_attack(wizard, "Wizard", "Human", items.wearing({"Longsword"}));
        REQUIRE_FALSE(attack.proficient);
        REQUIRE(attack.attack_bonus == 0);  // STR +0, no proficiency
    }

    SECTION("unarmed strike: 1 + STR, always proficient") {
        auto attack = dnd::weapon_attack(fighter, "Fighter", "Human", {});
        REQUIRE(attack.name == "Unarmed Strike");
        REQUIRE(attack.attack_bonus == 5);
        REQUIRE(attack.damage_dice_count == 0);
        REQUIRE(attack.damage_bonus == 4);
    }
}

TEST_CASE("equipping swaps out what an item replaces", "[equipment]") {
    Items items;
    auto fighter = stats(16, 12);
    std::vector<dnd::InventoryEntry> inventory = {
        {items("Chain Mail"), 1, true},  {items("Leather Armor"), 1, false}, {items("Longsword"), 1, true},
        {items("Greataxe"), 1, false},   {items("Shield"), 1, true},         {items("Torch"), 5, false},
    };
    auto equipped = [&inventory](const std::string& name) {
        for (const auto& e : inventory) {
            if (e.item.name == name) return e.equipped;
        }
        return false;
    };

    SECTION("one armor at a time") {
        auto result = dnd::equip(inventory, items("Leather Armor").item_id, fighter, "Fighter", "Human");
        REQUIRE(result.ok);
        REQUIRE(equipped("Leather Armor"));
        REQUIRE_FALSE(equipped("Chain Mail"));
        REQUIRE(result.unequipped == std::vector<int>{items("Chain Mail").item_id});
    }

    SECTION("a two-handed weapon takes off the shield and the other weapon") {
        auto result = dnd::equip(inventory, items("Greataxe").item_id, fighter, "Fighter", "Human");
        REQUIRE(result.ok);
        REQUIRE(equipped("Greataxe"));
        REQUIRE_FALSE(equipped("Longsword"));
        REQUIRE_FALSE(equipped("Shield"));
        REQUIRE(result.unequipped.size() == 2);

        auto back = dnd::equip(inventory, items("Shield").item_id, fighter, "Fighter", "Human");
        REQUIRE(back.ok);
        REQUIRE_FALSE(equipped("Greataxe"));  // and a shield takes off the two-handed weapon
    }

    SECTION("gear can't be equipped") {
        auto result = dnd::equip(inventory, items("Torch").item_id, fighter, "Fighter", "Human");
        REQUIRE_FALSE(result.ok);
        REQUIRE(result.error == "Torch can't be equipped.");
    }

    SECTION("warnings for missing proficiency and strength") {
        auto wizard = stats(8, 14);
        auto result = dnd::equip(inventory, items("Chain Mail").item_id, wizard, "Wizard", "Human");
        REQUIRE(result.ok);  // allowed, with consequences
        REQUIRE(result.warnings.size() == 2);
        REQUIRE(result.warnings[0].rfind("Not proficient with Chain Mail", 0) == 0);
        REQUIRE(result.warnings[1] == "Chain Mail needs STR 13: speed drops by 10 ft.");
        REQUIRE(dnd::wearing_armor_without_proficiency("Wizard", inventory));
        REQUIRE_FALSE(dnd::wearing_armor_without_proficiency("Fighter", inventory));
    }

    SECTION("unequip") {
        REQUIRE(dnd::unequip(inventory, items("Shield").item_id));
        REQUIRE_FALSE(equipped("Shield"));
        REQUIRE_FALSE(dnd::unequip(inventory, items("Shield").item_id));
    }
}

TEST_CASE("walking speed", "[equipment]") {
    Items items;
    REQUIRE(dnd::walking_speed(stats(10, 10), "Human", {}) == 30);
    REQUIRE(dnd::walking_speed(stats(10, 10), "Dwarf", {}) == 25);
    REQUIRE(dnd::walking_speed(stats(12, 10), "Human", items.wearing({"Chain Mail"})) == 20);  // needs STR 13
    REQUIRE(dnd::walking_speed(stats(13, 10), "Human", items.wearing({"Chain Mail"})) == 30);
    REQUIRE(dnd::walking_speed(stats(8, 10), "Human", items.wearing({"Scale Mail"})) == 30);  // medium: no requirement
}

TEST_CASE("Martial Arts: an unarmored Monk's strikes", "[equipment][class_features]") {
    Items items;
    auto monk = stats(10, 16);  // STR +0, DEX +3; level 1: d4

    auto unarmed = dnd::weapon_attack(monk, "Monk", "Human", {});
    REQUIRE(unarmed.martial_arts);
    REQUIRE(unarmed.attack_bonus == 5);  // DEX +3 + 2
    REQUIRE(unarmed.damage_dice_count == 1);
    REQUIRE(unarmed.damage_dice_sides == 4);
    REQUIRE(unarmed.damage_bonus == 3);
    REQUIRE_FALSE(unarmed.uses_strength);

    auto shortsword = dnd::weapon_attack(monk, "Monk", "Human", items.wearing({"Shortsword"}));
    REQUIRE(shortsword.martial_arts);
    REQUIRE(shortsword.damage_dice_sides == 6);  // the weapon's d6 beats the d4

    auto club_like = dnd::weapon_attack(monk, "Monk", "Human", items.wearing({"Mace"}));
    REQUIRE(club_like.martial_arts);
    REQUIRE(club_like.attack_bonus == 5);  // DEX with a simple melee weapon

    auto greataxe = dnd::weapon_attack(monk, "Monk", "Human", items.wearing({"Greataxe"}));
    REQUIRE_FALSE(greataxe.martial_arts);  // martial, heavy, two-handed

    // Armor or a shield turns Martial Arts off.
    auto armored = dnd::weapon_attack(monk, "Monk", "Human", items.wearing({"Leather Armor"}));
    REQUIRE_FALSE(armored.martial_arts);
    REQUIRE(armored.damage_bonus == 1);  // plain unarmed strike: 1 + STR

    auto higher = stats(10, 16, 10, 10, 5);
    REQUIRE(dnd::weapon_attack(higher, "Monk", "Human", {}).damage_dice_sides == 6);  // d6 at level 5
}

TEST_CASE("weapon attacks report STR use and Sneak Attack eligibility", "[equipment]") {
    Items items;
    auto strong = stats(16, 12);
    auto quick = stats(10, 16);
    REQUIRE(dnd::weapon_attack(strong, "Fighter", "Human", items.wearing({"Greataxe"})).uses_strength);
    REQUIRE_FALSE(dnd::weapon_attack(strong, "Fighter", "Human", items.wearing({"Greataxe"})).finesse_or_ranged);
    REQUIRE(dnd::weapon_attack(strong, "Rogue", "Human", items.wearing({"Rapier"})).uses_strength);  // STR is higher
    REQUIRE_FALSE(dnd::weapon_attack(quick, "Rogue", "Human", items.wearing({"Rapier"})).uses_strength);
    REQUIRE(dnd::weapon_attack(quick, "Rogue", "Human", items.wearing({"Rapier"})).finesse_or_ranged);
    REQUIRE_FALSE(dnd::weapon_attack(strong, "Ranger", "Human", items.wearing({"Longbow"})).uses_strength);
    REQUIRE(dnd::weapon_attack(strong, "Ranger", "Human", items.wearing({"Longbow"})).finesse_or_ranged);
    REQUIRE(dnd::weapon_attack(strong, "Fighter", "Human", {}).unarmed);
}

TEST_CASE("Fighting Styles change AC and weapon attacks", "[equipment][class_features]") {
    Items items;
    auto fighter = stats(16, 14);  // STR +3, DEX +2, proficiency +2

    SECTION("Defense: +1 AC in armor only") {
        fighter.fighting_style = "Defense";
        REQUIRE(dnd::calculate_armor_class(fighter, "Fighter", items.wearing({"Chain Mail"})).text() ==
                "Chain Mail 16 + Defense 1 = 17");
        REQUIRE(dnd::calculate_armor_class(fighter, "Fighter", {}).total == 12);
    }

    SECTION("Archery: +2 to hit with ranged weapons") {
        fighter.fighting_style = "Archery";
        REQUIRE(dnd::weapon_attack(fighter, "Fighter", "Human", items.wearing({"Longbow"})).attack_bonus == 6);
        REQUIRE(dnd::weapon_attack(fighter, "Fighter", "Human", items.wearing({"Longsword"})).attack_bonus == 5);
    }

    SECTION("Dueling: +2 damage one-handed, not two-handed") {
        fighter.fighting_style = "Dueling";
        REQUIRE(dnd::weapon_attack(fighter, "Fighter", "Human", items.wearing({"Longsword", "Shield"})).damage_bonus ==
                5);
        // No shield: the longsword is used two-handed (d10), so no Dueling.
        auto two_handed = dnd::weapon_attack(fighter, "Fighter", "Human", items.wearing({"Longsword"}));
        REQUIRE(two_handed.damage_dice_sides == 10);
        REQUIRE(two_handed.damage_bonus == 3);
    }

    SECTION("Great Weapon Fighting: two-handed melee weapons only") {
        fighter.fighting_style = "GreatWeaponFighting";
        REQUIRE(dnd::weapon_attack(fighter, "Fighter", "Human", items.wearing({"Greatsword"})).great_weapon_fighting);
        REQUIRE_FALSE(
            dnd::weapon_attack(fighter, "Fighter", "Human", items.wearing({"Longsword", "Shield"})).great_weapon_fighting);
        REQUIRE_FALSE(dnd::weapon_attack(fighter, "Fighter", "Human", items.wearing({"Longbow"})).great_weapon_fighting);
    }

    SECTION("only Fighters") {
        fighter.fighting_style = "Archery";
        REQUIRE(dnd::weapon_attack(fighter, "Ranger", "Human", items.wearing({"Longbow"})).attack_bonus == 4);
    }
}
