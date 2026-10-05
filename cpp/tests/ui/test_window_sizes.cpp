// Every window fits a small screen: its minimum size (what the layout won't
// shrink below) fits 800x600, and it opens within the screen. Edit Character
// once couldn't shrink below ~900 px tall; this is the guard.

#include <catch2/catch_test_macros.hpp>

#include <QGuiApplication>

#include <algorithm>
#include <QScreen>
#include <QWizard>

#include "AdventureDialogs.hpp"
#include "AdventurePickerDialog.hpp"
#include "CampaignDialog.hpp"
#include "CharacterCreationWizard.hpp"
#include "ClassOptionsDialog.hpp"
#include "CharacterEditorDialog.hpp"
#include "CharacterSheetDialog.hpp"
#include "CombatEncounterDialog.hpp"
#include "ImportDialog.hpp"
#include "InitiativeTrackerDialog.hpp"
#include "InventoryDialog.hpp"
#include "LevelUpDialog.hpp"
#include "MainWindow.hpp"
#include "MonsterListDialog.hpp"
#include "ShopDialog.hpp"
#include "SpellbookDialog.hpp"
#include "SubclassDialog.hpp"
#include "dnd/character_io.hpp"
#include "dnd/encounter_difficulty.hpp"
#include "dnd/progression.hpp"
#include "ui_helpers.hpp"

using namespace ui_test;

namespace {

constexpr int kSmallWidth = 800;
constexpr int kSmallHeight = 600;

void requireFits(QWidget& window, const char* name) {
    window.show();
    settle();
    QSize minimum = window.minimumSizeHint().expandedTo(window.minimumSize());
    INFO(name << ": minimum " << minimum.width() << "x" << minimum.height() << ", opened at " << window.width()
              << "x" << window.height());
    CHECK(minimum.width() <= kSmallWidth);
    CHECK(minimum.height() <= kSmallHeight);
    QSize screen = QGuiApplication::primaryScreen()->availableGeometry().size();
    CHECK(window.width() <= screen.width());
    CHECK(window.height() <= screen.height());
}

}  // namespace

TEST_CASE("every window fits a small screen", "[ui][sizes]") {
    World world;
    auto& repo = world.repo;
    auto sera = world.add("Sera", "Paladin", 5, {"Chain Mail", "Longsword", "Shield"});
    auto zanna = world.add("Zanna", "Wizard", 5);
    for (const auto& spell : repo.get_spells()) {
        if (spell.level <= 3 && std::find(spell.class_names.begin(), spell.class_names.end(), "Wizard") !=
                                    spell.class_names.end()) {
            repo.add_character_spell(zanna.character_id, spell.spell_id);
        }
    }
    auto classes = repo.get_classes();

    SECTION("main window") {
        MainWindow window(repo);
        requireFits(window, "MainWindow");
    }
    SECTION("main window with double-size fonts (Windows' are wider than Linux's)") {
        // On Windows the button rows once forced the window 924 px wide. They wrap now.
        QFont original = QApplication::font();
        QFont bigger = original;
        if (original.pointSizeF() > 0) {
            bigger.setPointSizeF(original.pointSizeF() * 2);
        } else {
            bigger.setPixelSize(original.pixelSize() * 2);  // some platforms size fonts in pixels
        }
        QApplication::setFont(bigger);
        {
            MainWindow window(repo);
            INFO("font " << QApplication::font().pointSizeF() << "pt / " << QApplication::font().pixelSize() << "px, was "
                         << original.pointSizeF() << "pt / " << original.pixelSize() << "px");
            requireFits(window, "MainWindow (2x font)");
        }
        QApplication::setFont(original);
    }
    SECTION("character sheet (a caster with a long spell list)") {
        CharacterSheetDialog sheet(zanna, classes, repo.get_races(), repo.get_alignments(),
                                   repo.get_inventory(zanna.character_id), repo.get_character_spells(zanna.character_id));
        requireFits(sheet, "CharacterSheetDialog");
    }
    SECTION("edit character") {
        CharacterEditorDialog editor(classes, repo.get_races(), repo.get_alignments(), &sera);
        requireFits(editor, "CharacterEditorDialog");
    }
    SECTION("creation wizard") {
        CharacterCreationWizard wizard(repo, world.player);
        requireFits(wizard, "CharacterCreationWizard");
    }
    SECTION("inventory") {
        InventoryDialog inventory(repo, sera, classes, repo.get_races());
        requireFits(inventory, "InventoryDialog");
    }
    SECTION("class options") {
        ClassOptionsDialog options(repo, world.add("Hex", "Warlock", 5), classes);
        requireFits(options, "ClassOptionsDialog");
    }
    SECTION("subclass chooser") {
        SubclassDialog subclass(repo, world.add("Fern", "Druid", 2), classes);
        requireFits(subclass, "SubclassDialog");
    }
    SECTION("campaigns") {
        CampaignDialog campaigns(repo);
        requireFits(campaigns, "CampaignDialog");
    }
    SECTION("shop and give coins") {
        ShopDialog shop(repo, sera, classes, repo.get_races());
        requireFits(shop, "ShopDialog");
        GiveCoinsDialog give(repo, sera);
        requireFits(give, "GiveCoinsDialog");
        GiveItemDialog giveItem(repo, sera, repo.get_inventory(sera.character_id).front());
        requireFits(giveItem, "GiveItemDialog");
    }
    SECTION("spellbook") {
        SpellbookDialog spellbook(repo, zanna, classes);
        requireFits(spellbook, "SpellbookDialog");
    }
    SECTION("level up") {
        zanna.experience = dnd::xp_threshold_for_level(6);
        LevelUpDialog levelUp(repo, zanna, classes);
        requireFits(levelUp, "LevelUpDialog");
    }
    SECTION("combat encounter (an adventure fight)") {
        AdventureFight fight;
        fight.title = "Test";
        fight.scene.title = "The Ambush";
        fight.scene.narration = "A long narration that has to wrap across the width of the window when it is narrow.";
        fight.scene.monsters = {{"Goblin", 6}, {"Orc", 3}};
        CombatEncounterDialog combat(repo, repo.get_characters(), repo.get_monsters(), nullptr, &fight);
        requireFits(combat, "CombatEncounterDialog");
    }
    SECTION("adventure picker, party, and between scenes") {
        dnd::AdventureRun run;
        run.title = "The Goblin Warren";
        run.next_scene = 2;
        run.party = {sera.character_id, zanna.character_id};
        run.run_id = repo.add_adventure_run(run);
        AdventurePickerDialog picker({5, 5}, repo.get_monsters(), repo.get_adventure_runs(true));
        requireFits(picker, "AdventurePickerDialog");
        PartyDialog party(repo.get_characters(), repo.get_players(), classes, world.player, "The Goblin Warren");
        requireFits(party, "PartyDialog");
        BetweenScenesDialog between(repo, run, *dnd::find_adventure(run.title), classes);
        requireFits(between, "BetweenScenesDialog");
    }
    SECTION("short rest") {
        ShortRestDialog rest(repo, repo.get_characters(), classes);
        requireFits(rest, "ShortRestDialog");
    }
    SECTION("import preview") {
        ImportDialog import(repo, dnd::export_player(repo, world.player));
        requireFits(import, "ImportDialog");
    }
    SECTION("monsters and initiative") {
        MonsterListDialog monsters(repo.get_monsters());
        requireFits(monsters, "MonsterListDialog");
        InitiativeTrackerDialog initiative(repo.get_characters());
        requireFits(initiative, "InitiativeTrackerDialog");
    }
}
