#include "dnd/repository.hpp"

#include <sqlite3.h>

#include <stdexcept>
#include <utility>

#include "dnd/detail/statement.hpp"
#include "dnd/migrations.hpp"

namespace dnd {

namespace {

using detail::Statement;
using detail::bind_text;

constexpr const char* kCharacterColumns =
    "CharacterId, CharacterName, Class, Race, Alignment, PlayerID, Level, "
    "Strength, Dexterity, Constitution, Intelligence, Wisdom, Charisma, "
    "CurrentHP, MaxHP, ArmorClass, SkillProficiencies, SaveProficiencies";

Character read_character_row(sqlite3_stmt* stmt) {
    Character character;
    character.character_id = sqlite3_column_int(stmt, 0);
    const unsigned char* name = sqlite3_column_text(stmt, 1);
    character.character_name = name ? reinterpret_cast<const char*>(name) : "";
    character.class_id = sqlite3_column_int(stmt, 2);
    character.race = sqlite3_column_int(stmt, 3);
    character.alignment = sqlite3_column_int(stmt, 4);
    character.player_id = sqlite3_column_int(stmt, 5);
    character.level = sqlite3_column_int(stmt, 6);
    character.strength = sqlite3_column_int(stmt, 7);
    character.dexterity = sqlite3_column_int(stmt, 8);
    character.constitution = sqlite3_column_int(stmt, 9);
    character.intelligence = sqlite3_column_int(stmt, 10);
    character.wisdom = sqlite3_column_int(stmt, 11);
    character.charisma = sqlite3_column_int(stmt, 12);
    character.current_hp = sqlite3_column_int(stmt, 13);
    character.max_hp = sqlite3_column_int(stmt, 14);
    character.armor_class = sqlite3_column_int(stmt, 15);
    character.skill_proficiencies = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 16));
    character.save_proficiencies = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 17));
    return character;
}

std::vector<LookupEntry> get_lookup_entries(sqlite3* db, const char* table, const char* id_column,
                                             const char* name_column) {
    std::string sql =
        std::string("SELECT ") + id_column + ", " + name_column + " FROM " + table + " ORDER BY " + name_column + ";";
    Statement stmt(db, sql.c_str());
    std::vector<LookupEntry> entries;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        LookupEntry entry;
        entry.id = sqlite3_column_int(stmt.get(), 0);
        const unsigned char* name = sqlite3_column_text(stmt.get(), 1);
        entry.name = name ? reinterpret_cast<const char*>(name) : "";
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::vector<Character> run_character_query(sqlite3* db, const std::string& sql, int bind_player_id = -1) {
    Statement stmt(db, sql.c_str());
    if (bind_player_id >= 0) {
        sqlite3_bind_int(stmt.get(), 1, bind_player_id);
    }
    std::vector<Character> characters;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        characters.push_back(read_character_row(stmt.get()));
    }
    return characters;
}

}  // namespace

Repository::Repository(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        std::string message = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("failed to open database '" + db_path + "': " + message);
    }
    sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    run_migrations(db_);
}

Repository::~Repository() {
    if (db_) sqlite3_close(db_);
}

Repository::Repository(Repository&& other) noexcept : db_(other.db_) {
    other.db_ = nullptr;
}

Repository& Repository::operator=(Repository&& other) noexcept {
    if (this != &other) {
        if (db_) sqlite3_close(db_);
        db_ = std::exchange(other.db_, nullptr);
    }
    return *this;
}

std::vector<Player> Repository::get_players() const {
    Statement stmt(db_, "SELECT PlayerID, PlayerName FROM Players ORDER BY PlayerID;");
    std::vector<Player> players;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        Player player;
        player.player_id = sqlite3_column_int(stmt.get(), 0);
        const unsigned char* name = sqlite3_column_text(stmt.get(), 1);
        player.player_name = name ? reinterpret_cast<const char*>(name) : "";
        players.push_back(std::move(player));
    }
    return players;
}

std::vector<LookupEntry> Repository::get_classes() const {
    return get_lookup_entries(db_, "Classes", "ClassID", "ClassName");
}

std::vector<LookupEntry> Repository::get_races() const {
    return get_lookup_entries(db_, "Races", "RaceID", "RaceName");
}

std::vector<LookupEntry> Repository::get_alignments() const {
    return get_lookup_entries(db_, "Alignments", "AlignmentID", "AlignmentName");
}

std::vector<Character> Repository::get_characters() const {
    std::string sql = std::string("SELECT ") + kCharacterColumns + " FROM Characters ORDER BY CharacterId;";
    return run_character_query(db_, sql);
}

std::vector<Character> Repository::get_characters_by_player(int player_id) const {
    std::string sql = std::string("SELECT ") + kCharacterColumns +
                       " FROM Characters WHERE PlayerID = ? ORDER BY CharacterId;";
    return run_character_query(db_, sql, player_id);
}

std::vector<Monster> Repository::get_monsters() const {
    Statement stmt(db_,
        "SELECT MonsterID, Name, ArmorClass, HitPoints, Strength, Dexterity, Constitution, "
        "Intelligence, Wisdom, Charisma, ChallengeRating FROM Monsters ORDER BY ChallengeRating, Name;");
    std::vector<Monster> monsters;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        Monster monster;
        monster.monster_id = sqlite3_column_int(stmt.get(), 0);
        const unsigned char* name = sqlite3_column_text(stmt.get(), 1);
        monster.name = name ? reinterpret_cast<const char*>(name) : "";
        monster.armor_class = sqlite3_column_int(stmt.get(), 2);
        monster.hit_points = sqlite3_column_int(stmt.get(), 3);
        monster.strength = sqlite3_column_int(stmt.get(), 4);
        monster.dexterity = sqlite3_column_int(stmt.get(), 5);
        monster.constitution = sqlite3_column_int(stmt.get(), 6);
        monster.intelligence = sqlite3_column_int(stmt.get(), 7);
        monster.wisdom = sqlite3_column_int(stmt.get(), 8);
        monster.charisma = sqlite3_column_int(stmt.get(), 9);
        monster.challenge_rating = sqlite3_column_double(stmt.get(), 10);
        monsters.push_back(std::move(monster));
    }
    return monsters;
}

int Repository::add_player(const std::string& player_name) {
    Statement stmt(db_, "INSERT INTO Players (PlayerName) VALUES (?);");
    bind_text(stmt, 1, player_name);
    detail::run_and_expect_done(stmt, "insert player");
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

int Repository::add_character(const Character& character) {
    Statement stmt(db_,
        "INSERT INTO Characters (CharacterName, Class, Race, Alignment, PlayerID, Level, "
        "Strength, Dexterity, Constitution, Intelligence, Wisdom, Charisma, "
        "CurrentHP, MaxHP, ArmorClass, SkillProficiencies, SaveProficiencies) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
    bind_text(stmt, 1, character.character_name);
    sqlite3_bind_int(stmt.get(), 2, character.class_id);
    sqlite3_bind_int(stmt.get(), 3, character.race);
    sqlite3_bind_int(stmt.get(), 4, character.alignment);
    sqlite3_bind_int(stmt.get(), 5, character.player_id);
    sqlite3_bind_int(stmt.get(), 6, character.level);
    sqlite3_bind_int(stmt.get(), 7, character.strength);
    sqlite3_bind_int(stmt.get(), 8, character.dexterity);
    sqlite3_bind_int(stmt.get(), 9, character.constitution);
    sqlite3_bind_int(stmt.get(), 10, character.intelligence);
    sqlite3_bind_int(stmt.get(), 11, character.wisdom);
    sqlite3_bind_int(stmt.get(), 12, character.charisma);
    sqlite3_bind_int(stmt.get(), 13, character.current_hp);
    sqlite3_bind_int(stmt.get(), 14, character.max_hp);
    sqlite3_bind_int(stmt.get(), 15, character.armor_class);
    sqlite3_bind_int(stmt.get(), 16, static_cast<int>(character.skill_proficiencies));
    sqlite3_bind_int(stmt.get(), 17, static_cast<int>(character.save_proficiencies));
    detail::run_and_expect_done(stmt, "insert character");
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Repository::update_player(const Player& player) {
    Statement stmt(db_, "UPDATE Players SET PlayerName = ? WHERE PlayerID = ?;");
    bind_text(stmt, 1, player.player_name);
    sqlite3_bind_int(stmt.get(), 2, player.player_id);
    detail::run_and_expect_done(stmt, "update player");
}

void Repository::delete_player(int player_id) {
    sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, nullptr);
    try {
        {
            Statement stmt(db_, "DELETE FROM Characters WHERE PlayerID = ?;");
            sqlite3_bind_int(stmt.get(), 1, player_id);
            detail::run_and_expect_done(stmt, "delete player's characters");
        }
        {
            Statement stmt(db_, "DELETE FROM Players WHERE PlayerID = ?;");
            sqlite3_bind_int(stmt.get(), 1, player_id);
            detail::run_and_expect_done(stmt, "delete player");
        }
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        throw;
    }
}

void Repository::update_character(const Character& character) {
    Statement stmt(db_,
        "UPDATE Characters SET CharacterName = ?, Class = ?, Race = ?, Alignment = ?, Level = ?, "
        "Strength = ?, Dexterity = ?, Constitution = ?, Intelligence = ?, Wisdom = ?, Charisma = ?, "
        "CurrentHP = ?, MaxHP = ?, ArmorClass = ?, SkillProficiencies = ?, SaveProficiencies = ? "
        "WHERE CharacterId = ?;");
    bind_text(stmt, 1, character.character_name);
    sqlite3_bind_int(stmt.get(), 2, character.class_id);
    sqlite3_bind_int(stmt.get(), 3, character.race);
    sqlite3_bind_int(stmt.get(), 4, character.alignment);
    sqlite3_bind_int(stmt.get(), 5, character.level);
    sqlite3_bind_int(stmt.get(), 6, character.strength);
    sqlite3_bind_int(stmt.get(), 7, character.dexterity);
    sqlite3_bind_int(stmt.get(), 8, character.constitution);
    sqlite3_bind_int(stmt.get(), 9, character.intelligence);
    sqlite3_bind_int(stmt.get(), 10, character.wisdom);
    sqlite3_bind_int(stmt.get(), 11, character.charisma);
    sqlite3_bind_int(stmt.get(), 12, character.current_hp);
    sqlite3_bind_int(stmt.get(), 13, character.max_hp);
    sqlite3_bind_int(stmt.get(), 14, character.armor_class);
    sqlite3_bind_int(stmt.get(), 15, static_cast<int>(character.skill_proficiencies));
    sqlite3_bind_int(stmt.get(), 16, static_cast<int>(character.save_proficiencies));
    sqlite3_bind_int(stmt.get(), 17, character.character_id);
    detail::run_and_expect_done(stmt, "update character");
}

void Repository::delete_character(int character_id) {
    Statement stmt(db_, "DELETE FROM Characters WHERE CharacterId = ?;");
    sqlite3_bind_int(stmt.get(), 1, character_id);
    detail::run_and_expect_done(stmt, "delete character");
}

}  // namespace dnd
