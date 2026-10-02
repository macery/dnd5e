// The Shop: buying, selling, the temple, and giving coins.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>

#include "CharacterSheetDialog.hpp"
#include "InventoryDialog.hpp"
#include "ShopDialog.hpp"
#include "dnd/shop.hpp"
#include "ui_helpers.hpp"

using namespace ui_test;

namespace {

// Selects the row whose first column is `name` in whichever table has it.
bool selectRow(QWidget& root, const QString& name) {
    for (auto* table : root.findChildren<QTableWidget*>()) {
        for (int row = 0; row < table->rowCount(); ++row) {
            if (table->isVisible() && table->item(row, 0)->text() == name) {
                table->selectRow(row);
                settle();
                return true;
            }
        }
    }
    return false;
}

dnd::Character withCoins(World& world, dnd::Character c, int gp) {
    c.coins = {0, 0, 0, gp, 0};
    world.repo.update_character(c);
    return world.get(c.character_id);
}

int count(World& world, int character_id, const std::string& item) {
    for (const auto& entry : world.repo.get_inventory(character_id)) {
        if (entry.item.name == item) return entry.quantity;
    }
    return 0;
}

}  // namespace

TEST_CASE("shop: buying, with warnings and a price on the button", "[ui][shop]") {
    World world;
    auto zanna = withCoins(world, world.add("Zanna", "Wizard", 1), 20);
    ShopDialog shop(world.repo, zanna, world.repo.get_classes(), world.repo.get_races());
    shop.show();
    settle();
    REQUIRE(visibleText(shop).contains("Coins:</b> 20 gp"));

    REQUIRE(selectRow(shop, "Plate Armor"));
    QPushButton* buy = button(shop, "Buy for");
    REQUIRE(buy != nullptr);
    REQUIRE(buy->text() == "Buy for 1500 gp");
    REQUIRE_FALSE(buy->isEnabled());
    REQUIRE(buy->toolTip().startsWith("1480 gp short"));
    REQUIRE(visibleText(shop).contains("Not proficient with Plate Armor"));

    REQUIRE(selectRow(shop, "Dagger"));
    for (auto* spin : shop.findChildren<QSpinBox*>()) {
        if (spin->isVisible()) spin->setValue(2);  // the Buy tab's quantity
    }
    settle();
    REQUIRE(button(shop, "Buy for")->text() == "Buy for 4 gp");
    button(shop, "Buy for")->click();
    settle();
    REQUIRE(visibleText(shop).contains("Zanna bought 2 × Dagger for 4 gp."));
    REQUIRE(visibleText(shop).contains("Coins:</b> 16 gp"));
    REQUIRE(count(world, zanna.character_id, "Dagger") == 2);
}

TEST_CASE("shop: equipped gear can't be sold until it's unequipped", "[ui][shop]") {
    World world;
    auto aria = world.add("Aria", "Fighter", 1, {"Longsword"});
    world.repo.add_to_inventory(aria.character_id, world.itemId("Dagger"));
    ShopDialog shop(world.repo, aria, world.repo.get_classes(), world.repo.get_races());
    shop.show();
    shop.tabs()->setCurrentIndex(1);
    settle();

    REQUIRE(selectRow(shop, "Longsword"));
    REQUIRE_FALSE(button(shop, "Sell for")->isEnabled());
    REQUIRE(visibleText(shop).contains("Unequip the Longsword in the Inventory before selling it."));

    REQUIRE(selectRow(shop, "Dagger"));
    QPushButton* sell = button(shop, "Sell for");
    REQUIRE(sell->text() == "Sell for 1 gp");
    REQUIRE(sell->isEnabled());
    sell->click();
    settle();
    REQUIRE(count(world, aria.character_id, "Dagger") == 0);
    REQUIRE(world.get(aria.character_id).coins[3] == 1);
}

TEST_CASE("shop: pooling coins to raise the dead", "[ui][shop]") {
    World world;
    auto bram = world.add("Bram", "Fighter", 3);
    bram.dead = true;
    bram.current_hp = 0;
    world.repo.update_character(bram);
    auto aria = withCoins(world, world.add("Aria", "Fighter", 3), 300);
    auto cleo = withCoins(world, world.add("Cleo", "Cleric", 3), 300);

    ShopDialog shop(world.repo, world.get(bram.character_id), world.repo.get_classes(), world.repo.get_races());
    shop.show();
    settle();
    REQUIRE(shop.tabs()->currentIndex() == 2);  // a dead character opens on the Temple
    REQUIRE(visibleText(shop).contains("raise dead"));
    REQUIRE(choose(shop, "Aria"));
    QPushButton* raise = button(shop, "Raise Dead");
    REQUIRE(raise->text() == "Raise Dead (550 gp)");
    REQUIRE_FALSE(raise->isEnabled());  // 300 gp each: not enough alone
    REQUIRE(raise->toolTip().startsWith("250 gp short"));

    // Cleo gives Aria 250 gp.
    {
        GiveCoinsDialog give(world.repo, world.get(cleo.character_id));
        give.show();
        REQUIRE(choose(give, "Aria"));
        for (auto* spin : give.findChildren<QSpinBox*>()) {
            if (spin->suffix() == " of 300") spin->setValue(250);
        }
        button(give, "Give")->click();
        settle();
        REQUIRE(give.result() == QDialog::Accepted);
        REQUIRE(give.message() == "Cleo gave Aria 250 gp.");
    }

    ShopDialog temple(world.repo, world.get(bram.character_id), world.repo.get_classes(), world.repo.get_races());
    temple.show();
    settle();
    REQUIRE(choose(temple, "Aria"));
    button(temple, "Raise Dead")->click();
    settle();
    auto raised = world.get(bram.character_id);
    REQUIRE_FALSE(raised.dead);
    REQUIRE(raised.current_hp == 1);
    REQUIRE(raised.revival_penalty == 4);
    REQUIRE(world.get(aria.character_id).coins[3] == 0);
    REQUIRE(visibleText(temple).contains("No one needs raising."));

    CharacterSheetDialog sheet(raised, world.repo.get_classes(), world.repo.get_races(), world.repo.get_alignments(),
                               {}, {});
    sheet.show();
    settle();
    REQUIRE(visibleText(sheet).contains("-4 to attack rolls, saving throws, and ability checks"));
}

TEST_CASE("inventory: giving an item to another player's character", "[ui][shop]") {
    World world;
    auto clark = world.add("Clark", "Fighter", 1, {"Longsword"});
    world.repo.add_to_inventory(clark.character_id, world.itemId("Potion of Healing"), 2);
    world.player = world.repo.add_player("you");  // phi belongs to another player
    dnd::Character phi = world.add("phi", "Rogue", 1);

    {
        InventoryDialog inventory(world.repo, world.get(clark.character_id), world.repo.get_classes(),
                                  world.repo.get_races());
        inventory.show();
        settle();
        REQUIRE(selectRow(inventory, "Longsword"));
        REQUIRE_FALSE(button(inventory, "Give")->isEnabled());  // equipped
        REQUIRE(selectRow(inventory, "Potion of Healing"));
        REQUIRE(button(inventory, "Give")->isEnabled());
    }

    auto potion = world.repo.get_inventory(clark.character_id);
    auto entry = std::find_if(potion.begin(), potion.end(),
                              [](const dnd::InventoryEntry& e) { return e.item.name == "Potion of Healing"; });
    GiveItemDialog give(world.repo, world.get(clark.character_id), *entry);
    give.show();
    settle();
    REQUIRE(comboWith(give, "phi (you)") != nullptr);
    give.findChild<QSpinBox*>()->setValue(2);
    button(give, "Give")->click();
    settle();
    REQUIRE(give.result() == QDialog::Accepted);
    REQUIRE(give.message() == "Clark gave phi 2 × Potion of Healing.");
    REQUIRE(count(world, clark.character_id, "Potion of Healing") == 0);
    REQUIRE(count(world, phi.character_id, "Potion of Healing") == 2);
}
