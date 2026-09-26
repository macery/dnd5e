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
constexpr std::array<Migration, 7> kMigrations = {{
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
    {4, "add Races and Alignments lookup tables, seeded", R"sql(
        CREATE TABLE IF NOT EXISTS Races (
            RaceID INTEGER PRIMARY KEY AUTOINCREMENT,
            RaceName TEXT NOT NULL UNIQUE
        );

        CREATE TABLE IF NOT EXISTS Alignments (
            AlignmentID INTEGER PRIMARY KEY AUTOINCREMENT,
            AlignmentName TEXT NOT NULL UNIQUE
        );

        INSERT INTO Races (RaceName) VALUES
            ('Dragonborn'), ('Dwarf'), ('Elf'), ('Gnome'), ('Half-Elf'),
            ('Half-Orc'), ('Halfling'), ('Human'), ('Tiefling');

        INSERT INTO Alignments (AlignmentName) VALUES
            ('Lawful Good'), ('Neutral Good'), ('Chaotic Good'),
            ('Lawful Neutral'), ('True Neutral'), ('Chaotic Neutral'),
            ('Lawful Evil'), ('Neutral Evil'), ('Chaotic Evil');
    )sql"},
    {5, "add Monsters table, seeded with sample low-CR enemies", R"sql(
        CREATE TABLE IF NOT EXISTS Monsters (
            MonsterID INTEGER PRIMARY KEY AUTOINCREMENT,
            Name TEXT NOT NULL UNIQUE,
            ArmorClass INTEGER NOT NULL,
            HitPoints INTEGER NOT NULL,
            Strength INTEGER NOT NULL,
            Dexterity INTEGER NOT NULL,
            Constitution INTEGER NOT NULL,
            Intelligence INTEGER NOT NULL,
            Wisdom INTEGER NOT NULL,
            Charisma INTEGER NOT NULL,
            ChallengeRating REAL NOT NULL
        );

        INSERT INTO Monsters (Name, ArmorClass, HitPoints, Strength, Dexterity, Constitution,
                               Intelligence, Wisdom, Charisma, ChallengeRating) VALUES
            ('Kobold',   12, 5,  7, 15,  9, 8,  7, 8, 0.125),
            ('Goblin',   15, 7,  8, 14, 10, 10, 8, 8, 0.25),
            ('Skeleton', 13, 13, 10, 14, 15, 6,  8, 5, 0.25),
            ('Wolf',     13, 11, 12, 15, 12, 3,  12, 6, 0.25),
            ('Zombie',   8,  22, 13, 6,  16, 3,  6, 5, 0.25),
            ('Orc',      13, 15, 16, 12, 16, 7,  11, 10, 0.5);
    )sql"},
    {6, "add Weapons table, seeded, and equip existing characters with it", R"sql(
        CREATE TABLE IF NOT EXISTS Weapons (
            WeaponID INTEGER PRIMARY KEY AUTOINCREMENT,
            Name TEXT NOT NULL UNIQUE,
            DamageDiceCount INTEGER NOT NULL,
            DamageDiceSides INTEGER NOT NULL,
            IsFinesse INTEGER NOT NULL DEFAULT 0,
            IsRanged INTEGER NOT NULL DEFAULT 0
        );

        INSERT INTO Weapons (Name, DamageDiceCount, DamageDiceSides, IsFinesse, IsRanged) VALUES
            ('Unarmed Strike', 1, 1, 0, 0),
            ('Dagger',         1, 4, 1, 0),
            ('Shortsword',     1, 6, 1, 0),
            ('Rapier',         1, 8, 1, 0),
            ('Longsword',      1, 8, 0, 0),
            ('Greataxe',       1, 12, 0, 0),
            ('Shortbow',       1, 6, 0, 1),
            ('Longbow',        1, 8, 0, 1);

        ALTER TABLE Characters ADD COLUMN WeaponID INTEGER;

        UPDATE Characters SET WeaponID = (SELECT WeaponID FROM Weapons WHERE Name = 'Unarmed Strike');
    )sql"},
    {7, "add attack data to Monsters", R"sql(
        ALTER TABLE Monsters ADD COLUMN AttackName TEXT NOT NULL DEFAULT '';
        ALTER TABLE Monsters ADD COLUMN AttackBonus INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Monsters ADD COLUMN DamageDiceCount INTEGER NOT NULL DEFAULT 1;
        ALTER TABLE Monsters ADD COLUMN DamageDiceSides INTEGER NOT NULL DEFAULT 4;
        ALTER TABLE Monsters ADD COLUMN DamageBonus INTEGER NOT NULL DEFAULT 0;

        UPDATE Monsters SET AttackName = 'Dagger',    AttackBonus = 4, DamageDiceCount = 1, DamageDiceSides = 4,  DamageBonus = 2 WHERE Name = 'Kobold';
        UPDATE Monsters SET AttackName = 'Scimitar',  AttackBonus = 4, DamageDiceCount = 1, DamageDiceSides = 6,  DamageBonus = 2 WHERE Name = 'Goblin';
        UPDATE Monsters SET AttackName = 'Shortsword',AttackBonus = 4, DamageDiceCount = 1, DamageDiceSides = 6,  DamageBonus = 2 WHERE Name = 'Skeleton';
        UPDATE Monsters SET AttackName = 'Bite',      AttackBonus = 4, DamageDiceCount = 2, DamageDiceSides = 4,  DamageBonus = 2 WHERE Name = 'Wolf';
        UPDATE Monsters SET AttackName = 'Slam',      AttackBonus = 3, DamageDiceCount = 1, DamageDiceSides = 6,  DamageBonus = 1 WHERE Name = 'Zombie';
        UPDATE Monsters SET AttackName = 'Greataxe',  AttackBonus = 5, DamageDiceCount = 1, DamageDiceSides = 12, DamageBonus = 3 WHERE Name = 'Orc';
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
