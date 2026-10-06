// NPCs: the NPCs dialog, and the Shop's merchant and the Temple's priest.

#include <catch2/catch_test_macros.hpp>

#include <QCheckBox>
#include <QLineEdit>
#include <QListWidget>
#include <QTabWidget>

#include "NpcDialog.hpp"
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
