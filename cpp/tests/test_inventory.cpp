#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "dnd/inventory.hpp"
#include "dnd/repository.hpp"

namespace {

class FixedRandomSource : public dnd::IRandomSource {
public:
    explicit FixedRandomSource(int value) : value_(value) {}
    int roll_die(int /*sides*/) override { return value_; }

private:
    int value_;
};

dnd::Item potion_of_healing() {
    dnd::Item potion;
    potion.name = "Potion of Healing";
    potion.category = dnd::ItemCategory::Consumable;
    potion.heal_dice_count = 2;
    potion.heal_dice_sides = 4;
    potion.heal_bonus = 2;
    return potion;
}

int add_test_character(dnd::Repository& repo, const std::string& name) {
    int player_id = repo.add_player(name + "'s player");
    dnd::Character character;
    character.character_name = name;
    character.class_id = 1;
    character.race = 1;
    character.alignment = 1;
    character.player_id = player_id;
    return repo.add_character(character);
}

int item_id_named(const dnd::Repository& repo, const std::string& name) {
    for (const auto& item : repo.get_items()) {
        if (item.name == name) return item.item_id;
    }
    FAIL("no seeded item named " << name);
    return 0;
}

}  // namespace

TEST_CASE("item categories round-trip through their names", "[inventory]") {
    for (auto category : {dnd::ItemCategory::Gear, dnd::ItemCategory::Consumable, dnd::ItemCategory::Weapon,
                          dnd::ItemCategory::Armor, dnd::ItemCategory::Shield}) {
        REQUIRE(dnd::item_category_from_name(dnd::item_category_name(category)) == category);
    }
    REQUIRE_THROWS_AS(dnd::item_category_from_name("Spaceship"), std::invalid_argument);
}

TEST_CASE("use_healing_item rolls the item's dice and caps at max HP", "[inventory]") {
    FixedRandomSource fixed(3);  // 2d4 -> 3 + 3
    dnd::Dice dice(fixed);

    dnd::Character character;
    character.max_hp = 20;
    character.current_hp = 5;

    REQUIRE(dnd::use_healing_item(character, dice, potion_of_healing()) == 8);  // 6 + 2
    REQUIRE(character.current_hp == 13);

    REQUIRE(dnd::use_healing_item(character, dice, potion_of_healing()) == 7);  // capped
    REQUIRE(character.current_hp == 20);

    dnd::Item rope;
    rope.name = "Rope";
    REQUIRE_FALSE(dnd::is_healing_item(rope));
    REQUIRE_THROWS_AS(dnd::use_healing_item(character, dice, rope), std::invalid_argument);
}

TEST_CASE("total_weight and format_cost", "[inventory]") {
    dnd::Item rope;
    rope.weight_lb = 10;
    dnd::Item potion = potion_of_healing();
    potion.weight_lb = 0.5;
    REQUIRE(dnd::total_weight({{rope, 1}, {potion, 3}}) == 11.5);
    REQUIRE(dnd::total_weight({}) == 0.0);

    REQUIRE(dnd::format_cost(0) == "0 cp");
    REQUIRE(dnd::format_cost(1) == "1 cp");
    REQUIRE(dnd::format_cost(20) == "2 sp");
    REQUIRE(dnd::format_cost(250) == "2 gp 5 sp");
    REQUIRE(dnd::format_cost(5000) == "50 gp");
}

TEST_CASE("get_items returns the seeded items", "[inventory][repository]") {
    dnd::Repository repo(":memory:");
    auto items = repo.get_items();
    // 8 gear (including the Potion of Healing) + 16 weapons + 13 armor and
    // shield; the rest are magic items (test_magic_items.cpp).
    REQUIRE(std::count_if(items.begin(), items.end(), [](const dnd::Item& i) {
                return !i.magical() || i.name == "Potion of Healing";
            }) == 37);

    int potion_id = item_id_named(repo, "Potion of Healing");
    for (const auto& item : items) {
        if (item.item_id != potion_id) continue;
        REQUIRE(dnd::is_healing_item(item));
        REQUIRE(item.heal_dice_count == 2);
        REQUIRE(item.heal_dice_sides == 4);
        REQUIRE(item.heal_bonus == 2);
        REQUIRE(item.cost_cp == 5000);
    }
}

TEST_CASE("inventory add, stack, remove", "[inventory][repository]") {
    dnd::Repository repo(":memory:");
    int character_id = add_test_character(repo, "Aria");
    int potion_id = item_id_named(repo, "Potion of Healing");
    int torch_id = item_id_named(repo, "Torch");

    REQUIRE(repo.get_inventory(character_id).empty());

    repo.add_to_inventory(character_id, potion_id, 2);
    repo.add_to_inventory(character_id, potion_id);  // stacks
    repo.add_to_inventory(character_id, torch_id, 5);

    auto inventory = repo.get_inventory(character_id);
    REQUIRE(inventory.size() == 2);
    REQUIRE(inventory[0].item.name == "Potion of Healing");  // ordered by name
    REQUIRE(inventory[0].quantity == 3);
    REQUIRE(inventory[1].item.name == "Torch");
    REQUIRE(inventory[1].quantity == 5);

    REQUIRE(repo.remove_from_inventory(character_id, potion_id) == 1);
    REQUIRE(repo.get_inventory(character_id)[0].quantity == 2);

    SECTION("removing more than held removes the whole stack") {
        REQUIRE(repo.remove_from_inventory(character_id, potion_id, 10) == 2);
        inventory = repo.get_inventory(character_id);
        REQUIRE(inventory.size() == 1);
        REQUIRE(inventory[0].item.name == "Torch");
    }

    SECTION("removing an item not held does nothing") {
        int rope_id = item_id_named(repo, "Hempen Rope (50 feet)");
        REQUIRE(repo.remove_from_inventory(character_id, rope_id) == 0);
    }

    SECTION("quantities must be positive") {
        REQUIRE_THROWS_AS(repo.add_to_inventory(character_id, torch_id, 0), std::invalid_argument);
        REQUIRE_THROWS_AS(repo.remove_from_inventory(character_id, torch_id, -1), std::invalid_argument);
    }
}

TEST_CASE("inventories are per character and deleted with the character", "[inventory][repository]") {
    dnd::Repository repo(":memory:");
    int aria = add_test_character(repo, "Aria");
    int bram = add_test_character(repo, "Bram");
    int torch_id = item_id_named(repo, "Torch");

    repo.add_to_inventory(aria, torch_id, 2);
    repo.add_to_inventory(bram, torch_id, 7);
    REQUIRE(repo.get_inventory(aria)[0].quantity == 2);
    REQUIRE(repo.get_inventory(bram)[0].quantity == 7);

    repo.delete_character(aria);
    REQUIRE(repo.get_inventory(aria).empty());
    REQUIRE(repo.get_inventory(bram).size() == 1);

    // delete_player cascades to characters, and from there to inventory.
    repo.delete_player(repo.get_characters()[0].player_id);
    REQUIRE(repo.get_inventory(bram).empty());
}

TEST_CASE("weapons and armor load with their PHB stats", "[inventory][repository]") {
    dnd::Repository repo(":memory:");
    auto find = [&](const std::string& name) {
        for (const auto& item : repo.get_items()) {
            if (item.name == name) return item;
        }
        FAIL("no item " << name);
        return dnd::Item{};
    };

    auto longsword = find("Longsword");
    REQUIRE(longsword.category == dnd::ItemCategory::Weapon);
    REQUIRE(longsword.weapon.has_value());
    REQUIRE_FALSE(longsword.armor.has_value());
    REQUIRE(longsword.weapon->category == dnd::WeaponCategory::Martial);
    REQUIRE(longsword.weapon->damage_dice_sides == 8);
    REQUIRE(longsword.weapon->versatile_dice_sides == 10);
    REQUIRE(longsword.weapon->damage_type == "slashing");
    REQUIRE(longsword.cost_cp == 1500);

    auto crossbow = find("Light Crossbow");
    REQUIRE(crossbow.weapon->ranged);
    REQUIRE(crossbow.weapon->two_handed);
    REQUIRE(crossbow.weapon->loading);

    auto greatsword = find("Greatsword");
    REQUIRE(greatsword.weapon->damage_dice_count == 2);
    REQUIRE(greatsword.weapon->heavy);

    auto chain = find("Chain Mail");
    REQUIRE(chain.category == dnd::ItemCategory::Armor);
    REQUIRE(chain.armor->type == dnd::ArmorType::Heavy);
    REQUIRE(chain.armor->base_ac == 16);
    REQUIRE(chain.armor->strength_requirement == 13);
    REQUIRE(chain.armor->stealth_disadvantage);

    auto half_plate = find("Half Plate");
    REQUIRE(half_plate.armor->type == dnd::ArmorType::Medium);
    REQUIRE(half_plate.armor->max_dex_bonus == 2);

    auto shield = find("Shield");
    REQUIRE(shield.category == dnd::ItemCategory::Shield);
    REQUIRE(shield.armor->type == dnd::ArmorType::Shield);
    REQUIRE(shield.armor->base_ac == 2);

    REQUIRE_FALSE(find("Torch").weapon.has_value());
    REQUIRE_FALSE(find("Torch").armor.has_value());
}

TEST_CASE("equipped flags persist", "[inventory][repository]") {
    dnd::Repository repo(":memory:");
    int character_id = add_test_character(repo, "Aria");
    int sword = item_id_named(repo, "Longsword");
    int mail = item_id_named(repo, "Chain Mail");
    repo.add_to_inventory(character_id, sword);
    repo.add_to_inventory(character_id, mail);

    for (const auto& entry : repo.get_inventory(character_id)) REQUIRE_FALSE(entry.equipped);
    repo.set_equipped(character_id, mail, true);
    for (const auto& entry : repo.get_inventory(character_id)) {
        REQUIRE(entry.equipped == (entry.item.item_id == mail));
        if (entry.item.item_id == mail) REQUIRE(entry.item.armor->base_ac == 16);
    }
    repo.set_equipped(character_id, mail, false);
    for (const auto& entry : repo.get_inventory(character_id)) REQUIRE_FALSE(entry.equipped);
}
