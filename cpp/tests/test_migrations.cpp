#include <catch2/catch_test_macros.hpp>

#include <sqlite3.h>

#include <cstdio>
#include <memory>
#include <string>

#include "dnd/migrations.hpp"
#include "dnd/repository.hpp"

namespace {

// RAII so a failed assertion still closes the connection.
struct RawDb {
    sqlite3* handle = nullptr;

    explicit RawDb(const std::string& path) {
        REQUIRE(sqlite3_open(path.c_str(), &handle) == SQLITE_OK);
    }
    ~RawDb() { sqlite3_close(handle); }
};

int row_count(sqlite3* db, const char* sql) {
    sqlite3_stmt* stmt = nullptr;
    REQUIRE(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

bool table_exists(sqlite3* db, const char* name) {
    sqlite3_stmt* stmt = nullptr;
    REQUIRE(sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=?;", -1,
                                &stmt, nullptr) == SQLITE_OK);
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    int found = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) found = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return found > 0;
}

}  // namespace

TEST_CASE("run_migrations brings a fresh database to the latest schema", "[migrations]") {
    RawDb db(":memory:");

    dnd::run_migrations(db.handle);

    REQUIRE(dnd::current_schema_version(db.handle) == 30);
    REQUIRE(table_exists(db.handle, "Players"));
    REQUIRE(table_exists(db.handle, "Classes"));
    REQUIRE(table_exists(db.handle, "Characters"));
    REQUIRE(table_exists(db.handle, "Races"));
    REQUIRE(table_exists(db.handle, "Alignments"));
    REQUIRE(table_exists(db.handle, "Monsters"));
    REQUIRE(table_exists(db.handle, "Weapons"));
    REQUIRE(table_exists(db.handle, "Items"));
    REQUIRE(table_exists(db.handle, "CharacterInventory"));
    // Column added by migration 2 -- this would fail to prepare if it were missing.
    REQUIRE(row_count(db.handle, "SELECT COUNT(*) FROM Characters WHERE Strength = 10;") == 0);
    // Seeded by migrations 3, 4, 5, and 6.
    REQUIRE(row_count(db.handle, "SELECT COUNT(*) FROM Classes;") == 12);
    REQUIRE(row_count(db.handle, "SELECT COUNT(*) FROM Races;") == 9);
    REQUIRE(row_count(db.handle, "SELECT COUNT(*) FROM Alignments;") == 9);
    REQUIRE(row_count(db.handle, "SELECT COUNT(*) FROM Monsters;") == 24);
    REQUIRE(row_count(db.handle, "SELECT COUNT(*) FROM Weapons;") == 12);
    // Backfilled by migration 7.
    REQUIRE(row_count(db.handle, "SELECT COUNT(*) FROM Monsters WHERE AttackName = 'Scimitar' AND Name = 'Goblin';") ==
            1);
}

TEST_CASE("run_migrations is a no-op the second time", "[migrations]") {
    RawDb db(":memory:");

    dnd::run_migrations(db.handle);
    dnd::run_migrations(db.handle);

    REQUIRE(row_count(db.handle, "SELECT COUNT(*) FROM SchemaMigrations;") == 30);
    REQUIRE(row_count(db.handle, "SELECT COUNT(*) FROM Classes;") == 12);
    REQUIRE(row_count(db.handle, "SELECT COUNT(*) FROM Races;") == 9);
    REQUIRE(row_count(db.handle, "SELECT COUNT(*) FROM Monsters;") == 24);
    REQUIRE(row_count(db.handle, "SELECT COUNT(*) FROM Weapons;") == 12);
}

TEST_CASE("migration 6 equips existing characters with Unarmed Strike", "[migrations]") {
    // Simulates the real scenario: a character that already existed before
    // Weapons/WeaponID were added. Manually builds a database recorded as
    // already at version 5 (migration 6's SQL only touches Characters and
    // Weapons; migration 7 (Monsters attack columns) also runs after it since
    // both are newer than the recorded version 5, so a minimal Monsters
    // table is included too even though it's not this test's focus; likewise
    // a Classes table, which migration 11 reads to seed class spell lists).
    RawDb db(":memory:");

    char* error = nullptr;
    int result = sqlite3_exec(db.handle,
        "CREATE TABLE Players (PlayerID INTEGER PRIMARY KEY AUTOINCREMENT, PlayerName TEXT NOT NULL UNIQUE);"
        "CREATE TABLE Characters (CharacterId INTEGER PRIMARY KEY AUTOINCREMENT, "
        "CharacterName TEXT NOT NULL UNIQUE, Class INTEGER NOT NULL, Race INTEGER NOT NULL, "
        "Alignment INTEGER NOT NULL, PlayerID INTEGER NOT NULL, Level INTEGER NOT NULL DEFAULT 1);"
        "CREATE TABLE Classes (ClassID INTEGER PRIMARY KEY AUTOINCREMENT, ClassName TEXT NOT NULL UNIQUE);"
        "CREATE TABLE Monsters (MonsterID INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT NOT NULL UNIQUE, "
        "ArmorClass INTEGER NOT NULL, HitPoints INTEGER NOT NULL, Strength INTEGER NOT NULL, "
        "Dexterity INTEGER NOT NULL, Constitution INTEGER NOT NULL, Intelligence INTEGER NOT NULL, "
        "Wisdom INTEGER NOT NULL, Charisma INTEGER NOT NULL, ChallengeRating REAL NOT NULL);"
        "INSERT INTO Players (PlayerName) VALUES ('Aria');"
        "INSERT INTO Characters (CharacterName, Class, Race, Alignment, PlayerID) "
        "VALUES ('Aria''s Fighter', 1, 1, 1, 1);"
        "CREATE TABLE SchemaMigrations (Version INTEGER PRIMARY KEY, Description TEXT NOT NULL, "
        "AppliedAt TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);"
        "INSERT INTO SchemaMigrations (Version, Description) VALUES "
        "(1, 'x'), (2, 'x'), (3, 'x'), (4, 'x'), (5, 'x');",
        nullptr, nullptr, &error);
    REQUIRE(result == SQLITE_OK);

    dnd::run_migrations(db.handle);

    REQUIRE(dnd::current_schema_version(db.handle) == 30);

    sqlite3_stmt* stmt = nullptr;
    REQUIRE(sqlite3_prepare_v2(db.handle,
        "SELECT Weapons.Name FROM Characters JOIN Weapons ON Characters.WeaponID = Weapons.WeaponID "
        "WHERE Characters.CharacterName = 'Aria''s Fighter';",
        -1, &stmt, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(stmt) == SQLITE_ROW);
    std::string weapon_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    REQUIRE(weapon_name == "Unarmed Strike");
}

TEST_CASE("Repository upgrades a real pre-migration database in place", "[migrations]") {
    // This is the exact scenario that motivated the migration layer: a
    // database created before ability scores/hp/ac/proficiencies existed,
    // with no SchemaMigrations table at all.
    std::string path = "test_migration_upgrade.db";
    std::remove(path.c_str());

    {
        RawDb db(path);
        char* error = nullptr;
        int result = sqlite3_exec(db.handle,
            "CREATE TABLE Players (PlayerID INTEGER PRIMARY KEY AUTOINCREMENT, PlayerName TEXT NOT NULL UNIQUE);"
            "CREATE TABLE Characters (CharacterId INTEGER PRIMARY KEY AUTOINCREMENT, "
            "CharacterName TEXT NOT NULL UNIQUE, Class INTEGER NOT NULL, Race INTEGER NOT NULL, "
            "Alignment INTEGER NOT NULL, PlayerID INTEGER NOT NULL, Level INTEGER);"
            "INSERT INTO Players (PlayerName) VALUES ('Aria');",
            nullptr, nullptr, &error);
        REQUIRE(result == SQLITE_OK);
    }

    {
        dnd::Repository repo(path);

        auto players = repo.get_players();
        REQUIRE(players.size() == 1);
        REQUIRE(players[0].player_name == "Aria");

        dnd::Character character;
        character.character_name = "Aria's Paladin";
        character.class_id = 1;
        character.race = 1;
        character.alignment = 1;
        character.player_id = players[0].player_id;
        character.strength = 16;
        repo.add_character(character);

        auto characters = repo.get_characters();
        REQUIRE(characters.size() == 1);
        REQUIRE(characters[0].strength == 16);
    }

    std::remove(path.c_str());
}

TEST_CASE("migration 15 turns each character's weapon into an equipped item", "[migrations]") {
    RawDb db(":memory:");
    dnd::run_migrations(db.handle, 14);
    REQUIRE(dnd::current_schema_version(db.handle) == 14);

    char* error = nullptr;
    int result = sqlite3_exec(db.handle,
        "INSERT INTO Players (PlayerName) VALUES ('Aria');"
        "INSERT INTO Characters (CharacterName, Class, Race, Alignment, PlayerID, WeaponID) "
        "VALUES ('Aria', 1, 1, 1, 1, (SELECT WeaponID FROM Weapons WHERE Name = 'Rapier'));"
        "INSERT INTO Characters (CharacterName, Class, Race, Alignment, PlayerID, WeaponID) "
        "VALUES ('Bram', 1, 1, 1, 1, (SELECT WeaponID FROM Weapons WHERE Name = 'Unarmed Strike'));",
        nullptr, nullptr, &error);
    REQUIRE(result == SQLITE_OK);

    dnd::run_migrations(db.handle);
    REQUIRE(dnd::current_schema_version(db.handle) == 30);
    REQUIRE(row_count(db.handle,
        "SELECT COUNT(*) FROM CharacterInventory JOIN Items ON Items.ItemID = CharacterInventory.ItemID "
        "JOIN Characters ON Characters.CharacterId = CharacterInventory.CharacterID "
        "WHERE CharacterName = 'Aria' AND Items.Name = 'Rapier' AND Equipped = 1 AND Quantity = 1;") == 1);
    // Unarmed Strike isn't an item: it's what you do with nothing wielded.
    REQUIRE(row_count(db.handle,
        "SELECT COUNT(*) FROM CharacterInventory "
        "JOIN Characters ON Characters.CharacterId = CharacterInventory.CharacterID "
        "WHERE CharacterName = 'Bram';") == 0);
}
