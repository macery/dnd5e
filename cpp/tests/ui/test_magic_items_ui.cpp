// Magic items on screen: reading a scroll in combat, attuning during a
// short rest, the Shop's magic filter, and the sheet's raised scores.

#include <catch2/catch_test_macros.hpp>

#include <QPlainTextEdit>
#include <QTableWidget>

#include "AdventureDialogs.hpp"
#include "CharacterSheetDialog.hpp"
#include "CombatEncounterDialog.hpp"
#include "InventoryDialog.hpp"
#include "ShopDialog.hpp"
#include "dnd/magic_items.hpp"
#include "ui_helpers.hpp"

using namespace ui_test;

namespace {

bool tableHas(QWidget& root, const QString& text) {
    for (auto* table : root.findChildren<QTableWidget*>()) {
        for (int row = 0; row < table->rowCount(); ++row) {
            if (table->item(row, 0) && table->item(row, 0)->text() == text) return true;
        }
    }
    return false;
}

}  // namespace

TEST_CASE("combat: reading a spell scroll", "[ui][magic_items]") {
    World world;
    auto zanna = world.add("Zanna", "Wizard", 3);
    zanna.max_hp = 200;
    zanna.current_hp = 200;
    world.repo.update_character(zanna);
    world.repo.add_to_inventory(zanna.character_id, world.itemId("Spell Scroll (Magic Missile)"));
    AdventureFight fight;
    fight.title = "Test";
    fight.scene.title = "Orcs";
    fight.scene.narration = "Orcs.";
    fight.scene.monsters = {{"Orc", 2}};
    CombatEncounterDialog combat(world.repo, {zanna}, world.repo.get_monsters(), nullptr, &fight);
    combat.show();
    button(combat, "Roll Initiative")->click();
    settle();

    REQUIRE(choose(combat, "Read Scroll"));
    REQUIRE(comboWith(combat, "1 × Spell Scroll (Magic Missile)") != nullptr);
    REQUIRE(visibleText(combat).contains("Always hits"));
    button(combat, "Read Scroll")->click();
    settle();
    QString log = combat.findChild<QPlainTextEdit*>()->toPlainText();
    REQUIRE(log.contains("Zanna reads a Spell Scroll (Magic Missile). Zanna casts Magic Missile."));
    REQUIRE(world.repo.get_inventory(zanna.character_id).empty());  // used up, and saved
    REQUIRE(world.get(zanna.character_id).spell_slots_used[0] == 0);  // no slot spent
}

TEST_CASE("short rest: attuning to a magic item", "[ui][magic_items]") {
    World world;
    auto aria = world.add("Aria", "Fighter", 3);
    world.repo.add_to_inventory(aria.character_id, world.itemId("Cloak of Protection"));
    ShortRestDialog rest(world.repo, {world.get(aria.character_id)}, world.repo.get_classes());
    rest.show();
    settle();
    REQUIRE(choose(rest, "Attune: Cloak of Protection"));
    button(rest, "Rest")->click();
    settle();
    REQUIRE(rest.summary().join("\n").contains("Aria: attunes to the Cloak of Protection"));
    REQUIRE(world.repo.get_inventory(aria.character_id).front().attuned);

    // Attuned: it can't be dropped or given away until attunement ends.
    InventoryDialog inventory(world.repo, world.get(aria.character_id), world.repo.get_classes(),
                              world.repo.get_races());
    inventory.show();
    settle();
    auto* table = inventory.findChild<QTableWidget*>();
    table->selectRow(0);
    settle();
    REQUIRE(table->item(0, 0)->text() == "Cloak of Protection (Uncommon)");
    REQUIRE_FALSE(button(inventory, "Drop One")->isEnabled());
    REQUIRE_FALSE(button(inventory, "Give")->isEnabled());
    REQUIRE(visibleText(inventory).contains("Attuned:</b> 1 of 3"));
}

TEST_CASE("shop: common and uncommon magic only", "[ui][magic_items][shop]") {
    World world;
    auto aria = world.add("Aria", "Fighter", 3);
    ShopDialog shop(world.repo, aria, world.repo.get_classes(), world.repo.get_races());
    shop.show();
    settle();
    REQUIRE(choose(shop, "Magic items"));
    REQUIRE(tableHas(shop, "Longsword +1 (Uncommon)"));
    REQUIRE(tableHas(shop, "Spell Scroll (Magic Missile) (Common)"));
    REQUIRE_FALSE(tableHas(shop, "Longsword +2 (Rare)"));
    REQUIRE_FALSE(tableHas(shop, "Amulet of Health (Rare)"));
    REQUIRE_FALSE(tableHas(shop, "Torch"));  // not magic
}

TEST_CASE("sheet: an attuned item's raised score", "[ui][magic_items]") {
    World world;
    auto aria = world.add("Aria", "Fighter", 3);  // STR 16
    world.repo.add_to_inventory(aria.character_id, world.itemId("Gauntlets of Ogre Power"));
    REQUIRE(dnd::attune(world.repo, aria.character_id, world.itemId("Gauntlets of Ogre Power")).ok);
    CharacterSheetDialog sheet(world.get(aria.character_id), world.repo.get_classes(), world.repo.get_races(),
                               world.repo.get_alignments(), world.repo.get_inventory(aria.character_id), {});
    sheet.show();
    settle();
    REQUIRE(visibleText(sheet).contains("19 (+4) — Gauntlets of Ogre Power; 16 without it"));
}
