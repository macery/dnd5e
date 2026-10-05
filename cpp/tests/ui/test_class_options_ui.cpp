// Class Options: choosing a warlock's invocations and Pact Boon.

#include <catch2/catch_test_macros.hpp>

#include <QGroupBox>
#include <QListWidget>
#include <QTableView>

#include "ClassOptionsDialog.hpp"
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
