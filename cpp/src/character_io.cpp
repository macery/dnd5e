#include "dnd/character_io.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <set>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "dnd/class_features.hpp"
#include "dnd/currency.hpp"
#include "dnd/rules.hpp"

namespace dnd {

// Insertion-ordered, so files read top-down: format and version first.
using json = nlohmann::ordered_json;

namespace {

std::string name_for(const std::vector<LookupEntry>& entries, int id) {
    for (const auto& entry : entries) {
        if (entry.id == id) return entry.name;
    }
    return "";
}

int id_for(const std::vector<LookupEntry>& entries, const std::string& name) {
    for (const auto& entry : entries) {
        if (entry.name == name) return entry.id;
    }
    return 0;
}

std::string now_utc() {
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    char text[32];
    std::strftime(text, sizeof text, "%Y-%m-%dT%H:%M:%SZ", &utc);
    return text;
}

const std::array<int*, 6> ability_fields(Character& c) {
    return {&c.strength, &c.dexterity, &c.constitution, &c.intelligence, &c.wisdom, &c.charisma};
}

ExportedCharacter export_one(const Repository& repo, const Character& c, const std::vector<LookupEntry>& classes,
                             const std::vector<LookupEntry>& races, const std::vector<LookupEntry>& alignments) {
    ExportedCharacter out;
    out.name = c.character_name;
    out.class_name = name_for(classes, c.class_id);
    out.race = name_for(races, c.race);
    out.alignment = name_for(alignments, c.alignment);
    out.background = c.background;
    out.level = c.level;
    out.experience = c.experience;
    out.abilities = {c.strength, c.dexterity, c.constitution, c.intelligence, c.wisdom, c.charisma};
    out.current_hp = c.current_hp;
    out.max_hp = c.max_hp;
    out.hit_dice_used = c.hit_dice_used;
    out.dead = c.dead;
    out.revival_penalty = c.revival_penalty;
    for (int a = 0; a < kAbilityCount; ++a) {
        if (c.save_proficiencies & (1u << a)) out.saving_throws.push_back(ability_name(static_cast<Ability>(a)));
    }
    for (int s = 0; s < kSkillCount; ++s) {
        if (c.skill_proficiencies & (1u << s)) out.skills.push_back(skill_name(static_cast<Skill>(s)));
    }
    out.fighting_style = c.fighting_style;
    out.spell_slots_used = c.spell_slots_used;
    out.feature_uses_spent = c.feature_uses_spent;
    for (const auto& entry : repo.get_inventory(c.character_id)) {
        out.inventory.push_back({entry.item.name, entry.quantity, entry.equipped, entry.attuned});
    }
    for (const auto& spell : repo.get_character_spells(c.character_id)) {
        out.spells.push_back(spell.name);
        if (!spell.prepared) out.unprepared_spells.push_back(spell.name);
    }
    out.coins = c.coins;
    return out;
}

CharacterExport start_export(const Repository& repo, int player_id) {
    CharacterExport file;
    file.exported_at = now_utc();
    for (const auto& player : repo.get_players()) {
        if (player.player_id == player_id) file.player_name = player.player_name;
    }
    return file;
}

json coins_json(const std::array<int, 5>& coins) {
    json out = json::object();
    for (int i = 0; i < kCoinCount; ++i) out[coin_abbreviation(static_cast<Coin>(i))] = coins[static_cast<size_t>(i)];
    return out;
}

}  // namespace

// --- JSON ------------------------------------------------------------------------

std::string to_json(const CharacterExport& file) {
    json characters = json::array();
    for (const auto& c : file.characters) {
        json abilities = json::object();
        for (int a = 0; a < kAbilityCount; ++a) {
            abilities[ability_name(static_cast<Ability>(a))] = c.abilities[static_cast<size_t>(a)];
        }
        json inventory = json::array();
        for (const auto& item : c.inventory) {
            json entry = {{"item", item.item}, {"quantity", item.quantity}, {"equipped", item.equipped}};
            if (item.attuned) entry["attuned"] = true;
            inventory.push_back(entry);
        }
        characters.push_back({
            {"name", c.name},
            {"class", c.class_name},
            {"race", c.race},
            {"alignment", c.alignment},
            {"background", c.background},
            {"level", c.level},
            {"experience", c.experience},
            {"abilities", abilities},
            {"hp", {{"current", c.current_hp}, {"max", c.max_hp}}},
            {"hit_dice_used", c.hit_dice_used},
            {"dead", c.dead},
            {"revival_penalty", c.revival_penalty},
            {"saving_throws", c.saving_throws},
            {"skills", c.skills},
            {"fighting_style", c.fighting_style},
            {"spell_slots_used", c.spell_slots_used},
            {"feature_uses_spent", c.feature_uses_spent},
            {"inventory", inventory},
            {"spells", c.spells},
            {"unprepared_spells", c.unprepared_spells},
            {"coins", coins_json(c.coins)},
        });
    }
    json root = {
        {"format", kCharacterFileFormat},
        {"version", file.version},
        {"exported_at", file.exported_at},
        {"app_version", file.app_version},
        {"player", {{"name", file.player_name}}},
        {"characters", characters},
    };
    return root.dump(2) + "\n";
}

ParseResult parse_character_file(const std::string& text) {
    ParseResult result;
    json root;
    try {
        root = json::parse(text);
    } catch (const json::parse_error& error) {
        result.error = "This file isn't valid JSON (" + std::string(error.what()) + ").";
        return result;
    }
    if (!root.is_object() || root.value("format", std::string()) != kCharacterFileFormat) {
        result.error = "This isn't a DnD Engine character file.";
        return result;
    }

    try {
        CharacterExport file;
        file.version = root.at("version").get<int>();
        if (file.version > kCharacterFileVersion) {
            result.error = "This file was made by a newer version of the app (format version " +
                           std::to_string(file.version) + "; this version reads up to " +
                           std::to_string(kCharacterFileVersion) + "). Update the app to import it.";
            return result;
        }
        // (Upgrades from older format versions go here, one step at a time.)
        file.exported_at = root.value("exported_at", std::string());
        file.app_version = root.value("app_version", std::string());
        if (root.contains("player")) file.player_name = root.at("player").value("name", std::string());

        for (const auto& c : root.at("characters")) {
            ExportedCharacter out;
            out.name = c.at("name").get<std::string>();
            out.class_name = c.at("class").get<std::string>();
            out.race = c.at("race").get<std::string>();
            out.alignment = c.value("alignment", std::string());
            out.background = c.value("background", std::string());
            out.level = c.value("level", 1);
            out.experience = c.value("experience", 0);
            if (c.contains("abilities")) {
                for (int a = 0; a < kAbilityCount; ++a) {
                    out.abilities[static_cast<size_t>(a)] =
                        c.at("abilities").value(ability_name(static_cast<Ability>(a)), 10);
                }
            }
            if (c.contains("hp")) {
                out.current_hp = c.at("hp").value("current", 0);
                out.max_hp = c.at("hp").value("max", 0);
            }
            out.hit_dice_used = c.value("hit_dice_used", 0);
            out.dead = c.value("dead", false);
            out.revival_penalty = c.value("revival_penalty", 0);  // optional, like coins
            out.saving_throws = c.value("saving_throws", std::vector<std::string>{});
            out.skills = c.value("skills", std::vector<std::string>{});
            out.fighting_style = c.value("fighting_style", std::string());
            if (c.contains("spell_slots_used")) {
                auto slots = c.at("spell_slots_used").get<std::vector<int>>();
                for (size_t i = 0; i < slots.size() && i < out.spell_slots_used.size(); ++i) {
                    out.spell_slots_used[i] = slots[i];
                }
            }
            out.feature_uses_spent = c.value("feature_uses_spent", std::map<std::string, int>{});
            if (c.contains("inventory")) {
                for (const auto& item : c.at("inventory")) {
                    out.inventory.push_back(
                        {item.at("item").get<std::string>(), item.value("quantity", 1), item.value("equipped", false),
                         item.value("attuned", false)});
                }
            }
            out.spells = c.value("spells", std::vector<std::string>{});
            out.unprepared_spells = c.value("unprepared_spells", std::vector<std::string>{});
            if (c.contains("coins")) {  // optional: files from before currency have none
                for (int i = 0; i < kCoinCount; ++i) {
                    out.coins[static_cast<size_t>(i)] = c.at("coins").value(coin_abbreviation(static_cast<Coin>(i)), 0);
                }
            }
            file.characters.push_back(std::move(out));
        }
        result.file = std::move(file);
    } catch (const json::exception& error) {
        result.error = "This character file is damaged or incomplete (" + std::string(error.what()) + ").";
    }
    return result;
}

// --- Exporting ---------------------------------------------------------------------

CharacterExport export_player(const Repository& repo, int player_id) {
    auto players = repo.get_players();
    if (std::none_of(players.begin(), players.end(), [&](const Player& p) { return p.player_id == player_id; })) {
        throw std::invalid_argument("unknown player");
    }
    CharacterExport file = start_export(repo, player_id);
    auto classes = repo.get_classes();
    auto races = repo.get_races();
    auto alignments = repo.get_alignments();
    for (const auto& c : repo.get_characters_by_player(player_id)) {
        file.characters.push_back(export_one(repo, c, classes, races, alignments));
    }
    return file;
}

CharacterExport export_character(const Repository& repo, int character_id) {
    for (const auto& c : repo.get_characters()) {
        if (c.character_id != character_id) continue;
        CharacterExport file = start_export(repo, c.player_id);
        file.characters.push_back(export_one(repo, c, repo.get_classes(), repo.get_races(), repo.get_alignments()));
        return file;
    }
    throw std::invalid_argument("unknown character");
}

// --- Importing ---------------------------------------------------------------------

ImportPlan plan_import(const Repository& repo, const CharacterExport& file) {
    ImportPlan plan;
    plan.player_name = file.player_name.empty() ? "Imported" : file.player_name;
    for (const auto& player : repo.get_players()) {
        if (player.player_name == plan.player_name) plan.existing_player_id = player.player_id;
    }

    auto classes = repo.get_classes();
    auto races = repo.get_races();
    auto alignments = repo.get_alignments();
    auto items = repo.get_items();
    auto spells = repo.get_spells();
    std::set<std::string> taken;  // existing names, plus copy names handed out in this plan
    std::map<std::string, int> existing;
    for (const auto& c : repo.get_characters()) {
        taken.insert(c.character_name);
        existing[c.character_name] = c.character_id;
    }

    for (const auto& source : file.characters) {
        PlannedCharacter planned;
        planned.source = source;
        std::vector<std::string> missing;
        if (id_for(classes, source.class_name) == 0) missing.push_back("class \"" + source.class_name + "\"");
        if (id_for(races, source.race) == 0) missing.push_back("race \"" + source.race + "\"");
        if (id_for(alignments, source.alignment) == 0) missing.push_back("alignment \"" + source.alignment + "\"");
        if (!missing.empty()) {
            planned.importable = false;
            planned.error = "Unknown ";
            for (size_t i = 0; i < missing.size(); ++i) planned.error += (i ? ", " : "") + missing[i];
            planned.error += ".";
        } else if (source.name.empty()) {
            planned.importable = false;
            planned.error = "The character has no name.";
        } else if (source.level < 1 || source.level > 20) {
            planned.importable = false;
            planned.error = "Level " + std::to_string(source.level) + " isn't valid (1 to 20).";
        }

        for (const auto& item : source.inventory) {
            bool known = std::any_of(items.begin(), items.end(), [&](const Item& i) { return i.name == item.item; });
            if (!known) planned.warnings.push_back("Unknown item \"" + item.item + "\" won't be imported.");
        }
        for (const auto& name : source.spells) {
            bool known = std::any_of(spells.begin(), spells.end(), [&](const Spell& s) { return s.name == name; });
            if (!known) planned.warnings.push_back("Unknown spell \"" + name + "\" won't be imported.");
        }
        for (const auto& name : source.skills) {
            bool known = false;
            for (int s = 0; s < kSkillCount; ++s) known = known || name == skill_name(static_cast<Skill>(s));
            if (!known) planned.warnings.push_back("Unknown skill \"" + name + "\" won't be imported.");
        }
        if (!source.fighting_style.empty()) {
            const auto& styles = fighting_styles();
            bool known = std::any_of(styles.begin(), styles.end(),
                                     [&](const FightingStyleInfo& s) { return source.fighting_style == s.key; });
            if (!known) planned.warnings.push_back("Unknown Fighting Style \"" + source.fighting_style + "\" won't be imported.");
        }

        auto found = existing.find(source.name);
        if (found != existing.end()) {
            planned.existing_character_id = found->second;
            for (int n = 2;; ++n) {
                std::string candidate = source.name + " (" + std::to_string(n) + ")";
                if (!taken.count(candidate)) {
                    planned.copy_name = candidate;
                    break;
                }
            }
            taken.insert(planned.copy_name);
        } else {
            taken.insert(source.name);
        }
        plan.characters.push_back(std::move(planned));
    }
    return plan;
}

ImportResult apply_import(Repository& repo, const ImportPlan& plan, const ImportChoices& choices) {
    ImportResult result;
    repo.with_transaction([&] {
        result.player_id = choices.target_player_id != 0 ? choices.target_player_id
                           : plan.existing_player_id != 0 ? plan.existing_player_id
                                                          : repo.add_player(plan.player_name);
        auto classes = repo.get_classes();
        auto races = repo.get_races();
        auto alignments = repo.get_alignments();
        auto items = repo.get_items();
        auto spells = repo.get_spells();

        for (size_t i = 0; i < plan.characters.size(); ++i) {
            const auto& planned = plan.characters[i];
            const auto& source = planned.source;
            if (!planned.importable) {
                result.messages.push_back(source.name + ": not imported. " + planned.error);
                continue;
            }
            ConflictChoice choice = i < choices.conflicts.size() ? choices.conflicts[i] : ConflictChoice::ImportAsCopy;
            std::string name = source.name;
            if (planned.existing_character_id != 0) {
                if (choice == ConflictChoice::Skip) {
                    result.messages.push_back(source.name + ": skipped (a character with that name already exists).");
                    continue;
                }
                if (choice == ConflictChoice::Replace) {
                    repo.delete_character(planned.existing_character_id);
                } else {
                    name = planned.copy_name;
                }
            }

            Character c;
            c.character_name = name;
            c.player_id = result.player_id;
            c.class_id = id_for(classes, source.class_name);
            c.race = id_for(races, source.race);
            c.alignment = id_for(alignments, source.alignment);
            c.background = source.background;
            c.level = source.level;
            c.experience = source.experience;
            auto fields = ability_fields(c);
            for (size_t a = 0; a < fields.size(); ++a) *fields[a] = source.abilities[a];
            c.current_hp = source.current_hp;
            c.max_hp = source.max_hp;
            c.hit_dice_used = source.hit_dice_used;
            c.dead = source.dead;
            c.revival_penalty = source.revival_penalty;
            for (const auto& ability : source.saving_throws) {
                for (int a = 0; a < kAbilityCount; ++a) {
                    if (ability == ability_name(static_cast<Ability>(a))) c.save_proficiencies |= 1u << a;
                }
            }
            for (const auto& skill : source.skills) {
                for (int s = 0; s < kSkillCount; ++s) {
                    if (skill == skill_name(static_cast<Skill>(s))) c.skill_proficiencies |= 1u << s;
                }
            }
            const auto& styles = fighting_styles();
            if (std::any_of(styles.begin(), styles.end(),
                            [&](const FightingStyleInfo& s) { return source.fighting_style == s.key; })) {
                c.fighting_style = source.fighting_style;
            }
            c.spell_slots_used = source.spell_slots_used;
            c.feature_uses_spent = source.feature_uses_spent;
            c.coins = source.coins;
            int id = repo.add_character(c);

            for (const auto& entry : source.inventory) {
                auto item = std::find_if(items.begin(), items.end(), [&](const Item& i) { return i.name == entry.item; });
                if (item == items.end() || entry.quantity <= 0) continue;
                repo.add_to_inventory(id, item->item_id, entry.quantity);
                if (entry.equipped) repo.set_equipped(id, item->item_id, true);
                // Max HP was exported with any Amulet of Health already counted.
                if (entry.attuned) repo.set_attuned(id, item->item_id, true);
            }
            for (const auto& spell_name : source.spells) {
                auto spell = std::find_if(spells.begin(), spells.end(), [&](const Spell& s) { return s.name == spell_name; });
                bool prepared = std::find(source.unprepared_spells.begin(), source.unprepared_spells.end(),
                                          spell_name) == source.unprepared_spells.end();
                if (spell != spells.end()) repo.add_character_spell(id, spell->spell_id, prepared);
            }
            result.imported_character_ids.push_back(id);
            std::string how = planned.existing_character_id == 0          ? "imported"
                              : choice == ConflictChoice::Replace         ? "replaced the existing character"
                                                                          : "imported as \"" + name + "\"";
            result.messages.push_back(source.name + ": " + how + ".");
        }
    });
    return result;
}

}  // namespace dnd
