#include "dnd/repository.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "dnd/currency.hpp"
#include "dnd/damage.hpp"
#include "dnd/detail/statement.hpp"
#include "dnd/inventory.hpp"
#include "dnd/spellcasting.hpp"
#include "dnd/migrations.hpp"

namespace dnd {

namespace {

using detail::Statement;
using detail::bind_text;

constexpr const char* kCharacterColumns =
    "CharacterId, CharacterName, Class, Race, Alignment, PlayerID, Level, "
    "Strength, Dexterity, Constitution, Intelligence, Wisdom, Charisma, "
    "CurrentHP, MaxHP, SkillProficiencies, SaveProficiencies, "
    "Experience, HitDiceUsed, Background, SpellSlotsUsed, Dead, FeatureUses, FightingStyle, Coins, RevivalPenalty, CampaignID, MayPrepare, Subclass, SubclassChoice, Exhaustion, TempHP, ClassOptions";
// (Characters.ArmorClass and WeaponID still exist but are no longer used:
// AC and weapons come from equipped inventory items since migration 15.)

std::string column_text(sqlite3_stmt* stmt, int column) {
    const unsigned char* text = sqlite3_column_text(stmt, column);
    return text ? reinterpret_cast<const char*>(text) : "";
}

// SpellSlotsUsed is stored as nine comma-separated counts ("0,1,0,...").
std::string format_spell_slots(const std::array<int, 9>& slots) {
    std::string text;
    for (size_t i = 0; i < slots.size(); ++i) {
        if (i > 0) text += ',';
        text += std::to_string(slots[i]);
    }
    return text;
}

std::array<int, 9> parse_spell_slots(const std::string& text) {
    std::array<int, 9> slots{};
    size_t index = 0;
    size_t start = 0;
    while (index < slots.size() && start <= text.size()) {
        size_t end = text.find(',', start);
        if (end == std::string::npos) end = text.size();
        if (end > start) slots[index] = std::stoi(text.substr(start, end - start));
        ++index;
        start = end + 1;
    }
    return slots;
}

// FeatureUses is stored as "Key:count" pairs, comma-separated ("SecondWind:1,Rage:2").
std::string format_feature_uses(const std::map<std::string, int>& uses) {
    std::string text;
    for (const auto& [key, count] : uses) {
        if (count <= 0) continue;
        if (!text.empty()) text += ',';
        text += key + ":" + std::to_string(count);
    }
    return text;
}

std::map<std::string, int> parse_feature_uses(const std::string& text) {
    std::map<std::string, int> uses;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find(',', start);
        if (end == std::string::npos) end = text.size();
        std::string pair = text.substr(start, end - start);
        size_t colon = pair.find(':');
        if (colon != std::string::npos) uses[pair.substr(0, colon)] = std::stoi(pair.substr(colon + 1));
        start = end + 1;
    }
    return uses;
}

Character read_character_row(sqlite3_stmt* stmt) {
    Character character;
    character.character_id = sqlite3_column_int(stmt, 0);
    character.character_name = column_text(stmt, 1);
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
    character.skill_proficiencies = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 15));
    character.save_proficiencies = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 16));
    character.experience = sqlite3_column_int(stmt, 17);
    character.hit_dice_used = sqlite3_column_int(stmt, 18);
    character.background = column_text(stmt, 19);
    character.spell_slots_used = parse_spell_slots(column_text(stmt, 20));
    character.dead = sqlite3_column_int(stmt, 21) != 0;
    character.feature_uses_spent = parse_feature_uses(column_text(stmt, 22));
    character.fighting_style = column_text(stmt, 23);
    character.coins = parse_coins(column_text(stmt, 24));
    character.revival_penalty = sqlite3_column_int(stmt, 25);
    character.campaign_id = sqlite3_column_int(stmt, 26);
    character.may_prepare = sqlite3_column_int(stmt, 27) != 0;
    character.subclass = column_text(stmt, 28);
    character.subclass_choice = column_text(stmt, 29);
    character.exhaustion = sqlite3_column_int(stmt, 30);
    character.temp_hp = sqlite3_column_int(stmt, 31);
    character.class_options = parse_class_options(column_text(stmt, 32));
    return character;
}

// Item columns, plus weapon and armor stats from the LEFT JOINs in kItemJoins
// (NULL when the item has none).
constexpr const char* kItemColumns =
    "Items.ItemID, Items.Name, Items.Category, Items.Description, Items.WeightLb, Items.CostCp, "
    "Items.HealDiceCount, Items.HealDiceSides, Items.HealBonus, "
    "WeaponStats.ItemID, WeaponStats.WeaponCategory, WeaponStats.DamageDiceCount, WeaponStats.DamageDiceSides, "
    "WeaponStats.DamageType, WeaponStats.VersatileDiceSides, WeaponStats.Finesse, WeaponStats.Light, "
    "WeaponStats.Heavy, WeaponStats.TwoHanded, WeaponStats.Ranged, WeaponStats.Thrown, WeaponStats.Reach, "
    "WeaponStats.Loading, WeaponStats.Ammunition, "
    "ArmorStats.ItemID, ArmorStats.ArmorType, ArmorStats.BaseAC, ArmorStats.MaxDexBonus, "
    "ArmorStats.StrengthRequirement, ArmorStats.StealthDisadvantage, "
    "Items.Rarity, Items.BaseItem, Items.MagicBonus, Items.RequiresAttunement, Items.AcBonus, Items.SaveBonus, "
    "Items.UnarmoredAcBonus, Items.SetsAbility, Items.SetsAbilityTo, Items.ScrollSpellID";
constexpr int kItemColumnCount = 40;
constexpr const char* kItemJoins =
    " LEFT JOIN WeaponStats ON WeaponStats.ItemID = Items.ItemID"
    " LEFT JOIN ArmorStats ON ArmorStats.ItemID = Items.ItemID";

// Reads kItemColumns starting at column 0.
Item read_item_row(sqlite3_stmt* stmt) {
    Item item;
    item.item_id = sqlite3_column_int(stmt, 0);
    item.name = column_text(stmt, 1);
    item.category = item_category_from_name(column_text(stmt, 2));
    item.description = column_text(stmt, 3);
    item.weight_lb = sqlite3_column_double(stmt, 4);
    item.cost_cp = sqlite3_column_int(stmt, 5);
    item.heal_dice_count = sqlite3_column_int(stmt, 6);
    item.heal_dice_sides = sqlite3_column_int(stmt, 7);
    item.heal_bonus = sqlite3_column_int(stmt, 8);

    if (sqlite3_column_type(stmt, 9) != SQLITE_NULL) {
        WeaponStats weapon;
        weapon.category = weapon_category_from_name(column_text(stmt, 10));
        weapon.damage_dice_count = sqlite3_column_int(stmt, 11);
        weapon.damage_dice_sides = sqlite3_column_int(stmt, 12);
        weapon.damage_type = column_text(stmt, 13);
        weapon.versatile_dice_sides = sqlite3_column_int(stmt, 14);
        weapon.finesse = sqlite3_column_int(stmt, 15) != 0;
        weapon.light = sqlite3_column_int(stmt, 16) != 0;
        weapon.heavy = sqlite3_column_int(stmt, 17) != 0;
        weapon.two_handed = sqlite3_column_int(stmt, 18) != 0;
        weapon.ranged = sqlite3_column_int(stmt, 19) != 0;
        weapon.thrown = sqlite3_column_int(stmt, 20) != 0;
        weapon.reach = sqlite3_column_int(stmt, 21) != 0;
        weapon.loading = sqlite3_column_int(stmt, 22) != 0;
        weapon.ammunition = sqlite3_column_int(stmt, 23) != 0;
        item.weapon = weapon;
    }
    if (sqlite3_column_type(stmt, 24) != SQLITE_NULL) {
        ArmorStats armor;
        armor.type = armor_type_from_name(column_text(stmt, 25));
        armor.base_ac = sqlite3_column_int(stmt, 26);
        armor.max_dex_bonus = sqlite3_column_int(stmt, 27);
        armor.strength_requirement = sqlite3_column_int(stmt, 28);
        armor.stealth_disadvantage = sqlite3_column_int(stmt, 29) != 0;
        item.armor = armor;
    }
    item.rarity = rarity_from_name(column_text(stmt, 30));
    item.base_item = column_text(stmt, 31);
    item.magic_bonus = sqlite3_column_int(stmt, 32);
    item.requires_attunement = sqlite3_column_int(stmt, 33) != 0;
    item.ac_bonus = sqlite3_column_int(stmt, 34);
    item.save_bonus = sqlite3_column_int(stmt, 35);
    item.unarmored_ac_bonus = sqlite3_column_int(stmt, 36);
    item.sets_ability = column_text(stmt, 37);
    item.sets_ability_to = sqlite3_column_int(stmt, 38);
    item.scroll_spell_id = sqlite3_column_int(stmt, 39);
    return item;
}

constexpr const char* kSpellColumns =
    "Spells.SpellID, Spells.Name, Spells.Level, Spells.School, Spells.Description, Spells.CastingTime, "
    "Spells.RangeText, Spells.DurationText, Spells.Concentration, Spells.Resolution, Spells.MeleeAttack, "
    "Spells.SaveAbility, Spells.SaveHalves, Spells.DiceCount, Spells.DiceSides, Spells.FlatBonus, "
    "Spells.AddsModifier, Spells.DamageType, Spells.Projectiles, Spells.MaxTargets, Spells.UpcastDice, "
    "Spells.UpcastProjectiles, Spells.CantripScaling, Spells.UpcastTargets, Spells.CombatSaveAdvantage, "
    "Spells.TargetCreatureType, Spells.ConsumedItem";

Spell read_spell_row(sqlite3_stmt* stmt) {
    Spell spell;
    spell.spell_id = sqlite3_column_int(stmt, 0);
    spell.name = column_text(stmt, 1);
    spell.level = sqlite3_column_int(stmt, 2);
    spell.school = column_text(stmt, 3);
    spell.description = column_text(stmt, 4);
    spell.casting_time = casting_time_from_name(column_text(stmt, 5));
    spell.range_text = column_text(stmt, 6);
    spell.duration_text = column_text(stmt, 7);
    spell.concentration = sqlite3_column_int(stmt, 8) != 0;
    spell.resolution = spell_resolution_from_name(column_text(stmt, 9));
    spell.melee_attack = sqlite3_column_int(stmt, 10) != 0;
    std::string save_ability = column_text(stmt, 11);
    if (!save_ability.empty()) spell.save_ability = ability_from_name(save_ability);
    spell.save_halves = sqlite3_column_int(stmt, 12) != 0;
    spell.dice_count = sqlite3_column_int(stmt, 13);
    spell.dice_sides = sqlite3_column_int(stmt, 14);
    spell.flat_bonus = sqlite3_column_int(stmt, 15);
    spell.adds_modifier = sqlite3_column_int(stmt, 16) != 0;
    spell.damage_type = column_text(stmt, 17);
    spell.projectiles = sqlite3_column_int(stmt, 18);
    spell.max_targets = sqlite3_column_int(stmt, 19);
    spell.upcast_dice = sqlite3_column_int(stmt, 20);
    spell.upcast_projectiles = sqlite3_column_int(stmt, 21);
    spell.cantrip_scaling = cantrip_scaling_from_name(column_text(stmt, 22));
    spell.upcast_targets = sqlite3_column_int(stmt, 23);
    spell.combat_save_advantage = sqlite3_column_int(stmt, 24) != 0;
    spell.target_creature_type = column_text(stmt, 25);
    spell.consumed_item = column_text(stmt, 26);
    if (sqlite3_column_count(stmt) > 27) spell.prepared = sqlite3_column_int(stmt, 27) != 0;  // a character's
    return spell;
}

// Runs a spell query (selecting kSpellColumns, optionally binding one int)
// and fills each spell's class_names (ClassSpells) and effects (SpellEffects).
std::vector<Spell> run_spell_query(sqlite3* db, const std::string& sql, int bind_id = -1) {
    std::vector<Spell> spells;
    {
        Statement stmt(db, sql.c_str());
        if (bind_id >= 0) sqlite3_bind_int(stmt.get(), 1, bind_id);
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) spells.push_back(read_spell_row(stmt.get()));
    }

    Statement classes(db,
        "SELECT Classes.ClassName FROM ClassSpells JOIN Classes ON Classes.ClassID = ClassSpells.ClassID "
        "WHERE ClassSpells.SpellID = ? ORDER BY Classes.ClassName;");
    Statement effects(db,
        "SELECT EffectTrigger, Kind, ConditionName, Amount, Duration, Rounds, UntilUsed, Label, RepeatSave FROM SpellEffects "
        "WHERE SpellID = ? ORDER BY SpellEffectID;");
    for (auto& spell : spells) {
        sqlite3_reset(classes.get());
        sqlite3_bind_int(classes.get(), 1, spell.spell_id);
        while (sqlite3_step(classes.get()) == SQLITE_ROW) spell.class_names.push_back(column_text(classes.get(), 0));

        sqlite3_reset(effects.get());
        sqlite3_bind_int(effects.get(), 1, spell.spell_id);
        while (sqlite3_step(effects.get()) == SQLITE_ROW) {
            SpellEffectSpec effect;
            effect.trigger = spell_effect_trigger_from_name(column_text(effects.get(), 0));
            effect.kind = effect_kind_from_name(column_text(effects.get(), 1));
            std::string condition = column_text(effects.get(), 2);
            if (!condition.empty()) effect.condition = condition_from_name(condition);
            effect.amount = sqlite3_column_int(effects.get(), 3);
            effect.duration = spell_effect_duration_from_name(column_text(effects.get(), 4));
            effect.rounds = sqlite3_column_int(effects.get(), 5);
            effect.until_used = sqlite3_column_int(effects.get(), 6) != 0;
            effect.repeat_save = sqlite3_column_int(effects.get(), 8) != 0;
            effect.label = column_text(effects.get(), 7);
            spell.effects.push_back(std::move(effect));
        }
    }
    return spells;
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
        "Intelligence, Wisdom, Charisma, ChallengeRating, CreatureType, DamageResistances, "
        "DamageVulnerabilities, DamageImmunities, ConditionImmunities, Multiattack, Traits "
        "FROM Monsters ORDER BY ChallengeRating, Name;");
    std::vector<Monster> monsters;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        Monster monster;
        monster.monster_id = sqlite3_column_int(stmt.get(), 0);
        monster.name = column_text(stmt.get(), 1);
        monster.armor_class = sqlite3_column_int(stmt.get(), 2);
        monster.hit_points = sqlite3_column_int(stmt.get(), 3);
        monster.strength = sqlite3_column_int(stmt.get(), 4);
        monster.dexterity = sqlite3_column_int(stmt.get(), 5);
        monster.constitution = sqlite3_column_int(stmt.get(), 6);
        monster.intelligence = sqlite3_column_int(stmt.get(), 7);
        monster.wisdom = sqlite3_column_int(stmt.get(), 8);
        monster.charisma = sqlite3_column_int(stmt.get(), 9);
        monster.challenge_rating = sqlite3_column_double(stmt.get(), 10);
        monster.creature_type = column_text(stmt.get(), 11);
        monster.damage.resistances = split_list(column_text(stmt.get(), 12));
        monster.damage.vulnerabilities = split_list(column_text(stmt.get(), 13));
        monster.damage.immunities = split_list(column_text(stmt.get(), 14));
        for (const auto& name : split_list(column_text(stmt.get(), 15))) {
            monster.condition_immunities.push_back(condition_from_name(name));
        }
        monster.multiattack = column_text(stmt.get(), 16);
        monster.traits = split_list(column_text(stmt.get(), 17));
        monsters.push_back(std::move(monster));
    }

    Statement attacks(db_,
        "SELECT MonsterID, Name, AttackBonus, DamageDiceCount, DamageDiceSides, DamageBonus, DamageType, Ranged, "
        "ExtraDiceCount, ExtraDiceSides, ExtraDamageType, OnHitSaveAbility, OnHitSaveDC, OnHitCondition, Count, "
        "OnHitRounds, OnHitRepeatSave FROM MonsterAttacks ORDER BY MonsterID, Position, MonsterAttackID;");
    while (sqlite3_step(attacks.get()) == SQLITE_ROW) {
        int monster_id = sqlite3_column_int(attacks.get(), 0);
        auto owner = std::find_if(monsters.begin(), monsters.end(),
                                  [&](const Monster& m) { return m.monster_id == monster_id; });
        if (owner == monsters.end()) continue;
        MonsterAttack attack;
        attack.name = column_text(attacks.get(), 1);
        attack.attack_bonus = sqlite3_column_int(attacks.get(), 2);
        attack.damage_dice_count = sqlite3_column_int(attacks.get(), 3);
        attack.damage_dice_sides = sqlite3_column_int(attacks.get(), 4);
        attack.damage_bonus = sqlite3_column_int(attacks.get(), 5);
        attack.damage_type = column_text(attacks.get(), 6);
        attack.ranged = sqlite3_column_int(attacks.get(), 7) != 0;
        attack.extra_dice_count = sqlite3_column_int(attacks.get(), 8);
        attack.extra_dice_sides = sqlite3_column_int(attacks.get(), 9);
        attack.extra_damage_type = column_text(attacks.get(), 10);
        std::string save_ability = column_text(attacks.get(), 11);
        if (!save_ability.empty()) attack.on_hit_save_ability = ability_from_name(save_ability);
        attack.on_hit_save_dc = sqlite3_column_int(attacks.get(), 12);
        std::string condition = column_text(attacks.get(), 13);
        if (!condition.empty()) attack.on_hit_condition = condition_from_name(condition);
        attack.count = std::max(1, sqlite3_column_int(attacks.get(), 14));
        attack.on_hit_rounds = sqlite3_column_int(attacks.get(), 15);
        attack.on_hit_repeat_save = sqlite3_column_int(attacks.get(), 16) != 0;
        owner->attacks.push_back(std::move(attack));
    }
    return monsters;
}


std::vector<Item> Repository::get_items() const {
    std::string sql = std::string("SELECT ") + kItemColumns + " FROM Items" + kItemJoins + " ORDER BY Category, Name;";
    Statement stmt(db_, sql.c_str());
    std::vector<Item> items;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        items.push_back(read_item_row(stmt.get()));
    }
    return items;
}

std::vector<InventoryEntry> Repository::get_inventory(int character_id) const {
    std::string sql = std::string("SELECT ") + kItemColumns +
                       ", CharacterInventory.Quantity, CharacterInventory.Equipped, CharacterInventory.Attuned "
                       "FROM CharacterInventory "
                       "JOIN Items ON Items.ItemID = CharacterInventory.ItemID" + kItemJoins +
                       " WHERE CharacterInventory.CharacterID = ? ORDER BY Items.Name;";
    Statement stmt(db_, sql.c_str());
    sqlite3_bind_int(stmt.get(), 1, character_id);
    std::vector<InventoryEntry> inventory;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        InventoryEntry entry;
        entry.item = read_item_row(stmt.get());
        entry.quantity = sqlite3_column_int(stmt.get(), kItemColumnCount);
        entry.equipped = sqlite3_column_int(stmt.get(), kItemColumnCount + 1) != 0;
        entry.attuned = sqlite3_column_int(stmt.get(), kItemColumnCount + 2) != 0;
        inventory.push_back(std::move(entry));
    }
    return inventory;
}

void Repository::set_equipped(int character_id, int item_id, bool equipped) {
    Statement stmt(db_, "UPDATE CharacterInventory SET Equipped = ? WHERE CharacterID = ? AND ItemID = ?;");
    sqlite3_bind_int(stmt.get(), 1, equipped ? 1 : 0);
    sqlite3_bind_int(stmt.get(), 2, character_id);
    sqlite3_bind_int(stmt.get(), 3, item_id);
    detail::run_and_expect_done(stmt, "set equipped");
}

void Repository::set_attuned(int character_id, int item_id, bool attuned) {
    Statement stmt(db_, "UPDATE CharacterInventory SET Attuned = ? WHERE CharacterID = ? AND ItemID = ?;");
    sqlite3_bind_int(stmt.get(), 1, attuned ? 1 : 0);
    sqlite3_bind_int(stmt.get(), 2, character_id);
    sqlite3_bind_int(stmt.get(), 3, item_id);
    detail::run_and_expect_done(stmt, "set attuned");
}

void Repository::add_to_inventory(int character_id, int item_id, int quantity) {
    if (quantity <= 0) throw std::invalid_argument("quantity must be > 0");
    Statement stmt(db_,
        "INSERT INTO CharacterInventory (CharacterID, ItemID, Quantity) VALUES (?, ?, ?) "
        "ON CONFLICT (CharacterID, ItemID) DO UPDATE SET Quantity = Quantity + excluded.Quantity;");
    sqlite3_bind_int(stmt.get(), 1, character_id);
    sqlite3_bind_int(stmt.get(), 2, item_id);
    sqlite3_bind_int(stmt.get(), 3, quantity);
    detail::run_and_expect_done(stmt, "add to inventory");
}

int Repository::remove_from_inventory(int character_id, int item_id, int quantity) {
    if (quantity <= 0) throw std::invalid_argument("quantity must be > 0");

    int held = 0;
    {
        Statement stmt(db_, "SELECT Quantity FROM CharacterInventory WHERE CharacterID = ? AND ItemID = ?;");
        sqlite3_bind_int(stmt.get(), 1, character_id);
        sqlite3_bind_int(stmt.get(), 2, item_id);
        if (sqlite3_step(stmt.get()) == SQLITE_ROW) held = sqlite3_column_int(stmt.get(), 0);
    }
    if (held == 0) return 0;

    int removed = std::min(held, quantity);
    const char* sql = removed == held
        ? "DELETE FROM CharacterInventory WHERE CharacterID = ? AND ItemID = ?;"
        : "UPDATE CharacterInventory SET Quantity = Quantity - ? WHERE CharacterID = ? AND ItemID = ?;";
    Statement stmt(db_, sql);
    int index = 1;
    if (removed != held) sqlite3_bind_int(stmt.get(), index++, removed);
    sqlite3_bind_int(stmt.get(), index++, character_id);
    sqlite3_bind_int(stmt.get(), index, item_id);
    detail::run_and_expect_done(stmt, "remove from inventory");
    return removed;
}

std::vector<Spell> Repository::get_spells() const {
    return run_spell_query(db_, std::string("SELECT ") + kSpellColumns + " FROM Spells ORDER BY Level, Name;");
}

std::vector<Spell> Repository::get_character_spells(int character_id) const {
    return run_spell_query(db_,
        std::string("SELECT ") + kSpellColumns +
            ", CharacterSpells.Prepared FROM CharacterSpells JOIN Spells ON Spells.SpellID = CharacterSpells.SpellID "
            "WHERE CharacterSpells.CharacterID = ? ORDER BY Spells.Level, Spells.Name;",
        character_id);
}

void Repository::add_character_spell(int character_id, int spell_id, bool prepared) {
    Statement stmt(db_, "INSERT OR IGNORE INTO CharacterSpells (CharacterID, SpellID, Prepared) VALUES (?, ?, ?);");
    sqlite3_bind_int(stmt.get(), 1, character_id);
    sqlite3_bind_int(stmt.get(), 2, spell_id);
    sqlite3_bind_int(stmt.get(), 3, prepared ? 1 : 0);
    detail::run_and_expect_done(stmt, "add character spell");
}

void Repository::set_spell_prepared(int character_id, int spell_id, bool prepared) {
    Statement stmt(db_, "UPDATE CharacterSpells SET Prepared = ? WHERE CharacterID = ? AND SpellID = ?;");
    sqlite3_bind_int(stmt.get(), 1, prepared ? 1 : 0);
    sqlite3_bind_int(stmt.get(), 2, character_id);
    sqlite3_bind_int(stmt.get(), 3, spell_id);
    detail::run_and_expect_done(stmt, "set spell prepared");
}

void Repository::remove_character_spell(int character_id, int spell_id) {
    Statement stmt(db_, "DELETE FROM CharacterSpells WHERE CharacterID = ? AND SpellID = ?;");
    sqlite3_bind_int(stmt.get(), 1, character_id);
    sqlite3_bind_int(stmt.get(), 2, spell_id);
    detail::run_and_expect_done(stmt, "remove character spell");
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
        "CurrentHP, MaxHP, SkillProficiencies, SaveProficiencies, "
        "Experience, HitDiceUsed, Background, SpellSlotsUsed, Dead, FeatureUses, FightingStyle, Coins, RevivalPenalty, MayPrepare, "
        "Subclass, SubclassChoice, Exhaustion, TempHP, ClassOptions) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
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
    sqlite3_bind_int(stmt.get(), 15, static_cast<int>(character.skill_proficiencies));
    sqlite3_bind_int(stmt.get(), 16, static_cast<int>(character.save_proficiencies));
    sqlite3_bind_int(stmt.get(), 17, character.experience);
    sqlite3_bind_int(stmt.get(), 18, character.hit_dice_used);
    bind_text(stmt, 19, character.background);
    bind_text(stmt, 20, format_spell_slots(character.spell_slots_used));
    sqlite3_bind_int(stmt.get(), 21, character.dead ? 1 : 0);
    bind_text(stmt, 22, format_feature_uses(character.feature_uses_spent));
    bind_text(stmt, 23, character.fighting_style);
    bind_text(stmt, 24, serialize_coins(character.coins));
    sqlite3_bind_int(stmt.get(), 25, character.revival_penalty);
    sqlite3_bind_int(stmt.get(), 26, character.may_prepare ? 1 : 0);
    bind_text(stmt, 27, character.subclass);
    bind_text(stmt, 28, character.subclass_choice);
    sqlite3_bind_int(stmt.get(), 29, character.exhaustion);
    sqlite3_bind_int(stmt.get(), 30, character.temp_hp);
    bind_text(stmt, 31, format_class_options(character.class_options));
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
        "CurrentHP = ?, MaxHP = ?, SkillProficiencies = ?, SaveProficiencies = ?, "
        "Experience = ?, HitDiceUsed = ?, Background = ?, SpellSlotsUsed = ?, Dead = ?, FeatureUses = ?, "
        "FightingStyle = ?, Coins = ?, RevivalPenalty = ?, MayPrepare = ?, Subclass = ?, SubclassChoice = ?, "
        "Exhaustion = ?, TempHP = ?, ClassOptions = ? WHERE CharacterId = ?;");
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
    sqlite3_bind_int(stmt.get(), 14, static_cast<int>(character.skill_proficiencies));
    sqlite3_bind_int(stmt.get(), 15, static_cast<int>(character.save_proficiencies));
    sqlite3_bind_int(stmt.get(), 16, character.experience);
    sqlite3_bind_int(stmt.get(), 17, character.hit_dice_used);
    bind_text(stmt, 18, character.background);
    bind_text(stmt, 19, format_spell_slots(character.spell_slots_used));
    sqlite3_bind_int(stmt.get(), 20, character.dead ? 1 : 0);
    bind_text(stmt, 21, format_feature_uses(character.feature_uses_spent));
    bind_text(stmt, 22, character.fighting_style);
    bind_text(stmt, 23, serialize_coins(character.coins));
    sqlite3_bind_int(stmt.get(), 24, character.revival_penalty);
    sqlite3_bind_int(stmt.get(), 25, character.may_prepare ? 1 : 0);
    bind_text(stmt, 26, character.subclass);
    bind_text(stmt, 27, character.subclass_choice);
    sqlite3_bind_int(stmt.get(), 28, character.exhaustion);
    sqlite3_bind_int(stmt.get(), 29, character.temp_hp);
    bind_text(stmt, 30, format_class_options(character.class_options));
    sqlite3_bind_int(stmt.get(), 31, character.character_id);
    detail::run_and_expect_done(stmt, "update character");
}

void Repository::delete_character(int character_id) {
    Statement stmt(db_, "DELETE FROM Characters WHERE CharacterId = ?;");
    sqlite3_bind_int(stmt.get(), 1, character_id);
    detail::run_and_expect_done(stmt, "delete character");
}

namespace {

std::string join_ids(const std::vector<int>& ids) {
    std::string text;
    for (int id : ids) text += (text.empty() ? "" : ",") + std::to_string(id);
    return text;
}

std::vector<int> split_ids(const std::string& text) {
    std::vector<int> ids;
    for (const auto& part : split_list(text)) {
        try {
            ids.push_back(std::stoi(part));
        } catch (...) {
            // malformed: ignored
        }
    }
    return ids;
}

}  // namespace

int Repository::add_adventure_run(const AdventureRun& run) {
    Statement stmt(db_,
        "INSERT INTO AdventureRuns (Title, NextScene, Party, Status, LongRestTaken, StartedAt, CampaignID) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);");
    bind_text(stmt, 1, run.title);
    sqlite3_bind_int(stmt.get(), 2, run.next_scene);
    bind_text(stmt, 3, join_ids(run.party));
    bind_text(stmt, 4, adventure_status_name(run.status));
    sqlite3_bind_int(stmt.get(), 5, run.long_rest_taken ? 1 : 0);
    bind_text(stmt, 6, run.started_at);
    sqlite3_bind_int(stmt.get(), 7, run.campaign_id);
    detail::run_and_expect_done(stmt, "insert adventure run");
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Repository::update_adventure_run(const AdventureRun& run) {
    Statement stmt(db_,
        "UPDATE AdventureRuns SET Title = ?, NextScene = ?, Party = ?, Status = ?, LongRestTaken = ?, StartedAt = ? "
        "WHERE AdventureRunID = ?;");
    bind_text(stmt, 1, run.title);
    sqlite3_bind_int(stmt.get(), 2, run.next_scene);
    bind_text(stmt, 3, join_ids(run.party));
    bind_text(stmt, 4, adventure_status_name(run.status));
    sqlite3_bind_int(stmt.get(), 5, run.long_rest_taken ? 1 : 0);
    bind_text(stmt, 6, run.started_at);
    sqlite3_bind_int(stmt.get(), 7, run.run_id);
    detail::run_and_expect_done(stmt, "update adventure run");
}

std::vector<AdventureRun> Repository::get_adventure_runs(bool in_progress_only) const {
    std::string sql = "SELECT AdventureRunID, Title, NextScene, Party, Status, LongRestTaken, StartedAt, CampaignID "
                      "FROM AdventureRuns";
    if (in_progress_only) sql += " WHERE Status = 'InProgress'";
    sql += " ORDER BY AdventureRunID DESC;";
    Statement stmt(db_, sql.c_str());
    std::vector<AdventureRun> runs;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        AdventureRun run;
        run.run_id = sqlite3_column_int(stmt.get(), 0);
        run.title = column_text(stmt.get(), 1);
        run.next_scene = sqlite3_column_int(stmt.get(), 2);
        run.party = split_ids(column_text(stmt.get(), 3));
        run.status = adventure_status_from_name(column_text(stmt.get(), 4));
        run.long_rest_taken = sqlite3_column_int(stmt.get(), 5) != 0;
        run.started_at = column_text(stmt.get(), 6);
        run.campaign_id = sqlite3_column_int(stmt.get(), 7);
        runs.push_back(std::move(run));
    }
    return runs;
}

// --- Campaigns -------------------------------------------------------------------

int Repository::add_campaign(const Campaign& campaign) {
    Statement stmt(db_, "INSERT INTO Campaigns (Name, Notes, Active, CreatedAt) VALUES (?, ?, 0, ?);");
    bind_text(stmt, 1, campaign.name);
    bind_text(stmt, 2, campaign.notes);
    bind_text(stmt, 3, campaign.created_at);
    detail::run_and_expect_done(stmt, "insert campaign");
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Repository::update_campaign(const Campaign& campaign) {
    Statement stmt(db_, "UPDATE Campaigns SET Name = ?, Notes = ? WHERE CampaignID = ?;");
    bind_text(stmt, 1, campaign.name);
    bind_text(stmt, 2, campaign.notes);
    sqlite3_bind_int(stmt.get(), 3, campaign.campaign_id);
    detail::run_and_expect_done(stmt, "update campaign");
}

void Repository::delete_campaign(int campaign_id) {
    with_transaction([&] {
        for (const char* sql : {"UPDATE Characters SET CampaignID = 0 WHERE CampaignID = ?;",
                                "UPDATE AdventureRuns SET CampaignID = 0 WHERE CampaignID = ?;",
                                "DELETE FROM Campaigns WHERE CampaignID = ?;"}) {
            Statement stmt(db_, sql);
            sqlite3_bind_int(stmt.get(), 1, campaign_id);
            detail::run_and_expect_done(stmt, "delete campaign");
        }
    });
}

std::vector<Campaign> Repository::get_campaigns() const {
    Statement stmt(db_, "SELECT CampaignID, Name, Notes, Active, CreatedAt FROM Campaigns ORDER BY Name;");
    std::vector<Campaign> campaigns;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        Campaign campaign;
        campaign.campaign_id = sqlite3_column_int(stmt.get(), 0);
        campaign.name = column_text(stmt.get(), 1);
        campaign.notes = column_text(stmt.get(), 2);
        campaign.active = sqlite3_column_int(stmt.get(), 3) != 0;
        campaign.created_at = column_text(stmt.get(), 4);
        campaigns.push_back(std::move(campaign));
    }
    return campaigns;
}

int Repository::active_campaign_id() const {
    Statement stmt(db_, "SELECT CampaignID FROM Campaigns WHERE Active = 1 LIMIT 1;");
    return sqlite3_step(stmt.get()) == SQLITE_ROW ? sqlite3_column_int(stmt.get(), 0) : 0;
}

void Repository::set_active_campaign(int campaign_id) {
    Statement stmt(db_, "UPDATE Campaigns SET Active = (CampaignID = ?);");
    sqlite3_bind_int(stmt.get(), 1, campaign_id);
    detail::run_and_expect_done(stmt, "set active campaign");
}

void Repository::set_character_campaign(int character_id, int campaign_id) {
    Statement stmt(db_, "UPDATE Characters SET CampaignID = ? WHERE CharacterId = ?;");
    sqlite3_bind_int(stmt.get(), 1, campaign_id);
    sqlite3_bind_int(stmt.get(), 2, character_id);
    detail::run_and_expect_done(stmt, "set character campaign");
}

void Repository::with_transaction(const std::function<void()>& work) {
    static int depth = 0;  // unique savepoint names for nested calls
    std::string name = "tx" + std::to_string(++depth);
    sqlite3_exec(db_, ("SAVEPOINT " + name + ";").c_str(), nullptr, nullptr, nullptr);
    try {
        work();
        sqlite3_exec(db_, ("RELEASE " + name + ";").c_str(), nullptr, nullptr, nullptr);
    } catch (...) {
        sqlite3_exec(db_, ("ROLLBACK TO " + name + "; RELEASE " + name + ";").c_str(), nullptr, nullptr, nullptr);
        --depth;
        throw;
    }
    --depth;
}

void Repository::backup_to(const std::string& path) const {
    sqlite3* destination = nullptr;
    if (sqlite3_open(path.c_str(), &destination) != SQLITE_OK) {
        std::string error = destination ? sqlite3_errmsg(destination) : "out of memory";
        sqlite3_close(destination);
        throw std::runtime_error("couldn't open " + path + ": " + error);
    }
    sqlite3_backup* backup = sqlite3_backup_init(destination, "main", db_, "main");
    int result = backup ? sqlite3_backup_step(backup, -1) : SQLITE_ERROR;
    if (backup) sqlite3_backup_finish(backup);
    std::string error = sqlite3_errmsg(destination);
    sqlite3_close(destination);
    if (result != SQLITE_DONE) throw std::runtime_error("couldn't back up the database to " + path + ": " + error);
}

}  // namespace dnd
