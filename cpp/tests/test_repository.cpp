#include <catch2/catch_test_macros.hpp>

#include "dnd/repository.hpp"

TEST_CASE("Repository bootstraps its schema on an empty database", "[repository]") {
    dnd::Repository repo(":memory:");

    REQUIRE(repo.get_players().empty());
    REQUIRE(repo.get_characters().empty());
}

namespace {
bool contains_name(const std::vector<dnd::LookupEntry>& entries, const std::string& name) {
    for (const auto& entry : entries) {
        if (entry.name == name) return true;
    }
    return false;
}
}  // namespace

TEST_CASE("get_classes returns the seeded 5e classes", "[repository]") {
    dnd::Repository repo(":memory:");

    auto classes = repo.get_classes();
    REQUIRE(classes.size() == 12);
    REQUIRE(contains_name(classes, "Fighter"));
}

TEST_CASE("get_races returns the seeded 5e races", "[repository]") {
    dnd::Repository repo(":memory:");

    auto races = repo.get_races();
    REQUIRE(races.size() == 9);
    REQUIRE(contains_name(races, "Human"));
}

TEST_CASE("get_alignments returns the seeded 5e alignments", "[repository]") {
    dnd::Repository repo(":memory:");

    auto alignments = repo.get_alignments();
    REQUIRE(alignments.size() == 9);
    REQUIRE(contains_name(alignments, "True Neutral"));
}

TEST_CASE("get_monsters returns the seeded sample enemies", "[repository]") {
    dnd::Repository repo(":memory:");

    auto monsters = repo.get_monsters();
    REQUIRE(monsters.size() == 6);

    const dnd::Monster* goblin = nullptr;
    for (const auto& monster : monsters) {
        if (monster.name == "Goblin") goblin = &monster;
    }
    REQUIRE(goblin != nullptr);
    REQUIRE(goblin->armor_class == 15);
    REQUIRE(goblin->hit_points == 7);
    REQUIRE(goblin->challenge_rating == 0.25);
    REQUIRE(goblin->attack_name == "Scimitar");
    REQUIRE(goblin->attack_bonus == 4);
    REQUIRE(goblin->damage_dice_count == 1);
    REQUIRE(goblin->damage_dice_sides == 6);
    REQUIRE(goblin->damage_bonus == 2);
}

TEST_CASE("the Wolf's bite can knock a target prone", "[repository]") {
    dnd::Repository repo(":memory:");
    for (const auto& monster : repo.get_monsters()) {
        if (monster.name == "Wolf") {
            REQUIRE(monster.on_hit_save_dc == 11);
            REQUIRE(monster.on_hit_save_ability == dnd::Ability::Strength);
            REQUIRE(monster.on_hit_condition == dnd::Condition::Prone);
        } else {
            REQUIRE(monster.on_hit_save_dc == 0);
        }
    }
}

TEST_CASE("get_weapons returns the seeded weapon list", "[repository]") {
    dnd::Repository repo(":memory:");

    auto weapons = repo.get_weapons();
    REQUIRE(weapons.size() == 12);

    const dnd::Weapon* rapier = nullptr;
    for (const auto& weapon : weapons) {
        if (weapon.name == "Rapier") rapier = &weapon;
    }
    REQUIRE(rapier != nullptr);
    REQUIRE(rapier->damage_dice_count == 1);
    REQUIRE(rapier->damage_dice_sides == 8);
    REQUIRE(rapier->is_finesse);
    REQUIRE_FALSE(rapier->is_ranged);
}

TEST_CASE("a character's weapon_id round-trips and new characters default to Unarmed Strike", "[repository]") {
    dnd::Repository repo(":memory:");
    int player_id = repo.add_player("Aria");

    auto weapons = repo.get_weapons();
    int unarmed_id = 0;
    int dagger_id = 0;
    for (const auto& weapon : weapons) {
        if (weapon.name == "Unarmed Strike") unarmed_id = weapon.weapon_id;
        if (weapon.name == "Dagger") dagger_id = weapon.weapon_id;
    }
    REQUIRE(unarmed_id != 0);
    REQUIRE(dagger_id != 0);

    dnd::Character character;
    character.character_name = "Aria's Rogue";
    character.class_id = 1;
    character.race = 1;
    character.alignment = 1;
    character.player_id = player_id;
    character.weapon_id = unarmed_id;
    repo.add_character(character);

    auto characters = repo.get_characters();
    REQUIRE(characters.size() == 1);
    REQUIRE(characters[0].weapon_id == unarmed_id);

    dnd::Character updated = characters[0];
    updated.weapon_id = dagger_id;
    repo.update_character(updated);

    auto reloaded = repo.get_characters();
    REQUIRE(reloaded[0].weapon_id == dagger_id);
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
    character.experience = 7000;
    character.hit_dice_used = 2;
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
    REQUIRE(loaded.experience == 7000);
    REQUIRE(loaded.hit_dice_used == 2);
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
    REQUIRE(loaded.experience == 0);
    REQUIRE(loaded.hit_dice_used == 0);
    REQUIRE_FALSE(loaded.dead);
}

TEST_CASE("update_player renames an existing player", "[repository]") {
    dnd::Repository repo(":memory:");
    int player_id = repo.add_player("Aria");

    dnd::Player player;
    player.player_id = player_id;
    player.player_name = "Aria the Bold";
    repo.update_player(player);

    auto players = repo.get_players();
    REQUIRE(players.size() == 1);
    REQUIRE(players[0].player_name == "Aria the Bold");
}

TEST_CASE("update_character changes fields in place", "[repository]") {
    dnd::Repository repo(":memory:");
    int player_id = repo.add_player("Aria");

    dnd::Character character;
    character.character_name = "Aria's Fighter";
    character.class_id = 1;
    character.race = 1;
    character.alignment = 1;
    character.player_id = player_id;
    character.level = 1;
    character.strength = 10;
    int character_id = repo.add_character(character);

    auto loaded = repo.get_characters();
    REQUIRE(loaded.size() == 1);
    dnd::Character updated = loaded[0];
    updated.level = 4;
    updated.strength = 16;
    updated.experience = 2700;
    updated.hit_dice_used = 1;
    updated.dead = true;
    repo.update_character(updated);

    auto reloaded = repo.get_characters();
    REQUIRE(reloaded.size() == 1);
    REQUIRE(reloaded[0].character_id == character_id);
    REQUIRE(reloaded[0].level == 4);
    REQUIRE(reloaded[0].strength == 16);
    REQUIRE(reloaded[0].experience == 2700);
    REQUIRE(reloaded[0].hit_dice_used == 1);
    REQUIRE(reloaded[0].dead);
}

TEST_CASE("delete_character removes only that character", "[repository]") {
    dnd::Repository repo(":memory:");
    int player_id = repo.add_player("Aria");

    dnd::Character a;
    a.character_name = "Keep Me";
    a.class_id = 1;
    a.race = 1;
    a.alignment = 1;
    a.player_id = player_id;
    repo.add_character(a);

    dnd::Character b;
    b.character_name = "Delete Me";
    b.class_id = 1;
    b.race = 1;
    b.alignment = 1;
    b.player_id = player_id;
    int delete_id = repo.add_character(b);

    repo.delete_character(delete_id);

    auto remaining = repo.get_characters();
    REQUIRE(remaining.size() == 1);
    REQUIRE(remaining[0].character_name == "Keep Me");
}

TEST_CASE("delete_player cascades to that player's characters", "[repository]") {
    dnd::Repository repo(":memory:");
    int player_id = repo.add_player("Aria");

    dnd::Character character;
    character.character_name = "Aria's Fighter";
    character.class_id = 1;
    character.race = 1;
    character.alignment = 1;
    character.player_id = player_id;
    repo.add_character(character);

    repo.delete_player(player_id);

    REQUIRE(repo.get_players().empty());
    REQUIRE(repo.get_characters().empty());
}
