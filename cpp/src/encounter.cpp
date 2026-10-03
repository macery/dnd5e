#include "dnd/encounter.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>

#include "dnd/character_creation.hpp"
#include "dnd/class_features.hpp"
#include "dnd/combat.hpp"
#include "dnd/currency.hpp"
#include "dnd/equipment.hpp"
#include "dnd/inventory.hpp"
#include "dnd/magic_items.hpp"
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

std::string join(const std::vector<std::string>& parts, const std::string& separator) {
    std::string text;
    for (const auto& part : parts) text += (text.empty() ? "" : separator) + part;
    return text;
}

// " with advantage (target prone)" etc., or "" for a normal roll.
// " -4 (raise dead)" for a raised character's rolls; "" otherwise.
std::string penalty_note(int penalty) {
    return penalty > 0 ? " " + format_modifier(-penalty) + " (raise dead)" : "";
}

std::string mode_note(const RollModifiers& m) {
    switch (m.mode()) {
        case RollMode::Advantage: return " with advantage (" + join(m.advantage, ", ") + ")";
        case RollMode::Disadvantage: return " with disadvantage (" + join(m.disadvantage, ", ") + ")";
        case RollMode::Normal: break;
    }
    if (!m.advantage.empty()) {
        return " (advantage and disadvantage cancel: " + join(m.advantage, ", ") + " / " +
               join(m.disadvantage, ", ") + ")";
    }
    return "";
}

// " +1d4(3)" for each bonus die rolled.
std::string bonus_note(const std::vector<int>& sides, const std::vector<int>& rolls) {
    std::string text;
    for (size_t i = 0; i < rolls.size() && i < sides.size(); ++i) {
        text += " +1d" + std::to_string(sides[i]) + "(" + std::to_string(rolls[i]) + ")";
    }
    return text;
}

// "✓○○ ✗✗○": death save successes, then failures, out of three each.
std::string death_save_pips(const DeathSaveState& saves) {
    std::string pips;
    for (int i = 0; i < 3; ++i) pips += i < saves.successes ? "✓" : "○";
    pips += " ";
    for (int i = 0; i < 3; ++i) pips += i < saves.failures ? "✗" : "○";
    return pips;
}

int sum(const std::vector<int>& values) {
    int total = 0;
    for (int value : values) total += value;
    return total;
}

}  // namespace

Encounter::Encounter(const Dice& dice, std::vector<LookupEntry> classes, std::vector<LookupEntry> races)
    : dice_(dice), classes_(std::move(classes)), races_(std::move(races)) {}

// --- Setup -------------------------------------------------------------------

void Encounter::add_character(const Character& character, std::vector<Spell> known_spells,
                              std::vector<InventoryEntry> inventory) {
    characters_.push_back(character);
    known_spells_[character.character_id] = std::move(known_spells);

    std::string class_name;
    for (const auto& entry : classes_) {
        if (entry.id == character.class_id) class_name = entry.name;
    }
    std::string race_name;
    for (const auto& entry : races_) {
        if (entry.id == character.race) race_name = entry.name;
    }
    WeaponAttack weapon = dnd::weapon_attack(character, class_name, race_name, inventory);

    Combatant combatant;
    combatant.name = character.character_name;
    combatant.is_character = true;
    combatant.character_id = character.character_id;
    combatant.armor_class = calculate_armor_class(character, class_name, inventory).total;
    combatant.current_hp = character.dead ? 0 : character.current_hp;
    combatant.max_hp = character.max_hp;
    combatant.dex_modifier = ability_modifier(character.dexterity);
    combatant.attack_name = weapon.name;
    combatant.attack_bonus = weapon.attack_bonus;
    combatant.damage_dice_count = weapon.damage_dice_count;
    combatant.damage_dice_sides = weapon.damage_dice_sides;
    combatant.damage_bonus = weapon.damage_bonus;
    combatant.melee_attack = weapon.melee;
    combatant.armor_not_proficient = wearing_armor_without_proficiency(class_name, inventory);
    combatant.attacks_per_action = attacks_per_attack_action(class_name, character.level);
    combatant.attack_uses_strength = weapon.uses_strength;
    combatant.attack_finesse_or_ranged = weapon.finesse_or_ranged;
    combatant.attack_martial_arts = weapon.martial_arts;
    combatant.attack_great_weapon_fighting = weapon.great_weapon_fighting;
    combatant.attack_damage_type = weapon.damage_type;
    combatant.race_name = race_name;
    combatant.class_name = class_name;
    combatant.level = character.level;
    combatant.sneak_attack_dice = class_name == "Rogue" ? sneak_attack_dice(character.level) : 0;
    const InventoryEntry* armor = equipped_armor(inventory);
    combatant.wearing_heavy_armor = armor && armor->item.armor && armor->item.armor->type == ArmorType::Heavy;
    if (class_name == "Monk") {
        // The Martial Arts bonus strike is always unarmed, whatever is wielded.
        auto empty_handed = inventory;
        for (auto& entry : empty_handed) {
            if (entry.item.weapon) entry.equipped = false;
        }
        WeaponAttack strike = dnd::weapon_attack(character, class_name, race_name, empty_handed);
        if (strike.martial_arts) combatant.martial_arts_strike = strike;
        combatant.unarmed_strike = strike;
    }
    combatant.death_saves.dead = character.dead;
    combatant.roll_penalty = character.revival_penalty;
    combatants_.push_back(combatant);
    inventories_[character.character_id] = std::move(inventory);

    int index = static_cast<int>(combatants_.size()) - 1;
    if (combatant.armor_not_proficient) {
        ActiveEffect untrained;
        untrained.kind = EffectKind::ArmorNotProficient;
        untrained.target = index;
        untrained.label = "Armor not proficient";
        add_effect(untrained);
    }

    // Joining at 0 HP: unconscious but stable, not dying.
    if (!character.dead && character.current_hp <= 0) {
        combatants_.back().death_saves.stable = true;
        fall_unconscious(index);
    }
}

void Encounter::add_monsters(const Monster& monster, int count) {
    for (int i = 1; i <= count; ++i) {
        Combatant combatant;
        combatant.name = count > 1 ? monster.name + " #" + std::to_string(i) : monster.name;
        combatant.armor_class = monster.armor_class;
        combatant.current_hp = monster.hit_points;
        combatant.max_hp = monster.hit_points;
        combatant.dex_modifier = ability_modifier(monster.dexterity);
        if (!monster.attacks.empty()) {  // the first attack, for display
            const MonsterAttack& first = monster.attacks.front();
            combatant.attack_name = first.name;
            combatant.attack_bonus = first.attack_bonus;
            combatant.damage_dice_count = first.damage_dice_count;
            combatant.damage_dice_sides = first.damage_dice_sides;
            combatant.damage_bonus = first.damage_bonus;
            combatant.melee_attack = !first.ranged;
            combatant.attack_damage_type = first.damage_type;
        }
        combatant.monster_attacks = monster.attacks;
        combatant.creature_type = monster.creature_type;
        combatant.damage_traits = monster.damage;
        combatant.condition_immunities = monster.condition_immunities;
        combatant.multiattack = monster.multiattack;
        combatant.traits = monster.traits;
        combatant.xp = xp_for_challenge_rating(monster.challenge_rating);
        combatant.ability_scores = {monster.strength,     monster.dexterity, monster.constitution,
                                    monster.intelligence, monster.wisdom,    monster.charisma};
        combatants_.push_back(combatant);
    }
}

void Encounter::set_victory_reward(std::string victory_text, std::vector<LootGrant> loot, int coins_cp) {
    victory_text_ = std::move(victory_text);
    loot_ = std::move(loot);
    coins_cp_ = coins_cp;
}

// --- Effects -----------------------------------------------------------------

int Encounter::add_effect(ActiveEffect effect) {
    if (effect.kind == EffectKind::Condition && effect.target >= 0 &&
        effect.target < static_cast<int>(combatants_.size())) {
        const auto& immune = combatants_[static_cast<size_t>(effect.target)].condition_immunities;
        if (std::find(immune.begin(), immune.end(), effect.condition) != immune.end()) {
            std::string condition = condition_name(effect.condition);
            for (char& c : condition) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            log("    " + combatants_[static_cast<size_t>(effect.target)].name + " is immune to being " + condition +
                ".");
            return 0;
        }
    }
    effect.id = next_effect_id_++;
    // "Until the end of X's next turn", applied during X's own turn, must
    // survive the end of this one.
    if (effect.duration.kind == DurationKind::UntilEndOfTurn && effect.duration.participant >= 0 &&
        effect.duration.participant == current_index()) {
        effect.duration.skip_current_turn = true;
    }
    effects_.push_back(std::move(effect));
    return effects_.back().id;
}

void Encounter::remove_effect(int effect_id) {
    effects_.erase(std::remove_if(effects_.begin(), effects_.end(),
                                  [effect_id](const ActiveEffect& e) { return e.id == effect_id; }),
                   effects_.end());
}

void Encounter::use_effects(const std::vector<int>& effect_ids) {
    for (int id : effect_ids) remove_effect(id);
}

void Encounter::start_concentration(int source, std::string label) {
    auto& combatant = combatants_.at(static_cast<size_t>(source));
    if (!combatant.concentration.empty()) end_concentration(source, "starts concentrating on " + label);
    combatant.concentration = std::move(label);
}

void Encounter::end_concentration(int source, const std::string& reason) {
    std::string line = drop_concentration(source, reason);
    if (!line.empty()) log(line);
}

std::string Encounter::drop_concentration(int index, const std::string& reason) {
    auto& combatant = combatants_.at(static_cast<size_t>(index));
    if (combatant.concentration.empty()) return "";
    std::string line = "    " + combatant.name + " stops concentrating on " + combatant.concentration + " (" + reason + ").";
    combatant.concentration.clear();
    effects_.erase(std::remove_if(effects_.begin(), effects_.end(),
                                  [index](const ActiveEffect& e) { return e.concentration && e.source == index; }),
                   effects_.end());
    return line;
}

int Encounter::armor_class(int index) const {
    return combatants_.at(static_cast<size_t>(index)).armor_class + armor_class_bonus(effects_, index);
}

std::string Encounter::status_text(int index) const {
    const Combatant& combatant = combatants_.at(static_cast<size_t>(index));
    const DeathSaveState& saves = combatant.death_saves;
    if (saves.dead) return "Dead";
    if (combatant.current_hp <= 0) {
        if (!combatant.is_character) return "Down";
        if (saves.stable) return "Stable";
        return "Dying " + death_save_pips(saves);
    }

    std::vector<std::string> notes;
    for (const auto& effect : effects_) {
        if (effect.target != index) continue;
        if (effect.kind == EffectKind::Dodging) {
            notes.push_back("Dodging");
        } else if (effect.kind == EffectKind::Condition) {
            notes.push_back(effect.label.empty() ? condition_name(effect.condition) : effect.label);
        } else if (!effect.label.empty()) {
            notes.push_back(effect.label);
        }
    }
    if (!combatant.concentration.empty()) notes.push_back("Concentrating: " + combatant.concentration);
    return notes.empty() ? "Up" : "Up (" + join(notes, ", ") + ")";
}

std::vector<std::string> Encounter::status_tooltip(int index) const {
    std::vector<std::string> lines;
    const Combatant& combatant = combatants_.at(static_cast<size_t>(index));
    const DeathSaveState& saves = combatant.death_saves;
    auto name_of = [this](int who) {
        return who >= 0 && who < static_cast<int>(combatants_.size()) ? combatants_[static_cast<size_t>(who)].name
                                                                       : std::string("someone");
    };

    if (saves.dead) {
        lines.push_back("Dead. Only revival magic can bring them back.");
        return lines;
    }
    if (combatant.current_hp <= 0) {
        if (!combatant.is_character) {
            lines.push_back("Defeated.");
        } else if (saves.stable) {
            lines.push_back("Stable: unconscious at 0 HP, but no longer dying. Any healing wakes them.");
        } else {
            lines.push_back("Dying: rolls a death save at the start of each turn. 10 or higher is a success; three "
                            "successes means stable, three failures means dead. A natural 20 wakes them with 1 HP; "
                            "any healing brings them back. Taking damage counts as a failure.");
            lines.push_back("Successes " + std::to_string(saves.successes) + "/3, failures " +
                            std::to_string(saves.failures) + "/3.");
        }
    }

    for (const auto& effect : effects_) {
        if (effect.target != index) continue;
        std::string name = effect.kind == EffectKind::Dodging   ? "Dodging"
                           : !effect.label.empty()              ? effect.label
                           : effect.kind == EffectKind::Condition ? condition_name(effect.condition)
                                                                  : effect_kind_name(effect.kind);
        std::string line = name + ": " + effect_description(effect);

        std::vector<std::string> notes;
        if (effect.source >= 0 && effect.source != index) notes.push_back("from " + name_of(effect.source));
        switch (effect.duration.kind) {
            case DurationKind::UntilRemoved: break;
            case DurationKind::Rounds:
                notes.push_back(std::to_string(effect.duration.rounds) +
                                (effect.duration.rounds == 1 ? " round left" : " rounds left"));
                break;
            case DurationKind::UntilStartOfTurn:
                notes.push_back("until the start of " +
                                (effect.duration.participant == index ? std::string("its") : name_of(effect.duration.participant) + "'s") +
                                " next turn");
                break;
            case DurationKind::UntilEndOfTurn:
                notes.push_back("until the end of " +
                                (effect.duration.participant == index ? std::string("its") : name_of(effect.duration.participant) + "'s") +
                                " next turn");
                break;
        }
        if (effect.duration.until_used) notes.push_back("or until used");
        if (effect.concentration) notes.push_back("while " + name_of(effect.source) + " concentrates");
        if (effect.save_to_end) {
            notes.push_back("DC " + std::to_string(effect.save_to_end->dc) + " " +
                            ability_name(effect.save_to_end->ability) + " save at the end of each turn ends it");
        }
        if (!notes.empty()) line += " (" + join(notes, "; ") + ")";
        lines.push_back(line);
    }

    if (!combatant.is_character) {
        const DamageTraits& traits = combatant.damage_traits;
        std::vector<std::string> traitLines;
        if (!combatant.creature_type.empty()) traitLines.push_back(combatant.creature_type + ".");
        if (!traits.resistances.empty()) traitLines.push_back("Resistant to " + join(traits.resistances, ", ") + ".");
        if (!traits.vulnerabilities.empty()) {
            traitLines.push_back("Vulnerable to " + join(traits.vulnerabilities, ", ") + " (double damage).");
        }
        if (!traits.immunities.empty()) traitLines.push_back("Immune to " + join(traits.immunities, ", ") + " damage.");
        if (!combatant.condition_immunities.empty()) {
            std::vector<std::string> names;
            for (Condition c : combatant.condition_immunities) names.push_back(condition_name(c));
            traitLines.push_back("Can't be " + join(names, ", ") + ".");
        }
        if (!traitLines.empty()) lines.push_back(join(traitLines, " "));
    }
    if (!combatant.concentration.empty()) {
        lines.push_back("Concentrating on " + combatant.concentration +
                        ": taking damage forces a CON save (DC 10 or half the damage) to keep it. Dropping to 0 HP "
                        "or casting another concentration spell ends it.");
    }
    if (lines.empty()) lines.push_back("No conditions or effects.");
    return lines;
}

EncounterRating Encounter::difficulty() const {
    std::vector<int> party_levels;
    std::vector<int> monster_xp;
    for (const auto& combatant : combatants_) {
        if (combatant.is_character) {
            if (!combatant.death_saves.dead) party_levels.push_back(combatant.level);
        } else {
            monster_xp.push_back(combatant.xp);
        }
    }
    return rate_encounter(party_levels, monster_xp);
}

// --- Turns -------------------------------------------------------------------

void Encounter::roll_initiative() {
    for (auto& combatant : combatants_) {
        int d20 = dice_.roll(20, 1).front();
        combatant.initiative = d20 + combatant.dex_modifier - combatant.roll_penalty;
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
    end_turn(turn_order_[static_cast<size_t>(current_turn_)]);
    if (status_ != EncounterStatus::InProgress) return;
    current_turn_ = (current_turn_ + 1) % static_cast<int>(turn_order_.size());
    advance_to_actionable_turn();
}

void Encounter::advance_to_actionable_turn() {
    turn_ = TurnState{};
    if (turn_order_.empty()) return;

    // Bounded to one full lap, so the UI gets control back even if nobody on
    // the players' side can act (everyone stunned, say); Next Turn continues.
    for (size_t guard = 0; guard < turn_order_.size() && status_ == EncounterStatus::InProgress; ++guard) {
        int index = turn_order_[static_cast<size_t>(current_turn_)];
        Combatant& current = combatants_[static_cast<size_t>(index)];
        turn_ = TurnState{};  // each combatant's turn starts fresh (monsters' once-per-turn flags too)
        auto move_on = [this, index]() {
            end_turn(index);
            current_turn_ = (current_turn_ + 1) % static_cast<int>(turn_order_.size());
        };

        if (current.death_saves.dead || (!current.is_character && current.current_hp <= 0)) {
            current_turn_ = (current_turn_ + 1) % static_cast<int>(turn_order_.size());
            continue;
        }

        start_turn(index);

        if (current.is_character && current.current_hp <= 0) {
            bool back_up = !current.death_saves.stable && resolve_death_save(index);
            check_for_victory_or_defeat();
            if (!back_up) {
                if (status_ == EncounterStatus::InProgress) move_on();
                continue;
            }
        }

        if (is_incapacitated(effects_, index)) {
            std::string why = "incapacitated";
            for (auto c : {Condition::Paralyzed, Condition::Petrified, Condition::Stunned, Condition::Incapacitated}) {
                if (has_condition(effects_, index, c)) {
                    why = condition_name(c);
                    for (char& ch : why) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                    break;
                }
            }
            log(current.name + " is " + why + " and can't act.");
            move_on();
            continue;
        }

        if (!current.is_character) {
            resolve_monster_turn(current);
            check_for_victory_or_defeat();
            if (status_ != EncounterStatus::InProgress) break;
            move_on();
            continue;
        }

        return;  // a character who can act: wait for the player
    }
}

void Encounter::start_turn(int index) {
    // Regeneration (Troll): 10 HP at the start of its turn, unless it took
    // fire or acid damage since its last one.
    Combatant& self = combatants_[static_cast<size_t>(index)];
    if (has_trait(index, "Regeneration") && self.current_hp > 0) {
        if (self.regeneration_blocked) {
            log(self.name + " doesn't regenerate (fire or acid).");
        } else if (self.current_hp < self.max_hp) {
            int before = self.current_hp;
            self.current_hp = std::min(self.max_hp, self.current_hp + 10);
            log(self.name + " regenerates " + std::to_string(self.current_hp - before) + " HP (now " +
                hp_text(self) + ").");
        }
        self.regeneration_blocked = false;
    }
    // Durations that run out now: "until the start of X's next turn", and
    // rounds counted on the source's turn.
    for (auto& effect : effects_) {
        if (effect.duration.kind == DurationKind::Rounds && effect.source == index) --effect.duration.rounds;
    }
    std::vector<std::string> expired;
    effects_.erase(std::remove_if(effects_.begin(), effects_.end(),
                                  [&](const ActiveEffect& e) {
                                      bool ends = (e.duration.kind == DurationKind::UntilStartOfTurn &&
                                                   e.duration.participant == index) ||
                                                  (e.duration.kind == DurationKind::Rounds && e.duration.rounds <= 0);
                                      if (ends && !e.label.empty() && e.kind != EffectKind::Dodging) {
                                          expired.push_back(e.label + " on " +
                                                            combatants_[static_cast<size_t>(e.target)].name);
                                      }
                                      return ends;
                                  }),
                   effects_.end());
    for (const auto& what : expired) log(what + " ends.");

    // A prone creature that can act stands up (it costs movement, which isn't tracked).
    Combatant& combatant = combatants_[static_cast<size_t>(index)];
    bool prone = std::any_of(effects_.begin(), effects_.end(), [index](const ActiveEffect& e) {
        return e.target == index && e.kind == EffectKind::Condition && e.condition == Condition::Prone;
    });
    if (prone && combatant.current_hp > 0 && !is_incapacitated(effects_, index)) {
        effects_.erase(std::remove_if(effects_.begin(), effects_.end(),
                                      [index](const ActiveEffect& e) {
                                          return e.target == index && e.kind == EffectKind::Condition &&
                                                 e.condition == Condition::Prone;
                                      }),
                       effects_.end());
        log(combatant.name + " stands up.");
    }
}

void Encounter::end_turn(int index) {
    // Rage ends if the barbarian neither attacked nor took damage since their last turn.
    Combatant& ending = combatants_[static_cast<size_t>(index)];
    if (is_raging(index) && !ending.rage_kept_up) {
        log(end_rage(index, "didn't attack or take damage this turn").substr(4));
    }
    ending.rage_kept_up = false;

    // "Until the end of X's next turn" effects end now -- unless they were
    // applied during this very turn, in which case they last one more.
    std::vector<std::string> expired;
    effects_.erase(std::remove_if(effects_.begin(), effects_.end(),
                                  [&](ActiveEffect& e) {
                                      if (e.duration.kind != DurationKind::UntilEndOfTurn ||
                                          e.duration.participant != index) {
                                          return false;
                                      }
                                      if (e.duration.skip_current_turn) {
                                          e.duration.skip_current_turn = false;
                                          return false;
                                      }
                                      if (!e.label.empty()) {
                                          expired.push_back(e.label + " on " +
                                                            combatants_[static_cast<size_t>(e.target)].name);
                                      }
                                      return true;
                                  }),
                   effects_.end());
    for (const auto& what : expired) log(what + " ends.");

    // Effects on this combatant that it can shake off with a save.
    std::vector<ActiveEffect> saves;
    for (const auto& effect : effects_) {
        if (effect.target == index && effect.save_to_end) saves.push_back(effect);
    }
    for (const auto& effect : saves) {
        std::string detail;
        bool saved = roll_save(index, effect.save_to_end->ability, effect.save_to_end->dc, detail);
        std::string what = effect.label.empty() ? "the effect" : effect.label;
        log(combatants_[static_cast<size_t>(index)].name + " tries to end " + what + ": " + detail + " -> " +
            (saved ? "success, it ends." : "failure."));
        if (saved) remove_effect(effect.id);
    }
}

bool Encounter::resolve_death_save(int index) {
    Combatant& combatant = combatants_[static_cast<size_t>(index)];
    DeathSaveResult result = roll_death_save(dice_, combatant.death_saves, -combatant.roll_penalty);
    std::string line = combatant.name + " makes a death saving throw: d20(" + std::to_string(result.roll) + ")" +
                       penalty_note(combatant.roll_penalty) + " -> ";

    if (result.regained_hp) {
        combatant.current_hp = 1;
        effects_.erase(std::remove_if(effects_.begin(), effects_.end(),
                                      [index](const ActiveEffect& e) {
                                          return e.target == index && e.kind == EffectKind::Condition &&
                                                 e.condition == Condition::Unconscious;
                                      }),
                       effects_.end());
        sync_character_hp(combatant);
        log(line + "natural 20! " + combatant.name + " regains 1 HP and wakes up.");
        return true;
    }
    if (result.died) {
        log(line + (result.roll == 1 ? "natural 1. " : "failure. ") + combatant.name +
            " has failed three death saves.");
        for (const auto& dead_line : mark_dead(index, "dies")) log(dead_line);
        return false;
    }
    if (result.became_stable) {
        log(line + "success. " + combatant.name + " is stable.");
        return false;
    }
    const auto& saves = combatant.death_saves;
    log(line + (result.roll >= 10 ? "success" : result.roll == 1 ? "natural 1, two failures" : "failure") + " (" +
        std::to_string(saves.successes) + " successes, " + std::to_string(saves.failures) + " failures).");
    return false;
}

void Encounter::fall_unconscious(int index) {
    ActiveEffect unconscious;
    unconscious.kind = EffectKind::Condition;
    unconscious.condition = Condition::Unconscious;
    unconscious.target = index;
    unconscious.label = "Unconscious";
    add_effect(unconscious);
}

std::string Encounter::revive(int index) {
    Combatant& combatant = combatants_[static_cast<size_t>(index)];
    combatant.death_saves = DeathSaveState{};
    combatant.died_this_fight = false;
    combatant.current_hp = 1;
    if (Character* character = character_mut(combatant.character_id)) {
        character->dead = false;
        character->current_hp = 1;
        changed(character->character_id);
    }
    return "returns to life with 1 HP";
}

std::vector<std::string> Encounter::mark_dead(int index, const std::string& how) {
    std::vector<std::string> lines;
    Combatant& combatant = combatants_[static_cast<size_t>(index)];
    combatant.death_saves.dead = true;
    combatant.died_this_fight = true;
    combatant.current_hp = 0;
    effects_.erase(std::remove_if(effects_.begin(), effects_.end(),
                                  [index](const ActiveEffect& e) { return e.target == index; }),
                   effects_.end());
    std::string concentration = drop_concentration(index, "died");
    if (!concentration.empty()) lines.push_back(concentration);
    if (Character* character = character_mut(combatant.character_id)) {
        character->dead = true;
        character->current_hp = 0;
        changed(character->character_id);
    }
    lines.push_back("*** " + combatant.name + " " + how + ". ***");
    return lines;
}

void Encounter::stabilize_dying() {
    for (auto& combatant : combatants_) {
        if (!combatant.is_character || combatant.current_hp > 0) continue;
        if (combatant.death_saves.dead || combatant.death_saves.stable) continue;
        combatant.death_saves = DeathSaveState{};
        combatant.death_saves.stable = true;
        log(combatant.name + " is stabilized.");
    }
}

void Encounter::resolve_monster_turn(Combatant& monster) {
    int monster_index = static_cast<int>(&monster - combatants_.data());

    // Monsters go after characters who are still up, ignoring anyone dying,
    // and can't attack whoever charmed them.
    std::vector<int> candidates;
    bool charm_blocked = false;
    for (int i = 0; i < static_cast<int>(combatants_.size()); ++i) {
        const auto& candidate = combatants_[static_cast<size_t>(i)];
        if (!candidate.is_character || candidate.current_hp <= 0) continue;
        if (charmed_by(monster_index, i)) {
            charm_blocked = true;
            continue;
        }
        candidates.push_back(i);
    }
    if (candidates.empty()) {
        if (charm_blocked) log(monster.name + " is charmed and won't attack.");
        return;
    }

    int pick = dice_.roll(static_cast<int>(candidates.size()), 1).front() - 1;
    int target = candidates[static_cast<size_t>(pick)];
    if (monster.monster_attacks.empty()) return;

    // Multiattack: every attack, each `count` times; otherwise just the first.
    std::vector<const MonsterAttack*> sequence;
    if (monster.multiattack.empty()) {
        sequence.push_back(&monster.monster_attacks.front());
    } else {
        log(monster.name + " uses Multiattack: " + monster.multiattack + ".");
        for (const auto& stat : monster.monster_attacks) {
            for (int n = 0; n < stat.count; ++n) sequence.push_back(&stat);
        }
    }
    for (const MonsterAttack* stat : sequence) {
        if (combatants_[static_cast<size_t>(target)].current_hp <= 0) {
            // The target dropped: the rest of the attacks go to someone still up.
            std::vector<int> still_up;
            for (int candidate : candidates) {
                if (combatants_[static_cast<size_t>(candidate)].current_hp > 0) still_up.push_back(candidate);
            }
            if (still_up.empty()) break;
            target = still_up[static_cast<size_t>(dice_.roll(static_cast<int>(still_up.size()), 1).front() - 1)];
        }
        WeaponAttack attack;
        attack.name = stat->name;
        attack.attack_bonus = stat->attack_bonus;
        attack.damage_dice_count = stat->damage_dice_count;
        attack.damage_dice_sides = stat->damage_dice_sides;
        attack.damage_bonus = stat->damage_bonus;
        attack.damage_type = stat->damage_type;
        attack.melee = !stat->ranged;
        weapon_attack(monster_index, target, attack, 0, stat);
        if (status_ != EncounterStatus::InProgress) break;
    }
}

int Encounter::typed_damage(int target, int amount, const std::string& type, std::string& note) {
    DamageModifier modifier = damage_modifier(damage_traits(target), type);
    if (modifier != DamageModifier::Normal && amount > 0) note += " (" + damage_modifier_note(modifier, type) + ")";
    int dealt = adjust_damage(amount, modifier);
    if (dealt > 0 && (type == "fire" || type == "acid")) {
        combatants_[static_cast<size_t>(target)].regeneration_blocked = true;
    }
    return dealt;
}

bool Encounter::has_trait(int index, const std::string& trait) const {
    const auto& traits = combatants_.at(static_cast<size_t>(index)).traits;
    return std::find(traits.begin(), traits.end(), trait) != traits.end();
}

bool Encounter::ally_fighting(int index) const {
    const Combatant& self = combatants_.at(static_cast<size_t>(index));
    for (int i = 0; i < static_cast<int>(combatants_.size()); ++i) {
        const Combatant& other = combatants_[static_cast<size_t>(i)];
        if (i != index && other.is_character == self.is_character && other.current_hp > 0 &&
            !is_incapacitated(effects_, i)) {
            return true;
        }
    }
    return false;
}

DamageTraits Encounter::damage_traits(int index) const {
    DamageTraits traits = combatants_.at(static_cast<size_t>(index)).damage_traits;
    if (is_raging(index)) {
        for (const auto& type : physical_damage_types()) traits.resistances.push_back(type);
    }
    return traits;
}

void Encounter::weapon_attack(int attacker_index, int target_index, const WeaponAttack& attack, int smite_slot,
                              const MonsterAttack* monster_attack) {
    Combatant& attacker = combatants_[static_cast<size_t>(attacker_index)];
    Combatant& target = combatants_[static_cast<size_t>(target_index)];
    int attack_bonus = attack.attack_bonus - attacker.roll_penalty;
    attacker.rage_kept_up = true;  // attacking a hostile creature keeps a rage going

    RollModifiers mods = attack_roll_modifiers(effects_, attacker_index, target_index, attack.melee, alive_flags());
    if (turn_.reckless && attacker_index == current_index() && attack.melee && attack.uses_strength) {
        mods.advantage.push_back("Reckless Attack");
    }
    if (has_trait(attacker_index, "PackTactics") && ally_fighting(attacker_index)) {
        mods.advantage.push_back("Pack Tactics");
    }
    std::vector<int> bonus_rolls;
    for (int sides : mods.bonus_dice) bonus_rolls.push_back(dice_.roll(sides, 1).front());

    auto result = resolve_attack(dice_, attack_bonus + sum(bonus_rolls), armor_class(target_index),
                                 attack.damage_dice_count, attack.damage_dice_sides, attack.damage_bonus, mods.mode(),
                                 mods.auto_crit, attack.great_weapon_fighting);
    use_effects(mods.used_effects);

    std::string extra_note;
    if (result.rerolled > 0) {
        extra_note += " (Great Weapon Fighting rerolled " + std::to_string(result.rerolled) + " " +
                      (result.rerolled == 1 ? "die" : "dice") + ")";
    }
    // Damage by type: the weapon's own (with Rage and Sneak Attack), radiant
    // (Divine Favor, Divine Smite), and a monster attack's extra dice -- each
    // adjusted for the target's resistances separately.
    int radiant = 0;
    int extra_damage = 0;
    if (result.hit) {
        // Divine Favor-style extra damage dice (doubled on a crit, like all attack dice). Radiant.
        for (const auto& effect : effects_) {
            if (effect.target != attacker_index || effect.kind != EffectKind::ExtraWeaponDamageDice) continue;
            auto extra = dice_.roll(effect.amount, result.critical ? 2 : 1);
            radiant += sum(extra);
            extra_note += " (+" + std::to_string(sum(extra)) + " " + (effect.label.empty() ? "extra" : effect.label) + ")";
        }
        if (monster_attack && monster_attack->extra_dice_count > 0) {
            auto extra = dice_.roll(monster_attack->extra_dice_sides,
                                    monster_attack->extra_dice_count * (result.critical ? 2 : 1));
            extra_damage = sum(extra);
            extra_note += " (+" + std::to_string(extra_damage) + " " + monster_attack->extra_damage_type + ")";
        }
        // Rage: extra damage on STR melee weapon attacks.
        if (is_raging(attacker_index) && attack.melee && attack.uses_strength) {
            int bonus = rage_damage_bonus(attacker.level);
            result.damage_total += bonus;
            extra_note += " (+" + std::to_string(bonus) + " Rage)";
        }
        // Sneak Attack: once per turn, finesse or ranged, with advantage -- or,
        // with no map, when another party member is fighting too (docs/class-features.md).
        if (attacker.sneak_attack_dice > 0 && attack.finesse_or_ranged && !turn_.sneak_attack_used &&
            mods.mode() != RollMode::Disadvantage) {
            if (mods.mode() == RollMode::Advantage || ally_fighting(attacker_index)) {
                turn_.sneak_attack_used = true;
                auto extra = dice_.roll(6, attacker.sneak_attack_dice * (result.critical ? 2 : 1));
                result.damage_total += sum(extra);
                extra_note += " (+" + std::to_string(sum(extra)) + " Sneak Attack, " +
                              (mods.mode() == RollMode::Advantage ? "advantage" : "an ally is fighting the target") +
                              ")";
            }
        }
        // Martial Advantage (Hobgoblin): +2d6 once per turn when an ally is fighting too.
        if (has_trait(attacker_index, "MartialAdvantage") && !turn_.martial_advantage_used &&
            ally_fighting(attacker_index)) {
            turn_.martial_advantage_used = true;
            auto extra = dice_.roll(6, result.critical ? 4 : 2);
            result.damage_total += sum(extra);
            extra_note += " (+" + std::to_string(sum(extra)) + " Martial Advantage)";
        }
        // Divine Smite: a slot for 2d8 radiant, +1d8 per slot level above 1st (max 5d8).
        if (smite_slot > 0) {
            Character* paladin = character_mut(attacker.character_id);
            const ClassInfo* info = paladin ? find_class_info_by_id(classes_, paladin->class_id) : nullptr;
            if (paladin && info && spend_spell_slot(*paladin, *info, smite_slot)) {
                changed(paladin->character_id);
                // +1d8 against undead and fiends (beyond the 5d8 cap).
                bool undead_or_fiend = target.creature_type == "Undead" || target.creature_type == "Fiend";
                int count = divine_smite_dice(smite_slot) + (undead_or_fiend ? 1 : 0);
                auto extra = dice_.roll(8, count * (result.critical ? 2 : 1));
                radiant += sum(extra);
                std::string versus = target.creature_type;
                for (char& c : versus) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                extra_note += " (+" + std::to_string(sum(extra)) + " radiant, Divine Smite " +
                              std::to_string(count * (result.critical ? 2 : 1)) + "d8" +
                              (undead_or_fiend ? " vs " + versus : std::string()) + ", level " +
                              std::to_string(smite_slot) + " slot)";
            }
        }
        // Resistance, vulnerability, immunity (Rage's resistance included).
        result.damage_total = typed_damage(target_index, result.damage_total, attack.damage_type, extra_note) +
                              typed_damage(target_index, radiant, "radiant", extra_note) +
                              (monster_attack ? typed_damage(target_index, extra_damage,
                                                             monster_attack->extra_damage_type, extra_note)
                                              : 0);
    }

    auto followups = result.hit ? apply_damage(target_index, result.damage_total, result.critical, attacker_index,
                                               radiant > 0 ? "radiant" : attack.damage_type)
                                : std::vector<std::string>{};

    log(attacker.name + " attacks " + target.name + ": d20(" + std::to_string(result.attack_roll) + ") " +
        format_modifier(attack.attack_bonus) + penalty_note(attacker.roll_penalty) +
        bonus_note(mods.bonus_dice, bonus_rolls) + mode_note(mods) + " -> " + attack_outcome(result.hit, result.critical, result.damage_total) + extra_note + ". " + target.name +
        " is now " + hp_text(target) + " HP.");
    for (const auto& line : followups) log(line);

    // A monster attack's rider (Wolf: DC 11 STR save or knocked prone).
    bool spared = monster_attack && monster_attack->on_hit_condition == Condition::Paralyzed &&
                  (target.race_name == "Elf" || target.creature_type == "Undead");  // a Ghoul's claws
    if (result.hit && monster_attack && monster_attack->on_hit_save_dc > 0 && target.current_hp > 0 && !spared) {
        std::string detail;
        bool saved = roll_save(target_index, monster_attack->on_hit_save_ability, monster_attack->on_hit_save_dc,
                               detail);
        std::string condition = condition_name(monster_attack->on_hit_condition);
        for (char& c : condition) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        log("    " + target.name + ": " + detail + " vs DC " + std::to_string(monster_attack->on_hit_save_dc) +
            " -> " + (saved ? "saved." : "failed, " + condition + "."));
        if (!saved) {
            ActiveEffect effect;
            effect.kind = EffectKind::Condition;
            effect.condition = monster_attack->on_hit_condition;
            effect.target = target_index;
            effect.source = attacker_index;
            effect.label = condition_name(monster_attack->on_hit_condition);
            if (monster_attack->on_hit_rounds > 0) {
                effect.duration.kind = DurationKind::Rounds;
                effect.duration.rounds = monster_attack->on_hit_rounds;
            }
            if (monster_attack->on_hit_repeat_save) {
                effect.save_to_end = SaveToEnd{monster_attack->on_hit_save_ability, monster_attack->on_hit_save_dc};
            }
            add_effect(effect);
        }
    }
}

std::vector<std::string> Encounter::apply_spell_effects(const Spell& spell, SpellEffectTrigger trigger, int caster,
                                                        int target, int save_dc) {
    std::vector<std::string> labels;
    for (const auto& spec : spell.effects) {
        if (spec.trigger != trigger) continue;
        ActiveEffect effect;
        effect.kind = spec.kind;
        effect.condition = spec.condition;
        effect.target = target;
        effect.source = caster;
        effect.amount = spec.amount;
        effect.concentration = spell.concentration;
        effect.label = spec.label;
        effect.duration.until_used = spec.until_used;
        if (spec.repeat_save) effect.save_to_end = SaveToEnd{spell.save_ability, save_dc};  // Hold Person
        switch (spec.duration) {
            case SpellEffectDuration::UntilRemoved:
                effect.duration.kind = DurationKind::UntilRemoved;
                break;
            case SpellEffectDuration::Rounds:
                effect.duration.kind = DurationKind::Rounds;
                effect.duration.rounds = spec.rounds;
                break;
            case SpellEffectDuration::UntilStartOfCasterTurn:
                effect.duration.kind = DurationKind::UntilStartOfTurn;
                effect.duration.participant = caster;
                break;
            case SpellEffectDuration::UntilEndOfCasterTurn:
                effect.duration.kind = DurationKind::UntilEndOfTurn;
                effect.duration.participant = caster;
                break;
            case SpellEffectDuration::UntilStartOfTargetTurn:
                effect.duration.kind = DurationKind::UntilStartOfTurn;
                effect.duration.participant = target;
                break;
            case SpellEffectDuration::UntilEndOfTargetTurn:
                effect.duration.kind = DurationKind::UntilEndOfTurn;
                effect.duration.participant = target;
                break;
        }
        if (add_effect(effect) == 0) continue;  // immune
        if (!spec.label.empty() && std::find(labels.begin(), labels.end(), spec.label) == labels.end()) {
            labels.push_back(spec.label);
        }
    }
    return labels;
}

std::vector<std::string> Encounter::apply_damage(int target_index, int amount, bool critical, int source,
                                                const std::string& damage_type) {
    std::vector<std::string> followups;
    Combatant& target = combatants_[static_cast<size_t>(target_index)];
    if (amount <= 0 || target.death_saves.dead) return followups;

    // Undead Fortitude (Zombie): damage that would drop it to 0 allows a CON
    // save (DC 5 + the damage) to stay at 1 HP -- not against radiant damage
    // or a critical hit.
    if (!target.is_character && target.current_hp > 0 && amount >= target.current_hp &&
        has_trait(target_index, "UndeadFortitude") && !critical && damage_type != "radiant") {
        std::string detail;
        int dc = 5 + amount;
        if (roll_save(target_index, Ability::Constitution, dc, detail)) {
            target.current_hp = 1;
            followups.push_back("    Undead Fortitude: " + target.name + " " + detail + " vs DC " + std::to_string(dc) +
                                " -> stays up with 1 HP!");
            return followups;
        }
        followups.push_back("    Undead Fortitude: " + target.name + " " + detail + " vs DC " + std::to_string(dc) +
                            " -> fails.");
    }

    // Any harm from the charmer's side breaks a charm (Charm Person).
    if (source >= 0) {
        bool source_is_character = combatants_[static_cast<size_t>(source)].is_character;
        auto before = effects_.size();
        effects_.erase(std::remove_if(effects_.begin(), effects_.end(),
                                      [&](const ActiveEffect& e) {
                                          return e.target == target_index && e.kind == EffectKind::Condition &&
                                                 e.condition == Condition::Charmed && e.source >= 0 &&
                                                 combatants_[static_cast<size_t>(e.source)].is_character ==
                                                     source_is_character;
                                      }),
                       effects_.end());
        if (effects_.size() != before) followups.push_back("    " + target.name + " is no longer charmed.");
    }

    if (target.is_character && target.current_hp <= 0) {
        bool was_stable = target.death_saves.stable;
        if (damage_at_zero_hp(target.death_saves, amount, critical, target.max_hp)) {
            auto lines = mark_dead(target_index, amount >= target.max_hp ? "is killed outright" : "dies");
            followups.insert(followups.end(), lines.begin(), lines.end());
        } else {
            const auto& saves = target.death_saves;
            followups.push_back("    " + target.name + (was_stable ? " is no longer stable and" : "") + " takes " +
                                (critical ? "two death save failures" : "a death save failure") + " (" +
                                std::to_string(saves.failures) + " failures).");
        }
        return followups;
    }

    int hp_before = target.current_hp;
    target.current_hp = std::max(0, target.current_hp - amount);
    target.rage_kept_up = true;  // taking damage keeps a rage going

    if (target.current_hp == 0) {
        if (target.is_character) {
            if (is_massive_damage(hp_before, amount, target.max_hp)) {
                auto lines = mark_dead(target_index, "is killed outright by massive damage");
                followups.insert(followups.end(), lines.begin(), lines.end());
                return followups;
            }
            target.death_saves = DeathSaveState{};
            std::string rage = end_rage(target_index, "knocked unconscious");
            if (!rage.empty()) followups.push_back(rage);
            fall_unconscious(target_index);
            followups.push_back("    " + target.name + " falls unconscious and is dying.");
        }
        std::string concentration = drop_concentration(target_index, "dropped to 0 HP");
        if (!concentration.empty()) followups.push_back(concentration);
        sync_character_hp(target);
        return followups;
    }

    // Still up: taking damage tests concentration.
    if (!target.concentration.empty()) {
        int dc = concentration_save_dc(amount);
        std::string detail;
        bool kept = roll_save(target_index, Ability::Constitution, dc, detail);
        followups.push_back("    " + target.name + " makes a DC " + std::to_string(dc) + " concentration save: " +
                            detail + (kept ? " -> keeps concentrating on " + target.concentration + "."
                                           : " -> fails."));
        if (!kept) followups.push_back(drop_concentration(target_index, "failed the save"));
    }
    sync_character_hp(target);
    return followups;
}

std::vector<std::string> Encounter::apply_healing(int target_index, int amount, int& healed) {
    std::vector<std::string> followups;
    healed = 0;
    Combatant& target = combatants_[static_cast<size_t>(target_index)];
    if (target.death_saves.dead) {
        followups.push_back("    " + target.name + " is dead; healing can't help.");
        return followups;
    }
    if (has_effect(effects_, target_index, EffectKind::CantRegainHitPoints)) {
        followups.push_back("    " + target.name + " can't regain hit points right now.");
        return followups;
    }

    int before = target.current_hp;
    target.current_hp = std::min(target.max_hp, target.current_hp + std::max(0, amount));
    healed = target.current_hp - before;
    if (before <= 0 && target.current_hp > 0) {
        target.death_saves = DeathSaveState{};
        effects_.erase(std::remove_if(effects_.begin(), effects_.end(),
                                      [target_index](const ActiveEffect& e) {
                                          return e.target == target_index && e.kind == EffectKind::Condition &&
                                                 e.condition == Condition::Unconscious;
                                      }),
                       effects_.end());
        followups.push_back("    " + target.name + " regains consciousness.");
    }
    sync_character_hp(target);
    return followups;
}

bool Encounter::roll_save(int target_index, Ability ability, int dc, std::string& detail) {
    RollModifiers mods = saving_throw_modifiers(effects_, target_index, ability);
    use_effects(mods.used_effects);  // e.g. a Bardic Inspiration die
    int roll = dice_.roll_d20(mods.mode()).result;
    std::vector<int> bonus_rolls;
    for (int sides : mods.bonus_dice) bonus_rolls.push_back(dice_.roll(sides, 1).front());
    int bonus = save_bonus(target_index, ability);
    detail = ability_tag(ability) + " save d20(" + std::to_string(roll) + ") " + format_modifier(bonus) +
             bonus_note(mods.bonus_dice, bonus_rolls) + mode_note(mods);
    if (mods.auto_fail) {
        detail += " (automatic failure)";
        return false;
    }
    return roll + bonus + sum(bonus_rolls) >= dc;
}

int Encounter::save_bonus(int index, Ability ability) const {
    const Combatant& combatant = combatants_.at(static_cast<size_t>(index));
    if (combatant.is_character) {
        if (const Character* character = this->character(combatant.character_id)) {
            auto inventory = inventories_.find(combatant.character_id);
            int bonus = inventory != inventories_.end() ? saving_throw_bonus(*character, ability, inventory->second)
                                                        : saving_throw_bonus(*character, ability);
            return bonus - combatant.roll_penalty;
        }
    }
    return ability_modifier(combatant.ability_scores[static_cast<size_t>(ability)]);
}

std::vector<bool> Encounter::alive_flags() const {
    std::vector<bool> alive;
    for (const auto& combatant : combatants_) alive.push_back(combatant.current_hp > 0);
    return alive;
}

bool Encounter::is_conscious(int index) const {
    return combatants_.at(static_cast<size_t>(index)).current_hp > 0;
}

bool Encounter::charmed_by(int attacker, int target) const {
    return std::any_of(effects_.begin(), effects_.end(), [&](const ActiveEffect& e) {
        return e.target == attacker && e.kind == EffectKind::Condition && e.condition == Condition::Charmed &&
               e.source == target;
    });
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
        stabilize_dying();  // the fight is lost, but no one dies after it ends
    } else if (has_monsters && all_monsters_down) {
        status_ = EncounterStatus::Victory;
        result_message_ = "Victory! All enemies defeated.";
        if (!victory_text_.empty()) result_message_ += " " + victory_text_;
        log("--- " + result_message_ + " ---");
        stabilize_dying();  // the party tends to its fallen once the fight is won
        award_victory_experience();
        award_loot();
        award_coins();
    }
    if (status_ != EncounterStatus::InProgress) {
        effects_.clear();  // combat effects end with the fight
        for (auto& combatant : combatants_) combatant.concentration.clear();
    }
}

void Encounter::award_victory_experience() {
    int total_xp = 0;
    for (const auto& combatant : combatants_) {
        if (!combatant.is_character) total_xp += combatant.xp;
    }
    // The dead don't earn XP; everyone else in the party splits it.
    int survivors = static_cast<int>(std::count_if(characters_.begin(), characters_.end(),
                                                   [](const Character& c) { return !c.dead; }));
    if (survivors == 0 || total_xp == 0) return;

    int share = total_xp / survivors;
    log("The party earns " + std::to_string(total_xp) + " XP (" + std::to_string(share) + " each).");

    for (auto& character : characters_) {
        if (character.dead) continue;
        int levels_available = award_experience(character, share);
        changed(character.character_id);
        if (levels_available > 0) {
            log("*** " + character.character_name + " has enough XP for level " +
                std::to_string(level_for_xp(character.experience)) +
                "! Use Level Up on the main window. ***");
        }
    }
}

std::vector<LootShare> suggest_loot_shares(const std::vector<LootGrant>& loot,
                                           const std::vector<LootRecipient>& recipients) {
    std::vector<LootShare> shares;
    if (recipients.empty()) return shares;
    auto give = [&shares](const Item& item, int character_id) {
        for (auto& share : shares) {
            if (share.item.item_id == item.item_id && share.character_id == character_id) {
                ++share.quantity;
                return;
            }
        }
        shares.push_back({item, 1, character_id});
    };

    // Most hurt first (lowest HP fraction; ties in party order).
    std::vector<size_t> most_hurt(recipients.size());
    for (size_t i = 0; i < most_hurt.size(); ++i) most_hurt[i] = i;
    std::stable_sort(most_hurt.begin(), most_hurt.end(), [&](size_t a, size_t b) {
        const auto& x = recipients[a];
        const auto& y = recipients[b];
        return static_cast<long long>(x.current_hp) * std::max(1, y.max_hp) <
               static_cast<long long>(y.current_hp) * std::max(1, x.max_hp);
    });

    std::vector<int> received(recipients.size(), 0);
    size_t next_potion = 0;
    for (const auto& grant : loot) {
        if (!is_healing_item(grant.item)) continue;
        for (int i = 0; i < grant.quantity; ++i) {
            size_t who = most_hurt[next_potion++ % most_hurt.size()];
            ++received[who];
            give(grant.item, recipients[who].character_id);
        }
    }
    for (const auto& grant : loot) {
        if (is_healing_item(grant.item)) continue;
        for (int i = 0; i < grant.quantity; ++i) {
            size_t who = static_cast<size_t>(std::min_element(received.begin(), received.end()) - received.begin());
            ++received[who];
            give(grant.item, recipients[who].character_id);
        }
    }
    // Back in the loot's order, so the log and the UI read like the find.
    std::vector<LootShare> ordered;
    std::set<int> seen;
    for (const auto& grant : loot) {
        if (!seen.insert(grant.item.item_id).second) continue;  // the same item listed twice
        for (const auto& share : shares) {
            if (share.item.item_id == grant.item.item_id) ordered.push_back(share);
        }
    }
    return ordered;
}

std::vector<LootRecipient> Encounter::loot_recipients() const {
    std::vector<LootRecipient> recipients;
    for (const auto& character : characters_) {
        for (const auto& c : combatants_) {
            if (c.is_character && c.character_id == character.character_id && c.current_hp > 0) {
                recipients.push_back({character.character_id, c.current_hp, c.max_hp});
                break;
            }
        }
    }
    return recipients;
}

std::vector<LootShare> Encounter::suggested_loot_shares() const {
    return suggest_loot_shares(pending_loot_, loot_recipients());
}

void Encounter::award_loot() {
    if (loot_.empty() || characters_.empty()) return;
    auto recipients = loot_recipients();
    if (recipients.size() <= 1) {
        // One character standing takes it all. (Everyone down can't be a
        // victory, but fall back to the first character rather than drop it.)
        int only = recipients.empty() ? characters_.front().character_id : recipients.front().character_id;
        std::vector<LootShare> shares;
        for (const auto& grant : loot_) shares.push_back({grant.item, grant.quantity, only});
        hand_out(shares);
        return;
    }
    pending_loot_ = loot_;
    std::string found;
    for (const auto& grant : loot_) {
        if (!found.empty()) found += ", ";
        found += std::to_string(grant.quantity) + " × " + grant.item.name;
    }
    log("Loot found: " + found + ". Divide it among the party.");
}

bool Encounter::divide_loot(const std::vector<LootShare>& shares) {
    if (pending_loot_.empty()) return false;
    auto recipients = loot_recipients();
    std::map<int, int> owed;  // item id -> quantity still to hand out
    for (const auto& grant : pending_loot_) owed[grant.item.item_id] += grant.quantity;
    for (const auto& share : shares) {
        bool standing = std::any_of(recipients.begin(), recipients.end(),
                                    [&](const LootRecipient& r) { return r.character_id == share.character_id; });
        if (!standing || share.quantity <= 0 || !owed.count(share.item.item_id)) return false;
        owed[share.item.item_id] -= share.quantity;
    }
    if (std::any_of(owed.begin(), owed.end(), [](const auto& entry) { return entry.second != 0; })) return false;

    pending_loot_.clear();
    hand_out(shares);
    return true;
}

void Encounter::hand_out(const std::vector<LootShare>& shares) {
    // One log line per character, in party order.
    for (const auto& character : characters_) {
        std::string found;
        auto& inventory = inventories_[character.character_id];
        for (const auto& share : shares) {
            if (share.character_id != character.character_id) continue;
            EncounterEvent event;
            event.kind = EncounterEvent::Kind::ItemGained;
            event.character_id = character.character_id;
            event.item_id = share.item.item_id;
            event.quantity = share.quantity;
            events_.push_back(event);

            auto held = std::find_if(inventory.begin(), inventory.end(),
                                     [&](const InventoryEntry& e) { return e.item.item_id == share.item.item_id; });
            if (held != inventory.end()) {
                held->quantity += share.quantity;
            } else {
                inventory.push_back({share.item, share.quantity});
            }

            if (!found.empty()) found += ", ";
            found += std::to_string(share.quantity) + " × " + share.item.name;
        }
        if (!found.empty()) log("Loot! " + character.character_name + " takes: " + found + ".");
    }
}

void Encounter::award_coins() {
    std::vector<Character*> survivors;
    for (auto& character : characters_) {
        if (!character.dead) survivors.push_back(&character);
    }
    if (coins_cp_ <= 0 || survivors.empty()) return;

    auto shares = split_cp(coins_cp_, static_cast<int>(survivors.size()));
    bool even = std::all_of(shares.begin(), shares.end(), [&](int share) { return share == shares.front(); });
    std::string line = "The party finds " + format_cp(coins_cp_);
    if (survivors.size() > 1 && even) line += " (" + format_cp(shares.front()) + " each)";
    std::vector<std::string> parts;
    for (size_t i = 0; i < survivors.size(); ++i) {
        add_coins(survivors[i]->coins, coins_from_cp(shares[i]));
        changed(survivors[i]->character_id);
        parts.push_back(survivors[i]->character_name + " " + format_cp(shares[i]));
    }
    if (survivors.size() > 1 && !even) line += ": " + join(parts, "; ");
    log(line + ".");
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
    return character_can_act() && (turn_.actions_left > 0 || turn_.attacks_left > 0);
}

bool Encounter::bonus_action_available() const {
    return character_can_act() && !turn_.bonus_action_used;
}

bool Encounter::character_can_act() const {
    int index = current_index();
    if (index < 0) return false;
    const Combatant& actor = combatants_[static_cast<size_t>(index)];
    return actor.is_character && actor.current_hp > 0 && !is_incapacitated(effects_, index);
}

bool Encounter::can_take_action() {
    if (!character_can_act()) return false;
    if (turn_.actions_left > 0) return true;
    log(current()->name + " has no action left this turn" +
        (turn_.attacks_left > 0 ? " (only the rest of the Attack action)." : "."));
    return false;
}

void Encounter::use_action() {
    turn_.actions_left -= 1;
    turn_.attacks_left = 0;
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
    int actor_index = current_index();
    if (actor_index < 0) return targets;
    const Combatant& actor = combatants_[static_cast<size_t>(actor_index)];
    for (int i = 0; i < static_cast<int>(combatants_.size()); ++i) {
        const auto& candidate = combatants_[static_cast<size_t>(i)];
        if (candidate.current_hp <= 0) continue;
        if (candidate.is_character == actor.is_character) continue;  // only the opposing side
        if (charmed_by(actor_index, i)) continue;
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
    if (actor && actor->armor_not_proficient) return spells;  // PHB ch. 5: no spellcasting
    if (actor && is_raging(current_index())) return spells;   // Rage: no spellcasting
    const Character* caster = actor ? character(actor->character_id) : nullptr;
    const ClassInfo* info = caster ? find_class_info_by_id(classes_, caster->class_id) : nullptr;
    if (!caster || !info) return spells;

    for (const auto& spell : known_spells_.at(caster->character_id)) {
        if (spell_casting_blocked(turn_, spell)) continue;
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
    if (spell_targets_nobody(spell)) return candidates;
    int actor_index = current_index();
    for (int i = 0; i < static_cast<int>(combatants_.size()); ++i) {
        const auto& candidate = combatants_[static_cast<size_t>(i)];
        if (spell.resolution == SpellResolution::Revive) {  // only the dead, and only this fight's
            if (candidate.is_character && candidate.death_saves.dead && candidate.died_this_fight) {
                candidates.push_back(i);
            }
            continue;
        }
        if (candidate.death_saves.dead) continue;
        bool eligible = false;
        switch (spell.resolution) {
            case SpellResolution::Heal:  // the party, including anyone down
                eligible = candidate.is_character;
                break;
            case SpellResolution::Buff:  // the party, conscious
                eligible = candidate.is_character && candidate.current_hp > 0;
                break;
            case SpellResolution::Stabilize:  // the dying
                eligible = candidate.is_character && candidate.current_hp <= 0 && !candidate.death_saves.stable;
                break;
            default:  // living enemies (not whoever charmed the caster)
                eligible = !candidate.is_character && candidate.current_hp > 0 &&
                           !(actor_index >= 0 && charmed_by(actor_index, i)) &&
                           (spell.target_creature_type.empty() ||
                            candidate.creature_type == spell.target_creature_type);
                break;
        }
        if (eligible) candidates.push_back(i);
    }
    return candidates;
}

std::vector<int> Encounter::stabilize_targets() const {
    std::vector<int> targets;
    if (!current_character()) return targets;
    for (int i = 0; i < static_cast<int>(combatants_.size()); ++i) {
        const auto& candidate = combatants_[static_cast<size_t>(i)];
        if (candidate.is_character && candidate.current_hp <= 0 && !candidate.death_saves.stable &&
            !candidate.death_saves.dead) {
            targets.push_back(i);
        }
    }
    return targets;
}

bool Encounter::has_healers_kit() const {
    const Combatant* actor = current_character();
    if (!actor) return false;
    auto inventory = inventories_.find(actor->character_id);
    if (inventory == inventories_.end()) return false;
    return std::any_of(inventory->second.begin(), inventory->second.end(), [](const InventoryEntry& entry) {
        return entry.item.name == "Healer's Kit" && entry.quantity > 0;
    });
}

// --- Class features ------------------------------------------------------------

bool Encounter::is_raging(int index) const {
    return has_effect(effects_, index, EffectKind::Raging);
}

std::string Encounter::end_rage(int index, const std::string& reason) {
    if (!is_raging(index)) return "";
    effects_.erase(std::remove_if(effects_.begin(), effects_.end(),
                                  [index](const ActiveEffect& e) { return e.target == index && e.kind == EffectKind::Raging; }),
                   effects_.end());
    return "    " + combatants_[static_cast<size_t>(index)].name + "'s rage ends (" + reason + ").";
}

const std::vector<InventoryEntry>* Encounter::inventory_of(int character_id) const {
    auto found = inventories_.find(character_id);
    return found == inventories_.end() ? nullptr : &found->second;
}

std::vector<Encounter::BonusActionOption> Encounter::bonus_action_options() const {
    std::vector<BonusActionOption> options;
    if (!bonus_action_available()) return options;
    int index = current_index();
    const Combatant& actor = combatants_[static_cast<size_t>(index)];
    const Character* character = this->character(actor.character_id);
    if (!character) return options;
    auto uses = [&](Feature feature) { return feature_uses_left(feature, *character, actor.class_name); };
    auto left = [](int n) { return " (" + std::to_string(n) + " left)"; };

    if (uses(Feature::SecondWind) > 0) {
        options.push_back({BonusAction::SecondWind, "Second Wind" + left(uses(Feature::SecondWind)),
                           feature_info(Feature::SecondWind).description, BonusTarget::None});
    }
    if (uses(Feature::Rage) > 0 && !is_raging(index) && !actor.wearing_heavy_armor) {
        options.push_back({BonusAction::Rage, "Rage" + left(uses(Feature::Rage)), feature_info(Feature::Rage).description,
                           BonusTarget::None});
    }
    if (actor.class_name == "Rogue" && actor.level >= 2) {
        options.push_back({BonusAction::CunningDash, "Cunning Action: Dash",
                           "Double your movement this turn (shown only: there's no map).", BonusTarget::None});
        options.push_back({BonusAction::CunningDisengage, "Cunning Action: Disengage",
                           "Your movement doesn't provoke opportunity attacks this turn (shown only: there's no map "
                           "or reactions yet).",
                           BonusTarget::None});
    }
    if (uses(Feature::BardicInspiration) > 0 && !bonus_action_targets(BonusTarget::Ally).empty()) {
        options.push_back({BonusAction::BardicInspiration,
                           "Bardic Inspiration, d" + std::to_string(bardic_inspiration_die(actor.level)) +
                               left(uses(Feature::BardicInspiration)),
                           feature_info(Feature::BardicInspiration).description, BonusTarget::Ally});
    }
    if (actor.martial_arts_strike && turn_.attack_action_martial &&
        !bonus_action_targets(BonusTarget::Enemy).empty()) {
        options.push_back({BonusAction::MartialArtsStrike, "Martial Arts: unarmed strike",
                           "After the Attack action with an unarmed strike or monk weapon, one unarmed strike as a "
                           "bonus action.",
                           BonusTarget::Enemy});
    }
    if (uses(Feature::Ki) > 0) {
        std::string ki = " (1 ki, " + std::to_string(uses(Feature::Ki)) + " left)";
        if (actor.unarmed_strike && turn_.attack_action_taken && !bonus_action_targets(BonusTarget::Enemy).empty()) {
            options.push_back({BonusAction::FlurryOfBlows, "Flurry of Blows" + ki,
                               "After the Attack action, two unarmed strikes as a bonus action (the second goes to "
                               "another enemy if the first drops the target).",
                               BonusTarget::Enemy});
        }
        options.push_back({BonusAction::PatientDefense, "Patient Defense" + ki,
                           "Take the Dodge action as a bonus action: attacks against you have disadvantage until "
                           "your next turn, and you have advantage on DEX saves.",
                           BonusTarget::None});
        options.push_back({BonusAction::StepOfTheWindDash, "Step of the Wind: Dash" + ki,
                           "Dash as a bonus action, and your jump distance doubles (shown only: there's no map).",
                           BonusTarget::None});
        options.push_back({BonusAction::StepOfTheWindDisengage, "Step of the Wind: Disengage" + ki,
                           "Disengage as a bonus action, and your jump distance doubles (shown only: there's no map "
                           "or reactions yet).",
                           BonusTarget::None});
    }
    return options;
}

std::vector<int> Encounter::bonus_action_targets(BonusTarget target) const {
    std::vector<int> targets;
    int actor = current_index();
    for (int i = 0; i < static_cast<int>(combatants_.size()); ++i) {
        const auto& candidate = combatants_[static_cast<size_t>(i)];
        if (candidate.current_hp <= 0) continue;
        if (target == BonusTarget::Ally && candidate.is_character && i != actor) targets.push_back(i);
        if (target == BonusTarget::Enemy && !candidate.is_character && !(actor >= 0 && charmed_by(actor, i))) {
            targets.push_back(i);
        }
    }
    return targets;
}

bool Encounter::use_bonus_action(BonusAction kind, int target) {
    if (!bonus_action_available()) {
        if (character_can_act()) log(current()->name + " has already used their bonus action this turn.");
        return false;
    }
    auto options = bonus_action_options();
    auto option = std::find_if(options.begin(), options.end(), [kind](const BonusActionOption& o) { return o.kind == kind; });
    if (option == options.end()) return false;
    if (option->target != BonusTarget::None) {
        auto targets = bonus_action_targets(option->target);
        if (std::find(targets.begin(), targets.end(), target) == targets.end()) return false;
    }

    int index = current_index();
    Combatant& actor = combatants_[static_cast<size_t>(index)];
    Character& character = *character_mut(actor.character_id);
    turn_.bonus_action_used = true;

    switch (kind) {
        case BonusAction::SecondWind: {
            spend_feature(character, Feature::SecondWind, actor.class_name);
            int roll = dice_.roll(10, 1).front();
            int healed = 0;
            auto followups = apply_healing(index, roll + second_wind_bonus(actor.level), healed);
            changed(actor.character_id);
            log(actor.name + " uses Second Wind: d10(" + std::to_string(roll) + ") + " +
                std::to_string(second_wind_bonus(actor.level)) + ", regains " + std::to_string(healed) + " HP (now " +
                hp_text(actor) + ").");
            for (const auto& line : followups) log(line);
            break;
        }
        case BonusAction::Rage: {
            spend_feature(character, Feature::Rage, actor.class_name);
            changed(actor.character_id);
            ActiveEffect rage;
            rage.kind = EffectKind::Raging;
            rage.target = index;
            rage.source = index;
            rage.duration.kind = DurationKind::Rounds;
            rage.duration.rounds = 10;
            rage.label = "Raging";
            add_effect(rage);
            actor.rage_kept_up = false;
            log(actor.name + " flies into a rage! (+" + std::to_string(rage_damage_bonus(actor.level)) +
                " damage on STR melee attacks, advantage on STR saves, physical damage halved)");
            std::string concentration = drop_concentration(index, "raging");
            if (!concentration.empty()) log(concentration);
            break;
        }
        case BonusAction::CunningDash:
            log(actor.name + " takes the Dash action as a bonus action (Cunning Action).");
            break;
        case BonusAction::CunningDisengage:
            log(actor.name + " takes the Disengage action as a bonus action (Cunning Action).");
            break;
        case BonusAction::BardicInspiration: {
            spend_feature(character, Feature::BardicInspiration, actor.class_name);
            changed(actor.character_id);
            int die = bardic_inspiration_die(actor.level);
            ActiveEffect inspiration;
            inspiration.kind = EffectKind::InspirationDie;
            inspiration.amount = die;
            inspiration.target = target;
            inspiration.source = index;
            inspiration.duration.kind = DurationKind::Rounds;
            inspiration.duration.rounds = 100;  // 10 minutes
            inspiration.duration.until_used = true;
            inspiration.label = "Inspired (d" + std::to_string(die) + ")";
            add_effect(inspiration);
            log(actor.name + " inspires " + combatants_[static_cast<size_t>(target)].name + " (Bardic Inspiration, d" +
                std::to_string(die) + " for their next attack roll or save).");
            break;
        }
        case BonusAction::MartialArtsStrike:
            weapon_attack(index, target, *actor.martial_arts_strike);
            break;
        case BonusAction::FlurryOfBlows: {
            spend_feature(character, Feature::Ki, actor.class_name);
            changed(actor.character_id);
            log(actor.name + " spends 1 ki on Flurry of Blows.");
            weapon_attack(index, target, *actor.unarmed_strike);
            int second = target;
            if (combatants_[static_cast<size_t>(target)].current_hp <= 0) {
                auto others = bonus_action_targets(BonusTarget::Enemy);
                second = others.empty() ? -1 : others.front();
            }
            if (second >= 0) weapon_attack(index, second, *actor.unarmed_strike);
            break;
        }
        case BonusAction::PatientDefense: {
            spend_feature(character, Feature::Ki, actor.class_name);
            changed(actor.character_id);
            ActiveEffect dodging;
            dodging.kind = EffectKind::Dodging;
            dodging.target = index;
            dodging.source = index;
            dodging.duration.kind = DurationKind::UntilStartOfTurn;
            dodging.duration.participant = index;
            add_effect(dodging);
            log(actor.name + " spends 1 ki on Patient Defense and takes the Dodge action as a bonus action.");
            break;
        }
        case BonusAction::StepOfTheWindDash:
            spend_feature(character, Feature::Ki, actor.class_name);
            changed(actor.character_id);
            log(actor.name + " spends 1 ki on Step of the Wind and takes the Dash action as a bonus action.");
            break;
        case BonusAction::StepOfTheWindDisengage:
            spend_feature(character, Feature::Ki, actor.class_name);
            changed(actor.character_id);
            log(actor.name + " spends 1 ki on Step of the Wind and takes the Disengage action as a bonus action.");
            break;
    }
    check_for_victory_or_defeat();
    return true;
}

bool Encounter::can_action_surge() const {
    if (!character_can_act() || turn_.action_surge_used) return false;
    const Combatant& actor = *current();
    const Character* character = this->character(actor.character_id);
    return character && feature_uses_left(Feature::ActionSurge, *character, actor.class_name) > 0;
}

bool Encounter::action_surge() {
    if (!can_action_surge()) return false;
    const Combatant& actor = *current();
    spend_feature(*character_mut(actor.character_id), Feature::ActionSurge, actor.class_name);
    changed(actor.character_id);
    turn_.actions_left += 1;
    turn_.action_surge_used = true;
    log(actor.name + " uses Action Surge: one more action this turn.");
    return true;
}

bool Encounter::can_attack_recklessly() const {
    if (!character_can_act() || turn_.attack_action_taken || turn_.reckless) return false;
    const Combatant& actor = *current();
    return actor.class_name == "Barbarian" && actor.level >= 2 && actor.melee_attack && actor.attack_uses_strength;
}

std::vector<int> Encounter::divine_smite_slots() const {
    std::vector<int> levels;
    const Combatant* actor = current_character();
    if (!actor || actor->class_name != "Paladin" || actor->level < 2 || !actor->melee_attack) return levels;
    const Character* paladin = character(actor->character_id);
    const ClassInfo* info = paladin ? find_class_info_by_id(classes_, paladin->class_id) : nullptr;
    if (!info) return levels;
    for (int level = 1; level <= kMaxSpellLevel; ++level) {
        if (slots_remaining(*paladin, *info, level) > 0) levels.push_back(level);
    }
    return levels;
}

int Encounter::lay_on_hands_pool() const {
    const Combatant* actor = current_character();
    const Character* paladin = actor ? character(actor->character_id) : nullptr;
    return paladin ? feature_uses_left(Feature::LayOnHands, *paladin, actor->class_name) : 0;
}

std::vector<int> Encounter::lay_on_hands_targets() const {
    std::vector<int> targets;
    if (lay_on_hands_pool() <= 0) return targets;
    for (int i = 0; i < static_cast<int>(combatants_.size()); ++i) {
        const auto& candidate = combatants_[static_cast<size_t>(i)];
        if (candidate.is_character && !candidate.death_saves.dead) targets.push_back(i);
    }
    return targets;
}

bool Encounter::lay_on_hands(int target, int amount) {
    if (!character_can_act()) return false;
    auto targets = lay_on_hands_targets();
    if (std::find(targets.begin(), targets.end(), target) == targets.end()) return false;
    if (amount <= 0 || amount > lay_on_hands_pool()) return false;
    if (!can_take_action()) return false;

    int index = current_index();
    Combatant& actor = combatants_[static_cast<size_t>(index)];
    spend_feature(*character_mut(actor.character_id), Feature::LayOnHands, actor.class_name, amount);
    changed(actor.character_id);
    int healed = 0;
    auto followups = apply_healing(target, amount, healed);
    const Combatant& patient = combatants_[static_cast<size_t>(target)];
    log(actor.name + " uses Lay on Hands on " + (target == index ? "themself" : patient.name) + ": " +
        std::to_string(amount) + " from the pool, " + std::to_string(healed) + " HP restored (now " +
        hp_text(patient) + "; " + std::to_string(lay_on_hands_pool()) + " left in the pool).");
    for (const auto& line : followups) log(line);
    use_action();
    check_for_victory_or_defeat();
    return true;
}

// --- Actions -----------------------------------------------------------------

bool Encounter::attack(int target_index, int attack_bonus, int damage_dice_count, int damage_dice_sides,
                       int damage_bonus, AttackOptions options) {
    if (!character_can_act()) return false;
    if (!awaiting_action()) {
        log(current()->name + " has no action left this turn.");
        return false;
    }
    auto targets = attack_targets();
    if (std::find(targets.begin(), targets.end(), target_index) == targets.end()) return false;

    if (options.divine_smite_slot > 0) {
        auto slots = divine_smite_slots();
        if (std::find(slots.begin(), slots.end(), options.divine_smite_slot) == slots.end()) {
            log(current()->name + " can't use Divine Smite with a level " +
                std::to_string(options.divine_smite_slot) + " slot.");
            return false;
        }
    }

    int actor = current_index();
    const Combatant& attacker = combatants_[static_cast<size_t>(actor)];
    if (options.reckless && can_attack_recklessly()) {
        turn_.reckless = true;
        ActiveEffect exposed;
        exposed.kind = EffectKind::AdvantageOnAttacksAgainst;
        exposed.target = actor;
        exposed.source = actor;
        exposed.duration.kind = DurationKind::UntilStartOfTurn;
        exposed.duration.participant = actor;
        exposed.label = "Reckless";
        add_effect(exposed);
        log(attacker.name + " attacks recklessly (advantage on STR melee attacks this turn; attacks against them "
                            "have advantage until their next turn).");
    }
    if (turn_.attacks_left > 0) {
        --turn_.attacks_left;  // the next attack of an Attack action already taken
    } else {
        turn_.actions_left -= 1;  // taking the Attack action
        turn_.attacks_left = attacker.attacks_per_action - 1;
        turn_.attack_action_taken = true;
        turn_.attack_action_martial = attacker.attack_martial_arts;
    }
    WeaponAttack attack;
    attack.name = attacker.attack_name;
    attack.attack_bonus = attack_bonus;
    attack.damage_dice_count = damage_dice_count;
    attack.damage_dice_sides = damage_dice_sides;
    attack.damage_bonus = damage_bonus;
    attack.melee = attacker.melee_attack;
    attack.uses_strength = attacker.attack_uses_strength;
    attack.finesse_or_ranged = attacker.attack_finesse_or_ranged;
    attack.martial_arts = attacker.attack_martial_arts;
    attack.great_weapon_fighting = attacker.attack_great_weapon_fighting;
    attack.damage_type = attacker.attack_damage_type;
    weapon_attack(actor, target_index, attack, options.divine_smite_slot);
    if (turn_.attacks_left > 0) {
        log("    (Extra Attack: " + std::to_string(turn_.attacks_left) + " more attack" +
            (turn_.attacks_left == 1 ? "" : "s") + " in this Attack action.)");
    }
    check_for_victory_or_defeat();
    return true;
}

bool Encounter::dash() {
    if (!can_take_action()) return false;
    log(current()->name + " takes the Dash action.");
    use_action();
    check_for_victory_or_defeat();
    return true;
}

bool Encounter::dodge() {
    if (!can_take_action()) return false;
    int index = current_index();
    ActiveEffect dodging;
    dodging.kind = EffectKind::Dodging;
    dodging.target = index;
    dodging.source = index;
    dodging.duration.kind = DurationKind::UntilStartOfTurn;
    dodging.duration.participant = index;
    add_effect(dodging);
    log(combatants_[static_cast<size_t>(index)].name +
        " takes the Dodge action (attacks against them have disadvantage until their next turn).");
    use_action();
    check_for_victory_or_defeat();
    return true;
}

bool Encounter::drink_potion(int item_id) {
    if (!can_take_action()) return false;
    int index = current_index();
    Combatant& actor = combatants_[static_cast<size_t>(index)];

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

    EncounterEvent used;
    used.kind = EncounterEvent::Kind::ItemUsed;
    used.character_id = actor.character_id;
    used.item_id = item.item_id;
    used.quantity = 1;
    events_.push_back(used);

    // Heal through the one healing path, which respects effects like Chill Touch.
    int healed = 0;
    auto followups = apply_healing(index, roll_healing(dice_, item), healed);

    log(actor.name + " drinks a " + item.name + " and regains " + std::to_string(healed) + " HP (now " +
        hp_text(actor) + ").");
    for (const auto& line : followups) log(line);
    use_action();
    check_for_victory_or_defeat();
    return true;
}

bool Encounter::stabilize(int target_index) {
    if (!can_take_action()) return false;
    auto targets = stabilize_targets();
    if (std::find(targets.begin(), targets.end(), target_index) == targets.end()) return false;

    int actor_index = current_index();
    const Combatant& actor = combatants_[static_cast<size_t>(actor_index)];
    Combatant& target = combatants_[static_cast<size_t>(target_index)];

    bool success = true;
    if (has_healers_kit()) {
        log(actor.name + " uses a Healer's Kit on " + target.name + ": " + target.name + " is stable.");
    } else {
        RollModifiers mods = ability_check_modifiers(effects_, actor_index, alive_flags());
        int roll = dice_.roll_d20(mods.mode()).result;
        int bonus = skill_bonus(*character(actor.character_id), Skill::Medicine);
        success = roll + bonus >= 10;
        log(actor.name + " tries to stabilize " + target.name + ": WIS (Medicine) check d20(" + std::to_string(roll) +
            ") " + format_modifier(bonus) + mode_note(mods) + " vs DC 10 -> " +
            (success ? "success, " + target.name + " is stable." : "failure."));
    }
    if (success) {
        target.death_saves = DeathSaveState{};
        target.death_saves.stable = true;
    }
    use_action();
    check_for_victory_or_defeat();
    return true;
}

bool Encounter::cast_spell(int spell_id, int slot_level, const std::vector<int>& target_indices) {
    if (!character_can_act()) return false;
    int actor_index = current_index();
    const Combatant& actor = combatants_[static_cast<size_t>(actor_index)];

    if (actor.armor_not_proficient) {
        log(actor.name + " can't cast spells while wearing armor they aren't proficient with.");
        return false;
    }
    if (is_raging(actor_index)) {
        log(actor.name + " can't cast spells while raging.");
        return false;
    }
    // Turn economy first, so the log can say why (e.g. the bonus-action spell rule).
    const auto& known = known_spells_[actor.character_id];
    auto known_spell = std::find_if(known.begin(), known.end(), [&](const Spell& s) { return s.spell_id == spell_id; });
    if (known_spell != known.end()) {
        if (auto blocked = spell_casting_blocked(turn_, *known_spell)) {
            log(*blocked);
            return false;
        }
    }
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
    caster.spell_attack_bonus -= actor.roll_penalty;
    int slot = spell.level == 0 ? 0 : slot_level;

    if (!spell_targets_ok(spell, caster.level, slot, target_indices)) return false;
    // A costly material component the spell uses up (Revivify's diamonds).
    auto& inventory = inventories_[actor.character_id];
    auto component = inventory.end();
    if (!spell.consumed_item.empty()) {
        component = std::find_if(inventory.begin(), inventory.end(), [&](const InventoryEntry& entry) {
            return entry.item.name == spell.consumed_item && entry.quantity > 0;
        });
        if (component == inventory.end()) {
            log(spell.name + " needs " + spell.consumed_item + ", which " + actor.name +
                " doesn't have. (The Shop sells them.)");
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
    if (component != inventory.end()) {
        EncounterEvent used;
        used.kind = EncounterEvent::Kind::ItemUsed;
        used.character_id = actor.character_id;
        used.item_id = component->item.item_id;
        used.quantity = 1;
        events_.push_back(used);
        if (--component->quantity == 0) inventory.erase(component);
    }
    resolve_spell(actor_index, spell, caster, slot, target_indices,
                  actor.name + " casts " + spell.name + (slot > 0 ? " (" + ordinal(slot) + "-level slot)" : "") + ".");
    return true;
}

bool Encounter::spell_targets_ok(const Spell& spell, int caster_level, int slot, const std::vector<int>& target_indices) {
    if (spell_targets_nobody(spell)) return true;
    int max_targets = spell_max_targets(spell, caster_level, slot);
    auto candidates = spell_target_candidates(spell);
    bool all_valid = std::all_of(target_indices.begin(), target_indices.end(), [&](int index) {
        return std::find(candidates.begin(), candidates.end(), index) != candidates.end();
    });
    if (target_indices.empty() || static_cast<int>(target_indices.size()) > max_targets || !all_valid) {
        log(spell.name + " needs 1 to " + std::to_string(max_targets) + " targets checked.");
        return false;
    }
    return true;
}

void Encounter::resolve_spell(int actor_index, const Spell& spell, const CasterStats& caster, int slot,
                              const std::vector<int>& target_indices, const std::string& cast_line) {
    const Combatant& actor = combatants_[static_cast<size_t>(actor_index)];
    bool no_targets = spell_targets_nobody(spell);
    record_spell_cast(turn_, spell);  // the action or bonus action

    // Each target's roll modifiers, from every effect in play.
    std::vector<int> indices = no_targets ? std::vector<int>{} : target_indices;
    std::vector<SpellTarget> targets;
    std::vector<RollModifiers> target_mods;
    auto alive = alive_flags();
    for (int index : indices) {
        SpellTarget spell_target;
        spell_target.armor_class = armor_class(index);
        spell_target.save_bonus = save_bonus(index, spell.save_ability);
        RollModifiers mods;
        if (spell.resolution == SpellResolution::Attack) {
            mods = attack_roll_modifiers(effects_, actor_index, index, spell.melee_attack, alive);
            spell_target.attack_mode = mods.mode();
            spell_target.critical_on_hit = mods.auto_crit;
            spell_target.attack_bonus_dice = mods.bonus_dice;
        } else if (spell.resolution == SpellResolution::Save) {
            mods = saving_throw_modifiers(effects_, index, spell.save_ability);
            if (spell.combat_save_advantage) mods.advantage.push_back("in combat with the caster");
            spell_target.save_mode = mods.mode();
            spell_target.save_auto_fail = mods.auto_fail;
            spell_target.save_bonus_dice = mods.bonus_dice;
        }
        targets.push_back(spell_target);
        target_mods.push_back(mods);
    }

    auto result = dnd::cast_spell(dice_, spell, caster, slot, targets);
    for (const auto& mods : target_mods) use_effects(mods.used_effects);

    log(cast_line);
    // Casting a concentration spell ends any earlier concentration.
    if (spell.concentration) start_concentration(actor_index, spell.name);

    auto self_labels = apply_spell_effects(spell, SpellEffectTrigger::OnCastSelf, actor_index, actor_index);
    if (!self_labels.empty()) log("    " + actor.name + ": " + join(self_labels, ", ") + ".");
    if (spell.resolution == SpellResolution::Utility) {
        log("    " + spell.description);
        check_for_victory_or_defeat();
        return;
    }
    if (no_targets) {
        check_for_victory_or_defeat();
        return;
    }
    if (spell.resolution == SpellResolution::Save && !result.dice.empty()) {
        std::string dice;
        for (int die : result.dice) dice += (dice.empty() ? "" : " + ") + std::to_string(die);
        log("    Damage roll: " + dice);
    }

    for (const auto& effect : result.effects) {
        size_t slot_index = static_cast<size_t>(effect.target);
        int target_index = indices[slot_index];
        Combatant& target = combatants_[static_cast<size_t>(target_index)];
        const RollModifiers& mods = target_mods[slot_index];
        std::string outcome;
        std::vector<std::string> followups;
        std::vector<std::string> labels;
        std::string damage_note;  // resistance, vulnerability, immunity
        bool deals_damage = spell.resolution == SpellResolution::Attack || spell.resolution == SpellResolution::Save ||
                            spell.resolution == SpellResolution::AutoHit;
        int dealt = deals_damage ? typed_damage(target_index, effect.amount, spell.damage_type, damage_note)
                                 : effect.amount;
        switch (spell.resolution) {
            case SpellResolution::Attack:
                followups = effect.hit ? apply_damage(target_index, dealt, effect.critical, actor_index, spell.damage_type)
                                       : std::vector<std::string>{};
                if (effect.hit && target.current_hp > 0) {
                    labels = apply_spell_effects(spell, SpellEffectTrigger::OnHit, actor_index, target_index);
                }
                outcome = "d20(" + std::to_string(effect.attack_roll) + ") " +
                          format_modifier(caster.spell_attack_bonus) + bonus_note(mods.bonus_dice, effect.bonus_rolls) +
                          mode_note(mods) + " -> " + attack_outcome(effect.hit, effect.critical, dealt) + damage_note;
                break;
            case SpellResolution::Save:
                followups = apply_damage(target_index, dealt, false, actor_index, spell.damage_type);
                if (!effect.saved && target.current_hp > 0) {
                    labels = apply_spell_effects(spell, SpellEffectTrigger::OnFailedSave, actor_index, target_index,
                                                 caster.spell_save_dc);
                }
                outcome = ability_tag(spell.save_ability) + " save d20(" + std::to_string(effect.save_roll) + ") " +
                          format_modifier(targets[slot_index].save_bonus) +
                          bonus_note(mods.bonus_dice, effect.bonus_rolls) + mode_note(mods) +
                          (effect.auto_failed ? " (automatic failure)" : "") + " vs DC " +
                          std::to_string(caster.spell_save_dc) + " -> " + (effect.saved ? "saved" : "failed");
                if (!result.dice.empty()) outcome += ", takes " + std::to_string(dealt) + damage_note;
                break;
            case SpellResolution::Buff:
                labels = apply_spell_effects(spell, SpellEffectTrigger::OnCastTarget, actor_index, target_index);
                outcome = labels.empty() ? "affected" : join(labels, ", ");
                labels.clear();  // already in the outcome
                break;
            case SpellResolution::Stabilize:
                target.death_saves = DeathSaveState{};
                target.death_saves.stable = true;
                outcome = "stable";
                break;
            case SpellResolution::Revive:
                outcome = revive(target_index);
                break;
            case SpellResolution::AutoHit:
                followups = apply_damage(target_index, dealt, false, actor_index, spell.damage_type);
                outcome = "hit for " + std::to_string(dealt) + damage_note;
                break;
            case SpellResolution::Heal: {
                int healed = 0;
                followups = apply_healing(target_index, effect.amount, healed);
                outcome = "regains " + std::to_string(healed) + " HP";
                break;
            }
            case SpellResolution::Utility:
                break;
        }
        if (!labels.empty()) outcome += "; " + join(labels, ", ");
        bool shows_hp = spell.resolution != SpellResolution::Buff && spell.resolution != SpellResolution::Stabilize &&
                        spell.resolution != SpellResolution::Revive;
        log("    " + target.name + ": " + outcome + (shows_hp ? ". Now " + hp_text(target) + " HP." : "."));
        for (const auto& line : followups) log(line);
    }

    check_for_victory_or_defeat();
}

// --- Spell scrolls -----------------------------------------------------------

void Encounter::set_scroll_spells(std::vector<Spell> spells) {
    scroll_spells_ = std::move(spells);
}

const Spell* Encounter::scroll_spell(const Item& scroll) const {
    for (const auto& spell : scroll_spells_) {
        if (spell.spell_id == scroll.scroll_spell_id) return &spell;
    }
    return nullptr;
}

std::vector<InventoryEntry> Encounter::scrolls() const {
    std::vector<InventoryEntry> held;
    const Combatant* actor = current();
    if (!actor || !actor->is_character) return held;
    auto inventory = inventories_.find(actor->character_id);
    if (inventory == inventories_.end()) return held;
    for (const auto& entry : inventory->second) {
        if (entry.item.category == ItemCategory::Scroll && entry.quantity > 0 && scroll_spell(entry.item)) {
            held.push_back(entry);
        }
    }
    return held;
}

std::optional<ScrollReading> Encounter::scroll_reading(const Item& scroll) const {
    const Combatant* actor = current();
    const Spell* spell = scroll_spell(scroll);
    if (!actor || !actor->is_character || !spell) return std::nullopt;
    const Character* reader = character(actor->character_id);
    const ClassInfo* info = reader ? find_class_info_by_id(classes_, reader->class_id) : nullptr;
    if (!info) return std::nullopt;
    return dnd::read_scroll(*reader, *info, *spell);
}

bool Encounter::read_scroll(int item_id, const std::vector<int>& target_indices) {
    if (!character_can_act()) return false;
    int actor_index = current_index();
    const Combatant& actor = combatants_[static_cast<size_t>(actor_index)];
    auto& inventory = inventories_[actor.character_id];
    auto held = std::find_if(inventory.begin(), inventory.end(), [&](const InventoryEntry& entry) {
        return entry.item.item_id == item_id && entry.item.category == ItemCategory::Scroll && entry.quantity > 0;
    });
    if (held == inventory.end() || !scroll_spell(held->item)) {
        log(actor.name + " has no such scroll -- choose another action.");
        return false;
    }
    const Item scroll = held->item;
    const Spell spell = *scroll_spell(scroll);
    if (actor.armor_not_proficient) {
        log(actor.name + " can't cast spells while wearing armor they aren't proficient with.");
        return false;
    }
    if (is_raging(actor_index)) {
        log(actor.name + " can't cast spells while raging.");
        return false;
    }
    if (auto blocked = spell_casting_blocked(turn_, spell)) {
        log(*blocked);
        return false;
    }
    auto reading = scroll_reading(scroll);
    if (!reading || !reading->readable) {
        log(reading ? reading->reason : actor.name + " can't read that scroll.");
        return false;
    }
    Character& reader = *character_mut(actor.character_id);
    const ClassInfo& info = *find_class_info_by_id(classes_, reader.class_id);
    CasterStats caster = caster_stats(reader, info);
    // The scroll's own numbers (DMG), at its own level.
    caster.spell_attack_bonus = reading->attack_bonus - actor.roll_penalty;
    caster.spell_save_dc = reading->save_dc;
    int slot = spell.level;
    if (!spell_targets_ok(spell, caster.level, slot, target_indices)) return false;

    // Read: the scroll is gone either way.
    if (--held->quantity == 0) inventory.erase(held);
    EncounterEvent used;
    used.kind = EncounterEvent::Kind::ItemUsed;
    used.character_id = actor.character_id;
    used.item_id = scroll.item_id;
    used.quantity = 1;
    events_.push_back(used);

    std::string line = actor.name + " reads a " + scroll.name;
    if (reading->needs_check) {
        int roll = dice_.roll(20, 1).front();
        int bonus = caster.spellcasting_modifier - actor.roll_penalty;
        bool passed = roll + bonus >= reading->check_dc;
        line += ": spellcasting check d20(" + std::to_string(roll) + ") " + format_modifier(bonus) + " vs DC " +
                std::to_string(reading->check_dc) + " -> " + (passed ? "success" : "failure");
        if (!passed) {
            record_spell_cast(turn_, spell);  // the action is spent all the same
            log(line + ". The scroll crumbles to dust, its magic wasted.");
            return true;
        }
        line += ". ";
    } else {
        line += ". ";
    }
    resolve_spell(actor_index, spell, caster, slot, target_indices, line + actor.name + " casts " + spell.name + ".");
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
