// Combat Encounter: class feature controls, the difficulty line, the
// adaptive layout, and finishing an adventure fight.

#include <catch2/catch_test_macros.hpp>

#include <QCheckBox>
#include <QGroupBox>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSplitter>

#include "CombatEncounterDialog.hpp"
#include "ui_helpers.hpp"

using namespace ui_test;

namespace {

// A tough character so the fight waits on their turn: initiative runs the
// monsters' turns until it's the character's.
dnd::Character tough(World& world, const std::string& name, const std::string& class_name, int level,
                     std::vector<std::string> gear) {
    auto c = world.add(name, class_name, level, std::move(gear));
    c.max_hp = 200;
    c.current_hp = 200;
    world.repo.update_character(c);
    return c;
}

QString log(CombatEncounterDialog& combat) {
    return combat.findChild<QPlainTextEdit*>()->toPlainText();
}

QString actions(CombatEncounterDialog& combat) {
    QComboBox* action = comboWith(combat, "Cast a Spell");
    QStringList names;
    for (int i = 0; i < action->count(); ++i) names << action->itemText(i);
    return names.join(",");
}

}  // namespace

TEST_CASE("combat: Reckless Attack is offered before a Barbarian's first attack only", "[ui][combat]") {
    World world;
    auto grog = tough(world, "Grog", "Barbarian", 2, {"Greataxe"});
    std::vector<dnd::Monster> goblins;
    for (const auto& m : world.repo.get_monsters()) {
        if (m.name == "Goblin") goblins.push_back(m);
    }
    AdventureFight fight;
    fight.title = "Test";
    fight.scene.title = "Goblins";
    fight.scene.narration = "Goblins.";
    fight.scene.monsters = {{"Goblin", 3}};
    CombatEncounterDialog combat(world.repo, {grog}, world.repo.get_monsters(), nullptr, &fight);
    combat.show();
    button(combat, "Roll Initiative")->click();
    settle();

    QCheckBox* reckless = nullptr;
    for (auto* box : combat.findChildren<QCheckBox*>()) {
        if (box->text() == "Attack recklessly") reckless = box;
    }
    REQUIRE(reckless != nullptr);
    REQUIRE(reckless->isVisible());
    REQUIRE_FALSE(actions(combat).contains("Lay on Hands"));  // not a Paladin
    reckless->setChecked(true);
    button(combat, "Roll Attack")->click();
    settle();
    REQUIRE(log(combat).contains("Grog attacks recklessly"));
    REQUIRE_FALSE(reckless->isVisible());  // only on the first attack of the turn
}

TEST_CASE("combat: a Paladin gets Divine Smite and Lay on Hands", "[ui][combat]") {
    World world;
    auto sera = tough(world, "Sera", "Paladin", 2, {"Chain Mail", "Longsword"});
    AdventureFight fight;
    fight.title = "Test";
    fight.scene.title = "Orcs";
    fight.scene.narration = "Orcs.";
    fight.scene.monsters = {{"Orc", 2}};
    CombatEncounterDialog combat(world.repo, {sera}, world.repo.get_monsters(), nullptr, &fight);
    combat.show();
    button(combat, "Roll Initiative")->click();
    settle();

    REQUIRE(actions(combat).contains("Lay on Hands"));
    QComboBox* smite = comboWith(combat, "No smite");
    REQUIRE(smite != nullptr);
    REQUIRE(smite->isVisible());
    REQUIRE(choose(combat, "1st-level slot"));
    button(combat, "Roll Attack")->click();
    settle();
    REQUIRE(smite->currentText() == "No smite");  // back to no smite after every attack
}

TEST_CASE("combat: the difficulty line follows the monsters added", "[ui][combat]") {
    World world;
    auto aria = world.add("Aria", "Fighter", 1);
    CombatEncounterDialog combat(world.repo, {aria}, world.repo.get_monsters());
    combat.show();
    settle();
    REQUIRE(visibleText(combat).contains("Add monsters to fight."));
    REQUIRE(choose(combat, "Goblin"));
    button(combat, "Add Monster")->click();
    settle();
    REQUIRE(visibleText(combat).contains("Difficulty: Hard: 75 adjusted XP"));  // 50 x1.5 for a lone character
}

TEST_CASE("combat: side by side when wide, stacked when narrow", "[ui][combat]") {
    World world;
    CombatEncounterDialog combat(world.repo, {world.add("Aria", "Fighter", 1)}, world.repo.get_monsters());
    combat.show();
    QSplitter* main = nullptr;
    for (auto* splitter : combat.findChildren<QSplitter*>()) {
        if (splitter->count() == 2 && splitter->widget(1)->inherits("QScrollArea")) main = splitter;
    }
    REQUIRE(main != nullptr);
    combat.resize(1100, 700);
    settle();
    REQUIRE(main->orientation() == Qt::Horizontal);
    combat.resize(700, 700);
    settle();
    REQUIRE(main->orientation() == Qt::Vertical);
}

TEST_CASE("combat: an adventure fight ends with Continue or End Adventure", "[ui][combat][adventure]") {
    World world;
    auto aria = tough(world, "Aria", "Fighter", 5, {"Chain Mail", "Longsword"});
    AdventureFight fight;
    fight.title = "The Goblin Warren — scene 1 of 3: The Lookouts";
    fight.scene.title = "The Lookouts";
    fight.scene.narration = "Two goblins keep a lazy watch.";
    fight.scene.monsters = {{"Goblin", 2}};
    fight.scene.coins_cp = 300;
    CombatEncounterDialog combat(world.repo, {aria}, world.repo.get_monsters(), nullptr, &fight);
    combat.show();
    REQUIRE(combat.windowTitle() == fight.title);
    REQUIRE(visibleText(combat).contains("Two goblins keep a lazy watch."));
    REQUIRE(button(combat, "Continue") == nullptr);  // not until the fight is over

    button(combat, "Roll Initiative")->click();
    for (int i = 0; i < 200 && combat.outcome() == dnd::EncounterStatus::InProgress; ++i) {
        if (auto* attack = button(combat, "Roll Attack"); attack && attack->isEnabled()) attack->click();
        if (auto* next = button(combat, "Next Turn")) next->click();
        settle();
    }
    REQUIRE(combat.outcome() == dnd::EncounterStatus::Victory);  // 200 HP against two goblins
    REQUIRE(button(combat, "Continue") != nullptr);
    REQUIRE(log(combat).contains("The party finds 3 gp."));
    REQUIRE(world.get(aria.character_id).coins[3] == 3);
}

TEST_CASE("combat: dividing the loot between two survivors", "[ui][combat][loot]") {
    World world;
    auto clark = tough(world, "Clark", "Fighter", 5, {"Chain Mail", "Longsword"});
    auto phi = tough(world, "phi", "Fighter", 5, {"Chain Mail", "Longsword"});
    AdventureFight fight;
    fight.title = "Ambush on the Trail";
    fight.scene.title = "Ambush on the Trail";
    fight.scene.narration = "Goblins burst from the underbrush.";
    fight.scene.monsters = {{"Goblin", 1}};
    fight.scene.loot = {{"Potion of Healing", 1}, {"Rations (1 day)", 2}};
    CombatEncounterDialog combat(world.repo, {clark, phi}, world.repo.get_monsters(), nullptr, &fight);
    combat.show();

    QGroupBox* loot = nullptr;
    for (auto* group : combat.findChildren<QGroupBox*>()) {
        if (group->title() == "Divide the Loot") loot = group;
    }
    REQUIRE(loot != nullptr);
    REQUIRE_FALSE(loot->isVisible());

    button(combat, "Roll Initiative")->click();
    for (int i = 0; i < 200 && combat.outcome() == dnd::EncounterStatus::InProgress; ++i) {
        if (auto* attack = button(combat, "Roll Attack"); attack && attack->isEnabled()) attack->click();
        if (auto* next = button(combat, "Next Turn")) next->click();
        settle();
    }
    REQUIRE(combat.outcome() == dnd::EncounterStatus::Victory);
    REQUIRE(log(combat).contains("Loot found: 1 × Potion of Healing, 2 × Rations (1 day)."));
    REQUIRE(loot->isVisible());

    // One row per item: the potion and each ration. Everything to phi.
    auto combos = loot->findChildren<QComboBox*>();
    REQUIRE(combos.size() == 3);
    for (auto* combo : combos) {
        REQUIRE(combo->count() == 2);
        combo->setCurrentIndex(combo->findText("phi", Qt::MatchStartsWith));
    }
    button(combat, "Hand Out Loot")->click();
    settle();
    REQUIRE_FALSE(loot->isVisible());
    REQUIRE(log(combat).contains("Loot! phi takes: 1 × Potion of Healing, 2 × Rations (1 day)."));

    auto count = [&](int id, const std::string& name) {
        for (const auto& entry : world.repo.get_inventory(id)) {
            if (entry.item.name == name) return entry.quantity;
        }
        return 0;
    };
    REQUIRE(count(phi.character_id, "Rations (1 day)") == 2);
    REQUIRE(count(clark.character_id, "Potion of Healing") == 0);
}

TEST_CASE("combat: closing the window hands out undivided loot", "[ui][combat][loot]") {
    World world;
    auto clark = tough(world, "Clark", "Fighter", 5, {"Chain Mail", "Longsword"});
    auto phi = tough(world, "phi", "Fighter", 5, {"Chain Mail", "Longsword"});
    AdventureFight fight;
    fight.title = "Test";
    fight.scene.title = "Test";
    fight.scene.narration = "Goblins.";
    fight.scene.monsters = {{"Goblin", 1}};
    fight.scene.loot = {{"Torch", 2}};
    CombatEncounterDialog combat(world.repo, {clark, phi}, world.repo.get_monsters(), nullptr, &fight);
    combat.show();
    button(combat, "Roll Initiative")->click();
    for (int i = 0; i < 200 && combat.outcome() == dnd::EncounterStatus::InProgress; ++i) {
        if (auto* attack = button(combat, "Roll Attack"); attack && attack->isEnabled()) attack->click();
        if (auto* next = button(combat, "Next Turn")) next->click();
        settle();
    }
    REQUIRE(combat.outcome() == dnd::EncounterStatus::Victory);
    button(combat, "Continue")->click();
    settle();
    int torches = 0;
    for (int id : {clark.character_id, phi.character_id}) {
        for (const auto& entry : world.repo.get_inventory(id)) {
            if (entry.item.name == "Torch") torches += entry.quantity;
        }
    }
    REQUIRE(torches == 2);  // none lost
}

TEST_CASE("combat: a level-5 wizard casts Fireball at several targets", "[ui][combat][spells23]") {
    World world;
    auto zanna = tough(world, "Zanna", "Wizard", 5, {});
    for (const auto& spell : world.repo.get_spells()) {
        if (spell.name == "Fireball") world.repo.add_character_spell(zanna.character_id, spell.spell_id);
    }
    AdventureFight fight;
    fight.title = "Test";
    fight.scene.title = "Orcs";
    fight.scene.narration = "Orcs.";
    fight.scene.monsters = {{"Orc", 3}};
    CombatEncounterDialog combat(world.repo, {zanna}, world.repo.get_monsters(), nullptr, &fight);
    combat.show();
    button(combat, "Roll Initiative")->click();
    settle();

    REQUIRE(choose(combat, "Cast a Spell"));
    REQUIRE(choose(combat, "Fireball (3rd)"));
    REQUIRE(visibleText(combat).contains("Choose up to 6 targets."));
    QListWidget* targets = nullptr;
    for (auto* list : combat.findChildren<QListWidget*>()) {
        if (list->isVisible() && list->count() == 3) targets = list;
    }
    REQUIRE(targets != nullptr);
    for (int row = 0; row < targets->count(); ++row) targets->item(row)->setCheckState(Qt::Checked);
    button(combat, "Cast Spell")->click();
    settle();
    QString text = log(combat);
    REQUIRE(text.contains("Zanna casts Fireball (3rd-level slot)."));
    REQUIRE(text.count("DEX save") == 3);
}

TEST_CASE("combat: the spell hint describes buffs and lasting spells", "[ui][combat]") {
    World world;
    auto zanna = tough(world, "Zanna", "Cleric", 3, {});
    for (const auto& spell : world.repo.get_spells()) {
        if (spell.name == "Bless" || spell.name == "Aid") {
            world.repo.add_character_spell(zanna.character_id, spell.spell_id);
        }
    }
    AdventureFight fight;
    fight.title = "Test";
    fight.scene.title = "Orcs";
    fight.scene.narration = "Orcs.";
    fight.scene.monsters = {{"Orc", 1}};
    CombatEncounterDialog combat(world.repo, {zanna}, world.repo.get_monsters(), nullptr, &fight);
    combat.show();
    button(combat, "Roll Initiative")->click();
    settle();

    REQUIRE(choose(combat, "Cast a Spell"));
    REQUIRE(choose(combat, "Bless (1st)"));
    REQUIRE(visibleText(combat).contains("Buff: Blessed (up to 3 allies)."));
    REQUIRE_FALSE(visibleText(combat).contains("returns with 1 HP"));
    REQUIRE(choose(combat, "Aid (2nd)"));
    REQUIRE(visibleText(combat).contains("Until a long rest: +5 max and current HP"));
    REQUIRE(visibleText(combat).contains("It lasts past this fight."));
}
