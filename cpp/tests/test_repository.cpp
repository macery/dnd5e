#include <catch2/catch_test_macros.hpp>

#include "dnd/progression.hpp"
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
    REQUIRE(monsters.size() == 40);  // 19 foes + 5 beasts for Wild Shape + 12 for levels 5-10 + 4 town folk

    const dnd::Monster* goblin = nullptr;
    for (const auto& monster : monsters) {
        if (monster.name == "Goblin") goblin = &monster;
    }
    REQUIRE(goblin != nullptr);
    REQUIRE(goblin->armor_class == 15);
    REQUIRE(goblin->hit_points == 7);
    REQUIRE(goblin->challenge_rating == 0.25);
    REQUIRE(goblin->creature_type == "Humanoid");
    REQUIRE(goblin->attacks.size() == 1);
    const auto& scimitar = goblin->attacks.front();
    REQUIRE(scimitar.name == "Scimitar");
    REQUIRE(scimitar.attack_bonus == 4);
    REQUIRE(scimitar.damage_dice_count == 1);
    REQUIRE(scimitar.damage_dice_sides == 6);
    REQUIRE(scimitar.damage_bonus == 2);
    REQUIRE(scimitar.damage_type == "slashing");
    REQUIRE(scimitar.count == 1);
}

TEST_CASE("every seeded monster has a type and typed attacks", "[repository]") {
    dnd::Repository repo(":memory:");
    for (const auto& monster : repo.get_monsters()) {
        INFO(monster.name);
        REQUIRE_FALSE(monster.creature_type.empty());
        REQUIRE_FALSE(monster.attacks.empty());
        for (const auto& attack : monster.attacks) REQUIRE_FALSE(attack.damage_type.empty());
    }
}

TEST_CASE("undead traits: the Skeleton and Zombie", "[repository]") {
    dnd::Repository repo(":memory:");
    for (const auto& monster : repo.get_monsters()) {
        if (monster.name == "Skeleton") {
            REQUIRE(monster.creature_type == "Undead");
            REQUIRE(monster.damage.vulnerabilities == std::vector<std::string>{"bludgeoning"});
            REQUIRE(monster.damage.immunities == std::vector<std::string>{"poison"});
            REQUIRE(monster.condition_immunities == std::vector<dnd::Condition>{dnd::Condition::Poisoned});
        }
        if (monster.name == "Zombie") {
            REQUIRE(monster.damage.immunities == std::vector<std::string>{"poison"});
            REQUIRE(monster.attacks.front().damage_type == "bludgeoning");
        }
    }
}

TEST_CASE("the Wolf's bite can knock a target prone", "[repository]") {
    dnd::Repository repo(":memory:");
    for (const auto& monster : repo.get_monsters()) {
        const auto& attack = monster.attacks.front();
        if (monster.name == "Wolf") {
            REQUIRE(attack.on_hit_save_dc == 11);
            REQUIRE(attack.on_hit_save_ability == dnd::Ability::Strength);
            REQUIRE(attack.on_hit_condition == dnd::Condition::Prone);
        } else if (monster.name != "Dire Wolf" && monster.name != "Ghoul") {
            REQUIRE(attack.on_hit_save_dc == 0);
        }
    }
}

TEST_CASE("the SRD roster: Multiattack, extra damage, riders, traits", "[repository]") {
    dnd::Repository repo(":memory:");
    auto find = [&](const std::string& name) {
        for (const auto& m : repo.get_monsters()) {
            if (m.name == name) return m;
        }
        FAIL("no monster " << name);
        return dnd::Monster{};
    };
    auto troll = find("Troll");
    REQUIRE(troll.challenge_rating == 5);
    REQUIRE(troll.multiattack == "A bite and two claw attacks");
    REQUIRE(troll.attacks.size() == 2);
    REQUIRE(troll.attacks[1].name == "Claw");
    REQUIRE(troll.attacks[1].count == 2);
    REQUIRE(troll.traits == std::vector<std::string>{"Regeneration"});

    auto hound = find("Hell Hound");
    REQUIRE(hound.creature_type == "Fiend");
    REQUIRE(hound.attacks.front().extra_dice_count == 2);
    REQUIRE(hound.attacks.front().extra_damage_type == "fire");
    REQUIRE(hound.damage.immunities == std::vector<std::string>{"fire"});

    auto ghoul = find("Ghoul");
    REQUIRE(ghoul.multiattack.empty());
    REQUIRE(ghoul.attacks.front().name == "Claws");  // without Multiattack, the first attack is used
    REQUIRE(ghoul.attacks.front().on_hit_condition == dnd::Condition::Paralyzed);
    REQUIRE(ghoul.attacks.front().on_hit_rounds == 10);
    REQUIRE(ghoul.attacks.front().on_hit_repeat_save);
    REQUIRE(ghoul.condition_immunities ==
            std::vector<dnd::Condition>{dnd::Condition::Charmed, dnd::Condition::Poisoned});

    REQUIRE(find("Kobold").traits == std::vector<std::string>{"PackTactics"});
    REQUIRE(find("Zombie").traits == std::vector<std::string>{"UndeadFortitude"});
    REQUIRE(find("Hobgoblin").traits == std::vector<std::string>{"MartialAdvantage"});
    for (const auto& m : repo.get_monsters()) {
        INFO(m.name);
        REQUIRE_NOTHROW(dnd::xp_for_challenge_rating(m.challenge_rating));
    }
}

TEST_CASE("Charm Person targets humanoids only", "[repository]") {
    dnd::Repository repo(":memory:");
    for (const auto& spell : repo.get_spells()) {
        if (spell.name == "Charm Person") REQUIRE(spell.target_creature_type == "Humanoid");
        if (spell.name == "Fire Bolt") REQUIRE(spell.target_creature_type.empty());
    }
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
    REQUIRE(loaded.current_hp == 0);
    REQUIRE(loaded.skill_proficiencies == 0);
    REQUIRE(loaded.save_proficiencies == 0);
    REQUIRE(loaded.experience == 0);
    REQUIRE(loaded.hit_dice_used == 0);
    REQUIRE_FALSE(loaded.dead);
    REQUIRE(loaded.feature_uses_spent.empty());
    REQUIRE(loaded.fighting_style.empty());
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
    updated.feature_uses_spent = {{"SecondWind", 1}, {"Rage", 2}};
    updated.fighting_style = "Defense";
    updated.coins = {12, 0, 0, 15, 1};
    repo.update_character(updated);

    auto reloaded = repo.get_characters();
    REQUIRE(reloaded.size() == 1);
    REQUIRE(reloaded[0].character_id == character_id);
    REQUIRE(reloaded[0].level == 4);
    REQUIRE(reloaded[0].strength == 16);
    REQUIRE(reloaded[0].experience == 2700);
    REQUIRE(reloaded[0].hit_dice_used == 1);
    REQUIRE(reloaded[0].dead);
    REQUIRE(reloaded[0].feature_uses_spent == std::map<std::string, int>{{"Rage", 2}, {"SecondWind", 1}});
    REQUIRE(reloaded[0].fighting_style == "Defense");
    REQUIRE(reloaded[0].coins == std::array<int, 5>{12, 0, 0, 15, 1});
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
