#include "dnd/migrations.hpp"

#include <sqlite3.h>

#include <array>
#include <stdexcept>
#include <string>

#include "dnd/detail/statement.hpp"

namespace dnd {

namespace {

using detail::Statement;

// The schema states this project has actually had. Append new entries here
// for future schema changes -- never edit an existing one, since a
// migration's sql is exactly what already ran on every database that has it
// recorded in SchemaMigrations.
constexpr std::array<Migration, 3> kMigrations = {{
    {1, "initial schema: Players, Classes, Characters", R"sql(
        CREATE TABLE IF NOT EXISTS Players (
            PlayerID INTEGER PRIMARY KEY AUTOINCREMENT,
            PlayerName TEXT NOT NULL UNIQUE
        );

        CREATE TABLE IF NOT EXISTS Classes (
            ClassID INTEGER PRIMARY KEY AUTOINCREMENT,
            ClassName TEXT NOT NULL UNIQUE
        );

        CREATE TABLE IF NOT EXISTS Characters (
            CharacterId INTEGER PRIMARY KEY AUTOINCREMENT,
            CharacterName TEXT NOT NULL UNIQUE,
            Class INTEGER NOT NULL,
            Race INTEGER NOT NULL,
            Alignment INTEGER NOT NULL,
            PlayerID INTEGER NOT NULL,
            Level INTEGER NOT NULL DEFAULT 1,
            FOREIGN KEY(PlayerID) REFERENCES Players(PlayerID)
        );
    )sql"},
    {2, "add ability scores, hp/ac, and proficiency bitmasks to Characters", R"sql(
        ALTER TABLE Characters ADD COLUMN Strength INTEGER NOT NULL DEFAULT 10;
        ALTER TABLE Characters ADD COLUMN Dexterity INTEGER NOT NULL DEFAULT 10;
        ALTER TABLE Characters ADD COLUMN Constitution INTEGER NOT NULL DEFAULT 10;
        ALTER TABLE Characters ADD COLUMN Intelligence INTEGER NOT NULL DEFAULT 10;
        ALTER TABLE Characters ADD COLUMN Wisdom INTEGER NOT NULL DEFAULT 10;
        ALTER TABLE Characters ADD COLUMN Charisma INTEGER NOT NULL DEFAULT 10;
        ALTER TABLE Characters ADD COLUMN CurrentHP INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Characters ADD COLUMN MaxHP INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Characters ADD COLUMN ArmorClass INTEGER NOT NULL DEFAULT 10;
        ALTER TABLE Characters ADD COLUMN SkillProficiencies INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Characters ADD COLUMN SaveProficiencies INTEGER NOT NULL DEFAULT 0;
    )sql"},
    {3, "seed the 12 core 5e classes", R"sql(
        INSERT INTO Classes (ClassName) VALUES
            ('Barbarian'), ('Bard'), ('Cleric'), ('Druid'),
            ('Fighter'), ('Monk'), ('Paladin'), ('Ranger'),
            ('Rogue'), ('Sorcerer'), ('Warlock'), ('Wizard');
    )sql"},
}};

void exec_or_throw(sqlite3* db, const char* sql) {
    char* error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        std::string message = error ? error : "unknown error";
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}

void ensure_migrations_table(sqlite3* db) {
    exec_or_throw(db, R"sql(
        CREATE TABLE IF NOT EXISTS SchemaMigrations (
            Version INTEGER PRIMARY KEY,
            Description TEXT NOT NULL,
            AppliedAt TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
    )sql");
}

void record_migration(sqlite3* db, const Migration& migration) {
    Statement stmt(db, "INSERT INTO SchemaMigrations (Version, Description) VALUES (?, ?);");
    sqlite3_bind_int(stmt.get(), 1, migration.version);
    detail::bind_text(stmt, 2, migration.description);
    detail::run_and_expect_done(stmt, "record migration");
}

}  // namespace

int current_schema_version(sqlite3* db) {
    ensure_migrations_table(db);

    Statement stmt(db, "SELECT COALESCE(MAX(Version), 0) FROM SchemaMigrations;");
    int version = 0;
    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        version = sqlite3_column_int(stmt.get(), 0);
    }
    return version;
}

void run_migrations(sqlite3* db) {
    int version = current_schema_version(db);

    for (const auto& migration : kMigrations) {
        if (migration.version <= version) continue;

        exec_or_throw(db, "BEGIN;");
        try {
            exec_or_throw(db, migration.sql);
            record_migration(db, migration);
            exec_or_throw(db, "COMMIT;");
        } catch (...) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            throw;
        }
    }
}

}  // namespace dnd
