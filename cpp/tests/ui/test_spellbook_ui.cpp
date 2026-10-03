// The Spellbook: a Wizard's prepared spells, and the after-a-long-rest window.

#include <catch2/catch_test_macros.hpp>

#include <QTableWidget>

#include "SpellbookDialog.hpp"
#include "dnd/spellbook.hpp"
#include "dnd/spellcasting.hpp"
#include "ui_helpers.hpp"

using namespace ui_test;

namespace {

int spellId(World& world, const std::string& name) {
    for (const auto& s : world.repo.get_spells()) {
        if (s.name == name) return s.spell_id;
    }
    return 0;
}

bool selectSpell(QWidget& root, const QString& name) {
    auto* table = root.findChild<QTableWidget*>();
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->item(row, 0)->text() == name) {
            table->selectRow(row);
            settle();
            return true;
        }
    }
    return false;
}

}  // namespace

TEST_CASE("spellbook: a Wizard prepares from the book", "[ui][preparing]") {
    World world;
    auto zanna = world.add("Zanna", "Wizard", 1);  // INT 12 (+1): 2 prepared
    const auto& info = *dnd::find_class_info("Wizard");
    for (const char* name : {"Magic Missile", "Burning Hands", "Thunderwave"}) {
        dnd::add_known_spell(world.repo, zanna, info, spellId(world, name));
    }
    SpellbookDialog book(world.repo, world.get(zanna.character_id), world.repo.get_classes());
    book.show();
    settle();
    REQUIRE(visibleText(book).contains("Spellbook: 3 / 6 · Prepared: 2 / 2"));
    REQUIRE(button(book, "Add to Spellbook") != nullptr);

    REQUIRE(selectSpell(book, "Thunderwave"));  // the third: not prepared
    REQUIRE(button(book, "Prepare")->isEnabled());
    button(book, "Prepare")->click();
    settle();
    REQUIRE(visibleText(book).contains("Already 2 spells prepared"));

    REQUIRE(selectSpell(book, "Burning Hands"));
    button(book, "Unprepare")->click();
    settle();
    REQUIRE(selectSpell(book, "Thunderwave"));
    button(book, "Prepare")->click();
    settle();
    REQUIRE(visibleText(book).contains("Prepared Thunderwave."));
}

TEST_CASE("spellbook: a Cleric's list is locked until a long rest", "[ui][preparing]") {
    World world;
    auto cleric = world.add("Sister", "Cleric", 1);
    cleric.may_prepare = false;  // she's been in a fight since her last long rest
    world.repo.update_character(cleric);
    SpellbookDialog book(world.repo, world.get(cleric.character_id), world.repo.get_classes());
    book.show();
    settle();
    REQUIRE(visibleText(book).contains("Prepared spells change only after a long rest"));
    REQUIRE_FALSE(button(book, "Prepare")->isEnabled());  // the Learn button, renamed for Clerics
}
