// The Spellbook: a Wizard's prepared spells, and the after-a-long-rest window.

#include <catch2/catch_test_macros.hpp>

#include <QListWidget>
#include <QTableWidget>

#include "SpellbookDialog.hpp"
#include "dnd/campaign.hpp"
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

TEST_CASE("spellbook: Aid on the party lasts until a long rest", "[ui][lasting]") {
    World world;
    auto cleric = world.add("Sister", "Cleric", 3);
    auto fighter = world.add("Aria", "Fighter", 3, {"Chain Mail"});
    auto stranger = world.add("Bram", "Fighter", 3);  // not in the campaign
    dnd::Campaign campaign;
    campaign.name = "Ashgrove";
    int campaignId = world.repo.add_campaign(campaign);
    world.repo.set_character_campaign(cleric.character_id, campaignId);
    world.repo.set_character_campaign(fighter.character_id, campaignId);
    dnd::add_known_spell(world.repo, world.get(cleric.character_id), *dnd::find_class_info("Cleric"),
                         spellId(world, "Aid"));

    SpellbookDialog book(world.repo, world.get(cleric.character_id), world.repo.get_classes());
    book.show();
    settle();
    REQUIRE(selectSpell(book, "Aid"));
    auto* targets = book.findChild<QListWidget*>();
    REQUIRE(targets->isVisible());
    REQUIRE(targets->count() == 2);  // Sister and Aria, not Bram
    REQUIRE(targets->item(0)->text() == "Sister (you)");
    REQUIRE(targets->item(0)->checkState() == Qt::Checked);
    targets->item(1)->setCheckState(Qt::Checked);
    button(book, "Cast")->click();
    settle();
    REQUIRE(visibleText(book).contains("Cast Aid with a 2nd-level slot. Sister: Aid: +5 max HP; Aria: Aid: +5 max HP."));
    REQUIRE(world.get(cleric.character_id).max_hp == 35);
    REQUIRE(world.get(fighter.character_id).max_hp == 35);
    REQUIRE(world.get(fighter.character_id).current_hp == 35);
    REQUIRE(world.get(stranger.character_id).max_hp == 30);

    // A stronger Aid could still replace it, but the same slot level is refused, and costs nothing.
    REQUIRE(selectSpell(book, "Aid"));
    REQUIRE(targets->item(1)->text() == "Aria");
    targets->item(0)->setCheckState(Qt::Unchecked);
    targets->item(1)->setCheckState(Qt::Checked);
    button(book, "Cast")->click();
    settle();
    REQUIRE(visibleText(book).contains("Aria already has Aid."));
    REQUIRE(world.get(cleric.character_id).spell_slots_used[1] == 1);
}
