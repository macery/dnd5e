#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "dnd/adventure.hpp"
#include "dnd/repository.hpp"

TEST_CASE("every adventure template is well-formed", "[adventure]") {
    const auto& templates = dnd::adventure_templates();
    REQUIRE_FALSE(templates.empty());

    for (const auto& adventure : templates) {
        REQUIRE_FALSE(adventure.title.empty());
        REQUIRE_FALSE(adventure.hook.empty());
        REQUIRE_FALSE(adventure.victory_text.empty());
        REQUIRE_FALSE(adventure.monster_groups.empty());

        for (const auto& group : adventure.monster_groups) {
            REQUIRE_FALSE(group.monster_name.empty());
            REQUIRE(group.count >= 1);
        }
    }
}

TEST_CASE("every adventure's monsters and loot exist in the seeded data", "[adventure]") {
    dnd::Repository repo(":memory:");
    auto monsters = repo.get_monsters();
    auto items = repo.get_items();

    for (const auto& adventure : dnd::adventure_templates()) {
        for (const auto& group : adventure.monster_groups) {
            bool found = std::any_of(monsters.begin(), monsters.end(),
                                     [&](const dnd::Monster& m) { return m.name == group.monster_name; });
            INFO(adventure.title << ": " << group.monster_name);
            REQUIRE(found);
        }
        for (const auto& loot : adventure.loot) {
            bool found = std::any_of(items.begin(), items.end(),
                                     [&](const dnd::Item& i) { return i.name == loot.item_name; });
            INFO(adventure.title << ": " << loot.item_name);
            REQUIRE(found);
            REQUIRE(loot.quantity >= 1);
        }
    }
}
