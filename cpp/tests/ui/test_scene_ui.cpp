// Scenes between fights: the Scene dialog for challenges, choices, and traps.

#include <catch2/catch_test_macros.hpp>

#include <QComboBox>
#include <QListWidget>

#include <algorithm>

#include "SceneDialog.hpp"
#include "ui_helpers.hpp"

using namespace ui_test;

namespace {

dnd::AdventureScene sceneTitled(const dnd::AdventureTemplate& adventure, const std::string& title) {
    for (const auto& scene : adventure.scenes) {
        if (scene.title == title) return scene;
    }
    FAIL("no scene " << title);
    return {};
}

}  // namespace

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
        const auto* crypt = dnd::find_adventure("Crypt of the Restless");
        dnd::AdventureScene scene = sceneTitled(*crypt, "The Sealed Door");  // Religion, DC 13
        SceneDialog dialog(world.repo, run, *crypt, scene, "The Sealed Door");
        dialog.show();
        settle();
        auto* who = comboWith(dialog, "Aria");
        REQUIRE(who->count() == 2);
        REQUIRE(who->itemText(0).contains("("));
        button(dialog, "Roll")->click();
        settle();
        REQUIRE(visibleText(dialog).contains("vs DC 13"));
        REQUIRE(button(dialog, "Continue")->isEnabled());
    }

    SECTION("a conversation: the NPC, reading them, and DCs from their attitude") {
        dnd::AdventureScene captive = sceneTitled(*warren, "The Captive");
        SceneDialog dialog(world.repo, run, *warren, captive, "The Captive");
        dialog.show();
        settle();
        QString text = visibleText(dialog);
        REQUIRE(text.contains("Snig"));
        REQUIRE(text.contains("Indifferent"));
        REQUIRE(text.contains("don't know yet"));
        auto* options = dialog.findChild<QListWidget*>();
        REQUIRE(options->item(0)->text().contains("help at no risk: DC 10"));
        REQUIRE(options->item(1)->text().contains("help at a minor risk: DC 20"));
        REQUIRE(comboWith(dialog, "Persuasion"));
        REQUIRE(comboWith(dialog, "Intimidation"));

        // Snig is saved in the run's campaign the first time it's met.
        auto npcs = world.repo.get_npcs(0);
        auto snig = std::find_if(npcs.begin(), npcs.end(), [](const dnd::Npc& n) { return n.key == "snig"; });
        REQUIRE(snig != npcs.end());

        button(dialog, "Read them")->click();
        settle();
        REQUIRE(visibleText(dialog).contains("Insight"));
        QPushButton* read = button(dialog, "Read them");  // gone once known; one try either way
        REQUIRE((!read || !read->isEnabled()));
        bool known = false;
        for (const auto& n : world.repo.get_npcs(0)) {
            if (n.key == "snig") known = n.known;
        }
        if (known) {
            REQUIRE(visibleText(dialog).contains("Wants:"));
            REQUIRE(options->item(0)->text().contains("DC 5"));
        }

        options->setCurrentRow(1);
        settle();
        button(dialog, "Roll")->click();
        settle();
        REQUIRE(visibleText(dialog).contains("Snig"));
        REQUIRE(button(dialog, "Continue")->isEnabled());
    }

    SECTION("a request a Friendly NPC grants needs no check; one out of reach can't be picked") {
        const auto* toll = dnd::find_adventure("The Hill Giant's Toll");
        run.title = toll->title;
        dnd::AdventureScene caravan = sceneTitled(*toll, "The Stranded Caravan");
        SceneDialog dialog(world.repo, run, *toll, caravan, "The Stranded Caravan");
        dialog.show();
        settle();
        auto* options = dialog.findChild<QListWidget*>();
        REQUIRE(options->item(0)->text().contains("no check needed"));
        REQUIRE(options->item(1)->text().contains("DC 20"));
        button(dialog, "Ask")->click();
        settle();
        REQUIRE(visibleText(dialog).contains("no check needed"));
        int potions = 0;
        for (int id : run.party) {
            for (const auto& entry : world.repo.get_inventory(id)) {
                if (entry.item.name == "Potion of Healing") potions += entry.quantity;
            }
        }
        REQUIRE(potions == 2);

        // A Hostile NPC won't take a risk for the party, whatever is said.
        auto npcs = world.repo.get_npcs(0);
        auto dagna = *std::find_if(npcs.begin(), npcs.end(), [](const dnd::Npc& n) { return n.key == "dagna"; });
        dagna.attitude = dnd::Attitude::Hostile;
        world.repo.update_npc(dagna);
        SceneDialog again(world.repo, run, *toll, caravan, "The Stranded Caravan");
        again.show();
        settle();
        auto* list = again.findChild<QListWidget*>();
        REQUIRE(list->item(1)->text().contains("not while they're Hostile"));
        REQUIRE_FALSE(list->item(1)->flags() & Qt::ItemIsEnabled);
        REQUIRE(list->currentRow() == 0);  // the first one that can be tried
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
