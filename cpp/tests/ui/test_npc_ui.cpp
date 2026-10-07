// NPCs: the NPCs dialog, and the Shop's merchant and the Temple's priest.

#include <catch2/catch_test_macros.hpp>

#include <QCheckBox>
#include <QLineEdit>
#include <QListWidget>
#include <QTabWidget>
#include <QTableWidget>

#include "CombatEncounterDialog.hpp"
#include "NpcDialog.hpp"
#include "SceneDialog.hpp"
#include "ShopDialog.hpp"
#include "dnd/npc.hpp"
#include "dnd/shop.hpp"
#include "ui_helpers.hpp"

using namespace ui_test;

TEST_CASE("NPCs dialog: the campaign's people, edited and saved", "[ui][npcs]") {
    World world;
    dnd::Campaign table;
    table.name = "Tuesday Table";
    int campaign = world.repo.add_campaign(table);
    world.repo.set_active_campaign(campaign);

    NpcDialog dialog(world.repo);
    dialog.show();
    settle();
    REQUIRE(dialog.windowTitle().contains("Tuesday Table"));
    auto* list = dialog.findChild<QListWidget*>();
    REQUIRE(list->count() == 2);  // the merchant and the priest are always there
    REQUIRE_FALSE(button(dialog, "Delete")->isEnabled());

    button(dialog, "New NPC")->click();
    settle();
    REQUIRE(list->count() == 3);
    auto edits = dialog.findChildren<QLineEdit*>();
    edits[0]->setText("Mother Grell");  // name
    edits[1]->setText("innkeeper");     // role
    REQUIRE(choose(dialog, "Friendly"));
    REQUIRE(choose(dialog, "Veteran"));
    edits[2]->setText("a quiet winter");
    edits[3]->setText("the cult");
    dialog.findChild<QCheckBox*>()->setChecked(true);
    button(dialog, "Save")->click();
    settle();

    auto npcs = world.repo.get_npcs(campaign);
    auto grell = std::find_if(npcs.begin(), npcs.end(), [](const dnd::Npc& n) { return n.name == "Mother Grell"; });
    REQUIRE(grell != npcs.end());
    REQUIRE(grell->role == "innkeeper");
    REQUIRE(grell->attitude == dnd::Attitude::Friendly);
    REQUIRE(grell->stat_block == "Veteran");
    REQUIRE(grell->wants == "a quiet winter");
    REQUIRE(grell->known);
    REQUIRE(button(dialog, "Delete")->isEnabled());
    button(dialog, "Delete")->click();
    settle();
    REQUIRE(world.repo.get_npcs(campaign).size() == 2);
    REQUIRE(world.repo.get_npcs(0).empty());  // nothing leaked outside the campaign
}

TEST_CASE("shop: the merchant haggles once between long rests", "[ui][npcs][shop]") {
    World world;
    auto aria = world.add("Aria", "Bard", 3);
    aria.coins = {0, 0, 0, 100, 0};
    world.repo.update_character(aria);

    // A Friendly merchant who's been read: DC 5.
    dnd::Npc merchant = dnd::shop_merchant(world.repo, 0);
    merchant.attitude = dnd::Attitude::Friendly;
    merchant.known = true;
    world.repo.update_npc(merchant);

    ShopDialog shop(world.repo, world.get(aria.character_id), world.repo.get_classes(), world.repo.get_races());
    shop.show();
    settle();
    REQUIRE(visibleText(shop).contains("Hilda Brannock"));
    REQUIRE(visibleText(shop).contains("Friendly"));
    QPushButton* haggle = button(shop, "Haggle (Persuasion, DC 5)");
    REQUIRE(haggle);
    REQUIRE(haggle->isEnabled());
    haggle->click();
    settle();
    REQUIRE(visibleText(shop).contains("Persuasion"));
    REQUIRE_FALSE(haggle->isEnabled());  // once between long rests
    REQUIRE(world.get(aria.character_id).haggle != 0);

    // The Temple shows the priest.
    shop.tabs()->setCurrentIndex(2);
    settle();
    REQUIRE(visibleText(shop).contains("Brother Aldous"));
}

TEST_CASE("combat: an NPC ally fights beside the party", "[ui][npcs][allies]") {
    World world;
    auto aria = world.add("Aria", "Fighter", 3, {"Chain Mail", "Longsword"});
    dnd::AdventureRun run;
    run.title = "The Hill Giant's Toll";
    run.party = {aria.character_id};
    run.allies = {"dagna"};
    auto monsters = world.repo.get_monsters();

    AdventureFight fight;
    fight.title = "The Ogre Gate";
    fight.scene.title = "The Ogre Gate";
    fight.scene.narration = "Two ogres.";
    fight.scene.monsters = {{"Goblin", 1}};
    fight.allies = dnd::next_fight_allies(world.repo, run, monsters);
    REQUIRE(fight.allies.size() == 1);
    CombatEncounterDialog combat(world.repo, {world.get(aria.character_id)}, monsters, nullptr, &fight);
    combat.show();
    settle();
    REQUIRE(visibleText(combat).contains("Fighting beside you: Dagna Ironfoot"));
    auto* table = combat.findChild<QTableWidget*>();
    bool listed = false;
    for (int row = 0; row < table->rowCount(); ++row) {
        listed = listed || table->item(row, 0)->text() == "Dagna Ironfoot (Veteran) — ally";
    }
    REQUIRE(listed);
}

TEST_CASE("scene: with nothing left to ask, the party can still move on", "[ui][npcs][scenes]") {
    World world;
    auto aria = world.add("Aria", "Bard", 3);
    const auto* ashgrove = dnd::find_adventure("Moonrise at Ashgrove");
    dnd::AdventureRun run;
    run.title = ashgrove->title;
    run.party = {aria.character_id};
    run.run_id = world.repo.add_adventure_run(run);
    const dnd::AdventureScene* talk = nullptr;
    for (const auto& scene : ashgrove->scenes) {
        if (scene.kind == dnd::SceneKind::Social) talk = &scene;
    }
    REQUIRE(talk);

    // Leaned on last time: Wenna is Hostile, and both requests are out of reach.
    dnd::Npc wenna = *dnd::find_run_npc(world.repo, run, "wenna");
    wenna.attitude = dnd::Attitude::Hostile;
    world.repo.update_npc(wenna);

    SceneDialog dialog(world.repo, run, *ashgrove, *talk, "The Miller's Wife");
    dialog.show();
    settle();
    auto* options = dialog.findChild<QListWidget*>();
    REQUIRE(options->count() == 3);
    REQUIRE(options->item(2)->text() == "Move on");
    REQUIRE(options->currentRow() == 2);
    button(dialog, "Ask")->click();
    settle();
    REQUIRE(visibleText(dialog).contains("nothing more to say"));
    REQUIRE(button(dialog, "Continue")->isEnabled());
}
