#include "dnd/spellbook.hpp"

#include <algorithm>

#include "dnd/spellcasting.hpp"
#include "dnd/subclass.hpp"

namespace dnd {

std::vector<Spell> spells_ready(const Repository& repo, const Character& character) {
    auto spells = repo.get_character_spells(character.character_id);
    auto always = always_prepared_spell_names(character);
    for (const auto& cantrip : class_option(character, "TomeCantrips")) always.push_back(cantrip);  // Pact of the Tome
    if (always.empty()) return spells;
    for (const auto& spell : repo.get_spells()) {
        if (std::find(always.begin(), always.end(), spell.name) == always.end()) continue;
        bool known = std::any_of(spells.begin(), spells.end(), [&](const Spell& s) { return s.spell_id == spell.spell_id; });
        if (!known) spells.push_back(spell);  // prepared, and free
    }
    std::stable_sort(spells.begin(), spells.end(), [](const Spell& a, const Spell& b) {
        return a.level != b.level ? a.level < b.level : a.name < b.name;
    });
    return spells;
}

void add_known_spell(Repository& repo, const Character& character, const ClassInfo& class_info, int spell_id) {
    auto known = repo.get_character_spells(character.character_id);
    for (const auto& spell : known) {
        if (spell.spell_id == spell_id) return;
    }
    for (const auto& spell : repo.get_spells()) {
        if (spell.spell_id != spell_id) continue;
        repo.add_character_spell(character.character_id, spell_id, starts_prepared(character, class_info, known, spell));
        return;
    }
}

PrepareResult set_prepared(Repository& repo, const Character& character, const ClassInfo& class_info, int spell_id,
                           bool prepared) {
    if (class_info.spellcasting.preparation != SpellPreparation::Spellbook) {
        return {false, "Only Wizards prepare from a spellbook."};
    }
    std::string refusal = preparation_refusal(character, class_info);
    if (!refusal.empty()) return {false, refusal};
    auto known = repo.get_character_spells(character.character_id);
    const Spell* spell = nullptr;
    for (const auto& s : known) {
        if (s.spell_id == spell_id) spell = &s;
    }
    if (!spell) return {false, "That spell isn't in the spellbook."};
    if (spell->level == 0) return {false, "Cantrips are always ready."};
    if (spell->prepared == prepared) return {true, ""};
    int limit = prepared_spell_count(character, class_info);
    if (prepared && prepared_leveled_count(known) >= limit) {
        return {false, "Already " + std::to_string(limit) + " spells prepared: unprepare one first."};
    }
    repo.set_spell_prepared(character.character_id, spell_id, prepared);
    return {true, (prepared ? "Prepared " : "Unprepared ") + spell->name + "."};
}

}  // namespace dnd
