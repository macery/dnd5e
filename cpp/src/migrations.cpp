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
constexpr std::array<Migration, 35> kMigrations = {{
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
    {8, "add experience and spent hit dice to Characters", R"sql(
        ALTER TABLE Characters ADD COLUMN Experience INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Characters ADD COLUMN HitDiceUsed INTEGER NOT NULL DEFAULT 0;
    )sql"},
    {9, "add Items (seeded) and CharacterInventory", R"sql(
        CREATE TABLE IF NOT EXISTS Items (
            ItemID INTEGER PRIMARY KEY AUTOINCREMENT,
            Name TEXT NOT NULL UNIQUE,
            Category TEXT NOT NULL,
            Description TEXT NOT NULL DEFAULT '',
            WeightLb REAL NOT NULL DEFAULT 0,
            CostCp INTEGER NOT NULL DEFAULT 0,
            HealDiceCount INTEGER NOT NULL DEFAULT 0,
            HealDiceSides INTEGER NOT NULL DEFAULT 0,
            HealBonus INTEGER NOT NULL DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS CharacterInventory (
            CharacterID INTEGER NOT NULL REFERENCES Characters(CharacterId) ON DELETE CASCADE,
            ItemID INTEGER NOT NULL REFERENCES Items(ItemID),
            Quantity INTEGER NOT NULL CHECK (Quantity > 0),
            PRIMARY KEY (CharacterID, ItemID)
        );

        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp, HealDiceCount, HealDiceSides, HealBonus) VALUES
            ('Potion of Healing', 'Consumable', 'A red, glimmering liquid. Drinking it restores 2d4 + 2 hit points.', 0.5, 5000, 2, 4, 2),
            ('Rations (1 day)',   'Consumable', 'Dry, long-lasting food: jerky, dried fruit, hardtack, and nuts.', 2, 50, 0, 0, 0),
            ('Waterskin',         'Gear', 'Holds 4 pints of liquid.', 5, 20, 0, 0, 0),
            ('Torch',             'Gear', 'Burns for 1 hour, shedding bright light in a 20-foot radius.', 1, 1, 0, 0, 0),
            ('Hempen Rope (50 feet)', 'Gear', 'Can be burst with a DC 17 Strength check.', 10, 100, 0, 0, 0),
            ('Bedroll',           'Gear', 'For sleeping in the wild.', 7, 100, 0, 0, 0),
            ('Healer''s Kit',     'Gear', 'Bandages, salves, and splints. Ten uses to stabilize a dying creature.', 3, 500, 0, 0, 0),
            ('Thieves'' Tools',   'Gear', 'Picks, a file, and a small mirror, for disarming traps and opening locks.', 1, 2500, 0, 0, 0);
    )sql"},
    {10, "add Background to Characters; seed the simple weapons classes start with", R"sql(
        ALTER TABLE Characters ADD COLUMN Background TEXT NOT NULL DEFAULT '';

        INSERT INTO Weapons (Name, DamageDiceCount, DamageDiceSides, IsFinesse, IsRanged) VALUES
            ('Mace',           1, 6, 0, 0),
            ('Quarterstaff',   1, 6, 0, 0),
            ('Handaxe',        1, 6, 0, 0),
            ('Light Crossbow', 1, 8, 0, 1);
    )sql"},
    {11, "add Spells and ClassSpells (seeded starter list), CharacterSpells, and spent spell slots", R"sql(
        CREATE TABLE IF NOT EXISTS Spells (
            SpellID INTEGER PRIMARY KEY AUTOINCREMENT,
            Name TEXT NOT NULL UNIQUE,
            Level INTEGER NOT NULL CHECK (Level BETWEEN 0 AND 9),
            School TEXT NOT NULL,
            Description TEXT NOT NULL DEFAULT '',
            CastingTime TEXT NOT NULL,
            RangeText TEXT NOT NULL,
            DurationText TEXT NOT NULL,
            Concentration INTEGER NOT NULL DEFAULT 0,
            Resolution TEXT NOT NULL,
            MeleeAttack INTEGER NOT NULL DEFAULT 0,
            SaveAbility TEXT NOT NULL DEFAULT '',
            SaveHalves INTEGER NOT NULL DEFAULT 0,
            DiceCount INTEGER NOT NULL,
            DiceSides INTEGER NOT NULL,
            FlatBonus INTEGER NOT NULL DEFAULT 0,
            AddsModifier INTEGER NOT NULL DEFAULT 0,
            DamageType TEXT NOT NULL DEFAULT '',
            Projectiles INTEGER NOT NULL DEFAULT 1,
            MaxTargets INTEGER NOT NULL DEFAULT 1,
            UpcastDice INTEGER NOT NULL DEFAULT 0,
            UpcastProjectiles INTEGER NOT NULL DEFAULT 0,
            CantripScaling TEXT NOT NULL DEFAULT 'Dice'
        );

        CREATE TABLE IF NOT EXISTS ClassSpells (
            ClassID INTEGER NOT NULL REFERENCES Classes(ClassID),
            SpellID INTEGER NOT NULL REFERENCES Spells(SpellID),
            PRIMARY KEY (ClassID, SpellID)
        );

        CREATE TABLE IF NOT EXISTS CharacterSpells (
            CharacterID INTEGER NOT NULL REFERENCES Characters(CharacterId) ON DELETE CASCADE,
            SpellID INTEGER NOT NULL REFERENCES Spells(SpellID),
            PRIMARY KEY (CharacterID, SpellID)
        );

        ALTER TABLE Characters ADD COLUMN SpellSlotsUsed TEXT NOT NULL DEFAULT '0,0,0,0,0,0,0,0,0';

        INSERT INTO Spells (Name, Level, School, Description, CastingTime, RangeText, DurationText, Concentration,
                            Resolution, MeleeAttack, SaveAbility, SaveHalves, DiceCount, DiceSides, FlatBonus,
                            AddsModifier, DamageType, Projectiles, MaxTargets, UpcastDice, UpcastProjectiles,
                            CantripScaling) VALUES
            ('Fire Bolt', 0, 'Evocation', 'You hurl a mote of fire at a creature within range.', 'Action', '120 feet', 'Instantaneous', 0, 'Attack', 0, '', 0, 1, 10, 0, 0, 'fire', 1, 1, 0, 0, 'Dice'),
            ('Ray of Frost', 0, 'Evocation', 'A frigid beam of blue-white light streaks toward a creature. (Its speed reduction isn''t simulated yet.)', 'Action', '60 feet', 'Instantaneous', 0, 'Attack', 0, '', 0, 1, 8, 0, 0, 'cold', 1, 1, 0, 0, 'Dice'),
            ('Chill Touch', 0, 'Necromancy', 'A ghostly, skeletal hand assails a creature with the chill of the grave. (Its no-healing effect isn''t simulated yet.)', 'Action', '120 feet', '1 round', 0, 'Attack', 0, '', 0, 1, 8, 0, 0, 'necrotic', 1, 1, 0, 0, 'Dice'),
            ('Shocking Grasp', 0, 'Evocation', 'Lightning springs from your hand to shock a creature you touch. (Its advantage vs metal armor and no-reactions effect aren''t simulated yet.)', 'Action', 'Touch', 'Instantaneous', 0, 'Attack', 1, '', 0, 1, 8, 0, 0, 'lightning', 1, 1, 0, 0, 'Dice'),
            ('Produce Flame', 0, 'Conjuration', 'A flickering flame appears in your hand, which you can hurl at a creature.', 'Action', 'Self (thrown 30 feet)', '10 minutes', 0, 'Attack', 0, '', 0, 1, 8, 0, 0, 'fire', 1, 1, 0, 0, 'Dice'),
            ('Eldritch Blast', 0, 'Evocation', 'A beam of crackling energy streaks toward a creature. You fire more beams at levels 5, 11, and 17.', 'Action', '120 feet', 'Instantaneous', 0, 'Attack', 0, '', 0, 1, 10, 0, 0, 'force', 1, 1, 0, 0, 'Projectiles'),
            ('Sacred Flame', 0, 'Evocation', 'Flame-like radiance descends on a creature you can see. It gains no benefit from cover.', 'Action', '60 feet', 'Instantaneous', 0, 'Save', 0, 'Dexterity', 0, 1, 8, 0, 0, 'radiant', 1, 1, 0, 0, 'Dice'),
            ('Poison Spray', 0, 'Conjuration', 'You project a puff of noxious gas from your palm.', 'Action', '10 feet', 'Instantaneous', 0, 'Save', 0, 'Constitution', 0, 1, 12, 0, 0, 'poison', 1, 1, 0, 0, 'Dice'),
            ('Vicious Mockery', 0, 'Enchantment', 'You unleash a string of insults laced with subtle enchantments. (The disadvantage on its next attack isn''t simulated yet.)', 'Action', '60 feet', 'Instantaneous', 0, 'Save', 0, 'Wisdom', 0, 1, 4, 0, 0, 'psychic', 1, 1, 0, 0, 'Dice'),
            ('Magic Missile', 1, 'Evocation', 'Three glowing darts of magical force strike creatures of your choice, one or more. Each dart deals 1d4 + 1 force damage. One more dart per slot level above 1st.', 'Action', '120 feet', 'Instantaneous', 0, 'AutoHit', 0, '', 0, 1, 4, 1, 0, 'force', 3, 3, 0, 1, 'Dice'),
            ('Burning Hands', 1, 'Evocation', 'A thin sheet of flames shoots from your fingertips. Hits up to 3 creatures (a 15-foot cone).', 'Action', 'Self (15-foot cone)', 'Instantaneous', 0, 'Save', 0, 'Dexterity', 1, 3, 6, 0, 0, 'fire', 1, 3, 1, 0, 'Dice'),
            ('Thunderwave', 1, 'Evocation', 'A wave of thunderous force sweeps out from you. Hits up to 3 creatures (a 15-foot cube). (The push isn''t simulated yet.)', 'Action', 'Self (15-foot cube)', 'Instantaneous', 0, 'Save', 0, 'Constitution', 1, 2, 8, 0, 0, 'thunder', 1, 3, 1, 0, 'Dice'),
            ('Guiding Bolt', 1, 'Evocation', 'A flash of light streaks toward a creature. (The advantage it grants the next attack isn''t simulated yet.)', 'Action', '120 feet', '1 round', 0, 'Attack', 0, '', 0, 4, 6, 0, 0, 'radiant', 1, 1, 1, 0, 'Dice'),
            ('Inflict Wounds', 1, 'Necromancy', 'A creature you touch is wracked with necrotic energy.', 'Action', 'Touch', 'Instantaneous', 0, 'Attack', 1, '', 0, 3, 10, 0, 0, 'necrotic', 1, 1, 1, 0, 'Dice'),
            ('Dissonant Whispers', 1, 'Enchantment', 'You whisper a discordant melody only one creature can hear. (Its forced movement isn''t simulated yet.)', 'Action', '60 feet', 'Instantaneous', 0, 'Save', 0, 'Wisdom', 1, 3, 6, 0, 0, 'psychic', 1, 1, 1, 0, 'Dice'),
            ('Arms of Hadar', 1, 'Conjuration', 'Tendrils of dark energy erupt from you. Hits up to 4 creatures (a 10-foot radius). (The no-reactions effect isn''t simulated yet.)', 'Action', 'Self (10-foot radius)', 'Instantaneous', 0, 'Save', 0, 'Strength', 1, 2, 6, 0, 0, 'necrotic', 1, 4, 1, 0, 'Dice'),
            ('Cure Wounds', 1, 'Evocation', 'A creature you touch regains 1d8 + your spellcasting modifier hit points.', 'Action', 'Touch', 'Instantaneous', 0, 'Heal', 0, '', 0, 1, 8, 0, 1, '', 1, 1, 1, 0, 'Dice'),
            ('Healing Word', 1, 'Evocation', 'A creature you can see regains 1d4 + your spellcasting modifier hit points. (A bonus action in the rules; cast as an action here for now.)', 'BonusAction', '60 feet', 'Instantaneous', 0, 'Heal', 0, '', 0, 1, 4, 0, 1, '', 1, 1, 1, 0, 'Dice');

        WITH Pairs (ClassName, SpellName) AS (VALUES
            ('Sorcerer', 'Fire Bolt'),
            ('Wizard', 'Fire Bolt'),
            ('Sorcerer', 'Ray of Frost'),
            ('Wizard', 'Ray of Frost'),
            ('Sorcerer', 'Chill Touch'),
            ('Warlock', 'Chill Touch'),
            ('Wizard', 'Chill Touch'),
            ('Sorcerer', 'Shocking Grasp'),
            ('Wizard', 'Shocking Grasp'),
            ('Druid', 'Produce Flame'),
            ('Warlock', 'Eldritch Blast'),
            ('Cleric', 'Sacred Flame'),
            ('Druid', 'Poison Spray'),
            ('Sorcerer', 'Poison Spray'),
            ('Warlock', 'Poison Spray'),
            ('Wizard', 'Poison Spray'),
            ('Bard', 'Vicious Mockery'),
            ('Sorcerer', 'Magic Missile'),
            ('Wizard', 'Magic Missile'),
            ('Sorcerer', 'Burning Hands'),
            ('Wizard', 'Burning Hands'),
            ('Bard', 'Thunderwave'),
            ('Druid', 'Thunderwave'),
            ('Sorcerer', 'Thunderwave'),
            ('Wizard', 'Thunderwave'),
            ('Cleric', 'Guiding Bolt'),
            ('Cleric', 'Inflict Wounds'),
            ('Bard', 'Dissonant Whispers'),
            ('Warlock', 'Arms of Hadar'),
            ('Bard', 'Cure Wounds'),
            ('Cleric', 'Cure Wounds'),
            ('Druid', 'Cure Wounds'),
            ('Paladin', 'Cure Wounds'),
            ('Ranger', 'Cure Wounds'),
            ('Bard', 'Healing Word'),
            ('Cleric', 'Healing Word'),
            ('Druid', 'Healing Word')
        )
        INSERT INTO ClassSpells (ClassID, SpellID)
        SELECT Classes.ClassID, Spells.SpellID FROM Pairs
        JOIN Classes ON Classes.ClassName = Pairs.ClassName
        JOIN Spells ON Spells.Name = Pairs.SpellName;
    )sql"},
    {12, "seed utility spells (no automated effect yet) so every caster can fill its level-1 picks", R"sql(
        INSERT INTO Spells (Name, Level, School, Description, CastingTime, RangeText, DurationText, Concentration,
                            Resolution, DiceCount, DiceSides) VALUES
            ('Light', 0, 'Evocation', 'You touch an object, which sheds bright light in a 20-foot radius for 1 hour. (No automated effect yet.)', 'Action', 'Touch', '1 hour', 0, 'Utility', 0, 0),
            ('Mage Hand', 0, 'Conjuration', 'A spectral, floating hand appears that can manipulate objects up to 10 pounds. (No automated effect yet.)', 'Action', '30 feet', '1 minute', 0, 'Utility', 0, 0),
            ('Mending', 0, 'Transmutation', 'You repair a single break or tear in an object you touch. (No automated effect yet.)', 'Action', 'Touch', 'Instantaneous', 0, 'Utility', 0, 0),
            ('Minor Illusion', 0, 'Illusion', 'You create a sound or an image of an object that lasts for 1 minute. (No automated effect yet.)', 'Action', '30 feet', '1 minute', 0, 'Utility', 0, 0),
            ('Prestidigitation', 0, 'Transmutation', 'A minor magical trick: a harmless sensory effect, lighting a candle, cleaning an object, and so on. (No automated effect yet.)', 'Action', '10 feet', 'Up to 1 hour', 0, 'Utility', 0, 0),
            ('Guidance', 0, 'Divination', 'A willing creature you touch can add 1d4 to one ability check of its choice. (No automated effect yet.)', 'Action', 'Touch', 'Concentration, up to 1 minute', 1, 'Utility', 0, 0),
            ('Thaumaturgy', 0, 'Transmutation', 'You manifest a minor wonder: a booming voice, flickering flames, tremors, and the like. (No automated effect yet.)', 'Action', '30 feet', 'Up to 1 minute', 0, 'Utility', 0, 0),
            ('Spare the Dying', 0, 'Necromancy', 'You touch a living creature that has 0 hit points, and it becomes stable. (No automated effect yet.)', 'Action', 'Touch', 'Instantaneous', 0, 'Utility', 0, 0),
            ('Druidcraft', 0, 'Transmutation', 'You create a tiny, harmless natural effect: predict the weather, make a flower bloom, and so on. (No automated effect yet.)', 'Action', '30 feet', 'Instantaneous', 0, 'Utility', 0, 0),
            ('Detect Magic', 1, 'Divination', 'You sense the presence of magic within 30 feet of you. (Ritual.) (No automated effect yet.)', 'Action', 'Self', 'Concentration, up to 10 minutes', 1, 'Utility', 0, 0),
            ('Charm Person', 1, 'Enchantment', 'A humanoid must make a Wisdom save or be charmed by you for 1 hour. (No automated effect yet.)', 'Action', '30 feet', '1 hour', 0, 'Utility', 0, 0),
            ('Comprehend Languages', 1, 'Divination', 'You understand the literal meaning of any spoken language you hear. (Ritual.) (No automated effect yet.)', 'Action', 'Self', '1 hour', 0, 'Utility', 0, 0),
            ('Feather Fall', 1, 'Transmutation', 'Up to five falling creatures descend slowly and take no falling damage. (No automated effect yet.)', 'Reaction', '60 feet', '1 minute', 0, 'Utility', 0, 0),
            ('Mage Armor', 1, 'Abjuration', 'A creature not wearing armor has a base AC of 13 + its Dexterity modifier for 8 hours. (No automated effect yet.)', 'Action', 'Touch', '8 hours', 0, 'Utility', 0, 0),
            ('Bless', 1, 'Enchantment', 'Up to three creatures add 1d4 to attack rolls and saving throws. (No automated effect yet.)', 'Action', '30 feet', 'Concentration, up to 1 minute', 1, 'Utility', 0, 0),
            ('Command', 1, 'Enchantment', 'You speak a one-word command to a creature, which must make a Wisdom save or obey on its next turn. (No automated effect yet.)', 'Action', '60 feet', '1 round', 0, 'Utility', 0, 0),
            ('Sanctuary', 1, 'Abjuration', 'A creature you touch is warded: attackers must make a Wisdom save or choose a new target. (No automated effect yet.)', 'BonusAction', '30 feet', '1 minute', 0, 'Utility', 0, 0),
            ('Faerie Fire', 1, 'Evocation', 'Creatures in a 20-foot cube are outlined in light; attacks against them have advantage. (No automated effect yet.)', 'Action', '60 feet', 'Concentration, up to 1 minute', 1, 'Utility', 0, 0),
            ('Goodberry', 1, 'Transmutation', 'Up to ten magical berries appear; eating one restores 1 hit point and feeds you for a day. (No automated effect yet.)', 'Action', 'Touch', 'Instantaneous', 0, 'Utility', 0, 0),
            ('Speak with Animals', 1, 'Divination', 'You can comprehend and verbally communicate with beasts for 10 minutes. (Ritual.) (No automated effect yet.)', 'Action', 'Self', '10 minutes', 0, 'Utility', 0, 0),
            ('Divine Favor', 1, 'Evocation', 'Your weapon attacks deal an extra 1d4 radiant damage. (No automated effect yet.)', 'BonusAction', 'Self', 'Concentration, up to 1 minute', 1, 'Utility', 0, 0),
            ('Heroism', 1, 'Enchantment', 'A willing creature is immune to being frightened and gains temporary hit points each turn. (No automated effect yet.)', 'Action', 'Touch', 'Concentration, up to 1 minute', 1, 'Utility', 0, 0),
            ('Protection from Evil and Good', 1, 'Abjuration', 'A willing creature is protected against aberrations, celestials, elementals, fey, fiends, and undead. (No automated effect yet.)', 'Action', 'Touch', 'Concentration, up to 10 minutes', 1, 'Utility', 0, 0),
            ('Shield of Faith', 1, 'Abjuration', 'A shimmering field surrounds a creature, granting it +2 AC. (No automated effect yet.)', 'BonusAction', '60 feet', 'Concentration, up to 10 minutes', 1, 'Utility', 0, 0);

        WITH Pairs (ClassName, SpellName) AS (VALUES
            ('Bard', 'Light'),
            ('Cleric', 'Light'),
            ('Sorcerer', 'Light'),
            ('Wizard', 'Light'),
            ('Bard', 'Mage Hand'),
            ('Sorcerer', 'Mage Hand'),
            ('Warlock', 'Mage Hand'),
            ('Wizard', 'Mage Hand'),
            ('Bard', 'Mending'),
            ('Cleric', 'Mending'),
            ('Druid', 'Mending'),
            ('Sorcerer', 'Mending'),
            ('Wizard', 'Mending'),
            ('Bard', 'Minor Illusion'),
            ('Sorcerer', 'Minor Illusion'),
            ('Warlock', 'Minor Illusion'),
            ('Wizard', 'Minor Illusion'),
            ('Bard', 'Prestidigitation'),
            ('Sorcerer', 'Prestidigitation'),
            ('Warlock', 'Prestidigitation'),
            ('Wizard', 'Prestidigitation'),
            ('Cleric', 'Guidance'),
            ('Druid', 'Guidance'),
            ('Cleric', 'Thaumaturgy'),
            ('Cleric', 'Spare the Dying'),
            ('Druid', 'Druidcraft'),
            ('Bard', 'Detect Magic'),
            ('Cleric', 'Detect Magic'),
            ('Druid', 'Detect Magic'),
            ('Paladin', 'Detect Magic'),
            ('Ranger', 'Detect Magic'),
            ('Sorcerer', 'Detect Magic'),
            ('Wizard', 'Detect Magic'),
            ('Bard', 'Charm Person'),
            ('Druid', 'Charm Person'),
            ('Sorcerer', 'Charm Person'),
            ('Warlock', 'Charm Person'),
            ('Wizard', 'Charm Person'),
            ('Bard', 'Comprehend Languages'),
            ('Sorcerer', 'Comprehend Languages'),
            ('Warlock', 'Comprehend Languages'),
            ('Wizard', 'Comprehend Languages'),
            ('Bard', 'Feather Fall'),
            ('Sorcerer', 'Feather Fall'),
            ('Wizard', 'Feather Fall'),
            ('Sorcerer', 'Mage Armor'),
            ('Wizard', 'Mage Armor'),
            ('Cleric', 'Bless'),
            ('Paladin', 'Bless'),
            ('Cleric', 'Command'),
            ('Paladin', 'Command'),
            ('Cleric', 'Sanctuary'),
            ('Bard', 'Faerie Fire'),
            ('Druid', 'Faerie Fire'),
            ('Druid', 'Goodberry'),
            ('Ranger', 'Goodberry'),
            ('Bard', 'Speak with Animals'),
            ('Druid', 'Speak with Animals'),
            ('Ranger', 'Speak with Animals'),
            ('Paladin', 'Divine Favor'),
            ('Bard', 'Heroism'),
            ('Paladin', 'Heroism'),
            ('Cleric', 'Protection from Evil and Good'),
            ('Paladin', 'Protection from Evil and Good'),
            ('Warlock', 'Protection from Evil and Good'),
            ('Wizard', 'Protection from Evil and Good'),
            ('Cleric', 'Shield of Faith'),
            ('Paladin', 'Shield of Faith')
        )
        INSERT INTO ClassSpells (ClassID, SpellID)
        SELECT Classes.ClassID, Spells.SpellID FROM Pairs
        JOIN Classes ON Classes.ClassName = Pairs.ClassName
        JOIN Spells ON Spells.Name = Pairs.SpellName;
    )sql"},
    {13, "add Dead to Characters", R"sql(
        ALTER TABLE Characters ADD COLUMN Dead INTEGER NOT NULL DEFAULT 0;
    )sql"},
    {14, "add SpellEffects (seeded), Buff and Stabilize spells, and monster on-hit effects", R"sql(
        CREATE TABLE IF NOT EXISTS SpellEffects (
            SpellEffectID INTEGER PRIMARY KEY AUTOINCREMENT,
            SpellID INTEGER NOT NULL REFERENCES Spells(SpellID),
            EffectTrigger TEXT NOT NULL,
            Kind TEXT NOT NULL,
            ConditionName TEXT NOT NULL DEFAULT '',
            Amount INTEGER NOT NULL DEFAULT 0,
            Duration TEXT NOT NULL,
            Rounds INTEGER NOT NULL DEFAULT 0,
            UntilUsed INTEGER NOT NULL DEFAULT 0,
            Label TEXT NOT NULL DEFAULT ''
        );

        ALTER TABLE Spells ADD COLUMN UpcastTargets INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Spells ADD COLUMN CombatSaveAdvantage INTEGER NOT NULL DEFAULT 0;

        UPDATE Spells SET Resolution = 'Buff', MaxTargets = 3, UpcastTargets = 1 WHERE Name = 'Bless';
        UPDATE Spells SET Resolution = 'Buff', MaxTargets = 1 WHERE Name = 'Shield of Faith';
        UPDATE Spells SET Resolution = 'Buff', MaxTargets = 1 WHERE Name = 'Divine Favor';
        UPDATE Spells SET Resolution = 'Save', SaveAbility = 'Dexterity', DiceCount = 0, DiceSides = 1,
                          MaxTargets = 4 WHERE Name = 'Faerie Fire';
        UPDATE Spells SET Resolution = 'Save', SaveAbility = 'Wisdom', DiceCount = 0, DiceSides = 1,
                          MaxTargets = 1, CombatSaveAdvantage = 1 WHERE Name = 'Charm Person';
        UPDATE Spells SET Resolution = 'Stabilize', DiceCount = 0, DiceSides = 1 WHERE Name = 'Spare the Dying';
        UPDATE Spells SET Description = 'Up to three creatures add 1d4 to their attack rolls and saving throws for up to 1 minute. One more creature per slot level above 1st.' WHERE Name = 'Bless';
        UPDATE Spells SET Description = 'A creature gains +2 AC for up to 10 minutes. (A bonus action in the rules; cast as an action here for now.)' WHERE Name = 'Shield of Faith';
        UPDATE Spells SET Description = 'Your weapon attacks deal an extra 1d4 radiant damage on a hit for up to 1 minute. (A bonus action in the rules; cast as an action here for now.)' WHERE Name = 'Divine Favor';
        UPDATE Spells SET Description = 'Each creature in a 20-foot cube makes a DEX save or is outlined in light: attacks against it have advantage for up to 1 minute. Affects up to 4 creatures.' WHERE Name = 'Faerie Fire';
        UPDATE Spells SET Description = 'A humanoid makes a WIS save, with advantage because you''re fighting it, or is charmed by you for 1 hour: it won''t attack you. Harming it ends the charm. (Creature types aren''t modeled yet, so any monster can be targeted.)' WHERE Name = 'Charm Person';
        UPDATE Spells SET Description = 'You touch a living creature that has 0 hit points, and it becomes stable.' WHERE Name = 'Spare the Dying';
        UPDATE Spells SET Description = 'A frigid beam of blue-white light streaks toward a creature. On a hit, its speed drops by 10 feet until the start of your next turn (shown only; speed isn''t tracked).' WHERE Name = 'Ray of Frost';
        UPDATE Spells SET Description = 'A ghostly, skeletal hand assails a creature with the chill of the grave. On a hit, it can''t regain hit points until the start of your next turn.' WHERE Name = 'Chill Touch';
        UPDATE Spells SET Description = 'A flash of light streaks toward a creature. On a hit, the next attack roll against it before the end of your next turn has advantage.' WHERE Name = 'Guiding Bolt';
        UPDATE Spells SET Description = 'You unleash a string of insults laced with subtle enchantments. On a failed save, it has disadvantage on its next attack roll before the end of its next turn.' WHERE Name = 'Vicious Mockery';

        INSERT INTO SpellEffects (SpellID, EffectTrigger, Kind, ConditionName, Amount, Duration, Rounds, UntilUsed, Label)
            SELECT SpellID, 'OnCastTarget', 'AttackBonusDice', '', 4, 'Rounds', 10, 0, 'Blessed' FROM Spells WHERE Name = 'Bless';
        INSERT INTO SpellEffects (SpellID, EffectTrigger, Kind, ConditionName, Amount, Duration, Rounds, UntilUsed, Label)
            SELECT SpellID, 'OnCastTarget', 'SaveBonusDice', '', 4, 'Rounds', 10, 0, 'Blessed' FROM Spells WHERE Name = 'Bless';
        INSERT INTO SpellEffects (SpellID, EffectTrigger, Kind, ConditionName, Amount, Duration, Rounds, UntilUsed, Label)
            SELECT SpellID, 'OnCastTarget', 'ArmorClassBonus', '', 2, 'Rounds', 100, 0, 'Shield of Faith (+2 AC)' FROM Spells WHERE Name = 'Shield of Faith';
        INSERT INTO SpellEffects (SpellID, EffectTrigger, Kind, ConditionName, Amount, Duration, Rounds, UntilUsed, Label)
            SELECT SpellID, 'OnCastSelf', 'ExtraWeaponDamageDice', '', 4, 'Rounds', 10, 0, 'Divine Favor' FROM Spells WHERE Name = 'Divine Favor';
        INSERT INTO SpellEffects (SpellID, EffectTrigger, Kind, ConditionName, Amount, Duration, Rounds, UntilUsed, Label)
            SELECT SpellID, 'OnHit', 'AdvantageOnNextAttackAgainst', '', 0, 'UntilEndOfCasterTurn', 0, 1, 'Guiding Bolt' FROM Spells WHERE Name = 'Guiding Bolt';
        INSERT INTO SpellEffects (SpellID, EffectTrigger, Kind, ConditionName, Amount, Duration, Rounds, UntilUsed, Label)
            SELECT SpellID, 'OnHit', 'SpeedReduction', '', 10, 'UntilStartOfCasterTurn', 0, 0, 'Ray of Frost (-10 ft)' FROM Spells WHERE Name = 'Ray of Frost';
        INSERT INTO SpellEffects (SpellID, EffectTrigger, Kind, ConditionName, Amount, Duration, Rounds, UntilUsed, Label)
            SELECT SpellID, 'OnHit', 'CantRegainHitPoints', '', 0, 'UntilStartOfCasterTurn', 0, 0, 'Chill Touch' FROM Spells WHERE Name = 'Chill Touch';
        INSERT INTO SpellEffects (SpellID, EffectTrigger, Kind, ConditionName, Amount, Duration, Rounds, UntilUsed, Label)
            SELECT SpellID, 'OnFailedSave', 'DisadvantageOnNextAttack', '', 0, 'UntilEndOfTargetTurn', 0, 1, 'Vicious Mockery' FROM Spells WHERE Name = 'Vicious Mockery';
        INSERT INTO SpellEffects (SpellID, EffectTrigger, Kind, ConditionName, Amount, Duration, Rounds, UntilUsed, Label)
            SELECT SpellID, 'OnFailedSave', 'AdvantageOnAttacksAgainst', '', 0, 'Rounds', 10, 0, 'Faerie Fire' FROM Spells WHERE Name = 'Faerie Fire';
        INSERT INTO SpellEffects (SpellID, EffectTrigger, Kind, ConditionName, Amount, Duration, Rounds, UntilUsed, Label)
            SELECT SpellID, 'OnFailedSave', 'Condition', 'Charmed', 0, 'Rounds', 600, 0, 'Charmed' FROM Spells WHERE Name = 'Charm Person';

        ALTER TABLE Monsters ADD COLUMN OnHitSaveAbility TEXT NOT NULL DEFAULT '';
        ALTER TABLE Monsters ADD COLUMN OnHitSaveDC INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Monsters ADD COLUMN OnHitCondition TEXT NOT NULL DEFAULT '';
        UPDATE Monsters SET OnHitSaveAbility = 'Strength', OnHitSaveDC = 11, OnHitCondition = 'Prone' WHERE Name = 'Wolf';
    )sql"},
    {15, "add weapons, armor, and shields as items (WeaponStats, ArmorStats), and equipped inventory", R"sql(
        CREATE TABLE IF NOT EXISTS WeaponStats (
            ItemID INTEGER PRIMARY KEY REFERENCES Items(ItemID),
            WeaponCategory TEXT NOT NULL,
            DamageDiceCount INTEGER NOT NULL,
            DamageDiceSides INTEGER NOT NULL,
            DamageType TEXT NOT NULL,
            VersatileDiceSides INTEGER NOT NULL DEFAULT 0,
            Finesse INTEGER NOT NULL DEFAULT 0,
            Light INTEGER NOT NULL DEFAULT 0,
            Heavy INTEGER NOT NULL DEFAULT 0,
            TwoHanded INTEGER NOT NULL DEFAULT 0,
            Ranged INTEGER NOT NULL DEFAULT 0,
            Thrown INTEGER NOT NULL DEFAULT 0,
            Reach INTEGER NOT NULL DEFAULT 0,
            Loading INTEGER NOT NULL DEFAULT 0,
            Ammunition INTEGER NOT NULL DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS ArmorStats (
            ItemID INTEGER PRIMARY KEY REFERENCES Items(ItemID),
            ArmorType TEXT NOT NULL,
            BaseAC INTEGER NOT NULL,
            MaxDexBonus INTEGER NOT NULL DEFAULT -1,
            StrengthRequirement INTEGER NOT NULL DEFAULT 0,
            StealthDisadvantage INTEGER NOT NULL DEFAULT 0
        );

        ALTER TABLE CharacterInventory ADD COLUMN Equipped INTEGER NOT NULL DEFAULT 0;

        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Dagger', 'Weapon', 'Simple melee weapon: 1d4 piercing. Finesse, light, thrown.', 1, 200);
        INSERT INTO WeaponStats (ItemID, WeaponCategory, DamageDiceCount, DamageDiceSides, DamageType, VersatileDiceSides,
                                 Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Reach, Loading, Ammunition)
            SELECT ItemID, 'Simple', 1, 4, 'piercing', 0, 1, 1, 0, 0, 0, 1, 0, 0, 0 FROM Items WHERE Name = 'Dagger';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Handaxe', 'Weapon', 'Simple melee weapon: 1d6 slashing. Light, thrown.', 2, 500);
        INSERT INTO WeaponStats (ItemID, WeaponCategory, DamageDiceCount, DamageDiceSides, DamageType, VersatileDiceSides,
                                 Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Reach, Loading, Ammunition)
            SELECT ItemID, 'Simple', 1, 6, 'slashing', 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 FROM Items WHERE Name = 'Handaxe';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Javelin', 'Weapon', 'Simple melee weapon: 1d6 piercing. Thrown.', 2, 50);
        INSERT INTO WeaponStats (ItemID, WeaponCategory, DamageDiceCount, DamageDiceSides, DamageType, VersatileDiceSides,
                                 Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Reach, Loading, Ammunition)
            SELECT ItemID, 'Simple', 1, 6, 'piercing', 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 FROM Items WHERE Name = 'Javelin';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Mace', 'Weapon', 'Simple melee weapon: 1d6 bludgeoning.', 4, 500);
        INSERT INTO WeaponStats (ItemID, WeaponCategory, DamageDiceCount, DamageDiceSides, DamageType, VersatileDiceSides,
                                 Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Reach, Loading, Ammunition)
            SELECT ItemID, 'Simple', 1, 6, 'bludgeoning', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 FROM Items WHERE Name = 'Mace';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Quarterstaff', 'Weapon', 'Simple melee weapon: 1d6 bludgeoning. Versatile (1d8).', 4, 20);
        INSERT INTO WeaponStats (ItemID, WeaponCategory, DamageDiceCount, DamageDiceSides, DamageType, VersatileDiceSides,
                                 Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Reach, Loading, Ammunition)
            SELECT ItemID, 'Simple', 1, 6, 'bludgeoning', 8, 0, 0, 0, 0, 0, 0, 0, 0, 0 FROM Items WHERE Name = 'Quarterstaff';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Light Crossbow', 'Weapon', 'Simple ranged weapon: 1d8 piercing. Ammunition, loading, two-handed.', 5, 2500);
        INSERT INTO WeaponStats (ItemID, WeaponCategory, DamageDiceCount, DamageDiceSides, DamageType, VersatileDiceSides,
                                 Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Reach, Loading, Ammunition)
            SELECT ItemID, 'Simple', 1, 8, 'piercing', 0, 0, 0, 0, 1, 1, 0, 0, 1, 1 FROM Items WHERE Name = 'Light Crossbow';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Shortbow', 'Weapon', 'Simple ranged weapon: 1d6 piercing. Ammunition, two-handed.', 2, 2500);
        INSERT INTO WeaponStats (ItemID, WeaponCategory, DamageDiceCount, DamageDiceSides, DamageType, VersatileDiceSides,
                                 Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Reach, Loading, Ammunition)
            SELECT ItemID, 'Simple', 1, 6, 'piercing', 0, 0, 0, 0, 1, 1, 0, 0, 0, 1 FROM Items WHERE Name = 'Shortbow';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Battleaxe', 'Weapon', 'Martial melee weapon: 1d8 slashing. Versatile (1d10).', 4, 1000);
        INSERT INTO WeaponStats (ItemID, WeaponCategory, DamageDiceCount, DamageDiceSides, DamageType, VersatileDiceSides,
                                 Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Reach, Loading, Ammunition)
            SELECT ItemID, 'Martial', 1, 8, 'slashing', 10, 0, 0, 0, 0, 0, 0, 0, 0, 0 FROM Items WHERE Name = 'Battleaxe';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Greataxe', 'Weapon', 'Martial melee weapon: 1d12 slashing. Heavy, two-handed.', 7, 3000);
        INSERT INTO WeaponStats (ItemID, WeaponCategory, DamageDiceCount, DamageDiceSides, DamageType, VersatileDiceSides,
                                 Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Reach, Loading, Ammunition)
            SELECT ItemID, 'Martial', 1, 12, 'slashing', 0, 0, 0, 1, 1, 0, 0, 0, 0, 0 FROM Items WHERE Name = 'Greataxe';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Greatsword', 'Weapon', 'Martial melee weapon: 2d6 slashing. Heavy, two-handed.', 6, 5000);
        INSERT INTO WeaponStats (ItemID, WeaponCategory, DamageDiceCount, DamageDiceSides, DamageType, VersatileDiceSides,
                                 Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Reach, Loading, Ammunition)
            SELECT ItemID, 'Martial', 2, 6, 'slashing', 0, 0, 0, 1, 1, 0, 0, 0, 0, 0 FROM Items WHERE Name = 'Greatsword';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Longsword', 'Weapon', 'Martial melee weapon: 1d8 slashing. Versatile (1d10).', 3, 1500);
        INSERT INTO WeaponStats (ItemID, WeaponCategory, DamageDiceCount, DamageDiceSides, DamageType, VersatileDiceSides,
                                 Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Reach, Loading, Ammunition)
            SELECT ItemID, 'Martial', 1, 8, 'slashing', 10, 0, 0, 0, 0, 0, 0, 0, 0, 0 FROM Items WHERE Name = 'Longsword';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Rapier', 'Weapon', 'Martial melee weapon: 1d8 piercing. Finesse.', 2, 2500);
        INSERT INTO WeaponStats (ItemID, WeaponCategory, DamageDiceCount, DamageDiceSides, DamageType, VersatileDiceSides,
                                 Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Reach, Loading, Ammunition)
            SELECT ItemID, 'Martial', 1, 8, 'piercing', 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 FROM Items WHERE Name = 'Rapier';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Scimitar', 'Weapon', 'Martial melee weapon: 1d6 slashing. Finesse, light.', 3, 2500);
        INSERT INTO WeaponStats (ItemID, WeaponCategory, DamageDiceCount, DamageDiceSides, DamageType, VersatileDiceSides,
                                 Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Reach, Loading, Ammunition)
            SELECT ItemID, 'Martial', 1, 6, 'slashing', 0, 1, 1, 0, 0, 0, 0, 0, 0, 0 FROM Items WHERE Name = 'Scimitar';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Shortsword', 'Weapon', 'Martial melee weapon: 1d6 piercing. Finesse, light.', 2, 1000);
        INSERT INTO WeaponStats (ItemID, WeaponCategory, DamageDiceCount, DamageDiceSides, DamageType, VersatileDiceSides,
                                 Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Reach, Loading, Ammunition)
            SELECT ItemID, 'Martial', 1, 6, 'piercing', 0, 1, 1, 0, 0, 0, 0, 0, 0, 0 FROM Items WHERE Name = 'Shortsword';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Warhammer', 'Weapon', 'Martial melee weapon: 1d8 bludgeoning. Versatile (1d10).', 2, 1500);
        INSERT INTO WeaponStats (ItemID, WeaponCategory, DamageDiceCount, DamageDiceSides, DamageType, VersatileDiceSides,
                                 Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Reach, Loading, Ammunition)
            SELECT ItemID, 'Martial', 1, 8, 'bludgeoning', 10, 0, 0, 0, 0, 0, 0, 0, 0, 0 FROM Items WHERE Name = 'Warhammer';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Longbow', 'Weapon', 'Martial ranged weapon: 1d8 piercing. Ammunition, heavy, two-handed.', 2, 5000);
        INSERT INTO WeaponStats (ItemID, WeaponCategory, DamageDiceCount, DamageDiceSides, DamageType, VersatileDiceSides,
                                 Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Reach, Loading, Ammunition)
            SELECT ItemID, 'Martial', 1, 8, 'piercing', 0, 0, 0, 1, 1, 1, 0, 0, 0, 1 FROM Items WHERE Name = 'Longbow';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Padded Armor', 'Armor', 'Light armor: AC 11 + DEX. Disadvantage on Stealth checks.', 8, 500);
        INSERT INTO ArmorStats (ItemID, ArmorType, BaseAC, MaxDexBonus, StrengthRequirement, StealthDisadvantage)
            SELECT ItemID, 'Light', 11, -1, 0, 1 FROM Items WHERE Name = 'Padded Armor';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Leather Armor', 'Armor', 'Light armor: AC 11 + DEX.', 10, 1000);
        INSERT INTO ArmorStats (ItemID, ArmorType, BaseAC, MaxDexBonus, StrengthRequirement, StealthDisadvantage)
            SELECT ItemID, 'Light', 11, -1, 0, 0 FROM Items WHERE Name = 'Leather Armor';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Studded Leather Armor', 'Armor', 'Light armor: AC 12 + DEX.', 13, 4500);
        INSERT INTO ArmorStats (ItemID, ArmorType, BaseAC, MaxDexBonus, StrengthRequirement, StealthDisadvantage)
            SELECT ItemID, 'Light', 12, -1, 0, 0 FROM Items WHERE Name = 'Studded Leather Armor';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Hide Armor', 'Armor', 'Medium armor: AC 12 + DEX (max 2).', 12, 1000);
        INSERT INTO ArmorStats (ItemID, ArmorType, BaseAC, MaxDexBonus, StrengthRequirement, StealthDisadvantage)
            SELECT ItemID, 'Medium', 12, 2, 0, 0 FROM Items WHERE Name = 'Hide Armor';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Chain Shirt', 'Armor', 'Medium armor: AC 13 + DEX (max 2).', 20, 5000);
        INSERT INTO ArmorStats (ItemID, ArmorType, BaseAC, MaxDexBonus, StrengthRequirement, StealthDisadvantage)
            SELECT ItemID, 'Medium', 13, 2, 0, 0 FROM Items WHERE Name = 'Chain Shirt';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Scale Mail', 'Armor', 'Medium armor: AC 14 + DEX (max 2). Disadvantage on Stealth checks.', 45, 5000);
        INSERT INTO ArmorStats (ItemID, ArmorType, BaseAC, MaxDexBonus, StrengthRequirement, StealthDisadvantage)
            SELECT ItemID, 'Medium', 14, 2, 0, 1 FROM Items WHERE Name = 'Scale Mail';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Breastplate', 'Armor', 'Medium armor: AC 14 + DEX (max 2).', 20, 40000);
        INSERT INTO ArmorStats (ItemID, ArmorType, BaseAC, MaxDexBonus, StrengthRequirement, StealthDisadvantage)
            SELECT ItemID, 'Medium', 14, 2, 0, 0 FROM Items WHERE Name = 'Breastplate';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Half Plate', 'Armor', 'Medium armor: AC 15 + DEX (max 2). Disadvantage on Stealth checks.', 40, 75000);
        INSERT INTO ArmorStats (ItemID, ArmorType, BaseAC, MaxDexBonus, StrengthRequirement, StealthDisadvantage)
            SELECT ItemID, 'Medium', 15, 2, 0, 1 FROM Items WHERE Name = 'Half Plate';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Ring Mail', 'Armor', 'Heavy armor: AC 14. Disadvantage on Stealth checks.', 40, 3000);
        INSERT INTO ArmorStats (ItemID, ArmorType, BaseAC, MaxDexBonus, StrengthRequirement, StealthDisadvantage)
            SELECT ItemID, 'Heavy', 14, 0, 0, 1 FROM Items WHERE Name = 'Ring Mail';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Chain Mail', 'Armor', 'Heavy armor: AC 16. Needs STR 13 (otherwise speed -10 ft); disadvantage on Stealth checks.', 55, 7500);
        INSERT INTO ArmorStats (ItemID, ArmorType, BaseAC, MaxDexBonus, StrengthRequirement, StealthDisadvantage)
            SELECT ItemID, 'Heavy', 16, 0, 13, 1 FROM Items WHERE Name = 'Chain Mail';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Splint Armor', 'Armor', 'Heavy armor: AC 17. Needs STR 15 (otherwise speed -10 ft); disadvantage on Stealth checks.', 60, 20000);
        INSERT INTO ArmorStats (ItemID, ArmorType, BaseAC, MaxDexBonus, StrengthRequirement, StealthDisadvantage)
            SELECT ItemID, 'Heavy', 17, 0, 15, 1 FROM Items WHERE Name = 'Splint Armor';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Plate Armor', 'Armor', 'Heavy armor: AC 18. Needs STR 15 (otherwise speed -10 ft); disadvantage on Stealth checks.', 65, 150000);
        INSERT INTO ArmorStats (ItemID, ArmorType, BaseAC, MaxDexBonus, StrengthRequirement, StealthDisadvantage)
            SELECT ItemID, 'Heavy', 18, 0, 15, 1 FROM Items WHERE Name = 'Plate Armor';
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES ('Shield', 'Shield', 'A shield: +2 AC while carried. Wielding one uses a hand.', 6, 1000);
        INSERT INTO ArmorStats (ItemID, ArmorType, BaseAC, MaxDexBonus, StrengthRequirement, StealthDisadvantage)
            SELECT ItemID, 'Shield', 2, -1, 0, 0 FROM Items WHERE Name = 'Shield';

        -- Each character's old WeaponID becomes an equipped inventory item. (The Weapons
        -- table and Characters.WeaponID stay but are no longer used.)
        INSERT INTO CharacterInventory (CharacterID, ItemID, Quantity, Equipped)
            SELECT Characters.CharacterId, Items.ItemID, 1, 1 FROM Characters
            JOIN Weapons ON Weapons.WeaponID = Characters.WeaponID
            JOIN Items ON Items.Name = Weapons.Name AND Items.Category = 'Weapon'
            WHERE 1
            ON CONFLICT (CharacterID, ItemID) DO UPDATE SET Equipped = 1;
    )sql"},
    {16, "add FeatureUses (spent class feature uses) to Characters", R"sql(
        ALTER TABLE Characters ADD COLUMN FeatureUses TEXT NOT NULL DEFAULT '';
    )sql"},
    {17, "add FightingStyle to Characters", R"sql(
        ALTER TABLE Characters ADD COLUMN FightingStyle TEXT NOT NULL DEFAULT '';
    )sql"},
    {18, "creature types, damage traits, and MonsterAttacks", R"sql(
        ALTER TABLE Monsters ADD COLUMN CreatureType TEXT NOT NULL DEFAULT '';
        ALTER TABLE Monsters ADD COLUMN DamageResistances TEXT NOT NULL DEFAULT '';
        ALTER TABLE Monsters ADD COLUMN DamageVulnerabilities TEXT NOT NULL DEFAULT '';
        ALTER TABLE Monsters ADD COLUMN DamageImmunities TEXT NOT NULL DEFAULT '';
        ALTER TABLE Monsters ADD COLUMN ConditionImmunities TEXT NOT NULL DEFAULT '';
        ALTER TABLE Monsters ADD COLUMN Multiattack TEXT NOT NULL DEFAULT '';

        -- Every attack a stat block makes, in order. Replaces the single-attack
        -- columns on Monsters (AttackName ... OnHitCondition), which stay but are
        -- no longer read.
        CREATE TABLE MonsterAttacks (
            MonsterAttackID  INTEGER PRIMARY KEY AUTOINCREMENT,
            MonsterID        INTEGER NOT NULL REFERENCES Monsters(MonsterID) ON DELETE CASCADE,
            Position         INTEGER NOT NULL DEFAULT 1,
            Name             TEXT NOT NULL,
            AttackBonus      INTEGER NOT NULL DEFAULT 0,
            DamageDiceCount  INTEGER NOT NULL DEFAULT 1,
            DamageDiceSides  INTEGER NOT NULL DEFAULT 4,
            DamageBonus      INTEGER NOT NULL DEFAULT 0,
            DamageType       TEXT NOT NULL DEFAULT '',
            Ranged           INTEGER NOT NULL DEFAULT 0,
            ExtraDiceCount   INTEGER NOT NULL DEFAULT 0,
            ExtraDiceSides   INTEGER NOT NULL DEFAULT 0,
            ExtraDamageType  TEXT NOT NULL DEFAULT '',
            OnHitSaveAbility TEXT NOT NULL DEFAULT '',
            OnHitSaveDC      INTEGER NOT NULL DEFAULT 0,
            OnHitCondition   TEXT NOT NULL DEFAULT '',
            Count            INTEGER NOT NULL DEFAULT 1
        );
        INSERT INTO MonsterAttacks (MonsterID, Position, Name, AttackBonus, DamageDiceCount, DamageDiceSides,
                                    DamageBonus, OnHitSaveAbility, OnHitSaveDC, OnHitCondition)
            SELECT MonsterID, 1, AttackName, AttackBonus, DamageDiceCount, DamageDiceSides, DamageBonus,
                   OnHitSaveAbility, OnHitSaveDC, OnHitCondition
            FROM Monsters WHERE AttackName <> '';

        -- SRD 5.1 damage types for the seeded attacks.
        UPDATE MonsterAttacks SET DamageType = 'piercing'
            WHERE Name IN ('Dagger', 'Shortsword', 'Bite');
        UPDATE MonsterAttacks SET DamageType = 'slashing' WHERE Name IN ('Scimitar', 'Greataxe');
        UPDATE MonsterAttacks SET DamageType = 'bludgeoning' WHERE Name = 'Slam';

        -- SRD 5.1 creature types and traits.
        UPDATE Monsters SET CreatureType = 'Humanoid' WHERE Name IN ('Kobold', 'Goblin', 'Orc');
        UPDATE Monsters SET CreatureType = 'Beast' WHERE Name = 'Wolf';
        UPDATE Monsters SET CreatureType = 'Undead', DamageVulnerabilities = 'bludgeoning',
                            DamageImmunities = 'poison', ConditionImmunities = 'Poisoned'
            WHERE Name = 'Skeleton';
        UPDATE Monsters SET CreatureType = 'Undead', DamageImmunities = 'poison', ConditionImmunities = 'Poisoned'
            WHERE Name = 'Zombie';

        -- Spells limited to a creature type (Charm Person: humanoids).
        ALTER TABLE Spells ADD COLUMN TargetCreatureType TEXT NOT NULL DEFAULT '';
        UPDATE Spells SET TargetCreatureType = 'Humanoid',
            Description = 'A humanoid makes a WIS save, with advantage because you''re fighting it, or is charmed by you for 1 hour: it won''t attack you. Harming it ends the charm.'
            WHERE Name = 'Charm Person';
    )sql"},
    {19, "Multiattack roster: 13 SRD monsters (CR 1/2-5), monster traits", R"sql(
        -- Map-free traits the engine applies: PackTactics, MartialAdvantage,
        -- UndeadFortitude, Regeneration (comma list).
        ALTER TABLE Monsters ADD COLUMN Traits TEXT NOT NULL DEFAULT '';
        -- An on-hit condition can last OnHitRounds rounds (0 = until removed),
        -- with a save to end it at the end of each of the target's turns.
        ALTER TABLE MonsterAttacks ADD COLUMN OnHitRounds INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE MonsterAttacks ADD COLUMN OnHitRepeatSave INTEGER NOT NULL DEFAULT 0;

        UPDATE Monsters SET Traits = 'PackTactics' WHERE Name IN ('Kobold', 'Wolf');
        UPDATE Monsters SET Traits = 'UndeadFortitude' WHERE Name = 'Zombie';

        -- SRD 5.1 stat blocks.
        INSERT INTO Monsters (Name, ArmorClass, HitPoints, Strength, Dexterity, Constitution, Intelligence, Wisdom,
                              Charisma, ChallengeRating, CreatureType, DamageImmunities, ConditionImmunities,
                              Multiattack, Traits) VALUES
            ('Hobgoblin',      18, 11,  13, 12, 12, 10, 10, 9, 0.5, 'Humanoid',    '',       '',                 '', 'MartialAdvantage'),
            ('Gnoll',          15, 22,  14, 12, 11, 6,  10, 7, 0.5, 'Humanoid',    '',       '',                 '', ''),
            ('Bugbear',        16, 27,  15, 14, 13, 8,  11, 9, 1,   'Humanoid',    '',       '',                 '', ''),
            ('Dire Wolf',      14, 37,  17, 15, 15, 3,  12, 7, 1,   'Beast',       '',       '',                 '', 'PackTactics'),
            ('Ghoul',          12, 22,  13, 15, 10, 7,  10, 6, 1,   'Undead',      'poison', 'Charmed,Poisoned', '', ''),
            ('Ogre',           11, 59,  19, 8,  16, 5,  7,  7, 2,   'Giant',       '',       '',                 '', ''),
            ('Bandit Captain', 15, 65,  15, 16, 14, 14, 11, 14, 2,  'Humanoid',    '',       '',                 'Two scimitar attacks and a dagger attack', ''),
            ('Owlbear',        13, 59,  20, 12, 17, 3,  12, 7, 3,   'Monstrosity', '',       '',                 'A beak attack and a claws attack', ''),
            ('Hell Hound',     15, 45,  17, 12, 14, 6,  13, 6, 3,   'Fiend',       'fire',   '',                 '', 'PackTactics'),
            ('Minotaur',       14, 76,  18, 11, 16, 6,  16, 9, 3,   'Monstrosity', '',       '',                 '', ''),
            ('Ettin',          12, 85,  21, 8,  17, 6,  10, 8, 4,   'Giant',       '',       '',                 'A battleaxe attack and a morningstar attack', ''),
            ('Hill Giant',     13, 105, 21, 8,  19, 5,  9,  6, 5,   'Giant',       '',       '',                 'Two greatclub attacks', ''),
            ('Troll',          15, 84,  18, 13, 20, 7,  9,  7, 5,   'Giant',       '',       '',                 'A bite and two claw attacks', 'Regeneration');

        -- Attacks, in the order they're made. Without a Multiattack, only the
        -- first is used (a Ghoul claws rather than bites: the claws paralyze).
        INSERT INTO MonsterAttacks (MonsterID, Position, Name, AttackBonus, DamageDiceCount, DamageDiceSides,
                                    DamageBonus, DamageType, ExtraDiceCount, ExtraDiceSides, ExtraDamageType,
                                    OnHitSaveAbility, OnHitSaveDC, OnHitCondition, OnHitRounds, OnHitRepeatSave, Count)
        SELECT MonsterID, a.Position, a.Name, a.Bonus, a.DiceCount, a.DiceSides, a.DamageBonus, a.Type,
               a.ExtraCount, a.ExtraSides, a.ExtraType, a.SaveAbility, a.SaveDC, a.Condition, a.Rounds, a.Repeat,
               a.Count
        FROM Monsters JOIN (
            SELECT 'Hobgoblin' AS Monster, 1 AS Position, 'Longsword' AS Name, 3 AS Bonus, 1 AS DiceCount,
                   8 AS DiceSides, 1 AS DamageBonus, 'slashing' AS Type, 0 AS ExtraCount, 0 AS ExtraSides,
                   '' AS ExtraType, '' AS SaveAbility, 0 AS SaveDC, '' AS Condition, 0 AS Rounds, 0 AS Repeat,
                   1 AS Count
            UNION ALL SELECT 'Gnoll',          1, 'Spear',       4, 1, 6,  2, 'piercing',    0, 0, '',     '',             0,  '',          0,  0, 1
            UNION ALL SELECT 'Gnoll',          2, 'Bite',        4, 1, 4,  2, 'piercing',    0, 0, '',     '',             0,  '',          0,  0, 1
            UNION ALL SELECT 'Bugbear',        1, 'Morningstar', 4, 2, 8,  2, 'piercing',    0, 0, '',     '',             0,  '',          0,  0, 1
            UNION ALL SELECT 'Dire Wolf',      1, 'Bite',        5, 2, 6,  3, 'piercing',    0, 0, '',     'Strength',     13, 'Prone',     0,  0, 1
            UNION ALL SELECT 'Ghoul',          1, 'Claws',       4, 2, 4,  2, 'slashing',    0, 0, '',     'Constitution', 10, 'Paralyzed', 10, 1, 1
            UNION ALL SELECT 'Ghoul',          2, 'Bite',        2, 2, 6,  2, 'piercing',    0, 0, '',     '',             0,  '',          0,  0, 1
            UNION ALL SELECT 'Ogre',           1, 'Greatclub',   6, 2, 8,  4, 'bludgeoning', 0, 0, '',     '',             0,  '',          0,  0, 1
            UNION ALL SELECT 'Bandit Captain', 1, 'Scimitar',    5, 1, 6,  3, 'slashing',    0, 0, '',     '',             0,  '',          0,  0, 2
            UNION ALL SELECT 'Bandit Captain', 2, 'Dagger',      5, 1, 4,  3, 'piercing',    0, 0, '',     '',             0,  '',          0,  0, 1
            UNION ALL SELECT 'Owlbear',        1, 'Beak',        7, 1, 10, 5, 'piercing',    0, 0, '',     '',             0,  '',          0,  0, 1
            UNION ALL SELECT 'Owlbear',        2, 'Claws',       7, 2, 8,  5, 'slashing',    0, 0, '',     '',             0,  '',          0,  0, 1
            UNION ALL SELECT 'Hell Hound',     1, 'Bite',        5, 1, 8,  3, 'piercing',    2, 6, 'fire', '',             0,  '',          0,  0, 1
            UNION ALL SELECT 'Minotaur',       1, 'Greataxe',    6, 2, 12, 4, 'slashing',    0, 0, '',     '',             0,  '',          0,  0, 1
            UNION ALL SELECT 'Minotaur',       2, 'Gore',        6, 2, 8,  4, 'piercing',    0, 0, '',     '',             0,  '',          0,  0, 1
            UNION ALL SELECT 'Ettin',          1, 'Battleaxe',   7, 2, 8,  5, 'slashing',    0, 0, '',     '',             0,  '',          0,  0, 1
            UNION ALL SELECT 'Ettin',          2, 'Morningstar', 7, 2, 8,  5, 'piercing',    0, 0, '',     '',             0,  '',          0,  0, 1
            UNION ALL SELECT 'Hill Giant',     1, 'Greatclub',   8, 3, 8,  5, 'bludgeoning', 0, 0, '',     '',             0,  '',          0,  0, 2
            UNION ALL SELECT 'Troll',          1, 'Bite',        7, 1, 6,  4, 'piercing',    0, 0, '',     '',             0,  '',          0,  0, 1
            UNION ALL SELECT 'Troll',          2, 'Claw',        7, 2, 6,  4, 'slashing',    0, 0, '',     '',             0,  '',          0,  0, 2
        ) AS a ON a.Monster = Monsters.Name;
    )sql"},
    {20, "add Coins to Characters", R"sql(
        ALTER TABLE Characters ADD COLUMN Coins TEXT NOT NULL DEFAULT '';
    )sql"},
    {21, "add AdventureRuns (multi-scene adventure progress)", R"sql(
        CREATE TABLE AdventureRuns (
            AdventureRunID INTEGER PRIMARY KEY AUTOINCREMENT,
            Title          TEXT NOT NULL,
            NextScene      INTEGER NOT NULL DEFAULT 0,
            Party          TEXT NOT NULL DEFAULT '',  -- character ids: "3,5,9"
            Status         TEXT NOT NULL DEFAULT 'InProgress',
            LongRestTaken  INTEGER NOT NULL DEFAULT 0,
            StartedAt      TEXT NOT NULL DEFAULT ''
        );
    )sql"},
    {22, "add RevivalPenalty to Characters (raise dead)", R"sql(
        ALTER TABLE Characters ADD COLUMN RevivalPenalty INTEGER NOT NULL DEFAULT 0;
    )sql"},
    // Magic items (docs/magic-items.md): rarity, +N weapons and armor
    // generated from the mundane ones, potions, wondrous items, and scrolls
    // of the automated spells. Prices are inside the DMG's rarity ranges.
    {23, "add magic items and attunement", R"sql(
        ALTER TABLE Items ADD COLUMN Rarity TEXT NOT NULL DEFAULT '';
        ALTER TABLE Items ADD COLUMN BaseItem TEXT NOT NULL DEFAULT '';
        ALTER TABLE Items ADD COLUMN MagicBonus INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Items ADD COLUMN RequiresAttunement INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Items ADD COLUMN AcBonus INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Items ADD COLUMN SaveBonus INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Items ADD COLUMN UnarmoredAcBonus INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Items ADD COLUMN SetsAbility TEXT NOT NULL DEFAULT '';
        ALTER TABLE Items ADD COLUMN SetsAbilityTo INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Items ADD COLUMN ScrollSpellID INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE CharacterInventory ADD COLUMN Attuned INTEGER NOT NULL DEFAULT 0;

        UPDATE Items SET Rarity = 'Common' WHERE Name = 'Potion of Healing';

        -- +1/+2/+3 weapons, armor, and shields: a copy of each mundane item.
        CREATE TEMP TABLE MagicTiers (Category TEXT, Bonus INTEGER, Rarity TEXT, PriceCp INTEGER);
        INSERT INTO MagicTiers VALUES
            ('Weapon', 1, 'Uncommon', 40000),  ('Weapon', 2, 'Rare', 400000),     ('Weapon', 3, 'VeryRare', 2000000),
            ('Armor', 1, 'Rare', 150000),      ('Armor', 2, 'VeryRare', 1500000), ('Armor', 3, 'Legendary', 6000000),
            ('Shield', 1, 'Uncommon', 30000),  ('Shield', 2, 'Rare', 300000),     ('Shield', 3, 'VeryRare', 1500000);
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp, Rarity, BaseItem, MagicBonus)
            SELECT i.Name || ' +' || t.Bonus, i.Category,
                   CASE i.Category
                       WHEN 'Weapon' THEN 'A magic weapon: +' || t.Bonus || ' to attack and damage rolls. '
                       ELSE 'Magic ' || lower(i.Category) || ': +' || t.Bonus || ' to AC. '
                   END || i.Description,
                   i.WeightLb, i.CostCp + t.PriceCp, t.Rarity, i.Name, t.Bonus
            FROM Items i JOIN MagicTiers t ON t.Category = i.Category
            WHERE i.Category IN ('Weapon', 'Armor', 'Shield') AND i.BaseItem = ''
            ORDER BY i.Name, t.Bonus;
        INSERT INTO WeaponStats (ItemID, WeaponCategory, DamageDiceCount, DamageDiceSides, DamageType, VersatileDiceSides,
                                 Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Reach, Loading, Ammunition)
            SELECT m.ItemID, w.WeaponCategory, w.DamageDiceCount, w.DamageDiceSides, w.DamageType, w.VersatileDiceSides,
                   w.Finesse, w.Light, w.Heavy, w.TwoHanded, w.Ranged, w.Thrown, w.Reach, w.Loading, w.Ammunition
            FROM Items m JOIN Items i ON i.Name = m.BaseItem JOIN WeaponStats w ON w.ItemID = i.ItemID;
        INSERT INTO ArmorStats (ItemID, ArmorType, BaseAC, MaxDexBonus, StrengthRequirement, StealthDisadvantage)
            SELECT m.ItemID, a.ArmorType, a.BaseAC, a.MaxDexBonus, a.StrengthRequirement, a.StealthDisadvantage
            FROM Items m JOIN Items i ON i.Name = m.BaseItem JOIN ArmorStats a ON a.ItemID = i.ItemID;
        DROP TABLE MagicTiers;

        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp, HealDiceCount, HealDiceSides, HealBonus, Rarity) VALUES
            ('Potion of Greater Healing', 'Consumable', 'A red, glimmering liquid. Drinking it restores 4d4 + 4 hit points.', 0.5, 15000, 4, 4, 4, 'Uncommon'),
            ('Potion of Superior Healing', 'Consumable', 'A red, glimmering liquid. Drinking it restores 8d4 + 8 hit points.', 0.5, 60000, 8, 4, 8, 'Rare'),
            ('Potion of Supreme Healing', 'Consumable', 'A red, glimmering liquid. Drinking it restores 10d4 + 20 hit points.', 0.5, 600000, 10, 4, 20, 'VeryRare');

        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp, Rarity, RequiresAttunement,
                           AcBonus, SaveBonus, UnarmoredAcBonus, SetsAbility, SetsAbilityTo) VALUES
            ('Cloak of Protection', 'Wondrous', 'Requires attunement. +1 to AC and saving throws while you wear it.', 1, 40000, 'Uncommon', 1, 1, 1, 0, '', 0),
            ('Ring of Protection', 'Wondrous', 'Requires attunement. +1 to AC and saving throws while you wear it.', 0, 350000, 'Rare', 1, 1, 1, 0, '', 0),
            ('Bracers of Defense', 'Wondrous', 'Requires attunement. +2 to AC while you wear no armor and use no shield.', 1, 400000, 'Rare', 1, 0, 0, 2, '', 0),
            ('Gauntlets of Ogre Power', 'Wondrous', 'Requires attunement. Your Strength is 19 while you wear them (no effect if it''s already 19 or higher).', 2, 50000, 'Uncommon', 1, 0, 0, 0, 'Strength', 19),
            ('Amulet of Health', 'Wondrous', 'Requires attunement. Your Constitution is 19 while you wear it (no effect if it''s already 19 or higher).', 1, 400000, 'Rare', 1, 0, 0, 0, 'Constitution', 19);

        -- Spell scrolls of the automated spells, levels 0-3 (DMG "Spell Scroll").
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp, Rarity, ScrollSpellID)
            SELECT 'Spell Scroll (' || Name || ')', 'Scroll',
                   'Read it to cast ' || Name || (CASE Level WHEN 0 THEN ' (cantrip)' ELSE ' (level ' || Level || ')' END) ||
                   ' without a spell slot; the scroll crumbles to dust. Only classes with the spell on their list can ' ||
                   'read it, and a spell above your highest slot level needs a spellcasting check (DC 10 + its level). ' ||
                   'Save DC ' || (CASE WHEN Level <= 2 THEN 13 ELSE 15 END) || ', attack +' || (CASE WHEN Level <= 2 THEN 5 ELSE 7 END) || '.',
                   0, CASE Level WHEN 0 THEN 5000 WHEN 1 THEN 7500 WHEN 2 THEN 15000 ELSE 30000 END,
                   CASE WHEN Level <= 1 THEN 'Common' ELSE 'Uncommon' END, SpellID
            FROM Spells WHERE Level <= 3 AND Resolution <> 'Utility';
    )sql"},
    // Campaigns (docs/campaigns.md): rosters of characters from any players,
    // one active at a time, and adventure runs that belong to one.
    {24, "add Campaigns", R"sql(
        CREATE TABLE Campaigns (
            CampaignID INTEGER PRIMARY KEY AUTOINCREMENT,
            Name       TEXT NOT NULL UNIQUE,
            Notes      TEXT NOT NULL DEFAULT '',
            Active     INTEGER NOT NULL DEFAULT 0,
            CreatedAt  TEXT NOT NULL DEFAULT ''
        );
        ALTER TABLE Characters ADD COLUMN CampaignID INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE AdventureRuns ADD COLUMN CampaignID INTEGER NOT NULL DEFAULT 0;
    )sql"},
    // 2nd- and 3rd-level spells (docs/spells-2-3.md): automated ones that fit
    // combat, Utility ones to fill every class's picks, repeat saves,
    // Revivify and its diamonds, and scrolls of the new automated spells.
    {25, "add 2nd- and 3rd-level spells", R"sql(
        ALTER TABLE Spells ADD COLUMN ConsumedItem TEXT NOT NULL DEFAULT '';
        ALTER TABLE SpellEffects ADD COLUMN RepeatSave INTEGER NOT NULL DEFAULT 0;

        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp) VALUES
            ('Diamonds (300 gp)', 'Gear', 'Diamonds worth 300 gp: the material component Revivify consumes.', 0, 30000);

        INSERT INTO Spells (Name, Level, School, Description, CastingTime, RangeText, DurationText, Concentration,
                            Resolution, MeleeAttack, SaveAbility, SaveHalves, DiceCount, DiceSides, FlatBonus,
                            AddsModifier, DamageType, Projectiles, MaxTargets, UpcastDice, UpcastProjectiles,
                            CantripScaling, UpcastTargets, TargetCreatureType, ConsumedItem) VALUES
            ('Scorching Ray', 2, 'Evocation', 'You create three rays of fire and hurl them at targets within range: a ranged spell attack for each, 2d6 fire on a hit. One more ray per slot level above 2nd.', 'Action', '120 feet', 'Instantaneous', 0, 'Attack', 0, '', 0, 2, 6, 0, 0, 'fire', 3, 3, 0, 1, 'Dice', 0, '', ''),
            ('Shatter', 2, 'Evocation', 'A sudden loud ringing noise erupts from a point you choose. Hits up to 4 creatures (a 10-foot sphere): CON save, 3d8 thunder, half on a success.', 'Action', '60 feet', 'Instantaneous', 0, 'Save', 0, 'Constitution', 1, 3, 8, 0, 0, 'thunder', 1, 4, 1, 0, 'Dice', 0, '', ''),
            ('Hold Person', 2, 'Enchantment', 'A humanoid must succeed on a Wisdom save or be paralyzed. It repeats the save at the end of each of its turns, ending the spell on itself on a success. One more target per slot level above 2nd.', 'Action', '60 feet', 'Concentration, up to 1 minute', 1, 'Save', 0, 'Wisdom', 0, 0, 1, 0, 0, '', 1, 1, 0, 0, 'Dice', 1, 'Humanoid', ''),
            ('Blindness/Deafness', 2, 'Necromancy', 'A foe must succeed on a Constitution save or be blinded for the duration. It repeats the save at the end of each of its turns, ending the spell on a success. One more target per slot level above 2nd. (Deafness isn''t simulated.)', 'Action', '30 feet', '1 minute', 0, 'Save', 0, 'Constitution', 0, 0, 1, 0, 0, '', 1, 1, 0, 0, 'Dice', 1, '', ''),
            ('Fireball', 3, 'Evocation', 'A bright streak blossoms into an explosion of flame. Hits up to 6 creatures (a 20-foot sphere): DEX save, 8d6 fire, half on a success. One more d6 per slot level above 3rd.', 'Action', '150 feet', 'Instantaneous', 0, 'Save', 0, 'Dexterity', 1, 8, 6, 0, 0, 'fire', 1, 6, 1, 0, 'Dice', 0, '', ''),
            ('Lightning Bolt', 3, 'Evocation', 'A stroke of lightning forms a line 100 feet long. Hits up to 4 creatures: DEX save, 8d6 lightning, half on a success. One more d6 per slot level above 3rd.', 'Action', 'Self (100-foot line)', 'Instantaneous', 0, 'Save', 0, 'Dexterity', 1, 8, 6, 0, 0, 'lightning', 1, 4, 1, 0, 'Dice', 0, '', ''),
            ('Mass Healing Word', 3, 'Evocation', 'Up to six creatures of your choice that you can see regain 1d4 + your spellcasting modifier hit points. One more d4 per slot level above 3rd.', 'BonusAction', '60 feet', 'Instantaneous', 0, 'Heal', 0, '', 0, 1, 4, 0, 1, '', 1, 6, 1, 0, 'Dice', 0, '', ''),
            ('Revivify', 3, 'Necromancy', 'You touch a creature that has died within the last minute (here: in this fight). It returns to life with 1 hit point. Uses up diamonds worth 300 gp.', 'Action', 'Touch', 'Instantaneous', 0, 'Revive', 0, '', 0, 0, 0, 0, 0, '', 1, 1, 0, 0, 'Dice', 0, '', 'Diamonds (300 gp)');

        INSERT INTO SpellEffects (SpellID, EffectTrigger, Kind, ConditionName, Amount, Duration, Rounds, UntilUsed, Label, RepeatSave)
            SELECT SpellID, 'OnFailedSave', 'Condition', 'Paralyzed', 0, 'Rounds', 10, 0, 'Held (Hold Person)', 1 FROM Spells WHERE Name = 'Hold Person';
        INSERT INTO SpellEffects (SpellID, EffectTrigger, Kind, ConditionName, Amount, Duration, Rounds, UntilUsed, Label, RepeatSave)
            SELECT SpellID, 'OnFailedSave', 'Condition', 'Blinded', 0, 'Rounds', 10, 0, 'Blinded', 1 FROM Spells WHERE Name = 'Blindness/Deafness';

        INSERT INTO Spells (Name, Level, School, Description, CastingTime, RangeText, DurationText, Concentration,
                            Resolution, DiceCount, DiceSides) VALUES
            ('Aid', 2, 'Abjuration', 'Up to three creatures'' hit point maximum and current hit points rise by 5 for 8 hours. (No automated effect yet: it outlasts combat.)', 'Action', '30 feet', '8 hours', 0, 'Utility', 0, 0),
            ('Barkskin', 2, 'Transmutation', 'A willing creature''s skin becomes rough and bark-like: its AC can''t be less than 16. (No automated effect yet.)', 'Action', 'Touch', 'Concentration, up to 1 hour', 1, 'Utility', 0, 0),
            ('Darkvision', 2, 'Transmutation', 'A willing creature gains darkvision out to 60 feet for 8 hours. (No automated effect yet.)', 'Action', 'Touch', '8 hours', 0, 'Utility', 0, 0),
            ('Enhance Ability', 2, 'Transmutation', 'A creature gains a magical enhancement such as advantage on one ability''s checks. (No automated effect yet.)', 'Action', 'Touch', 'Concentration, up to 1 hour', 1, 'Utility', 0, 0),
            ('Invisibility', 2, 'Illusion', 'A creature you touch becomes invisible until it attacks or casts a spell. (No automated effect yet.)', 'Action', 'Touch', 'Concentration, up to 1 hour', 1, 'Utility', 0, 0),
            ('Lesser Restoration', 2, 'Abjuration', 'You end one disease or one condition afflicting a creature: blinded, deafened, paralyzed, or poisoned. (No automated effect yet.)', 'Action', 'Touch', 'Instantaneous', 0, 'Utility', 0, 0),
            ('Magic Weapon', 2, 'Transmutation', 'A nonmagical weapon becomes a +1 magic weapon. (No automated effect yet.)', 'BonusAction', 'Touch', 'Concentration, up to 1 hour', 1, 'Utility', 0, 0),
            ('Mirror Image', 2, 'Illusion', 'Three illusory duplicates of yourself appear and may take hits meant for you. (No automated effect yet.)', 'Action', 'Self', '1 minute', 0, 'Utility', 0, 0),
            ('Misty Step', 2, 'Conjuration', 'You teleport up to 30 feet to an unoccupied space you can see. (No automated effect yet: there''s no map.)', 'BonusAction', 'Self', 'Instantaneous', 0, 'Utility', 0, 0),
            ('Moonbeam', 2, 'Evocation', 'A silvery beam of pale light: creatures in it take 2d10 radiant (CON half). (No automated effect yet: it needs a map.)', 'Action', '120 feet', 'Concentration, up to 1 minute', 1, 'Utility', 0, 0),
            ('Pass without Trace', 2, 'Abjuration', 'The party gains +10 to Dexterity (Stealth) checks and can''t be tracked. (No automated effect yet.)', 'Action', 'Self', 'Concentration, up to 1 hour', 1, 'Utility', 0, 0),
            ('Silence', 2, 'Illusion', 'No sound can be created within a 20-foot sphere. (No automated effect yet.)', 'Action', '120 feet', 'Concentration, up to 10 minutes', 1, 'Utility', 0, 0),
            ('Spike Growth', 2, 'Transmutation', 'The ground in a 20-foot radius sprouts hard spikes: 2d4 piercing per 5 feet moved. (No automated effect yet: it needs a map.)', 'Action', '150 feet', 'Concentration, up to 10 minutes', 1, 'Utility', 0, 0),
            ('Spiritual Weapon', 2, 'Evocation', 'A floating spectral weapon that attacks with a bonus action: 1d8 + modifier force. (No automated effect yet.)', 'BonusAction', '60 feet', '1 minute', 0, 'Utility', 0, 0),
            ('Suggestion', 2, 'Enchantment', 'You suggest a course of activity to a creature that can hear and understand you (WIS save). (No automated effect yet.)', 'Action', '30 feet', 'Concentration, up to 8 hours', 1, 'Utility', 0, 0),
            ('Beacon of Hope', 3, 'Abjuration', 'Allies have advantage on WIS saves and death saves, and regain the maximum from healing. (No automated effect yet.)', 'Action', '30 feet', 'Concentration, up to 1 minute', 1, 'Utility', 0, 0),
            ('Call Lightning', 3, 'Conjuration', 'A storm cloud: each turn, call a bolt for 3d10 lightning (DEX half). (No automated effect yet.)', 'Action', '120 feet', 'Concentration, up to 10 minutes', 1, 'Utility', 0, 0),
            ('Conjure Animals', 3, 'Conjuration', 'You summon fey spirits that take the form of beasts. (No automated effect yet: summons aren''t simulated.)', 'Action', '60 feet', 'Concentration, up to 1 hour', 1, 'Utility', 0, 0),
            ('Crusader''s Mantle', 3, 'Evocation', 'Allies within 30 feet deal an extra 1d4 radiant damage with weapon attacks. (No automated effect yet.)', 'Action', 'Self (30-foot radius)', 'Concentration, up to 1 minute', 1, 'Utility', 0, 0),
            ('Daylight', 3, 'Evocation', 'A 60-foot-radius sphere of light spreads out from a point you choose. (No automated effect yet.)', 'Action', '60 feet', '1 hour', 0, 'Utility', 0, 0),
            ('Dispel Magic', 3, 'Abjuration', 'Spells of 3rd level or lower on a target end. (No automated effect yet.)', 'Action', '120 feet', 'Instantaneous', 0, 'Utility', 0, 0),
            ('Fly', 3, 'Transmutation', 'A willing creature gains a flying speed of 60 feet. (No automated effect yet.)', 'Action', 'Touch', 'Concentration, up to 10 minutes', 1, 'Utility', 0, 0),
            ('Haste', 3, 'Transmutation', 'A willing creature gets double speed, +2 AC, advantage on DEX saves, and an extra action. (No automated effect yet.)', 'Action', '30 feet', 'Concentration, up to 1 minute', 1, 'Utility', 0, 0),
            ('Hunger of Hadar', 3, 'Conjuration', 'A 20-foot sphere of blackness and bitter cold: 2d6 cold and 2d6 acid to those inside. (No automated effect yet: it needs a map.)', 'Action', '150 feet', 'Concentration, up to 1 minute', 1, 'Utility', 0, 0),
            ('Hypnotic Pattern', 3, 'Illusion', 'Creatures in a 30-foot cube that fail a WIS save are charmed and incapacitated. (No automated effect yet.)', 'Action', '120 feet', 'Concentration, up to 1 minute', 1, 'Utility', 0, 0),
            ('Remove Curse', 3, 'Abjuration', 'All curses affecting one creature or object end. (No automated effect yet.)', 'Action', 'Touch', 'Instantaneous', 0, 'Utility', 0, 0),
            ('Spirit Guardians', 3, 'Conjuration', 'Spirits flit around you: enemies near you are slowed and take 3d8 radiant or necrotic (WIS half). (No automated effect yet: it needs a map.)', 'Action', 'Self (15-foot radius)', 'Concentration, up to 10 minutes', 1, 'Utility', 0, 0),
            ('Vampiric Touch', 3, 'Necromancy', 'A melee spell attack for 3d6 necrotic; you regain half the damage. (No automated effect yet.)', 'Action', 'Self', 'Concentration, up to 1 minute', 1, 'Utility', 0, 0);

        WITH Pairs (ClassName, SpellName) AS (VALUES
            ('Sorcerer', 'Scorching Ray'), ('Wizard', 'Scorching Ray'),
            ('Bard', 'Shatter'), ('Sorcerer', 'Shatter'), ('Warlock', 'Shatter'), ('Wizard', 'Shatter'),
            ('Bard', 'Hold Person'), ('Cleric', 'Hold Person'), ('Druid', 'Hold Person'), ('Sorcerer', 'Hold Person'),
            ('Warlock', 'Hold Person'), ('Wizard', 'Hold Person'),
            ('Bard', 'Blindness/Deafness'), ('Cleric', 'Blindness/Deafness'), ('Sorcerer', 'Blindness/Deafness'),
            ('Wizard', 'Blindness/Deafness'),
            ('Sorcerer', 'Fireball'), ('Wizard', 'Fireball'),
            ('Sorcerer', 'Lightning Bolt'), ('Wizard', 'Lightning Bolt'),
            ('Cleric', 'Mass Healing Word'),
            ('Cleric', 'Revivify'), ('Paladin', 'Revivify'),
            ('Cleric', 'Aid'), ('Paladin', 'Aid'),
            ('Druid', 'Barkskin'), ('Ranger', 'Barkskin'),
            ('Druid', 'Darkvision'), ('Ranger', 'Darkvision'), ('Sorcerer', 'Darkvision'), ('Wizard', 'Darkvision'),
            ('Bard', 'Enhance Ability'), ('Cleric', 'Enhance Ability'), ('Druid', 'Enhance Ability'), ('Sorcerer', 'Enhance Ability'),
            ('Bard', 'Invisibility'), ('Sorcerer', 'Invisibility'), ('Warlock', 'Invisibility'), ('Wizard', 'Invisibility'),
            ('Bard', 'Lesser Restoration'), ('Cleric', 'Lesser Restoration'), ('Druid', 'Lesser Restoration'),
            ('Paladin', 'Lesser Restoration'), ('Ranger', 'Lesser Restoration'),
            ('Paladin', 'Magic Weapon'), ('Wizard', 'Magic Weapon'),
            ('Sorcerer', 'Mirror Image'), ('Warlock', 'Mirror Image'), ('Wizard', 'Mirror Image'),
            ('Sorcerer', 'Misty Step'), ('Warlock', 'Misty Step'), ('Wizard', 'Misty Step'),
            ('Druid', 'Moonbeam'),
            ('Druid', 'Pass without Trace'), ('Ranger', 'Pass without Trace'),
            ('Bard', 'Silence'), ('Cleric', 'Silence'), ('Ranger', 'Silence'),
            ('Druid', 'Spike Growth'), ('Ranger', 'Spike Growth'),
            ('Cleric', 'Spiritual Weapon'),
            ('Bard', 'Suggestion'), ('Sorcerer', 'Suggestion'), ('Warlock', 'Suggestion'), ('Wizard', 'Suggestion'),
            ('Cleric', 'Beacon of Hope'),
            ('Druid', 'Call Lightning'),
            ('Druid', 'Conjure Animals'), ('Ranger', 'Conjure Animals'),
            ('Paladin', 'Crusader''s Mantle'),
            ('Cleric', 'Daylight'), ('Druid', 'Daylight'), ('Paladin', 'Daylight'), ('Ranger', 'Daylight'), ('Sorcerer', 'Daylight'),
            ('Bard', 'Dispel Magic'), ('Cleric', 'Dispel Magic'), ('Druid', 'Dispel Magic'), ('Paladin', 'Dispel Magic'),
            ('Sorcerer', 'Dispel Magic'), ('Warlock', 'Dispel Magic'), ('Wizard', 'Dispel Magic'),
            ('Sorcerer', 'Fly'), ('Warlock', 'Fly'), ('Wizard', 'Fly'),
            ('Sorcerer', 'Haste'), ('Wizard', 'Haste'),
            ('Warlock', 'Hunger of Hadar'),
            ('Bard', 'Hypnotic Pattern'), ('Sorcerer', 'Hypnotic Pattern'), ('Warlock', 'Hypnotic Pattern'), ('Wizard', 'Hypnotic Pattern'),
            ('Cleric', 'Remove Curse'), ('Paladin', 'Remove Curse'), ('Warlock', 'Remove Curse'), ('Wizard', 'Remove Curse'),
            ('Cleric', 'Spirit Guardians'),
            ('Warlock', 'Vampiric Touch'), ('Wizard', 'Vampiric Touch')
        )
        INSERT INTO ClassSpells (ClassID, SpellID)
        SELECT Classes.ClassID, Spells.SpellID FROM Pairs
        JOIN Classes ON Classes.ClassName = Pairs.ClassName
        JOIN Spells ON Spells.Name = Pairs.SpellName;

        -- Scrolls of the new automated spells (DMG: 2nd 150 gp, 3rd 300 gp, uncommon).
        INSERT INTO Items (Name, Category, Description, WeightLb, CostCp, Rarity, ScrollSpellID)
            SELECT 'Spell Scroll (' || Name || ')', 'Scroll',
                   'Read it to cast ' || Name || ' (level ' || Level || ')' ||
                   ' without a spell slot or material components; the scroll crumbles to dust. Only classes with the ' ||
                   'spell on their list can read it, and a spell above your highest slot level needs a spellcasting ' ||
                   'check (DC 10 + its level). Save DC ' || (CASE WHEN Level <= 2 THEN 13 ELSE 15 END) ||
                   ', attack +' || (CASE WHEN Level <= 2 THEN 5 ELSE 7 END) || '.',
                   0, CASE Level WHEN 2 THEN 15000 ELSE 30000 END, 'Uncommon', SpellID
            FROM Spells WHERE Level IN (2, 3) AND Resolution <> 'Utility';
    )sql"},
    // Preparing spells (docs/preparing-spells.md): a Wizard's spellbook vs
    // its prepared spells, and the window after a long rest for changing them.
    {26, "add CharacterSpells.Prepared and Characters.MayPrepare", R"sql(
        ALTER TABLE CharacterSpells ADD COLUMN Prepared INTEGER NOT NULL DEFAULT 1;
        ALTER TABLE Characters ADD COLUMN MayPrepare INTEGER NOT NULL DEFAULT 1;
    )sql"},
    // Reaction spells (docs/reactions.md), cast on their own on monsters' turns.
    {27, "add Shield and Hellish Rebuke", R"sql(
        INSERT INTO Spells (Name, Level, School, Description, CastingTime, RangeText, DurationText, Concentration,
                            Resolution, MeleeAttack, SaveAbility, SaveHalves, DiceCount, DiceSides, FlatBonus,
                            AddsModifier, DamageType, Projectiles, MaxTargets, UpcastDice, UpcastProjectiles,
                            CantripScaling) VALUES
            ('Shield', 1, 'Abjuration', 'A reaction when an attack hits you: +5 AC until the start of your next turn, including against that attack. Cast on its own on a monster''s turn when it turns a hit into a miss.', 'Reaction', 'Self', '1 round', 0, 'Utility', 0, '', 0, 0, 0, 0, 0, '', 1, 1, 0, 0, 'Dice'),
            ('Hellish Rebuke', 1, 'Evocation', 'A reaction when a creature you can see damages you: it makes a DEX save, taking 2d10 fire, half on a success. One more d10 per slot level above 1st. Cast on its own when you''re hurt.', 'Reaction', '60 feet', 'Instantaneous', 0, 'Save', 0, 'Dexterity', 1, 2, 10, 0, 0, 'fire', 1, 1, 1, 0, 'Dice');

        WITH Pairs (ClassName, SpellName) AS (VALUES
            ('Sorcerer', 'Shield'), ('Wizard', 'Shield'), ('Warlock', 'Hellish Rebuke')
        )
        INSERT INTO ClassSpells (ClassID, SpellID)
        SELECT Classes.ClassID, Spells.SpellID FROM Pairs
        JOIN Classes ON Classes.ClassName = Pairs.ClassName
        JOIN Spells ON Spells.Name = Pairs.SpellName;
    )sql"},
    // Subclasses (docs/subclasses.md): the choice, exhaustion and temporary
    // HP they need, and the spells their lists name that weren't seeded yet.
    {28, "add subclasses, exhaustion, and temporary hit points", R"sql(
        ALTER TABLE Characters ADD COLUMN Subclass TEXT NOT NULL DEFAULT '';
        ALTER TABLE Characters ADD COLUMN SubclassChoice TEXT NOT NULL DEFAULT '';
        ALTER TABLE Characters ADD COLUMN Exhaustion INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Characters ADD COLUMN TempHP INTEGER NOT NULL DEFAULT 0;

        INSERT INTO Spells (Name, Level, School, Description, CastingTime, RangeText, DurationText, Concentration,
                            Resolution, DiceCount, DiceSides) VALUES
            ('Acid Arrow', 2, 'Evocation', 'A shimmering green arrow: 4d4 acid on a hit, and 2d4 more at the end of the target''s next turn. (No automated effect yet.)', 'Action', '90 feet', 'Instantaneous', 0, 'Utility', 0, 0),
            ('Blur', 2, 'Illusion', 'Your body blurs: attackers have disadvantage against you. (No automated effect yet.)', 'Action', 'Self', 'Concentration, up to 1 minute', 1, 'Utility', 0, 0),
            ('Darkness', 2, 'Evocation', 'Magical darkness fills a 15-foot sphere. (No automated effect yet.)', 'Action', '60 feet', 'Concentration, up to 10 minutes', 1, 'Utility', 0, 0),
            ('Spider Climb', 2, 'Transmutation', 'A creature can climb walls and ceilings, hands free. (No automated effect yet.)', 'Action', 'Touch', 'Concentration, up to 1 hour', 1, 'Utility', 0, 0),
            ('Web', 2, 'Conjuration', 'Thick, sticky webbing fills a 20-foot cube and restrains creatures caught in it (DEX save). (No automated effect yet.)', 'Action', '60 feet', 'Concentration, up to 1 hour', 1, 'Utility', 0, 0),
            ('Zone of Truth', 2, 'Enchantment', 'Creatures in a 15-foot sphere can''t deliberately lie (CHA save). (No automated effect yet.)', 'Action', '60 feet', '10 minutes', 0, 'Utility', 0, 0),
            ('Create Food and Water', 3, 'Conjuration', 'Enough food and water for fifteen humanoids or five steeds for a day. (No automated effect yet.)', 'Action', '30 feet', 'Instantaneous', 0, 'Utility', 0, 0),
            ('Gaseous Form', 3, 'Transmutation', 'A willing creature becomes a misty cloud. (No automated effect yet.)', 'Action', 'Touch', 'Concentration, up to 1 hour', 1, 'Utility', 0, 0),
            ('Meld into Stone', 3, 'Transmutation', 'You step into a stone object or surface large enough to hold you. (No automated effect yet.)', 'Action', 'Touch', '8 hours', 0, 'Utility', 0, 0),
            ('Plant Growth', 3, 'Transmutation', 'Plants in a 100-foot radius become thick and overgrown. (No automated effect yet.)', 'Action', '150 feet', 'Instantaneous', 0, 'Utility', 0, 0),
            ('Protection from Energy', 3, 'Abjuration', 'A willing creature has resistance to one damage type you choose. (No automated effect yet.)', 'Action', 'Touch', 'Concentration, up to 1 hour', 1, 'Utility', 0, 0),
            ('Sleet Storm', 3, 'Conjuration', 'Freezing rain and sleet fall in a 40-foot cylinder. (No automated effect yet.)', 'Action', '150 feet', 'Concentration, up to 1 minute', 1, 'Utility', 0, 0),
            ('Slow', 3, 'Transmutation', 'Up to six creatures are slowed: half speed, -2 AC and DEX saves, no reactions (WIS save). (No automated effect yet.)', 'Action', '120 feet', 'Concentration, up to 1 minute', 1, 'Utility', 0, 0),
            ('Stinking Cloud', 3, 'Conjuration', 'A 20-foot sphere of nauseating gas: creatures that fail a CON save waste their action. (No automated effect yet.)', 'Action', '90 feet', 'Concentration, up to 1 minute', 1, 'Utility', 0, 0),
            ('Water Breathing', 3, 'Transmutation', 'Up to ten willing creatures can breathe underwater. (No automated effect yet.)', 'Action', '30 feet', '24 hours', 0, 'Utility', 0, 0),
            ('Water Walk', 3, 'Transmutation', 'Up to ten willing creatures can walk on liquid. (No automated effect yet.)', 'Action', '30 feet', '1 hour', 0, 'Utility', 0, 0);

        WITH Pairs (ClassName, SpellName) AS (VALUES
            ('Wizard', 'Acid Arrow'),
            ('Sorcerer', 'Blur'), ('Wizard', 'Blur'),
            ('Sorcerer', 'Darkness'), ('Warlock', 'Darkness'), ('Wizard', 'Darkness'),
            ('Sorcerer', 'Spider Climb'), ('Warlock', 'Spider Climb'), ('Wizard', 'Spider Climb'),
            ('Sorcerer', 'Web'), ('Wizard', 'Web'),
            ('Bard', 'Zone of Truth'), ('Cleric', 'Zone of Truth'), ('Paladin', 'Zone of Truth'),
            ('Cleric', 'Create Food and Water'), ('Paladin', 'Create Food and Water'),
            ('Sorcerer', 'Gaseous Form'), ('Warlock', 'Gaseous Form'), ('Wizard', 'Gaseous Form'),
            ('Cleric', 'Meld into Stone'), ('Druid', 'Meld into Stone'),
            ('Bard', 'Plant Growth'), ('Druid', 'Plant Growth'), ('Ranger', 'Plant Growth'),
            ('Cleric', 'Protection from Energy'), ('Druid', 'Protection from Energy'), ('Ranger', 'Protection from Energy'),
            ('Sorcerer', 'Protection from Energy'), ('Wizard', 'Protection from Energy'),
            ('Druid', 'Sleet Storm'), ('Sorcerer', 'Sleet Storm'), ('Wizard', 'Sleet Storm'),
            ('Sorcerer', 'Slow'), ('Wizard', 'Slow'),
            ('Bard', 'Stinking Cloud'), ('Sorcerer', 'Stinking Cloud'), ('Wizard', 'Stinking Cloud'),
            ('Druid', 'Water Breathing'), ('Ranger', 'Water Breathing'), ('Sorcerer', 'Water Breathing'),
            ('Wizard', 'Water Breathing'),
            ('Cleric', 'Water Walk'), ('Druid', 'Water Walk'), ('Ranger', 'Water Walk'), ('Sorcerer', 'Water Walk')
        )
        INSERT INTO ClassSpells (ClassID, SpellID)
        SELECT Classes.ClassID, Spells.SpellID FROM Pairs
        JOIN Classes ON Classes.ClassName = Pairs.ClassName
        JOIN Spells ON Spells.Name = Pairs.SpellName;
    )sql"},
    // Class choices for levels 1-5 (docs/class-features-1-5.md): Metamagic,
    // invocations, Pact Boon, Expertise, ...
    {29, "add Characters.ClassOptions", R"sql(
        ALTER TABLE Characters ADD COLUMN ClassOptions TEXT NOT NULL DEFAULT '';
    )sql"},
    // Beasts for a druid's Wild Shape (docs/class-features-1-5.md), from the
    // SRD 5.1: walkers of CR 1/4 and 1/2. They fight as monsters too.
    {30, "add beasts for Wild Shape", R"sql(
        INSERT INTO Monsters (Name, ArmorClass, HitPoints, Strength, Dexterity, Constitution, Intelligence, Wisdom,
                              Charisma, ChallengeRating, CreatureType, DamageImmunities, ConditionImmunities,
                              Multiattack, Traits) VALUES
            ('Boar',         11, 11, 13, 11, 12, 2, 9,  5, 0.25, 'Beast', '', '', '', ''),
            ('Panther',      12, 13, 14, 15, 10, 3, 14, 7, 0.25, 'Beast', '', '', '', ''),
            ('Giant Badger', 10, 13, 13, 10, 15, 2, 12, 5, 0.25, 'Beast', '', '', 'A bite attack and a claws attack', ''),
            ('Black Bear',   11, 19, 15, 10, 14, 2, 12, 7, 0.5,  'Beast', '', '', 'A bite attack and a claws attack', ''),
            ('Ape',          12, 19, 16, 14, 14, 6, 12, 7, 0.5,  'Beast', '', '', 'Two fist attacks', '');

        INSERT INTO MonsterAttacks (MonsterID, Position, Name, AttackBonus, DamageDiceCount, DamageDiceSides,
                                    DamageBonus, DamageType, Count)
        SELECT MonsterID, a.Position, a.Name, a.Bonus, a.DiceCount, a.DiceSides, a.DamageBonus, a.Type, a.Count
        FROM Monsters JOIN (
            SELECT 'Boar' AS Monster, 1 AS Position, 'Tusk' AS Name, 3 AS Bonus, 1 AS DiceCount, 6 AS DiceSides,
                   1 AS DamageBonus, 'slashing' AS Type, 1 AS Count
            UNION ALL SELECT 'Panther',      1, 'Bite',   4, 1, 6, 2, 'piercing',    1
            UNION ALL SELECT 'Panther',      2, 'Claw',   4, 1, 4, 2, 'slashing',    1
            UNION ALL SELECT 'Giant Badger', 1, 'Bite',   3, 1, 6, 1, 'piercing',    1
            UNION ALL SELECT 'Giant Badger', 2, 'Claws',  3, 2, 4, 1, 'slashing',    1
            UNION ALL SELECT 'Black Bear',   1, 'Bite',   3, 1, 6, 2, 'piercing',    1
            UNION ALL SELECT 'Black Bear',   2, 'Claws',  3, 2, 4, 2, 'slashing',    1
            UNION ALL SELECT 'Ape',          1, 'Fist',   5, 1, 6, 3, 'bludgeoning', 2
        ) AS a ON a.Monster = Monsters.Name;
    )sql"},
    // Monster abilities for levels 5-10 (docs/content-5-10.md): nonmagical
    // weapon resistance, preferred (ranged) attacks, Life Drain, recharging
    // breath weapons, and spellcasting.
    {31, "add monster abilities: breath weapons, spellcasting, life drain, nonmagical resistance", R"sql(
        ALTER TABLE Monsters ADD COLUMN NonmagicalWeapons TEXT NOT NULL DEFAULT '';
        ALTER TABLE Monsters ADD COLUMN BreathName TEXT NOT NULL DEFAULT '';
        ALTER TABLE Monsters ADD COLUMN BreathDiceCount INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Monsters ADD COLUMN BreathDiceSides INTEGER NOT NULL DEFAULT 8;
        ALTER TABLE Monsters ADD COLUMN BreathDamageType TEXT NOT NULL DEFAULT '';
        ALTER TABLE Monsters ADD COLUMN BreathSaveAbility TEXT NOT NULL DEFAULT 'Constitution';
        ALTER TABLE Monsters ADD COLUMN BreathSaveDC INTEGER NOT NULL DEFAULT 10;
        ALTER TABLE Monsters ADD COLUMN BreathRecharge INTEGER NOT NULL DEFAULT 5;
        ALTER TABLE Monsters ADD COLUMN BreathTargets INTEGER NOT NULL DEFAULT 4;
        ALTER TABLE Monsters ADD COLUMN BreathPetrifying INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Monsters ADD COLUMN CasterLevel INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Monsters ADD COLUMN SpellSaveDC INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Monsters ADD COLUMN SpellAttackBonus INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Monsters ADD COLUMN SpellAbilityModifier INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Monsters ADD COLUMN SpellSlots TEXT NOT NULL DEFAULT '';
        ALTER TABLE Monsters ADD COLUMN Spells TEXT NOT NULL DEFAULT '';
        ALTER TABLE MonsterAttacks ADD COLUMN Preferred INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE MonsterAttacks ADD COLUMN LifeDrainDC INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE Characters ADD COLUMN MaxHpReduction INTEGER NOT NULL DEFAULT 0;
    )sql"},
    // Twelve SRD 5.1 monsters for levels 5-10 (docs/content-5-10.md).
    {32, "add monsters for levels 5-10", R"sql(
        INSERT INTO Monsters (Name, ArmorClass, HitPoints, Strength, Dexterity, Constitution, Intelligence, Wisdom,
                              Charisma, ChallengeRating, CreatureType, DamageResistances, DamageImmunities,
                              ConditionImmunities, Multiattack, Traits, NonmagicalWeapons) VALUES
            ('Cult Fanatic',       13, 33,  11, 14, 12, 10, 13, 14, 2, 'Humanoid',    '', '', '', 'Two dagger attacks', '', ''),
            ('Veteran',            17, 58,  16, 13, 14, 10, 11, 10, 3, 'Humanoid',    '', '', '', 'Two longsword attacks', '', ''),
            ('Wight',              14, 45,  15, 14, 16, 10, 13, 15, 3, 'Undead',      'necrotic', 'poison', 'Poisoned', 'Life Drain and a longsword attack', '', 'resistant'),
            ('Werewolf',           12, 58,  15, 13, 14, 10, 11, 10, 3, 'Humanoid',    '', '', '', 'A bite and a claws attack', '', 'immune'),
            ('Wraith',             13, 67,  6,  16, 16, 12, 14, 15, 5, 'Undead',      'acid,cold,fire,lightning,thunder', 'necrotic,poison', 'Charmed,Grappled,Paralyzed,Petrified,Poisoned,Prone,Restrained', '', '', 'resistant'),
            ('Bulette',            17, 94,  19, 11, 21, 2,  10, 5,  5, 'Monstrosity', '', '', '', '', '', ''),
            ('Flesh Golem',        9,  93,  19, 9,  18, 6,  10, 5,  5, 'Construct',   '', 'lightning,poison', 'Charmed,Frightened,Paralyzed,Petrified,Poisoned', 'Two slam attacks', '', 'immune'),
            ('Gorgon',             19, 114, 20, 11, 18, 2,  12, 7,  5, 'Monstrosity', '', '', 'Petrified', '', '', ''),
            ('Mage',               15, 40,  9,  14, 11, 17, 12, 11, 6, 'Humanoid',    '', '', '', '', '', ''),
            ('Young White Dragon', 17, 133, 18, 10, 18, 6,  11, 12, 6, 'Dragon',      '', 'cold', '', 'A bite and two claw attacks', '', ''),
            ('Stone Giant',        17, 126, 23, 15, 20, 10, 12, 9,  7, 'Giant',       '', '', '', 'Two greatclub attacks', '', ''),
            ('Young Green Dragon', 18, 136, 19, 12, 17, 16, 13, 15, 8, 'Dragon',      '', 'poison', 'Poisoned', 'A bite and two claw attacks', '', '');

        UPDATE Monsters SET BreathName = 'Cold Breath', BreathDiceCount = 10, BreathDiceSides = 8, BreathDamageType = 'cold',
                            BreathSaveAbility = 'Constitution', BreathSaveDC = 15, BreathRecharge = 5, BreathTargets = 4
            WHERE Name = 'Young White Dragon';
        UPDATE Monsters SET BreathName = 'Poison Breath', BreathDiceCount = 12, BreathDiceSides = 6, BreathDamageType = 'poison',
                            BreathSaveAbility = 'Constitution', BreathSaveDC = 14, BreathRecharge = 5, BreathTargets = 4
            WHERE Name = 'Young Green Dragon';
        UPDATE Monsters SET BreathName = 'Petrifying Breath', BreathDiceCount = 0, BreathSaveAbility = 'Constitution',
                            BreathSaveDC = 13, BreathRecharge = 5, BreathTargets = 4, BreathPetrifying = 1
            WHERE Name = 'Gorgon';
        UPDATE Monsters SET CasterLevel = 4, SpellSaveDC = 11, SpellAttackBonus = 3, SpellAbilityModifier = 1,
                            SpellSlots = '4,3', Spells = 'Sacred Flame|Inflict Wounds|Hold Person'
            WHERE Name = 'Cult Fanatic';
        UPDATE Monsters SET CasterLevel = 9, SpellSaveDC = 14, SpellAttackBonus = 6, SpellAbilityModifier = 3,
                            SpellSlots = '4,3,3,3,1', Spells = 'Fire Bolt|Magic Missile|Shield|Fireball'
            WHERE Name = 'Mage';

        INSERT INTO MonsterAttacks (MonsterID, Position, Name, AttackBonus, DamageDiceCount, DamageDiceSides,
                                    DamageBonus, DamageType, Ranged, ExtraDiceCount, ExtraDiceSides, ExtraDamageType,
                                    OnHitSaveAbility, OnHitSaveDC, OnHitCondition, Count, Preferred, LifeDrainDC)
        SELECT MonsterID, a.Position, a.Name, a.Bonus, a.DiceCount, a.DiceSides, a.DamageBonus, a.Type, a.Ranged,
               a.ExtraCount, a.ExtraSides, a.ExtraType, a.SaveAbility, a.SaveDC, a.Condition, a.Count, a.Preferred,
               a.Drain
        FROM Monsters JOIN (
            SELECT 'Cult Fanatic' AS Monster, 1 AS Position, 'Dagger' AS Name, 4 AS Bonus, 1 AS DiceCount,
                   4 AS DiceSides, 2 AS DamageBonus, 'piercing' AS Type, 0 AS Ranged, 0 AS ExtraCount,
                   0 AS ExtraSides, '' AS ExtraType, '' AS SaveAbility, 0 AS SaveDC, '' AS Condition, 2 AS Count,
                   0 AS Preferred, 0 AS Drain
            UNION ALL SELECT 'Veteran',            1, 'Longsword',      5, 1, 8,  3, 'slashing',    0, 0, 0, '',       '',         0,  '',      2, 0, 0
            UNION ALL SELECT 'Veteran',            2, 'Heavy Crossbow', 3, 1, 10, 1, 'piercing',    1, 0, 0, '',       '',         0,  '',      1, 1, 0
            UNION ALL SELECT 'Wight',              1, 'Life Drain',     4, 1, 6,  2, 'necrotic',    0, 0, 0, '',       '',         0,  '',      1, 0, 13
            UNION ALL SELECT 'Wight',              2, 'Longsword',      4, 1, 8,  2, 'slashing',    0, 0, 0, '',       '',         0,  '',      1, 0, 0
            UNION ALL SELECT 'Werewolf',           1, 'Bite',           4, 1, 8,  2, 'piercing',    0, 0, 0, '',       '',         0,  '',      1, 0, 0
            UNION ALL SELECT 'Werewolf',           2, 'Claws',          4, 2, 4,  2, 'slashing',    0, 0, 0, '',       '',         0,  '',      1, 0, 0
            UNION ALL SELECT 'Wraith',             1, 'Life Drain',     6, 4, 8,  3, 'necrotic',    0, 0, 0, '',       '',         0,  '',      1, 0, 14
            UNION ALL SELECT 'Bulette',            1, 'Bite',           7, 4, 12, 4, 'piercing',    0, 0, 0, '',       '',         0,  '',      1, 0, 0
            UNION ALL SELECT 'Flesh Golem',        1, 'Slam',           7, 2, 8,  4, 'bludgeoning', 0, 0, 0, '',       '',         0,  '',      2, 0, 0
            UNION ALL SELECT 'Gorgon',             1, 'Gore',           8, 2, 12, 5, 'piercing',    0, 0, 0, '',       '',         0,  '',      1, 0, 0
            UNION ALL SELECT 'Gorgon',             2, 'Hooves',         8, 2, 10, 5, 'bludgeoning', 0, 0, 0, '',       '',         0,  '',      1, 0, 0
            UNION ALL SELECT 'Mage',               1, 'Dagger',         5, 1, 4,  2, 'piercing',    0, 0, 0, '',       '',         0,  '',      1, 0, 0
            UNION ALL SELECT 'Young White Dragon', 1, 'Bite',           7, 2, 10, 4, 'piercing',    0, 1, 8, 'cold',   '',         0,  '',      1, 0, 0
            UNION ALL SELECT 'Young White Dragon', 2, 'Claw',           7, 2, 6,  4, 'slashing',    0, 0, 0, '',       '',         0,  '',      2, 0, 0
            UNION ALL SELECT 'Stone Giant',        1, 'Greatclub',      9, 3, 8,  6, 'bludgeoning', 0, 0, 0, '',       '',         0,  '',      2, 0, 0
            UNION ALL SELECT 'Stone Giant',        2, 'Rock',           9, 4, 10, 6, 'bludgeoning', 1, 0, 0, '',       'Strength', 17, 'Prone', 1, 1, 0
            UNION ALL SELECT 'Young Green Dragon', 1, 'Bite',           7, 2, 10, 4, 'piercing',    0, 2, 6, 'poison', '',         0,  '',      1, 0, 0
            UNION ALL SELECT 'Young Green Dragon', 2, 'Claw',           7, 2, 6,  4, 'slashing',    0, 0, 0, '',       '',         0,  '',      2, 0, 0
        ) AS a ON a.Monster = Monsters.Name;
    )sql"},
    // Scenes between fights (docs/scenes-and-npcs.md): inspiration, and what
    // a run's scenes unlocked and changed about the next fight.
    {33, "add inspiration and adventure run scene state", R"sql(
        ALTER TABLE Characters ADD COLUMN Inspiration INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE AdventureRuns ADD COLUMN Flags TEXT NOT NULL DEFAULT '';
        ALTER TABLE AdventureRuns ADD COLUMN NextFight TEXT NOT NULL DEFAULT '';
    )sql"},
    // NPCs (docs/scenes-and-npcs.md, phase 2): each belongs to a campaign (0 =
    // none), and the content's NPCs (an adventure's, the Shop's merchant, the
    // Temple's priest) are found by NpcKey. Haggle is a character's try at the
    // merchant's 10% off since their last long rest (1 = got it, -1 = failed).
    {34, "add Npcs and Characters.Haggle", R"sql(
        CREATE TABLE Npcs (
            NpcID       INTEGER PRIMARY KEY AUTOINCREMENT,
            CampaignID  INTEGER NOT NULL DEFAULT 0,
            NpcKey      TEXT NOT NULL DEFAULT '',
            Name        TEXT NOT NULL,
            Role        TEXT NOT NULL DEFAULT '',
            Attitude    TEXT NOT NULL DEFAULT 'Indifferent',
            Description TEXT NOT NULL DEFAULT '',
            Wants       TEXT NOT NULL DEFAULT '',
            Fears       TEXT NOT NULL DEFAULT '',
            Known       INTEGER NOT NULL DEFAULT 0,
            StatBlock   TEXT NOT NULL DEFAULT ''
        );
        ALTER TABLE Characters ADD COLUMN Haggle INTEGER NOT NULL DEFAULT 0;
    )sql"},
    // NPCs in fights (docs/scenes-and-npcs.md, phase 3): an ally can die.
    {35, "add Npcs.Dead", R"sql(
        ALTER TABLE Npcs ADD COLUMN Dead INTEGER NOT NULL DEFAULT 0;
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

void run_migrations(sqlite3* db, int up_to_version) {
    int version = current_schema_version(db);

    for (const auto& migration : kMigrations) {
        if (migration.version <= version) continue;
        if (migration.version > up_to_version) break;

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
