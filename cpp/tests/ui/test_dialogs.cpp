// Dialog behaviour, from the walkthroughs used while building them.

#include <catch2/catch_test_macros.hpp>

#include <QCheckBox>
#include <QLineEdit>
#include <QListWidget>
#include <QRadioButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QWizard>

#include "AdventureDialogs.hpp"
#include "CharacterCreationWizard.hpp"
#include "CharacterEditorDialog.hpp"
#include "CharacterSheetDialog.hpp"
#include "ImportDialog.hpp"
#include "LevelUpDialog.hpp"
#include "WindowSizing.hpp"
#include "dnd/character_io.hpp"
#include "dnd/level_up.hpp"
#include "dnd/progression.hpp"
#include "ui_helpers.hpp"

using namespace ui_test;

namespace {

// Clicks Next until the wizard stops moving (the last page, or a page that refuses).
void nextUntilStuck(QWizard& wizard) {
    for (int guard = 0; guard < 10; ++guard) {
        int before = wizard.currentId();
        wizard.next();
        settle();
        if (wizard.currentId() == before) return;
    }
}

}  // namespace

TEST_CASE("Level Up: two levels in a row, the first with an ASI", "[ui][level_up]") {
    // Regression: the ASI page once saved a default +2 STR even at levels
    // without an ASI, so the second Level Up failed validation.
    World world;
    auto aria = world.add("Aria", "Fighter", 3);
    aria.experience = dnd::xp_threshold_for_level(5);
    world.repo.update_character(aria);
    auto classes = world.repo.get_classes();

    {
        LevelUpDialog levelUp(world.repo, world.get(aria.character_id), classes);
        levelUp.show();
        nextUntilStuck(levelUp);
        REQUIRE(levelUp.currentPage()->title() == "Summary");
        levelUp.accept();
        REQUIRE(levelUp.result() == QDialog::Accepted);
    }
    auto four = world.get(aria.character_id);
    REQUIRE(four.level == 4);
    REQUIRE(four.strength == 18);   // the ASI page's default: both picks on Strength, +2
    REQUIRE(four.dexterity == 14);

    {
        LevelUpDialog levelUp(world.repo, four, classes);
        levelUp.show();
        nextUntilStuck(levelUp);  // no ASI page at level 5
        levelUp.accept();
        REQUIRE(levelUp.result() == QDialog::Accepted);
    }
    auto five = world.get(aria.character_id);
    REQUIRE(five.level == 5);
    REQUIRE(five.strength == 18);  // no second ASI leaked in
    REQUIRE(dnd::pending_levels(five) == 0);
}

TEST_CASE("Level Up: rolling for HP needs a roll before Next", "[ui][level_up]") {
    World world;
    auto aria = world.add("Aria", "Fighter", 1);
    aria.experience = dnd::xp_threshold_for_level(2);
    LevelUpDialog levelUp(world.repo, aria, world.repo.get_classes());
    levelUp.show();
    settle();
    auto radios = levelUp.currentPage()->findChildren<QRadioButton*>();
    REQUIRE(radios.size() == 2);
    radios[1]->setChecked(true);  // Roll
    settle();
    REQUIRE_FALSE(levelUp.button(QWizard::NextButton)->isEnabled());
    button(*levelUp.currentPage(), "Roll 1d10")->click();
    settle();
    REQUIRE(levelUp.button(QWizard::NextButton)->isEnabled());
    REQUIRE(visibleText(*levelUp.currentPage()).contains("Rolled"));
}

TEST_CASE("creation wizard: a Fighter needs a Fighting Style and starts with a purse", "[ui][creation]") {
    World world;
    CharacterCreationWizard wizard(world.repo, world.player);
    wizard.show();
    settle();
    auto* classes = wizard.currentPage()->findChild<QListWidget*>();
    classes->setCurrentItem(classes->findItems("Fighter", Qt::MatchExactly).front());
    settle();
    REQUIRE(choose(wizard, "Great Weapon Fighting"));

    // Race, abilities (the standard array, as assigned by default), background.
    nextUntilStuck(wizard);
    for (int guard = 0; guard < 8 && wizard.currentPage()->findChild<QLineEdit*>() == nullptr; ++guard) {
        // Pages that need a choice: pick the first option, tick skills, then try again.
        if (auto* list = wizard.currentPage()->findChild<QListWidget*>()) {
            if (!list->currentItem() && list->count() > 0) list->setCurrentRow(0);
        }
        int ticked = 0;
        for (auto* box : wizard.currentPage()->findChildren<QCheckBox*>()) {
            if (box->isEnabled() && !box->isChecked() && ticked < 2 && box->font().bold()) {
                box->setChecked(true);
                ++ticked;
            }
        }
        nextUntilStuck(wizard);
    }
    auto* name = wizard.currentPage()->findChild<QLineEdit*>();
    REQUIRE(name != nullptr);  // the details page
    name->setText("Brakka");
    nextUntilStuck(wizard);
    REQUIRE(wizard.currentPage()->title() == "Review");
    REQUIRE(visibleText(*wizard.currentPage()).contains("Great Weapon Fighting"));
    wizard.accept();

    auto characters = world.repo.get_characters();
    REQUIRE(characters.size() == 1);
    REQUIRE(characters[0].character_name == "Brakka");
    REQUIRE(characters[0].fighting_style == "GreatWeaponFighting");
    REQUIRE(characters[0].coins[3] > 0);  // the background's purse, in gold
}

TEST_CASE("edit character: Level follows XP; Fighting Style only for Fighters", "[ui][editor]") {
    World world;
    auto aria = world.add("Aria", "Fighter", 3);
    CharacterEditorDialog editor(world.repo.get_classes(), world.repo.get_races(), world.repo.get_alignments(), &aria);
    editor.show();
    settle();
    QSpinBox* level = nullptr;
    for (auto* spin : editor.findChildren<QSpinBox*>()) {
        if (spin->value() == 3 && spin->isReadOnly()) level = spin;
    }
    REQUIRE(level != nullptr);  // the Level box is read-only
    QComboBox* style = comboWith(editor, "(none)");
    REQUIRE(style != nullptr);
    REQUIRE(style->isEnabled());
    REQUIRE(choose(editor, "Wizard"));
    REQUIRE_FALSE(style->isEnabled());
}

TEST_CASE("character sheet: two columns when wide, one when narrow", "[ui][sheet]") {
    World world;
    auto sera = world.add("Sera", "Paladin", 5);
    CharacterSheetDialog sheet(sera, world.repo.get_classes(), world.repo.get_races(), world.repo.get_alignments(), {},
                               {});
    sheet.show();
    sheet.resize(1000, 700);
    settle();
    auto* columns = sheet.findChild<ReflowColumns*>();
    REQUIRE(columns != nullptr);
    REQUIRE(columns->isTwoColumns());
    sheet.resize(420, 700);
    settle();
    REQUIRE_FALSE(columns->isTwoColumns());
    REQUIRE(visibleText(sheet).contains("Divine Smite"));
}

TEST_CASE("import preview: a name clash can replace the existing character", "[ui][import]") {
    World source;
    source.add("Aria", "Fighter", 5);
    auto file = dnd::export_player(source.repo, source.player);

    World target;
    int beth = target.repo.add_player("Beth");
    dnd::Character old;
    old.character_name = "Aria";
    old.player_id = beth;
    old.class_id = target.classId("Fighter");
    old.race = 1;
    old.alignment = 1;
    old.level = 1;
    target.repo.add_character(old);

    ImportDialog preview(target.repo, file);
    preview.show();
    settle();
    REQUIRE(visibleText(preview).contains("from <b>Adam</b>"));
    auto* table = preview.findChild<QTableWidget*>();
    auto* conflict = qobject_cast<QComboBox*>(table->cellWidget(0, 2));
    REQUIRE(conflict != nullptr);
    REQUIRE(conflict->currentText().startsWith("Import as a copy"));
    REQUIRE(conflict->itemText(1) == "Replace Beth's Aria");
    conflict->setCurrentIndex(1);
    preview.accept();

    auto characters = target.repo.get_characters();
    REQUIRE(characters.size() == 1);
    REQUIRE(characters[0].level == 5);  // the imported Aria replaced level-1 Aria
}

TEST_CASE("short rest dialog: chosen hit dice are spent and saved", "[ui][rest]") {
    World world;
    auto aria = world.add("Aria", "Fighter", 3);
    aria.current_hp = 5;
    world.repo.update_character(aria);
    ShortRestDialog rest(world.repo, {world.get(aria.character_id)}, world.repo.get_classes());
    rest.show();
    settle();
    auto* spend = rest.findChild<QSpinBox*>();
    REQUIRE(spend->maximum() == 3);
    REQUIRE(spend->value() >= 1);  // enough to heal, on average
    spend->setValue(2);
    button(rest, "Rest")->click();
    settle();
    REQUIRE(rest.rested());
    auto after = world.get(aria.character_id);
    REQUIRE(after.hit_dice_used == 2);
    REQUIRE(after.current_hp > 5);
    REQUIRE(rest.summary().join(" ").contains("spends 2 hit dice"));
}
