#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "dnd/repository.hpp"

namespace dnd {

// Exporting and importing characters as JSON files (docs/save-load.md).
// Files name everything -- class, race, alignment, items, spells, skills,
// abilities -- so they load into any database, whatever its IDs.

constexpr const char* kCharacterFileFormat = "dnd5e-characters";
// Bump when the file layout changes, and teach parse_character_file to
// upgrade the older version.
constexpr int kCharacterFileVersion = 1;

struct ExportedItem {
    std::string item;
    int quantity = 1;
    bool equipped = false;
    bool attuned = false;  // optional in files (added after version 1 shipped)
};

// One character, entirely by name.
struct ExportedCharacter {
    std::string name;
    std::string class_name;
    std::string race;
    std::string alignment;
    std::string background;
    int level = 1;
    int experience = 0;
    std::array<int, 6> abilities{10, 10, 10, 10, 10, 10};  // STR..CHA
    int current_hp = 0;
    int max_hp = 0;
    int hit_dice_used = 0;
    bool dead = false;
    int revival_penalty = 0;  // optional in files, like coins
    std::string subclass;          // optional in files (added after version 1 shipped)
    std::string subclass_choice;
    int exhaustion = 0;
    int temp_hp = 0;
    std::vector<std::string> saving_throws;  // ability names
    std::vector<std::string> skills;         // skill names
    std::string fighting_style;
    std::array<int, 9> spell_slots_used{};
    std::map<std::string, int> feature_uses_spent;
    std::vector<ExportedItem> inventory;
    std::vector<std::string> spells;
    std::vector<std::string> unprepared_spells;  // a Wizard's spellbook spells not prepared; optional in files
    std::array<int, 5> coins{};  // cp, sp, ep, gp, pp; optional in files (added after version 1 shipped)
};

struct CharacterExport {
    int version = kCharacterFileVersion;
    std::string exported_at;  // ISO 8601 UTC
    std::string app_version;  // informational
    std::string player_name;
    std::vector<ExportedCharacter> characters;
};

std::string to_json(const CharacterExport& file);

// The parsed file, or an error for the player: not JSON, not a character
// file, made by a newer version, or a field of the wrong type.
struct ParseResult {
    std::optional<CharacterExport> file;
    std::string error;
};
ParseResult parse_character_file(const std::string& text);

// Everything about a player's characters (all of them) or one character.
// Throws std::invalid_argument for an unknown id.
CharacterExport export_player(const Repository& repo, int player_id);
CharacterExport export_character(const Repository& repo, int character_id);

// --- Importing ----------------------------------------------------------------

enum class ConflictChoice { ImportAsCopy, Replace, Skip };

struct PlannedCharacter {
    ExportedCharacter source;
    bool importable = true;
    std::string error;                // why not, when !importable (unknown class, ...)
    int existing_character_id = 0;    // a character with the same name already exists
    std::string copy_name;            // "Aria (2)": the name for ImportAsCopy
    std::vector<std::string> warnings;  // items and spells that won't come across
};

struct ImportPlan {
    std::string player_name;
    int existing_player_id = 0;  // the file's player, matched by name (0: will be created)
    std::vector<PlannedCharacter> characters;
};

// Resolves the file against the database without changing anything.
ImportPlan plan_import(const Repository& repo, const CharacterExport& file);

struct ImportChoices {
    // 0: the plan's player (matched by name, or created). Otherwise an
    // existing player's id.
    int target_player_id = 0;
    // One per plan character; missing entries default to ImportAsCopy (or
    // a plain import when there's no conflict).
    std::vector<ConflictChoice> conflicts;
};

struct ImportResult {
    int player_id = 0;
    std::vector<int> imported_character_ids;
    std::vector<std::string> messages;  // one line per character
};

// Applies the plan in one transaction: either everything imports or nothing
// changes (the exception is rethrown).
ImportResult apply_import(Repository& repo, const ImportPlan& plan, const ImportChoices& choices);

}  // namespace dnd
