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
constexpr std::array<Migration, 13> kMigrations = {{
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
