#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "dnd/adventure.hpp"
#include "dnd/checks.hpp"
#include "dnd/repository.hpp"

TEST_CASE("every adventure template is well-formed", "[adventure]") {
    const auto& templates = dnd::adventure_templates();
    REQUIRE_FALSE(templates.empty());

    for (const auto& adventure : templates) {
        INFO(adventure.title);
        REQUIRE_FALSE(adventure.title.empty());
        REQUIRE_FALSE(adventure.hook.empty());
        REQUIRE_FALSE(adventure.victory_text.empty());
        REQUIRE(adventure.suggested_level >= 1);
        REQUIRE_FALSE(adventure.scenes.empty());
        REQUIRE_FALSE(adventure.scenes.back().camp_after);  // a camp is between scenes
        for (const auto& scene : adventure.scenes) {
            REQUIRE_FALSE(scene.title.empty());
            REQUIRE_FALSE(scene.narration.empty());
            INFO(scene.title);
            if (scene.kind == dnd::SceneKind::Trap) {
                dnd::Ability ability;
                REQUIRE_FALSE(scene.trap.name.empty());
                REQUIRE(dnd::check_ability(scene.trap.disarm_check, ability));
                REQUIRE(scene.trap.damage_dice >= 1);
                continue;
            }
            if (scene.kind != dnd::SceneKind::Fight) {
                REQUIRE(scene.monsters.empty());
                REQUIRE_FALSE(scene.options.empty());
                for (const auto& option : scene.options) {
                    INFO(option.label);
                    REQUIRE_FALSE(option.label.empty());
                    REQUIRE_FALSE(option.success.text.empty());
                    if (option.check.empty()) continue;
                    dnd::Ability ability;
                    REQUIRE(dnd::check_ability(option.check, ability));  // a real skill or ability
                    REQUIRE(option.dc >= 5);
                    REQUIRE_FALSE(option.failure.text.empty());
                }
                continue;
            }
            REQUIRE_FALSE(scene.monsters.empty());
            for (const auto& group : scene.monsters) {
                REQUIRE_FALSE(group.monster_name.empty());
                REQUIRE(group.count >= 1);
            }
        }
        REQUIRE(dnd::find_adventure(adventure.title) == &adventure);
    }
    REQUIRE(dnd::find_adventure("No Such Quest") == nullptr);
}

TEST_CASE("multi-scene adventures: camps and a boss at the end", "[adventure]") {
    int multi = 0;
    for (const auto& adventure : dnd::adventure_templates()) {
        if (adventure.scenes.size() < 2) continue;
        ++multi;
        INFO(adventure.title);
        REQUIRE(adventure.scenes.back().boss);
        REQUIRE(std::any_of(adventure.scenes.begin(), adventure.scenes.end(),
                            [](const dnd::AdventureScene& s) { return s.camp_after; }));
    }
    REQUIRE(multi >= 3);
}

TEST_CASE("every adventure's monsters and loot exist in the seeded data", "[adventure]") {
    dnd::Repository repo(":memory:");
    auto monsters = repo.get_monsters();
    auto items = repo.get_items();

    for (const auto& adventure : dnd::adventure_templates()) {
        for (const auto& scene : adventure.scenes) {
            for (const auto& group : scene.monsters) {
                bool found = std::any_of(monsters.begin(), monsters.end(),
                                         [&](const dnd::Monster& m) { return m.name == group.monster_name; });
                INFO(adventure.title << " / " << scene.title << ": " << group.monster_name);
                REQUIRE(found);
            }
            std::vector<dnd::AdventureLoot> all = scene.loot;
            for (const auto& option : scene.options) {
                all.insert(all.end(), option.success.loot.begin(), option.success.loot.end());
                all.insert(all.end(), option.failure.loot.begin(), option.failure.loot.end());
            }
            for (const auto& loot : all) {
                bool found = std::any_of(items.begin(), items.end(),
                                         [&](const dnd::Item& i) { return i.name == loot.item_name; });
                INFO(adventure.title << " / " << scene.title << ": " << loot.item_name);
                REQUIRE(found);
                REQUIRE(loot.quantity >= 1);
            }
        }
    }
}
