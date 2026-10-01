#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string>

#include "dnd/character_io.hpp"
#include "dnd/repository.hpp"

using dnd::ConflictChoice;

namespace {

int lookup(const std::vector<dnd::LookupEntry>& entries, const std::string& name) {
    for (const auto& e : entries) {
        if (e.name == name) return e.id;
    }
    FAIL("no " << name);
    return 0;
}

int item_id(const dnd::Repository& repo, const std::string& name) {
    for (const auto& i : repo.get_items()) {
        if (i.name == name) return i.item_id;
    }
    FAIL("no item " << name);
    return 0;
}

int spell_id(const dnd::Repository& repo, const std::string& name) {
    for (const auto& s : repo.get_spells()) {
        if (s.name == name) return s.spell_id;
    }
    FAIL("no spell " << name);
    return 0;
}

// Adam's party: Aria (a well-worn level-5 Fighter) and Zanna (a Wizard with spells).
int seed_party(dnd::Repository& repo) {
    int player = repo.add_player("Adam");

    dnd::Character aria;
    aria.character_name = "Aria";
    aria.player_id = player;
    aria.class_id = lookup(repo.get_classes(), "Fighter");
    aria.race = lookup(repo.get_races(), "Human");
    aria.alignment = lookup(repo.get_alignments(), "Lawful Good");
    aria.background = "Soldier";
    aria.level = 5;
    aria.experience = 6600;
    aria.strength = 17;
    aria.dexterity = 13;
    aria.constitution = 15;
    aria.intelligence = 9;
    aria.wisdom = 13;
    aria.charisma = 11;
    aria.max_hp = 44;
    aria.current_hp = 30;
    aria.hit_dice_used = 2;
    aria.save_proficiencies = (1u << 0) | (1u << 2);  // STR, CON
    aria.skill_proficiencies = (1u << static_cast<int>(dnd::Skill::Athletics)) |
                               (1u << static_cast<int>(dnd::Skill::Perception));
    aria.fighting_style = "Defense";
    aria.feature_uses_spent = {{"SecondWind", 1}};
    aria.coins = {7, 3, 0, 42, 1};
    int aria_id = repo.add_character(aria);
    repo.add_to_inventory(aria_id, item_id(repo, "Chain Mail"));
    repo.set_equipped(aria_id, item_id(repo, "Chain Mail"), true);
    repo.add_to_inventory(aria_id, item_id(repo, "Longsword"));
    repo.set_equipped(aria_id, item_id(repo, "Longsword"), true);
    repo.add_to_inventory(aria_id, item_id(repo, "Potion of Healing"), 3);

    dnd::Character zanna;
    zanna.character_name = "Zanna";
    zanna.player_id = player;
    zanna.class_id = lookup(repo.get_classes(), "Wizard");
    zanna.race = lookup(repo.get_races(), "Elf");
    zanna.alignment = lookup(repo.get_alignments(), "Neutral Good");
    zanna.level = 3;
    zanna.intelligence = 16;
    zanna.max_hp = 17;
    zanna.current_hp = 0;
    zanna.dead = true;
    zanna.spell_slots_used = {2, 1, 0, 0, 0, 0, 0, 0, 0};
    int zanna_id = repo.add_character(zanna);
    repo.add_character_spell(zanna_id, spell_id(repo, "Fire Bolt"));
    repo.add_character_spell(zanna_id, spell_id(repo, "Magic Missile"));
    return player;
}

const dnd::Character* named(const std::vector<dnd::Character>& characters, const std::string& name) {
    for (const auto& c : characters) {
        if (c.character_name == name) return &c;
    }
    return nullptr;
}

dnd::CharacterExport roundtrip(const dnd::CharacterExport& file) {
    auto parsed = dnd::parse_character_file(dnd::to_json(file));
    REQUIRE(parsed.error.empty());
    REQUIRE(parsed.file.has_value());
    return *parsed.file;
}

}  // namespace

TEST_CASE("a player's characters round-trip into another database", "[character_io]") {
    dnd::Repository source(":memory:");
    int player = seed_party(source);
    std::string text = dnd::to_json(dnd::export_player(source, player));

    // Names, never database IDs.
    REQUIRE(text.find("\"format\": \"dnd5e-characters\"") != std::string::npos);
    REQUIRE(text.find("\"class\": \"Fighter\"") != std::string::npos);
    REQUIRE(text.find("\"item\": \"Longsword\"") != std::string::npos);
    REQUIRE(text.find("\"skills\": [") != std::string::npos);

    dnd::Repository target(":memory:");
    auto parsed = dnd::parse_character_file(text);
    REQUIRE(parsed.file);
    auto plan = dnd::plan_import(target, *parsed.file);
    REQUIRE(plan.player_name == "Adam");
    REQUIRE(plan.existing_player_id == 0);
    REQUIRE(plan.characters.size() == 2);
    for (const auto& c : plan.characters) {
        REQUIRE(c.importable);
        REQUIRE(c.warnings.empty());
        REQUIRE(c.existing_character_id == 0);
    }
    auto result = dnd::apply_import(target, plan, {});
    REQUIRE(result.imported_character_ids.size() == 2);

    auto players = target.get_players();
    REQUIRE(players.size() == 1);
    REQUIRE(players[0].player_name == "Adam");
    auto imported = target.get_characters();
    const auto* aria = named(imported, "Aria");
    auto originals = source.get_characters();  // keep the vector alive: named() points into it
    const auto* before = named(originals, "Aria");
    REQUIRE(aria);
    REQUIRE(aria->player_id == players[0].player_id);
    REQUIRE(aria->class_id == lookup(target.get_classes(), "Fighter"));
    REQUIRE(aria->level == 5);
    REQUIRE(aria->experience == 6600);
    REQUIRE(aria->strength == 17);
    REQUIRE(aria->current_hp == 30);
    REQUIRE(aria->max_hp == 44);
    REQUIRE(aria->hit_dice_used == 2);
    REQUIRE(aria->save_proficiencies == before->save_proficiencies);
    REQUIRE(aria->skill_proficiencies == before->skill_proficiencies);
    REQUIRE(aria->background == "Soldier");
    REQUIRE(aria->fighting_style == "Defense");
    REQUIRE(aria->feature_uses_spent == std::map<std::string, int>{{"SecondWind", 1}});
    REQUIRE(aria->coins == std::array<int, 5>{7, 3, 0, 42, 1});

    auto inventory = target.get_inventory(aria->character_id);
    REQUIRE(inventory.size() == 3);
    for (const auto& entry : inventory) {
        if (entry.item.name == "Potion of Healing") REQUIRE(entry.quantity == 3);
        if (entry.item.name == "Longsword") REQUIRE(entry.equipped);
        if (entry.item.name == "Chain Mail") REQUIRE(entry.equipped);
    }

    const auto* zanna = named(imported, "Zanna");
    REQUIRE(zanna);
    REQUIRE(zanna->dead);
    REQUIRE(zanna->spell_slots_used[0] == 2);
    REQUIRE(zanna->spell_slots_used[1] == 1);
    REQUIRE(target.get_character_spells(zanna->character_id).size() == 2);
}

TEST_CASE("files from before coins existed still import, with no coins", "[character_io][currency]") {
    auto parsed = dnd::parse_character_file(R"({"format": "dnd5e-characters", "version": 1, "player": {"name": "Old"},
        "characters": [{"name": "Vex", "class": "Rogue", "race": "Halfling", "alignment": "Chaotic Good"}]})");
    REQUIRE(parsed.file);
    REQUIRE(parsed.file->characters[0].coins == std::array<int, 5>{});
    dnd::Repository repo(":memory:");
    auto result = dnd::apply_import(repo, dnd::plan_import(repo, *parsed.file), {});
    REQUIRE(result.imported_character_ids.size() == 1);
}

TEST_CASE("exporting one character", "[character_io]") {
    dnd::Repository repo(":memory:");
    seed_party(repo);
    int zanna = named(repo.get_characters(), "Zanna")->character_id;
    auto file = roundtrip(dnd::export_character(repo, zanna));
    REQUIRE(file.player_name == "Adam");
    REQUIRE(file.characters.size() == 1);
    REQUIRE(file.characters[0].name == "Zanna");
    REQUIRE(file.characters[0].spells == std::vector<std::string>{"Fire Bolt", "Magic Missile"});
    REQUIRE_THROWS_AS(dnd::export_character(repo, 999), std::invalid_argument);
}

TEST_CASE("name conflicts: copy, replace, or skip", "[character_io]") {
    dnd::Repository repo(":memory:");
    int player = seed_party(repo);
    auto file = roundtrip(dnd::export_player(repo, player));
    auto plan = dnd::plan_import(repo, file);
    REQUIRE(plan.existing_player_id == player);
    REQUIRE(plan.characters[0].existing_character_id != 0);
    REQUIRE(plan.characters[0].copy_name == "Aria (2)");
    REQUIRE(plan.characters[1].copy_name == "Zanna (2)");

    SECTION("as copies, by default") {
        auto result = dnd::apply_import(repo, plan, {});
        REQUIRE(result.player_id == player);
        REQUIRE(named(repo.get_characters(), "Aria (2)"));
        REQUIRE(named(repo.get_characters(), "Zanna (2)"));
        REQUIRE(repo.get_characters().size() == 4);
        // Importing the same file again picks the next free name.
        REQUIRE(dnd::plan_import(repo, file).characters[0].copy_name == "Aria (3)");
    }

    SECTION("replace one, skip the other") {
        int old_aria = plan.characters[0].existing_character_id;
        dnd::ImportChoices choices;
        choices.conflicts = {ConflictChoice::Replace, ConflictChoice::Skip};
        auto result = dnd::apply_import(repo, plan, choices);
        REQUIRE(result.imported_character_ids.size() == 1);
        REQUIRE(repo.get_characters().size() == 2);
        REQUIRE(named(repo.get_characters(), "Aria")->character_id != old_aria);
        REQUIRE(result.messages[1] == "Zanna: skipped (a character with that name already exists).");
    }

    SECTION("into another player instead") {
        int other = repo.add_player("Beth");
        dnd::ImportChoices choices;
        choices.target_player_id = other;
        auto result = dnd::apply_import(repo, plan, choices);
        REQUIRE(result.player_id == other);
        REQUIRE(named(repo.get_characters(), "Aria (2)")->player_id == other);
    }
}

TEST_CASE("unknown names: warnings, or the character is refused", "[character_io]") {
    dnd::Repository repo(":memory:");
    auto file = roundtrip(dnd::export_player(repo, seed_party(repo)));
    file.characters[0].inventory.push_back({"Vorpal Sword", 1, false});
    file.characters[0].spells.push_back("Wish");
    file.characters[1].class_name = "Artificer";

    dnd::Repository target(":memory:");
    auto plan = dnd::plan_import(target, file);
    REQUIRE(plan.characters[0].importable);
    REQUIRE(plan.characters[0].warnings == std::vector<std::string>{"Unknown item \"Vorpal Sword\" won't be imported.",
                                                                    "Unknown spell \"Wish\" won't be imported."});
    REQUIRE_FALSE(plan.characters[1].importable);
    REQUIRE(plan.characters[1].error == "Unknown class \"Artificer\".");

    auto result = dnd::apply_import(target, plan, {});
    REQUIRE(result.imported_character_ids.size() == 1);
    REQUIRE(target.get_inventory(result.imported_character_ids[0]).size() == 3);  // no Vorpal Sword
    REQUIRE(result.messages[1] == "Zanna: not imported. Unknown class \"Artificer\".");
}

TEST_CASE("bad files are refused with a clear message", "[character_io]") {
    REQUIRE(dnd::parse_character_file("not json").error.rfind("This file isn't valid JSON", 0) == 0);
    REQUIRE(dnd::parse_character_file("{\"hello\": 1}").error == "This isn't a DnD Engine character file.");
    auto newer = dnd::parse_character_file(R"({"format": "dnd5e-characters", "version": 99, "characters": []})");
    REQUIRE(newer.error.find("newer version of the app (format version 99") != std::string::npos);
    auto broken = dnd::parse_character_file(R"({"format": "dnd5e-characters", "version": 1,
                                                "characters": [{"class": "Fighter", "race": "Human"}]})");
    REQUIRE(broken.error.rfind("This character file is damaged or incomplete", 0) == 0);
    auto wrong_type = dnd::parse_character_file(R"({"format": "dnd5e-characters", "version": 1,
        "characters": [{"name": "Aria", "class": "Fighter", "race": "Human", "level": "five"}]})");
    REQUIRE_FALSE(wrong_type.error.empty());
}

TEST_CASE("an import that fails changes nothing", "[character_io]") {
    dnd::Repository source(":memory:");
    auto file = roundtrip(dnd::export_player(source, seed_party(source)));
    dnd::Repository target(":memory:");
    auto plan = dnd::plan_import(target, file);
    // Someone creates "Zanna" after the plan was made: the import hits the UNIQUE name and rolls back.
    int squatter = target.add_player("Squatter");
    dnd::Character zanna;
    zanna.character_name = "Zanna";
    zanna.player_id = squatter;
    zanna.class_id = 1;
    zanna.race = 1;
    zanna.alignment = 1;
    target.add_character(zanna);

    REQUIRE_THROWS(dnd::apply_import(target, plan, {}));
    REQUIRE(target.get_characters().size() == 1);  // no Aria either
    REQUIRE(target.get_players().size() == 1);      // and no "Adam"
}

TEST_CASE("backing up the whole database", "[character_io][repository]") {
    auto path = std::filesystem::temp_directory_path() / "dnd5e_backup_test.db";
    std::filesystem::remove(path);
    {
        dnd::Repository repo(":memory:");
        seed_party(repo);
        repo.backup_to(path.string());
    }
    {
        dnd::Repository copy(path.string());
        REQUIRE(copy.get_characters().size() == 2);
        REQUIRE(copy.get_players()[0].player_name == "Adam");
    }
    std::filesystem::remove(path);
}
