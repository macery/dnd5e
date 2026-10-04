// Subclasses on screen: choosing one, the sheet and table, and Channel
// Divinity and Fast Hands in combat.

#include <catch2/catch_test_macros.hpp>

#include <QListWidget>
#include <QPlainTextEdit>
#include <QTableView>

#include "CharacterSheetDialog.hpp"
#include "CombatEncounterDialog.hpp"
#include "MainWindow.hpp"
#include "SubclassDialog.hpp"
#include "ui_helpers.hpp"

using namespace ui_test;

TEST_CASE("subclass: choosing a Circle of the Land, with its land", "[ui][subclasses]") {
    World world;
    auto druid = world.add("Fern", "Druid", 2);
    SubclassDialog dialog(world.repo, world.get(druid.character_id), world.repo.get_classes());
    dialog.show();
    settle();
    REQUIRE(visibleText(dialog).contains("Natural Recovery"));
    REQUIRE(choose(dialog, "Forest"));
    button(dialog, "Choose")->click();
    settle();
    REQUIRE(dialog.result() == QDialog::Accepted);
    auto saved = world.get(druid.character_id);
    REQUIRE(saved.subclass == "Circle of the Land");
    REQUIRE(saved.subclass_choice == "Forest");

    CharacterSheetDialog sheet(saved, world.repo.get_classes(), world.repo.get_races(), world.repo.get_alignments(),
                               {}, {});
    sheet.show();
    settle();
    REQUIRE(visibleText(sheet).contains("Circle of the Land (Forest)"));
}

TEST_CASE("subclass: the table says when one is due", "[ui][subclasses]") {
    World world;
    world.add("Aria", "Fighter", 3);
    world.add("Bram", "Fighter", 2);
    MainWindow window(world.repo);
    window.show();
    settle();
    auto* model = window.findChild<QTableView*>()->model();
    int status = -1;
    for (int column = 0; column < model->columnCount(); ++column) {
        if (model->headerData(column, Qt::Horizontal).toString() == "Status") status = column;
    }
    REQUIRE(model->index(0, status).data().toString() == "Subclass due");
    REQUIRE(model->index(1, status).data().toString() == "");
    REQUIRE(button(window, "Choose Subclass") != nullptr);
}

TEST_CASE("combat: a cleric channels divinity to turn undead", "[ui][subclasses][combat]") {
    World world;
    auto cleric = world.add("Sister", "Cleric", 2);
    cleric.max_hp = 200;
    cleric.current_hp = 200;
    world.repo.update_character(cleric);
    AdventureFight fight;
    fight.title = "Test";
    fight.scene.title = "Crypt";
    fight.scene.narration = "Bones.";
    fight.scene.monsters = {{"Skeleton", 2}};
    CombatEncounterDialog combat(world.repo, {world.get(cleric.character_id)}, world.repo.get_monsters(), nullptr,
                                 &fight);
    combat.show();
    button(combat, "Roll Initiative")->click();
    settle();
    REQUIRE(comboWith(combat, "Channel Divinity: Turn Undead (1 left)") != nullptr);
    button(combat, "Channel")->click();
    settle();
    QString log = combat.findChild<QPlainTextEdit*>()->toPlainText();
    REQUIRE(log.contains("Sister turns undead (DC 12)."));  // 8 + 2 + WIS 2
    REQUIRE(log.count("Skeleton #") >= 2);
    REQUIRE(button(combat, "Channel") == nullptr);  // the use is spent (and the action)
}

TEST_CASE("combat: a Thief drinks a potion with Fast Hands", "[ui][subclasses][combat]") {
    World world;
    auto thief = world.add("Nim", "Rogue", 3);
    thief.subclass = "Thief";
    thief.max_hp = 200;
    thief.current_hp = 100;
    world.repo.update_character(thief);
    world.repo.add_to_inventory(thief.character_id, world.itemId("Potion of Healing"));
    AdventureFight fight;
    fight.title = "Test";
    fight.scene.title = "Goblins";
    fight.scene.narration = "Goblins.";
    fight.scene.monsters = {{"Goblin", 1}};
    CombatEncounterDialog combat(world.repo, {world.get(thief.character_id)}, world.repo.get_monsters(), nullptr,
                                 &fight);
    combat.show();
    button(combat, "Roll Initiative")->click();
    settle();
    REQUIRE(choose(combat, "Fast Hands: drink a potion"));
    REQUIRE(comboWith(combat, "Potion of Healing (1)") != nullptr);
    button(combat, "Use")->click();
    settle();
    REQUIRE(combat.findChild<QPlainTextEdit*>()->toPlainText().contains("Nim drinks a Potion of Healing"));
    REQUIRE(world.repo.get_inventory(thief.character_id).empty());
}
