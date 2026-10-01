#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "dnd/adventure.hpp"
#include "dnd/encounter_difficulty.hpp"
#include "dnd/repository.hpp"

using dnd::Difficulty;

TEST_CASE("XP thresholds by character level", "[difficulty]") {
    REQUIRE(dnd::xp_thresholds(1) == std::array<int, 4>{25, 50, 75, 100});
    REQUIRE(dnd::xp_thresholds(5) == std::array<int, 4>{250, 500, 750, 1100});
    REQUIRE(dnd::xp_thresholds(20) == std::array<int, 4>{2800, 5700, 8500, 12700});
    REQUIRE(dnd::xp_thresholds(0) == dnd::xp_thresholds(1));  // clamped
}

TEST_CASE("encounter multipliers", "[difficulty]") {
    REQUIRE(dnd::encounter_multiplier(1, 4) == 1.0);
    REQUIRE(dnd::encounter_multiplier(2, 4) == 1.5);
    REQUIRE(dnd::encounter_multiplier(3, 4) == 2.0);
    REQUIRE(dnd::encounter_multiplier(6, 4) == 2.0);
    REQUIRE(dnd::encounter_multiplier(7, 4) == 2.5);
    REQUIRE(dnd::encounter_multiplier(11, 4) == 3.0);
    REQUIRE(dnd::encounter_multiplier(15, 4) == 4.0);
    REQUIRE(dnd::encounter_multiplier(1, 1) == 1.5);   // small party: one step up
    REQUIRE(dnd::encounter_multiplier(15, 2) == 5.0);
    REQUIRE(dnd::encounter_multiplier(1, 6) == 0.5);   // large party: one step down
}

TEST_CASE("rating an encounter", "[difficulty]") {
    // DMG example-style: four level-3 characters vs a CR 2 Ogre (450 XP) and two goblins (50 each).
    auto rating = dnd::rate_encounter({3, 3, 3, 3}, {450, 50, 50});
    REQUIRE(rating.base_xp == 550);
    REQUIRE(rating.multiplier == 2.0);
    REQUIRE(rating.adjusted_xp == 1100);
    REQUIRE(rating.party_thresholds == std::array<int, 4>{300, 600, 900, 1600});
    REQUIRE(rating.difficulty == Difficulty::Hard);
    REQUIRE(rating.text() == "Hard: 1,100 adjusted XP (Easy 300, Medium 600, Hard 900, Deadly 1,600)");

    REQUIRE(dnd::rate_encounter({1}, {10}).difficulty == Difficulty::Trivial);  // 10 x1.5 = 15 < 25
    REQUIRE(dnd::rate_encounter({}, {450}).difficulty == Difficulty::Trivial);   // no party
    REQUIRE(dnd::rate_encounter({1}, {}).adjusted_xp == 0);
}

TEST_CASE("scaling scenes toward Medium-Hard (bosses toward Hard)", "[difficulty]") {
    dnd::Repository repo(":memory:");
    auto monsters = repo.get_monsters();
    const auto* goblins = dnd::find_adventure("Ambush on the Trail");
    const auto* troll = dnd::find_adventure("The Troll Bridge");
    REQUIRE(goblins);
    REQUIRE(troll);
    const auto& ambush = goblins->scenes.front();

    SECTION("three goblins are Deadly for a lone level-1 character: down to one") {
        auto scaled = dnd::scale_scene(ambush, {1}, monsters);
        REQUIRE(scaled.scaled);
        REQUIRE(scaled.scene.monsters[0].count == 1);  // 50 x1.5 = 75: Hard
        REQUIRE(scaled.rating.difficulty == Difficulty::Hard);
    }

    SECTION("more goblins for a level-3 party") {
        // Medium is 600 for four level-3s: 6 goblins x2 = 600.
        auto scaled = dnd::scale_scene(ambush, {3, 3, 3, 3}, monsters);
        REQUIRE(scaled.scene.monsters[0].count == 6);
        REQUIRE(scaled.rating.difficulty == Difficulty::Medium);
    }

    SECTION("a boss scene aims for Hard") {
        dnd::AdventureScene boss = ambush;
        boss.boss = true;
        // Hard is 900 for four level-3s: 9 goblins x2.5 = 1,125 (Hard); 8 x2.5 = 1,000 (Hard); 7 x2.5 = 875.
        auto scaled = dnd::scale_scene(boss, {3, 3, 3, 3}, monsters);
        REQUIRE(scaled.rating.difficulty == Difficulty::Hard);
        REQUIRE(scaled.scene.monsters[0].count == 8);
    }

    SECTION("goblins can't challenge a level-5 party: capped at 12") {
        auto scaled = dnd::scale_scene(ambush, {5, 5, 5, 5}, monsters);
        REQUIRE(scaled.scene.monsters[0].count == 12);  // 600 x3 = 1,800 < Medium 2,000
        REQUIRE(scaled.rating.difficulty == Difficulty::Easy);
    }

    SECTION("coins scale with the party's average level") {
        REQUIRE(dnd::scale_scene(ambush, {1}, monsters).scene.coins_cp == ambush.coins_cp);
        REQUIRE(dnd::scale_scene(ambush, {3, 4}, monsters).scene.coins_cp == ambush.coins_cp * 4);  // 3.5 rounds to 4
        REQUIRE(dnd::scale_scene(ambush, {}, monsters).scene.coins_cp == ambush.coins_cp);
    }

    SECTION("a lone troll stays a troll, even when it's Deadly") {
        auto scaled = dnd::scale_adventure(*troll, {1, 1}, monsters);
        REQUIRE(scaled.adventure.scenes[0].monsters[0].count == 1);
        REQUIRE(scaled.hardest == Difficulty::Deadly);
        REQUIRE_FALSE(scaled.scaled);
    }

    SECTION("every scene of every adventure scales for any party without breaking") {
        for (const auto& adventure : dnd::adventure_templates()) {
            for (std::vector<int> party : {std::vector<int>{1}, {3, 3, 3}, {7, 7, 7, 7}, {20}}) {
                auto scaled = dnd::scale_adventure(adventure, party, monsters);
                REQUIRE(scaled.ratings.size() == adventure.scenes.size());
                for (const auto& scene : scaled.adventure.scenes) {
                    int total = 0;
                    for (const auto& group : scene.monsters) {
                        REQUIRE(group.count >= 1);
                        total += group.count;
                    }
                    REQUIRE(total <= 12);
                }
            }
        }
    }
}
