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
constexpr std::array<Migration, 22> kMigrations = {{
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
