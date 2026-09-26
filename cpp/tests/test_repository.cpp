#include <catch2/catch_test_macros.hpp>

#include "dnd/repository.hpp"

TEST_CASE("Repository bootstraps its schema on an empty database", "[repository]") {
    dnd::Repository repo(":memory:");

    REQUIRE(repo.get_players().empty());
    REQUIRE(repo.get_characters().empty());
}

TEST_CASE("get_classes returns the seeded 5e classes", "[repository]") {
    dnd::Repository repo(":memory:");

    auto classes = repo.get_classes();
    REQUIRE(classes.size() == 12);

    bool found_fighter = false;
    for (const auto& character_class : classes) {
        if (character_class.class_name == "Fighter") found_fighter = true;
    }
    REQUIRE(found_fighter);
}

TEST_CASE("add_player and get_players round-trip", "[repository]") {
    dnd::Repository repo(":memory:");

    int player_id = repo.add_player("Aria");
    auto players = repo.get_players();

    REQUIRE(players.size() == 1);
    REQUIRE(players[0].player_id == player_id);
    REQUIRE(players[0].player_name == "Aria");
}

TEST_CASE("get_characters_by_player only returns that player's characters", "[repository]") {
    // Regression test: the old MainData.db's CharacterByPlayerID view hardcoded
    // `WHERE players.PlayerID = 1`, so this lookup for any other player silently
    // returned nothing. This exercises exactly that case.
    dnd::Repository repo(":memory:");

    int player_a = repo.add_player("Aria");
    int player_b = repo.add_player("Bram");

    dnd::Character character_a;
    character_a.character_name = "Aria's Fighter";
    character_a.class_id = 1;
    character_a.race = 1;
    character_a.alignment = 1;
    character_a.player_id = player_a;
    character_a.level = 1;
    repo.add_character(character_a);

    dnd::Character character_b;
    character_b.character_name = "Bram's Wizard";
    character_b.class_id = 2;
    character_b.race = 2;
    character_b.alignment = 2;
    character_b.player_id = player_b;
    character_b.level = 1;
    repo.add_character(character_b);

    auto a_characters = repo.get_characters_by_player(player_a);
    REQUIRE(a_characters.size() == 1);
    REQUIRE(a_characters[0].character_name == "Aria's Fighter");

    auto b_characters = repo.get_characters_by_player(player_b);
    REQUIRE(b_characters.size() == 1);
    REQUIRE(b_characters[0].character_name == "Bram's Wizard");

    REQUIRE(repo.get_characters().size() == 2);
}

TEST_CASE("ability scores, HP/AC, and proficiency bitmasks round-trip", "[repository]") {
    dnd::Repository repo(":memory:");
    int player_id = repo.add_player("Aria");

    dnd::Character character;
    character.character_name = "Aria's Ranger";
    character.class_id = 3;
    character.race = 1;
    character.alignment = 1;
    character.player_id = player_id;
    character.level = 5;
    character.strength = 12;
    character.dexterity = 18;
    character.constitution = 14;
    character.intelligence = 10;
    character.wisdom = 16;
    character.charisma = 8;
    character.current_hp = 32;
    character.max_hp = 40;
    character.armor_class = 15;
    character.skill_proficiencies = 0b101u;
    character.save_proficiencies = 0b10u;
    repo.add_character(character);

    auto characters = repo.get_characters();
    REQUIRE(characters.size() == 1);

    const auto& loaded = characters[0];
    REQUIRE(loaded.level == 5);
    REQUIRE(loaded.strength == 12);
    REQUIRE(loaded.dexterity == 18);
    REQUIRE(loaded.constitution == 14);
    REQUIRE(loaded.intelligence == 10);
    REQUIRE(loaded.wisdom == 16);
    REQUIRE(loaded.charisma == 8);
    REQUIRE(loaded.current_hp == 32);
    REQUIRE(loaded.max_hp == 40);
    REQUIRE(loaded.armor_class == 15);
    REQUIRE(loaded.skill_proficiencies == 0b101u);
    REQUIRE(loaded.save_proficiencies == 0b10u);
}

TEST_CASE("adding a character without setting the new fields uses sane defaults", "[repository]") {
    dnd::Repository repo(":memory:");
    int player_id = repo.add_player("Bram");

    dnd::Character character;
    character.character_name = "Bram's Cleric";
    character.class_id = 1;
    character.race = 1;
    character.alignment = 1;
    character.player_id = player_id;
    repo.add_character(character);

    auto characters = repo.get_characters();
    REQUIRE(characters.size() == 1);

    const auto& loaded = characters[0];
    REQUIRE(loaded.level == 1);
    REQUIRE(loaded.strength == 10);
    REQUIRE(loaded.armor_class == 10);
    REQUIRE(loaded.current_hp == 0);
    REQUIRE(loaded.skill_proficiencies == 0);
    REQUIRE(loaded.save_proficiencies == 0);
}
