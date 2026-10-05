// Scenes between fights: the Scene dialog for challenges, choices, and traps.

#include <catch2/catch_test_macros.hpp>

#include <QComboBox>
#include <QListWidget>

#include "SceneDialog.hpp"
#include "ui_helpers.hpp"

using namespace ui_test;

TEST_CASE("scene dialog: challenges, choices, and traps", "[ui][scenes]") {
    World world;
    auto aria = world.add("Aria", "Fighter", 3);
    auto bram = world.add("Bram", "Rogue", 3);
    const auto* warren = dnd::find_adventure("The Goblin Warren");
    dnd::AdventureRun run;
    run.title = warren->title;
    run.party = {aria.character_id, bram.character_id};
    run.run_id = world.repo.add_adventure_run(run);

    SECTION("a challenge shows each character's bonus; a choice applies at once") {
        SceneDialog dialog(world.repo, run, *warren, warren->scenes[0], "The Approach");
        dialog.show();
        settle();
        auto* options = dialog.findChild<QListWidget*>();
        REQUIRE(options->count() == 2);
        REQUIRE(options->item(0)->text().contains("Stealth, DC 12 (everyone rolls)"));
        REQUIRE(button(dialog, "Everyone rolls"));
        REQUIRE_FALSE(button(dialog, "Continue")->isEnabled());

        options->setCurrentRow(1);  // walk straight up the path: no roll
        settle();
        button(dialog, "Choose")->click();
        settle();
        REQUIRE(visibleText(dialog).contains("You make no secret of it."));
        REQUIRE(button(dialog, "Continue")->isEnabled());
        REQUIRE(run.next_scene == 1);
        REQUIRE(world.repo.get_adventure_runs(true).front().next_scene == 1);  // saved
    }

    SECTION("the roller list shows check bonuses") {
        dnd::AdventureScene scene = warren->scenes[4];  // the captive: Intimidation
        SceneDialog dialog(world.repo, run, *warren, scene, "The Captive");
        dialog.show();
        settle();
        auto* who = dialog.findChildren<QComboBox*>().front();
        REQUIRE(who->count() == 2);
        REQUIRE(who->itemText(0).contains("("));
        button(dialog, "Roll")->click();
        settle();
        REQUIRE(visibleText(dialog).contains("vs DC 12"));
        REQUIRE(button(dialog, "Continue")->isEnabled());
    }

    SECTION("an unnoticed trap is sprung on the leader") {
        dnd::AdventureScene pit;
        pit.title = "The Pit";
        pit.narration = "A dusty hall.";
        pit.kind = dnd::SceneKind::Trap;
        pit.trap.name = "a pit";
        pit.trap.spot_dc = 30;  // nobody notices
        pit.trap.save_dc = 30;  // nobody saves
        pit.trap.damage_dice = 1;
        pit.trap.damage_sides = 1;
        SceneDialog dialog(world.repo, run, *warren, pit, "The Pit");
        dialog.show();
        settle();
        REQUIRE(visibleText(dialog).contains("No one notices a pit"));
        REQUIRE(button(dialog, "Continue")->isEnabled());
        int hurt = 0;
        for (const auto& c : world.repo.get_characters()) {
            if (c.current_hp < c.max_hp) ++hurt;
        }
        REQUIRE(hurt == 1);
    }
}
