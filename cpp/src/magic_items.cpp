#include "dnd/magic_items.hpp"

#include <algorithm>
#include <optional>

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

}  // namespace dnd
