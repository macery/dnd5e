// Class Options (a warlock's invocations and Pact Boon), and Wild Shape in combat.

#include <catch2/catch_test_macros.hpp>

#include <QGroupBox>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QListWidget>
#include <QTableView>

#include "ClassOptionsDialog.hpp"
#include "CombatEncounterDialog.hpp"
#include "MainWindow.hpp"
#include "ui_helpers.hpp"

using namespace ui_test;

namespace {

void check(QWidget& root, const QString& group, const QStringList& names) {
    for (auto* box : root.findChildren<QGroupBox*>()) {
        if (!box->title().startsWith(group)) continue;
        auto* list = box->findChild<QListWidget*>();
        for (int row = 0; row < list->count(); ++row) {
            if (names.contains(list->item(row)->text())) list->item(row)->setCheckState(Qt::Checked);
        }
    }
}

}  // namespace

TEST_CASE("class options: a level-3 warlock's invocations and pact", "[ui][class_options]") {
    World world;
    auto warlock = world.add("Hex", "Warlock", 3);
    warlock.subclass = "The Fiend";  // a subclass due would show first
    world.repo.update_character(warlock);
    MainWindow window(world.repo);
    window.show();
    settle();
    auto* model = window.findChild<QTableView*>()->model();
    REQUIRE(model->index(0, 7).data().toString() == "Choices due");

    ClassOptionsDialog dialog(world.repo, world.get(warlock.character_id), world.repo.get_classes());
    dialog.show();
    settle();
    REQUIRE(visibleText(dialog).contains("has choices to make"));
    check(dialog, "Pact Boon", {"Pact of the Blade"});
    check(dialog, "Eldritch Invocations", {"Armor of Shadows"});
    button(dialog, "Choose")->click();
    settle();
    REQUIRE(visibleText(dialog).contains("Choose 2 for Eldritch Invocations."));  // one short
    check(dialog, "Eldritch Invocations", {"Devil's Sight"});
    button(dialog, "Choose")->click();
    settle();
    REQUIRE(dialog.result() == QDialog::Accepted);
    auto saved = world.get(warlock.character_id);
    REQUIRE(dnd::has_pact_blade(saved));
    REQUIRE(dnd::has_armor_of_shadows(saved));
}

TEST_CASE("combat: a druid wild shapes into a black bear", "[ui][wild_shape][combat]") {
    World world;
    auto druid = world.add("Fern", "Druid", 4);
    druid.max_hp = 200;
    druid.current_hp = 200;
    world.repo.update_character(druid);
    AdventureFight fight;
    fight.title = "Test";
    fight.scene.title = "Woods";
    fight.scene.narration = "Wolves.";
    fight.scene.monsters = {{"Zombie", 1}};
    CombatEncounterDialog combat(world.repo, {world.get(druid.character_id)}, world.repo.get_monsters(), nullptr,
                                 &fight);
    combat.show();
    button(combat, "Roll Initiative")->click();
    settle();
    REQUIRE(choose(combat, "Black Bear (CR 1/2, 19 HP, AC 11)"));
    button(combat, "Transform")->click();
    settle();
    REQUIRE(combat.findChild<QPlainTextEdit*>()->toPlainText().contains("Fern uses Wild Shape and becomes a Black Bear"));
    auto* table = combat.findChild<QTableWidget*>();
    bool shown = false;
    for (int row = 0; row < table->rowCount(); ++row) shown = shown || table->item(row, 0)->text() == "Fern (Black Bear)";
    REQUIRE(shown);
    REQUIRE(button(combat, "Transform") == nullptr);  // the action is spent
}
