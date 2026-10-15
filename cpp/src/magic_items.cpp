#include "dnd/magic_items.hpp"

#include <algorithm>
#include <optional>

#include "dnd/equipment.hpp"
#include "dnd/inventory.hpp"
#include "dnd/spellcasting.hpp"

namespace dnd {

namespace {

std::optional<Character> find_character(const Repository& repo, int character_id) {
    for (const auto& character : repo.get_characters()) {
        if (character.character_id == character_id) return character;
    }
    return std::nullopt;
}

const InventoryEntry* find_entry(const std::vector<InventoryEntry>& inventory, int item_id) {
    for (const auto& entry : inventory) {
        if (entry.item.item_id == item_id) return &entry;
    }
    return nullptr;
}

}  // namespace

bool item_active(const InventoryEntry& entry) {
    if (entry.item.category == ItemCategory::Wondrous) return entry.attuned;
    if (!entry.equipped) return false;
    return !entry.item.requires_attunement || entry.attuned;
}

Character with_item_scores(Character character, const std::vector<InventoryEntry>& inventory) {
    for (const auto& entry : inventory) {
        if (!item_active(entry) || entry.item.sets_ability.empty()) continue;
        int to = entry.item.sets_ability_to;
        if (entry.item.sets_ability == "Strength") character.strength = std::max(character.strength, to);
        if (entry.item.sets_ability == "Constitution") character.constitution = std::max(character.constitution, to);
    }
    return character;
}

int item_hp_bonus(const Character& character, const std::vector<InventoryEntry>& inventory) {
    int raised = with_item_scores(character, inventory).constitution;
    int per_level = ability_modifier(raised) - ability_modifier(character.constitution);
    return std::max(0, per_level) * character.level;
}

void without_item_hp(Character& character, const std::vector<InventoryEntry>& inventory,
                     const std::function<void(Character&)>& change) {
    int before = item_hp_bonus(character, inventory);
    character.max_hp -= before;
    character.current_hp -= before;
    change(character);
    int after = item_hp_bonus(character, inventory);
    character.max_hp += after;
    character.current_hp += after;
}

int item_save_bonus(const std::vector<InventoryEntry>& inventory) {
    int bonus = 0;
    for (const auto& entry : inventory) {
        if (item_active(entry)) bonus += entry.item.save_bonus;
    }
    return bonus;
}

int saving_throw_bonus(const Character& character, Ability ability, const std::vector<InventoryEntry>& inventory) {
    return saving_throw_bonus(with_item_scores(character, inventory), ability) + item_save_bonus(inventory);
}

// --- Attunement ------------------------------------------------------------------

int attuned_count(const std::vector<InventoryEntry>& inventory) {
    return static_cast<int>(
        std::count_if(inventory.begin(), inventory.end(), [](const InventoryEntry& e) { return e.attuned; }));
}

std::string attunement_refusal(const std::vector<InventoryEntry>& inventory, int item_id) {
    const InventoryEntry* entry = find_entry(inventory, item_id);
    if (!entry) return "That item isn't in the pack.";
    if (!entry->item.requires_attunement) return entry->item.name + " doesn't need attunement.";
    if (entry->attuned) return "Already attuned to " + entry->item.name + ".";
    if (attuned_count(inventory) >= kMaxAttunedItems) {
        return "Already attuned to " + std::to_string(kMaxAttunedItems) +
               " items: end attunement to one first (that takes a short rest too).";
    }
    return "";
}

AttunementResult attune(Repository& repo, int character_id, int item_id) {
    auto character = find_character(repo, character_id);
    if (!character) return {false, "That character no longer exists."};
    if (character->dead) return {false, character->character_name + " is dead."};
    auto inventory = repo.get_inventory(character_id);
    std::string refusal = attunement_refusal(inventory, item_id);
    if (!refusal.empty()) return {false, refusal};

    int before = item_hp_bonus(*character, inventory);
    for (auto& entry : inventory) {
        if (entry.item.item_id == item_id) entry.attuned = true;
    }
    int gained = item_hp_bonus(*character, inventory) - before;
    character->max_hp += gained;
    character->current_hp += gained;
    repo.with_transaction([&] {
        repo.set_attuned(character_id, item_id, true);
        if (gained != 0) repo.update_character(*character);
    });
    std::string name = find_entry(inventory, item_id)->item.name;
    std::string message = character->character_name + " attunes to the " + name + ".";
    if (gained > 0) message += " Max HP +" + std::to_string(gained) + ".";
    return {true, message};
}

AttunementResult end_attunement(Repository& repo, int character_id, int item_id) {
    auto character = find_character(repo, character_id);
    if (!character) return {false, "That character no longer exists."};
    auto inventory = repo.get_inventory(character_id);
    const InventoryEntry* entry = find_entry(inventory, item_id);
    if (!entry || !entry->attuned) return {false, "Not attuned to that item."};
    std::string name = entry->item.name;

    int before = item_hp_bonus(*character, inventory);
    for (auto& e : inventory) {
        if (e.item.item_id == item_id) e.attuned = false;
    }
    int lost = before - item_hp_bonus(*character, inventory);
    character->max_hp -= lost;
    character->current_hp = std::min(character->current_hp, character->max_hp);
    repo.with_transaction([&] {
        repo.set_attuned(character_id, item_id, false);
        if (lost != 0) repo.update_character(*character);
    });
    std::string message = character->character_name + " ends attunement to the " + name + ".";
    if (lost > 0) message += " Max HP -" + std::to_string(lost) + ".";
    return {true, message};
}

// --- Spell scrolls -----------------------------------------------------------------

int scroll_save_dc(int spell_level) {
    if (spell_level <= 2) return 13;
    if (spell_level <= 4) return 15;
    if (spell_level <= 6) return 17;
    if (spell_level <= 8) return 18;
    return 19;
}

int scroll_attack_bonus(int spell_level) {
    return scroll_save_dc(spell_level) - 8;
}

ScrollReading read_scroll(const Character& character, const ClassInfo& class_info, const Spell& spell) {
    ScrollReading reading;
    bool on_list = std::find(spell.class_names.begin(), spell.class_names.end(), class_info.name) !=
                   spell.class_names.end();
    if (!on_list) {
        reading.reason = spell.name + " isn't on the " + class_info.name + " spell list, so " +
                         character.character_name + " can't read the scroll.";
        return reading;
    }
    reading.readable = true;
    reading.save_dc = scroll_save_dc(spell.level);
    reading.attack_bonus = scroll_attack_bonus(spell.level);
    if (spell.level > 0) {
        auto slots = spell_slots(class_info, character.level);
        int highest = 0;
        for (int level = 1; level <= static_cast<int>(slots.size()); ++level) {
            if (slots[static_cast<size_t>(level - 1)] > 0) highest = level;
        }
        if (spell.level > highest) {
            reading.needs_check = true;
            reading.check_dc = 10 + spell.level;
        }
    }
    return reading;
}

// --- Reading scrolls outside combat -----------------------------------------------

std::string scroll_outside_combat_refusal(const Spell& spell) {
    if (spell.resolution == SpellResolution::Heal) return "";
    return spell.name + " has nothing to do outside a fight. Read it in combat.";
}

ScrollUse read_scroll_outside_combat(Repository& repo, const Dice& dice, int reader_id, int scroll_item_id,
                                     const std::vector<int>& target_ids) {
    auto refuse = [](const std::string& why) { return ScrollUse{false, why}; };

    auto reader = find_character(repo, reader_id);
    if (!reader) return refuse("No such character.");
    if (reader->dead) return refuse(reader->character_name + " is dead.");

    auto pack = repo.get_inventory(reader_id);
    const InventoryEntry* held = find_entry(pack, scroll_item_id);
    if (!held || held->item.category != ItemCategory::Scroll || held->quantity <= 0) {
        return refuse("That scroll isn't in the pack.");
    }
    const Spell* spell = nullptr;
    auto spells = repo.get_spells();
    for (const auto& candidate : spells) {
        if (candidate.spell_id == held->item.scroll_spell_id) spell = &candidate;
    }
    if (!spell) return refuse("That scroll's spell isn't in the rules data.");
    if (auto why = scroll_outside_combat_refusal(*spell); !why.empty()) return refuse(why);

    const ClassInfo* info = find_class_info_by_id(repo.get_classes(), reader->class_id);
    if (!info) return refuse(reader->character_name + " has no class to read it with.");
    if (wearing_armor_without_proficiency(info->name, pack, reader->subclass)) {
        return refuse(reader->character_name + " can't cast spells while wearing armor they aren't proficient with.");
    }
    ScrollReading reading = read_scroll(*reader, *info, *spell);
    if (!reading.readable) return refuse(reading.reason);

    int most = spell_max_targets(*spell, reader->level, spell->level);
    if (target_ids.empty() || static_cast<int>(target_ids.size()) > most) {
        return refuse("Choose between 1 and " + std::to_string(most) + " target" + (most == 1 ? "" : "s") + ".");
    }
    std::vector<Character> targets;
    bool anyone_hurt = false;
    for (int id : target_ids) {
        if (std::count(target_ids.begin(), target_ids.end(), id) > 1) return refuse("Each target can be chosen once.");
        auto target = find_character(repo, id);
        if (!target) return refuse("No such character.");
        if (target->dead) return refuse(target->character_name + " is dead; " + spell->name + " can't help.");
        if (target->current_hp < target->max_hp) anyone_hurt = true;
        targets.push_back(*target);
    }
    if (!anyone_hurt) return refuse("Nobody needs healing, so the scroll is kept.");

    CasterStats caster = caster_stats(with_item_scores(*reader, pack), *info);
    std::string message = reader->character_name + " reads a " + held->item.name;
    bool wasted = false;
    if (reading.needs_check) {
        int roll = dice.roll(20, 1).front();
        bool passed = roll + caster.spellcasting_modifier >= reading.check_dc;
        message += ": spellcasting check d20(" + std::to_string(roll) + ") " +
                   (caster.spellcasting_modifier < 0 ? "-" : "+") + std::to_string(std::abs(caster.spellcasting_modifier)) +
                   " vs DC " + std::to_string(reading.check_dc) + " -> " + (passed ? "success" : "failure");
        wasted = !passed;
    }

    std::vector<SpellTarget> spell_targets(targets.size());
    SpellCastResult cast;
    if (wasted) {
        message += ". The scroll crumbles to dust, its magic wasted.";
    } else {
        // Disciple of Life, as in combat.
        int bonus = reader->subclass == "Life Domain" && spell->level > 0 ? 2 + spell->level : 0;
        cast = cast_spell(dice, *spell, caster, spell->level, spell_targets);
        message += ". " + reader->character_name + " casts " + spell->name + ".";
        for (const auto& roll : cast.effects) {
            Character& target = targets[static_cast<size_t>(roll.target)];
            int healed = std::min(roll.amount + bonus, target.max_hp - target.current_hp);
            healed = std::max(healed, 0);
            target.current_hp += healed;
            message += "\n" + target.character_name + " regains " + std::to_string(healed) + " HP (now " +
                       std::to_string(target.current_hp) + "/" + std::to_string(target.max_hp) + ").";
        }
    }

    repo.with_transaction([&] {
        repo.remove_from_inventory(reader_id, scroll_item_id);
        if (!wasted) {
            for (const auto& target : targets) repo.update_character(target);
        }
    });
    return {true, message};
}

}  // namespace dnd
