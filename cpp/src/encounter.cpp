#include "dnd/encounter.hpp"

#include <algorithm>
#include <cctype>

#include "dnd/character_creation.hpp"
#include "dnd/combat.hpp"
#include "dnd/inventory.hpp"
#include "dnd/progression.hpp"
#include "dnd/rules.hpp"
#include "dnd/spellcasting.hpp"

namespace dnd {

namespace {

std::string format_modifier(int value) {
    return value >= 0 ? "+" + std::to_string(value) : std::to_string(value);
}

// Short ability tag for log lines, e.g. "DEX".
std::string ability_tag(Ability ability) {
    std::string tag = std::string(ability_name(ability)).substr(0, 3);
    for (char& c : tag) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return tag;
}

std::string hp_text(const Combatant& combatant) {
    return std::to_string(combatant.current_hp) + "/" + std::to_string(combatant.max_hp);
}

// "miss" / "hit for N" / "CRITICAL HIT for N".
std::string attack_outcome(bool hit, bool critical, int amount) {
    if (!hit) return "miss";
    return (critical ? "CRITICAL HIT for " : "hit for ") + std::to_string(amount);
}

}  // namespace

Encounter::Encounter(const Dice& dice, std::vector<LookupEntry> classes, std::vector<Weapon> weapons)
    : dice_(dice), classes_(std::move(classes)), weapons_(std::move(weapons)) {}

// --- Setup -------------------------------------------------------------------

void Encounter::add_character(const Character& character, std::vector<Spell> known_spells,
                              std::vector<InventoryEntry> inventory) {
    characters_.push_back(character);
    known_spells_[character.character_id] = std::move(known_spells);
    inventories_[character.character_id] = std::move(inventory);

    Weapon weapon;  // defaults to an unarmed-like fallback if not found
    for (const auto& candidate : weapons_) {
        if (candidate.weapon_id == character.weapon_id) {
            weapon = candidate;
            break;
        }
    }

    Combatant combatant;
    combatant.name = character.character_name;
    combatant.is_character = true;
    combatant.character_id = character.character_id;
    combatant.armor_class = character.armor_class;
    combatant.current_hp = character.current_hp;
    combatant.max_hp = character.max_hp;
    combatant.dex_modifier = ability_modifier(character.dexterity);
    combatant.attack_bonus = weapon_attack_bonus(character, weapon);
    combatant.damage_dice_count = weapon.damage_dice_count;
    combatant.damage_dice_sides = weapon.damage_dice_sides;
    combatant.damage_bonus = weapon_attack_ability_modifier(character, weapon);
    combatants_.push_back(combatant);
}

void Encounter::add_monsters(const Monster& monster, int count) {
    for (int i = 1; i <= count; ++i) {
        Combatant combatant;
        combatant.name = count > 1 ? monster.name + " #" + std::to_string(i) : monster.name;
        combatant.armor_class = monster.armor_class;
        combatant.current_hp = monster.hit_points;
        combatant.max_hp = monster.hit_points;
        combatant.dex_modifier = ability_modifier(monster.dexterity);
        combatant.attack_bonus = monster.attack_bonus;
        combatant.damage_dice_count = monster.damage_dice_count;
        combatant.damage_dice_sides = monster.damage_dice_sides;
        combatant.damage_bonus = monster.damage_bonus;
        combatant.xp = xp_for_challenge_rating(monster.challenge_rating);
        combatant.ability_scores = {monster.strength,     monster.dexterity, monster.constitution,
                                    monster.intelligence, monster.wisdom,    monster.charisma};
        combatants_.push_back(combatant);
    }
}

void Encounter::set_victory_reward(std::string victory_text, std::vector<LootGrant> loot) {
    victory_text_ = std::move(victory_text);
    loot_ = std::move(loot);
}

// --- Turns -------------------------------------------------------------------

void Encounter::roll_initiative() {
    for (auto& combatant : combatants_) {
        int d20 = dice_.roll(20, 1).front();
        combatant.initiative = d20 + combatant.dex_modifier;
    }

    turn_order_.resize(combatants_.size());
    for (size_t i = 0; i < turn_order_.size(); ++i) turn_order_[i] = static_cast<int>(i);
    std::sort(turn_order_.begin(), turn_order_.end(), [this](int a, int b) {
        return combatants_[static_cast<size_t>(a)].initiative > combatants_[static_cast<size_t>(b)].initiative;
    });
    current_turn_ = turn_order_.empty() ? -1 : 0;

    log("--- Initiative rolled. Combat begins! ---");
    advance_to_actionable_turn();
}

void Encounter::next_turn() {
    if (turn_order_.empty() || status_ != EncounterStatus::InProgress || current_turn_ < 0) return;
    current_turn_ = (current_turn_ + 1) % static_cast<int>(turn_order_.size());
    advance_to_actionable_turn();
}

void Encounter::advance_to_actionable_turn() {
    action_taken_ = false;
    if (turn_order_.empty()) return;

    // Bounded to one full lap: check_for_victory_or_defeat() should always
    // catch "everyone on one side is down" before this could loop forever, but
    // the bound is a cheap defensive guard against that invariant slipping.
    for (size_t guard = 0; guard < turn_order_.size() && status_ == EncounterStatus::InProgress; ++guard) {
        Combatant& current = combatants_[static_cast<size_t>(turn_order_[static_cast<size_t>(current_turn_)])];

        if (current.current_hp <= 0) {
            current_turn_ = (current_turn_ + 1) % static_cast<int>(turn_order_.size());
            continue;
        }

        // Dodge protects only until the start of this combatant's own next
        // turn, which is exactly now.
        current.is_dodging = false;

        if (!current.is_character) {
            resolve_monster_turn(current);
            check_for_victory_or_defeat();
            if (status_ != EncounterStatus::InProgress) break;
            current_turn_ = (current_turn_ + 1) % static_cast<int>(turn_order_.size());
            continue;
        }

        return;  // a living character's turn: wait for the player
    }
}

void Encounter::resolve_monster_turn(Combatant& monster) {
    std::vector<int> living;
    for (int i = 0; i < static_cast<int>(combatants_.size()); ++i) {
        const auto& candidate = combatants_[static_cast<size_t>(i)];
        if (candidate.is_character && candidate.current_hp > 0) living.push_back(i);
    }
    if (living.empty()) return;  // shouldn't happen; win/loss would already have caught it

    int pick = dice_.roll(static_cast<int>(living.size()), 1).front() - 1;
    Combatant& target = combatants_[static_cast<size_t>(living[static_cast<size_t>(pick)])];

    auto mode = target.is_dodging ? RollMode::Disadvantage : RollMode::Normal;
    auto result = resolve_attack(dice_, monster.attack_bonus, target.armor_class, monster.damage_dice_count,
                                 monster.damage_dice_sides, monster.damage_bonus, mode);
    if (result.hit) target.current_hp = std::max(0, target.current_hp - result.damage_total);
    sync_character_hp(target);

    log(monster.name + " attacks " + target.name + ": d20(" + std::to_string(result.attack_roll) + ") " +
        format_modifier(monster.attack_bonus) + " -> " +
        attack_outcome(result.hit, result.critical, result.damage_total) + ". " + target.name + " is now " +
        hp_text(target) + " HP.");
}

void Encounter::check_for_victory_or_defeat() {
    if (status_ != EncounterStatus::InProgress) return;

    bool has_characters = false;
    bool has_monsters = false;
    bool all_characters_down = true;
    bool all_monsters_down = true;
    for (const auto& combatant : combatants_) {
        bool down = combatant.current_hp <= 0;
        if (combatant.is_character) {
            has_characters = true;
            all_characters_down = all_characters_down && down;
        } else {
            has_monsters = true;
            all_monsters_down = all_monsters_down && down;
        }
    }

    if (has_characters && all_characters_down) {
        status_ = EncounterStatus::Defeat;
        result_message_ = "Defeat! All characters have fallen.";
        log("--- " + result_message_ + " ---");
    } else if (has_monsters && all_monsters_down) {
        status_ = EncounterStatus::Victory;
        result_message_ = "Victory! All enemies defeated.";
        if (!victory_text_.empty()) result_message_ += " " + victory_text_;
        log("--- " + result_message_ + " ---");
        award_victory_experience();
        award_loot();
    }
}

void Encounter::award_victory_experience() {
    int total_xp = 0;
    for (const auto& combatant : combatants_) {
        if (!combatant.is_character) total_xp += combatant.xp;
    }
    if (characters_.empty() || total_xp == 0) return;

    int share = total_xp / static_cast<int>(characters_.size());
    log("The party earns " + std::to_string(total_xp) + " XP (" + std::to_string(share) + " each).");

    for (auto& character : characters_) {
        int levels_gained = award_experience(character, share, hit_die_for_class_id(classes_, character.class_id));
        changed(character.character_id);

        for (auto& combatant : combatants_) {
            if (combatant.is_character && combatant.character_id == character.character_id) {
                combatant.current_hp = character.current_hp;
                combatant.max_hp = character.max_hp;
            }
        }

        if (levels_gained > 0) {
            log("*** " + character.character_name + " reaches level " + std::to_string(character.level) +
                "! Max HP is now " + std::to_string(character.max_hp) + ". ***");
            const ClassInfo* info = find_class_info_by_id(classes_, character.class_id);
            if (info && is_spellcaster(*info)) {
                log("    " + character.character_name +
                    " may be able to learn new spells -- open the Spellbook from the main window.");
            }
        }
    }
}

void Encounter::award_loot() {
    if (loot_.empty() || characters_.empty()) return;

    // First character still standing; everyone's down can't be a victory,
    // but fall back to the first character rather than dropping the loot.
    const Character* recipient = &characters_.front();
    for (const auto& character : characters_) {
        bool standing = std::any_of(combatants_.begin(), combatants_.end(), [&](const Combatant& c) {
            return c.is_character && c.character_id == character.character_id && c.current_hp > 0;
        });
        if (standing) {
            recipient = &character;
            break;
        }
    }

    std::string found;
    auto& inventory = inventories_[recipient->character_id];
    for (const auto& grant : loot_) {
        EncounterEvent event;
        event.kind = EncounterEvent::Kind::ItemGained;
        event.character_id = recipient->character_id;
        event.item_id = grant.item.item_id;
        event.quantity = grant.quantity;
        events_.push_back(event);

        auto held = std::find_if(inventory.begin(), inventory.end(),
                                 [&](const InventoryEntry& e) { return e.item.item_id == grant.item.item_id; });
        if (held != inventory.end()) {
            held->quantity += grant.quantity;
        } else {
            inventory.push_back({grant.item, grant.quantity});
        }

        if (!found.empty()) found += ", ";
        found += std::to_string(grant.quantity) + " × " + grant.item.name;
    }
    log("Loot! " + recipient->character_name + " takes: " + found + ".");
}

// --- State -------------------------------------------------------------------

const Combatant* Encounter::current() const {
    int index = current_index();
    return index < 0 ? nullptr : &combatants_[static_cast<size_t>(index)];
}

int Encounter::current_index() const {
    if (status_ != EncounterStatus::InProgress || current_turn_ < 0 || turn_order_.empty()) return -1;
    return turn_order_[static_cast<size_t>(current_turn_)];
}

bool Encounter::awaiting_action() const {
    return can_act();
}

bool Encounter::can_act() const {
    const Combatant* actor = current();
    return actor && actor->is_character && actor->current_hp > 0 && !action_taken_;
}

const Combatant* Encounter::current_character() const {
    const Combatant* actor = current();
    return actor && actor->is_character ? actor : nullptr;
}

const Character* Encounter::character(int character_id) const {
    for (const auto& character : characters_) {
        if (character.character_id == character_id) return &character;
    }
    return nullptr;
}

Character* Encounter::character_mut(int character_id) {
    for (auto& character : characters_) {
        if (character.character_id == character_id) return &character;
    }
    return nullptr;
}

std::vector<int> Encounter::attack_targets() const {
    std::vector<int> targets;
    const Combatant* actor = current();
    if (!actor) return targets;
    for (int i = 0; i < static_cast<int>(combatants_.size()); ++i) {
        const auto& candidate = combatants_[static_cast<size_t>(i)];
        if (candidate.current_hp <= 0) continue;
        if (candidate.is_character == actor->is_character) continue;  // only the opposing side
        targets.push_back(i);
    }
    return targets;
}

std::vector<InventoryEntry> Encounter::healing_items() const {
    std::vector<InventoryEntry> items;
    const Combatant* actor = current_character();
    if (!actor) return items;
    auto inventory = inventories_.find(actor->character_id);
    if (inventory == inventories_.end()) return items;
    for (const auto& entry : inventory->second) {
        if (is_healing_item(entry.item)) items.push_back(entry);
    }
    return items;
}

std::vector<Spell> Encounter::castable_spells() const {
    std::vector<Spell> spells;
    const Combatant* actor = current_character();
    const Character* caster = actor ? character(actor->character_id) : nullptr;
    const ClassInfo* info = caster ? find_class_info_by_id(classes_, caster->class_id) : nullptr;
    if (!caster || !info) return spells;

    for (const auto& spell : known_spells_.at(caster->character_id)) {
        bool castable = spell.level == 0;
        for (int level = std::max(1, spell.level); !castable && level <= kMaxSpellLevel; ++level) {
            castable = slots_remaining(*caster, *info, level) > 0;
        }
        if (castable) spells.push_back(spell);
    }
    return spells;
}

std::vector<int> Encounter::spell_slot_options(const Spell& spell) const {
    std::vector<int> levels;
    const Combatant* actor = current_character();
    const Character* caster = actor ? character(actor->character_id) : nullptr;
    const ClassInfo* info = caster ? find_class_info_by_id(classes_, caster->class_id) : nullptr;
    if (!caster || !info || spell.level == 0) return levels;
    for (int level = spell.level; level <= kMaxSpellLevel; ++level) {
        if (slots_remaining(*caster, *info, level) > 0) levels.push_back(level);
    }
    return levels;
}

std::vector<int> Encounter::spell_target_candidates(const Spell& spell) const {
    std::vector<int> candidates;
    bool heals = spell.resolution == SpellResolution::Heal;
    for (int i = 0; i < static_cast<int>(combatants_.size()); ++i) {
        const auto& candidate = combatants_[static_cast<size_t>(i)];
        // Heals go to the party (including anyone down); everything else at living enemies.
        if (heals ? !candidate.is_character : (candidate.is_character || candidate.current_hp <= 0)) continue;
        candidates.push_back(i);
    }
    return candidates;
}

// --- Actions -----------------------------------------------------------------

bool Encounter::attack(int target_index, int attack_bonus, int damage_dice_count, int damage_dice_sides,
                       int damage_bonus) {
    if (!can_act()) return false;
    auto targets = attack_targets();
    if (std::find(targets.begin(), targets.end(), target_index) == targets.end()) return false;

    const Combatant& actor = *current();
    Combatant& target = combatants_[static_cast<size_t>(target_index)];
    auto mode = target.is_dodging ? RollMode::Disadvantage : RollMode::Normal;
    auto result =
        resolve_attack(dice_, attack_bonus, target.armor_class, damage_dice_count, damage_dice_sides, damage_bonus, mode);
    if (result.hit) target.current_hp = std::max(0, target.current_hp - result.damage_total);
    sync_character_hp(target);

    log(actor.name + " attacks " + target.name + ": d20(" + std::to_string(result.attack_roll) + ") " +
        format_modifier(attack_bonus) + " -> " + attack_outcome(result.hit, result.critical, result.damage_total) +
        ". " + target.name + " is now " + hp_text(target) + " HP.");

    action_taken_ = true;
    check_for_victory_or_defeat();
    return true;
}

bool Encounter::dash() {
    if (!can_act()) return false;
    log(current()->name + " takes the Dash action.");
    action_taken_ = true;
    check_for_victory_or_defeat();
    return true;
}

bool Encounter::dodge() {
    if (!can_act()) return false;
    Combatant& actor = combatants_[static_cast<size_t>(current_index())];
    actor.is_dodging = true;
    log(actor.name + " takes the Dodge action (attacks against them have disadvantage until their next turn).");
    action_taken_ = true;
    check_for_victory_or_defeat();
    return true;
}

bool Encounter::drink_potion(int item_id) {
    if (!can_act()) return false;
    Combatant& actor = combatants_[static_cast<size_t>(current_index())];

    auto& inventory = inventories_[actor.character_id];
    auto held = std::find_if(inventory.begin(), inventory.end(), [&](const InventoryEntry& entry) {
        return entry.item.item_id == item_id && is_healing_item(entry.item) && entry.quantity > 0;
    });
    if (held == inventory.end()) {
        log(actor.name + " has no healing potions -- choose another action.");
        return false;
    }
    const Item item = held->item;
    if (--held->quantity == 0) inventory.erase(held);

    Character& drinker = *character_mut(actor.character_id);
    int regained = use_healing_item(drinker, dice_, item);
    actor.current_hp = drinker.current_hp;

    EncounterEvent used;
    used.kind = EncounterEvent::Kind::ItemUsed;
    used.character_id = actor.character_id;
    used.item_id = item.item_id;
    used.quantity = 1;
    events_.push_back(used);
    changed(actor.character_id);

    log(actor.name + " drinks a " + item.name + " and regains " + std::to_string(regained) + " HP (now " +
        hp_text(actor) + ").");
    action_taken_ = true;
    check_for_victory_or_defeat();
    return true;
}

bool Encounter::cast_spell(int spell_id, int slot_level, const std::vector<int>& target_indices) {
    if (!can_act()) return false;
    const Combatant& actor = *current();

    auto castable = castable_spells();
    auto chosen = std::find_if(castable.begin(), castable.end(), [&](const Spell& s) { return s.spell_id == spell_id; });
    if (chosen == castable.end()) {
        log(actor.name + " has no spell to cast -- choose another action.");
        return false;
    }
    const Spell spell = *chosen;
    Character& caster_character = *character_mut(actor.character_id);
    const ClassInfo& info = *find_class_info_by_id(classes_, caster_character.class_id);
    CasterStats caster = caster_stats(caster_character, info);
    int slot = spell.level == 0 ? 0 : slot_level;

    if (spell.resolution != SpellResolution::Utility) {
        int max_targets = spell_max_targets(spell, caster.level, slot);
        auto candidates = spell_target_candidates(spell);
        bool all_valid = std::all_of(target_indices.begin(), target_indices.end(), [&](int index) {
            return std::find(candidates.begin(), candidates.end(), index) != candidates.end();
        });
        if (target_indices.empty() || static_cast<int>(target_indices.size()) > max_targets || !all_valid) {
            log(spell.name + " needs 1 to " + std::to_string(max_targets) + " targets checked.");
            return false;
        }
    }
    if (slot > 0) {
        auto options = spell_slot_options(spell);
        if (std::find(options.begin(), options.end(), slot) == options.end() ||
            !spend_spell_slot(caster_character, info, slot)) {
            log("No spell slots of that level left.");
            return false;
        }
    }
    changed(actor.character_id);  // the spent slot

    std::vector<int> indices = spell.resolution == SpellResolution::Utility ? std::vector<int>{} : target_indices;
    std::vector<SpellTarget> targets;
    for (int index : indices) {
        const auto& target = combatants_[static_cast<size_t>(index)];
        SpellTarget spell_target;
        spell_target.armor_class = target.armor_class;
        spell_target.save_bonus = ability_modifier(target.ability_scores[static_cast<size_t>(spell.save_ability)]);
        targets.push_back(spell_target);
    }

    auto result = dnd::cast_spell(dice_, spell, caster, slot, targets);
    log(actor.name + " casts " + spell.name + (slot > 0 ? " (" + ordinal(slot) + "-level slot)" : "") + ".");
    if (spell.resolution == SpellResolution::Utility) {
        log("    " + spell.description);
        action_taken_ = true;
        check_for_victory_or_defeat();
        return true;
    }
    if (spell.resolution == SpellResolution::Save) {
        std::string dice;
        for (int die : result.dice) dice += (dice.empty() ? "" : " + ") + std::to_string(die);
        log("    Damage roll: " + dice);
    }

    for (const auto& effect : result.effects) {
        Combatant& target = combatants_[static_cast<size_t>(indices[static_cast<size_t>(effect.target)])];
        std::string outcome;
        switch (spell.resolution) {
            case SpellResolution::Attack:
                outcome = "d20(" + std::to_string(effect.attack_roll) + ") " +
                          format_modifier(caster.spell_attack_bonus) + " -> " +
                          attack_outcome(effect.hit, effect.critical, effect.amount);
                break;
            case SpellResolution::Save:
                outcome = ability_tag(spell.save_ability) + " save d20(" + std::to_string(effect.save_roll) + ") " +
                          format_modifier(targets[static_cast<size_t>(effect.target)].save_bonus) + " vs DC " +
                          std::to_string(caster.spell_save_dc) + " -> " + (effect.saved ? "saved" : "failed") +
                          ", takes " + std::to_string(effect.amount);
                break;
            case SpellResolution::AutoHit:
                outcome = "hit for " + std::to_string(effect.amount);
                break;
            case SpellResolution::Heal:
                outcome = "regains " + std::to_string(effect.amount) + " HP";
                break;
            case SpellResolution::Utility:
                break;
        }

        if (spell.resolution == SpellResolution::Heal) {
            target.current_hp = std::min(target.max_hp, target.current_hp + effect.amount);
        } else {
            target.current_hp = std::max(0, target.current_hp - effect.amount);
        }
        sync_character_hp(target);
        log("    " + target.name + ": " + outcome + ". Now " + hp_text(target) + " HP.");
    }

    action_taken_ = true;
    check_for_victory_or_defeat();
    return true;
}

// --- Events ------------------------------------------------------------------

std::vector<EncounterEvent> Encounter::take_events() {
    std::vector<EncounterEvent> events;
    events.swap(events_);
    return events;
}

void Encounter::sync_character_hp(const Combatant& combatant) {
    if (!combatant.is_character) return;
    if (Character* character = character_mut(combatant.character_id)) {
        character->current_hp = combatant.current_hp;
        changed(combatant.character_id);
    }
}

void Encounter::log(std::string text) {
    EncounterEvent event;
    event.kind = EncounterEvent::Kind::Log;
    event.text = std::move(text);
    events_.push_back(std::move(event));
}

void Encounter::changed(int character_id) {
    EncounterEvent event;
    event.kind = EncounterEvent::Kind::CharacterChanged;
    event.character_id = character_id;
    events_.push_back(event);
}

}  // namespace dnd
