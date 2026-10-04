// Campaigns on screen: building a roster, choosing the active campaign on
// the main window, and trading staying inside it.

#include <catch2/catch_test_macros.hpp>

#include <QLineEdit>
#include <QListWidget>
#include <QTableView>

#include "CampaignDialog.hpp"
#include "MainWindow.hpp"
#include "ShopDialog.hpp"
#include "dnd/campaign.hpp"
#include "ui_helpers.hpp"

using namespace ui_test;

TEST_CASE("campaigns: a roster from two players, then the active campaign", "[ui][campaign]") {
    World world;
    auto clark = world.add("Clark", "Fighter", 2);
    world.player = world.repo.add_player("you");
    auto phi = world.add("phi", "Rogue", 2);
    auto loner = world.add("Loner", "Wizard", 2);

    {
        CampaignDialog dialog(world.repo);
        dialog.show();
        settle();
        button(dialog, "New Campaign")->click();
        settle();
        dialog.findChild<QLineEdit*>()->setText("Lost Mine");
        QListWidget* roster = nullptr;
        for (auto* list : dialog.findChildren<QListWidget*>()) {
            if (list->count() == 3) roster = list;  // every character, campaign list has 1
        }
        REQUIRE(roster != nullptr);
        for (int row = 0; row < roster->count(); ++row) {
            auto* item = roster->item(row);
            if (!item->text().startsWith("Loner")) item->setCheckState(Qt::Checked);
        }
        button(dialog, "Save")->click();
        settle();
        REQUIRE(visibleText(dialog).contains("Saved Lost Mine."));
    }
    int mine = world.repo.get_campaigns().front().campaign_id;
    REQUIRE(world.get(clark.character_id).campaign_id == mine);
    REQUIRE(world.get(phi.character_id).campaign_id == mine);
    REQUIRE(world.get(loner.character_id).campaign_id == 0);

    MainWindow window(world.repo);
    window.show();
    settle();
    REQUIRE(window.windowTitle() == "DnD Engine");
    REQUIRE(choose(window, "Lost Mine"));
    REQUIRE(world.repo.active_campaign_id() == mine);
    REQUIRE(window.windowTitle() == "DnD Engine — Lost Mine");
    REQUIRE(dnd::characters_in_play(world.repo).size() == 2);

    // Give Coins offers only the campaign's characters.
    GiveCoinsDialog give(world.repo, world.get(clark.character_id));
    REQUIRE(comboWith(give, "phi (you)") != nullptr);
    REQUIRE(comboWith(give, "Loner") == nullptr);

    REQUIRE(choose(window, "No campaign"));
    REQUIRE(world.repo.active_campaign_id() == 0);
}

TEST_CASE("main window: everyone in play, with their players, filterable", "[ui][campaign]") {
    World world;
    auto clark = world.add("Clark", "Fighter", 2);
    world.player = world.repo.add_player("you");
    auto phi = world.add("phi", "Rogue", 2);
    world.add("Loner", "Wizard", 2);
    dnd::Campaign campaign;
    campaign.name = "Lost Mine";
    int mine = world.repo.add_campaign(campaign);
    world.repo.set_character_campaign(clark.character_id, mine);
    world.repo.set_character_campaign(phi.character_id, mine);

    MainWindow window(world.repo);
    window.show();
    settle();
    auto* table = window.findChild<QTableView*>();
    auto names = [&] {
        QStringList shown;
        for (int row = 0; row < table->model()->rowCount(); ++row) {
            shown << table->model()->index(row, 0).data().toString() + " / " +
                         table->model()->index(row, 1).data().toString();
        }
        return shown;
    };
    REQUIRE(table->model()->headerData(1, Qt::Horizontal).toString() == "Player");
    REQUIRE(names() == QStringList{"Clark / Adam", "phi / you", "Loner / you"});  // all players by default
    REQUIRE_FALSE(button(window, "Edit Player")->isEnabled());

    REQUIRE(choose(window, "Lost Mine"));
    REQUIRE(names() == QStringList{"Clark / Adam", "phi / you"});  // the campaign's, across players

    REQUIRE(choose(window, "you"));
    REQUIRE(names() == QStringList{"phi / you"});
    REQUIRE(button(window, "Edit Player")->isEnabled());

    REQUIRE(choose(window, "All players"));
    REQUIRE(choose(window, "No campaign"));
    REQUIRE(names().size() == 3);
}
