#include <catch2/catch_test_macros.hpp>

#include "dnd/adventure.hpp"

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
