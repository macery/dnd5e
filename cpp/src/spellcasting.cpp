#include "dnd/spellcasting.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "dnd/combat.hpp"
#include "dnd/rules.hpp"

namespace dnd {

namespace {

using SlotRow = std::array<int, kMaxSpellLevel>;

// PHB full-caster Spell Slots per Spell Level, levels 1-20.
constexpr std::array<SlotRow, 20> kFullCasterSlots = {{
    {2, 0, 0, 0, 0, 0, 0, 0, 0},
    {3, 0, 0, 0, 0, 0, 0, 0, 0},
    {4, 2, 0, 0, 0, 0, 0, 0, 0},
    {4, 3, 0, 0, 0, 0, 0, 0, 0},
    {4, 3, 2, 0, 0, 0, 0, 0, 0},
    {4, 3, 3, 0, 0, 0, 0, 0, 0},
    {4, 3, 3, 1, 0, 0, 0, 0, 0},
    {4, 3, 3, 2, 0, 0, 0, 0, 0},
    {4, 3, 3, 3, 1, 0, 0, 0, 0},
    {4, 3, 3, 3, 2, 0, 0, 0, 0},
    {4, 3, 3, 3, 2, 1, 0, 0, 0},
    {4, 3, 3, 3, 2, 1, 0, 0, 0},
    {4, 3, 3, 3, 2, 1, 1, 0, 0},
    {4, 3, 3, 3, 2, 1, 1, 0, 0},
    {4, 3, 3, 3, 2, 1, 1, 1, 0},
    {4, 3, 3, 3, 2, 1, 1, 1, 0},
    {4, 3, 3, 3, 2, 1, 1, 1, 1},
    {4, 3, 3, 3, 3, 1, 1, 1, 1},
    {4, 3, 3, 3, 3, 2, 1, 1, 1},
    {4, 3, 3, 3, 3, 2, 2, 1, 1},
}};

// Spells Known columns of the PHB class tables, levels 1-20.
constexpr std::array<int, 20> kBardSpellsKnown = {4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 15, 16, 18, 19, 19, 20, 22, 22, 22};
constexpr std::array<int, 20> kRangerSpellsKnown = {0, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11};
constexpr std::array<int, 20> kSorcererSpellsKnown = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 12, 13, 13, 14, 14, 15, 15, 15, 15};
constexpr std::array<int, 20> kWarlockSpellsKnown = {2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15};

void require_level(int level) {
    if (level < 1 || level > 20) throw std::invalid_argument("level must be in [1, 20]");
}

size_t level_index(int level) {
    require_level(level);
    return static_cast<size_t>(level - 1);
}

}  // namespace

const char* spell_resolution_name(SpellResolution resolution) {
    switch (resolution) {
        case SpellResolution::Attack: return "Attack";
        case SpellResolution::Save: return "Save";
        case SpellResolution::AutoHit: return "AutoHit";
        case SpellResolution::Heal: return "Heal";
        case SpellResolution::Utility: return "Utility";
        case SpellResolution::Buff: return "Buff";
        case SpellResolution::Stabilize: return "Stabilize";
    }
    throw std::invalid_argument("unknown spell resolution");
}

SpellResolution spell_resolution_from_name(const std::string& name) {
    for (auto resolution : {SpellResolution::Attack, SpellResolution::Save, SpellResolution::AutoHit,
                            SpellResolution::Heal, SpellResolution::Utility, SpellResolution::Buff,
                            SpellResolution::Stabilize}) {
        if (name == spell_resolution_name(resolution)) return resolution;
    }
    throw std::invalid_argument("unknown spell resolution: " + name);
}

const char* casting_time_name(CastingTime casting_time) {
    switch (casting_time) {
        case CastingTime::Action: return "Action";
        case CastingTime::BonusAction: return "BonusAction";
        case CastingTime::Reaction: return "Reaction";
    }
    throw std::invalid_argument("unknown casting time");
}

CastingTime casting_time_from_name(const std::string& name) {
    for (auto casting_time : {CastingTime::Action, CastingTime::BonusAction, CastingTime::Reaction}) {
        if (name == casting_time_name(casting_time)) return casting_time;
    }
    throw std::invalid_argument("unknown casting time: " + name);
}

const char* spell_effect_trigger_name(SpellEffectTrigger trigger) {
    switch (trigger) {
        case SpellEffectTrigger::OnHit: return "OnHit";
        case SpellEffectTrigger::OnFailedSave: return "OnFailedSave";
        case SpellEffectTrigger::OnCastTarget: return "OnCastTarget";
        case SpellEffectTrigger::OnCastSelf: return "OnCastSelf";
    }
    throw std::invalid_argument("unknown spell effect trigger");
}

SpellEffectTrigger spell_effect_trigger_from_name(const std::string& name) {
    for (auto trigger : {SpellEffectTrigger::OnHit, SpellEffectTrigger::OnFailedSave, SpellEffectTrigger::OnCastTarget,
                         SpellEffectTrigger::OnCastSelf}) {
        if (name == spell_effect_trigger_name(trigger)) return trigger;
    }
    throw std::invalid_argument("unknown spell effect trigger: " + name);
}

const char* spell_effect_duration_name(SpellEffectDuration duration) {
    switch (duration) {
        case SpellEffectDuration::UntilRemoved: return "UntilRemoved";
        case SpellEffectDuration::Rounds: return "Rounds";
        case SpellEffectDuration::UntilStartOfCasterTurn: return "UntilStartOfCasterTurn";
        case SpellEffectDuration::UntilEndOfCasterTurn: return "UntilEndOfCasterTurn";
        case SpellEffectDuration::UntilStartOfTargetTurn: return "UntilStartOfTargetTurn";
        case SpellEffectDuration::UntilEndOfTargetTurn: return "UntilEndOfTargetTurn";
    }
    throw std::invalid_argument("unknown spell effect duration");
}

SpellEffectDuration spell_effect_duration_from_name(const std::string& name) {
    for (auto duration : {SpellEffectDuration::UntilRemoved, SpellEffectDuration::Rounds,
                          SpellEffectDuration::UntilStartOfCasterTurn, SpellEffectDuration::UntilEndOfCasterTurn,
                          SpellEffectDuration::UntilStartOfTargetTurn, SpellEffectDuration::UntilEndOfTargetTurn}) {
        if (name == spell_effect_duration_name(duration)) return duration;
    }
    throw std::invalid_argument("unknown spell effect duration: " + name);
}

const char* cantrip_scaling_name(CantripScaling scaling) {
    switch (scaling) {
        case CantripScaling::Dice: return "Dice";
        case CantripScaling::Projectiles: return "Projectiles";
    }
    throw std::invalid_argument("unknown cantrip scaling");
}

CantripScaling cantrip_scaling_from_name(const std::string& name) {
    for (auto scaling : {CantripScaling::Dice, CantripScaling::Projectiles}) {
        if (name == cantrip_scaling_name(scaling)) return scaling;
    }
    throw std::invalid_argument("unknown cantrip scaling: " + name);
}

Ability ability_from_name(const std::string& name) {
    for (int i = 0; i < kAbilityCount; ++i) {
        auto ability = static_cast<Ability>(i);
        if (name == ability_name(ability)) return ability;
    }
    throw std::invalid_argument("unknown ability: " + name);
}

std::string ordinal(int number) {
    int last_two = number % 100;
    const char* suffix = "th";
    if (last_two < 11 || last_two > 13) {
        switch (number % 10) {
            case 1: suffix = "st"; break;
            case 2: suffix = "nd"; break;
            case 3: suffix = "rd"; break;
            default: break;
        }
    }
    return std::to_string(number) + suffix;
}

bool is_spellcaster(const ClassInfo& class_info) {
    return class_info.spellcasting.caster_type != CasterType::None;
}

int spellcasting_modifier(const Character& character, const ClassInfo& class_info) {
    return ability_modifier(ability_score(character, class_info.spellcasting.ability));
}

int spell_save_dc(const Character& character, const ClassInfo& class_info) {
    return 8 + spell_attack_bonus(character, class_info);
}

int spell_attack_bonus(const Character& character, const ClassInfo& class_info) {
    return proficiency_bonus(character.level) + spellcasting_modifier(character, class_info);
}

int cantrips_known(const ClassInfo& class_info, int level) {
    require_level(level);
    int base = class_info.spellcasting.base_cantrips;
    if (base == 0) return 0;
    return base + (level >= 4 ? 1 : 0) + (level >= 10 ? 1 : 0);
}

int spells_known(const ClassInfo& class_info, int level) {
    size_t index = level_index(level);
    if (class_info.spellcasting.preparation != SpellPreparation::Known) return 0;
    if (class_info.name == "Bard") return kBardSpellsKnown[index];
    if (class_info.name == "Ranger") return kRangerSpellsKnown[index];
    if (class_info.name == "Sorcerer") return kSorcererSpellsKnown[index];
    if (class_info.name == "Warlock") return kWarlockSpellsKnown[index];
    return 0;
}

int spellbook_size(int level) {
    require_level(level);
    return 6 + 2 * (level - 1);
}

int prepared_spell_count(const Character& character, const ClassInfo& class_info) {
    auto preparation = class_info.spellcasting.preparation;
    if (preparation != SpellPreparation::Prepared && preparation != SpellPreparation::Spellbook) return 0;

    int levels = character.level;
    if (class_info.spellcasting.caster_type == CasterType::Half) {
        if (character.level < 2) return 0;
        levels = character.level / 2;
    }
    return std::max(1, spellcasting_modifier(character, class_info) + levels);
}

SpellAllowance spell_allowance(const Character& character, const ClassInfo& class_info) {
    SpellAllowance allowance;
    allowance.cantrips = cantrips_known(class_info, character.level);
    allowance.spells = class_info.spellcasting.preparation == SpellPreparation::Known
        ? spells_known(class_info, character.level)
        : prepared_spell_count(character, class_info);

    auto slots = spell_slots(class_info, character.level);
    for (int level = kMaxSpellLevel; level >= 1; --level) {
        if (slots[static_cast<size_t>(level - 1)] > 0) {
            allowance.max_spell_level = level;
            break;
        }
    }
    if (allowance.max_spell_level == 0) allowance.spells = 0;
    return allowance;
}

std::optional<std::string> validate_spell_selection(const Character& character, const ClassInfo& class_info,
                                                    const std::vector<Spell>& chosen, bool require_full) {
    SpellAllowance allowance = spell_allowance(character, class_info);
    int cantrips = 0;
    int spells = 0;
    for (size_t i = 0; i < chosen.size(); ++i) {
        const Spell& spell = chosen[i];
        const auto& classes = spell.class_names;
        if (std::find(classes.begin(), classes.end(), class_info.name) == classes.end()) {
            return spell.name + " isn't on the " + class_info.name + " spell list.";
        }
        if (spell.level > allowance.max_spell_level) {
            return spell.name + " is level " + std::to_string(spell.level) + "; you can cast up to level " +
                   std::to_string(allowance.max_spell_level) + ".";
        }
        for (size_t j = i + 1; j < chosen.size(); ++j) {
            if (chosen[j].spell_id == spell.spell_id && chosen[j].name == spell.name) {
                return spell.name + " is chosen twice.";
            }
        }
        (spell.level == 0 ? cantrips : spells) += 1;
    }

    auto describe = [](int count, const char* what) { return std::to_string(count) + " " + what; };
    if (cantrips > allowance.cantrips || (require_full && cantrips != allowance.cantrips)) {
        return "Choose " + describe(allowance.cantrips, "cantrips") + " (you have " + std::to_string(cantrips) + ").";
    }
    if (spells > allowance.spells || (require_full && spells != allowance.spells)) {
        return "Choose " + describe(allowance.spells, "spells of 1st level or higher") + " (you have " +
               std::to_string(spells) + ").";
    }
    return std::nullopt;
}

std::array<int, kMaxSpellLevel> spell_slots(const ClassInfo& class_info, int level) {
    size_t index = level_index(level);
    SlotRow slots{};
    switch (class_info.spellcasting.caster_type) {
        case CasterType::None:
            break;
        case CasterType::Full:
            slots = kFullCasterSlots[index];
            break;
        case CasterType::Half:
            if (level >= 2) slots = kFullCasterSlots[static_cast<size_t>((level + 1) / 2 - 1)];
            break;
        case CasterType::Pact: {
            int count = level >= 17 ? 4 : level >= 11 ? 3 : level >= 2 ? 2 : 1;
            int slot_level = std::min(5, (level + 1) / 2);
            slots[static_cast<size_t>(slot_level - 1)] = count;
            break;
        }
    }
    return slots;
}

int slots_remaining(const Character& character, const ClassInfo& class_info, int slot_level) {
    if (slot_level < 1 || slot_level > kMaxSpellLevel) throw std::invalid_argument("slot level must be in [1, 9]");
    auto index = static_cast<size_t>(slot_level - 1);
    return std::max(0, spell_slots(class_info, character.level)[index] - character.spell_slots_used[index]);
}

bool spend_spell_slot(Character& character, const ClassInfo& class_info, int slot_level) {
    if (slots_remaining(character, class_info, slot_level) == 0) return false;
    ++character.spell_slots_used[static_cast<size_t>(slot_level - 1)];
    return true;
}

int cantrip_tier(int level) {
    require_level(level);
    return 1 + (level >= 5 ? 1 : 0) + (level >= 11 ? 1 : 0) + (level >= 17 ? 1 : 0);
}

CasterStats caster_stats(const Character& character, const ClassInfo& class_info) {
    CasterStats stats;
    stats.level = character.level;
    stats.spell_attack_bonus = spell_attack_bonus(character, class_info);
    stats.spell_save_dc = spell_save_dc(character, class_info);
    stats.spellcasting_modifier = spellcasting_modifier(character, class_info);
    return stats;
}

int spell_projectiles(const Spell& spell, int caster_level, int slot_level) {
    if (spell.level == 0) {
        if (spell.cantrip_scaling == CantripScaling::Projectiles) {
            return spell.projectiles + cantrip_tier(caster_level) - 1;
        }
        return spell.projectiles;
    }
    return spell.projectiles + spell.upcast_projectiles * std::max(0, slot_level - spell.level);
}

int spell_dice_count(const Spell& spell, int caster_level, int slot_level) {
    if (spell.level == 0) {
        if (spell.cantrip_scaling == CantripScaling::Dice) return spell.dice_count * cantrip_tier(caster_level);
        return spell.dice_count;
    }
    return spell.dice_count + spell.upcast_dice * std::max(0, slot_level - spell.level);
}

int spell_max_targets(const Spell& spell, int caster_level, int slot_level) {
    int projectiles = spell_projectiles(spell, caster_level, slot_level);
    if (projectiles > 1) return projectiles;
    return spell.max_targets + spell.upcast_targets * std::max(0, slot_level - spell.level);
}

std::string spell_summary(const Spell& spell) {
    std::string dice = std::to_string(spell.dice_count) + "d" + std::to_string(spell.dice_sides);
    if (spell.flat_bonus > 0) dice += "+" + std::to_string(spell.flat_bonus);
    std::string damage = spell.damage_type.empty() ? dice : dice + " " + spell.damage_type;

    auto labels = [&spell](SpellEffectTrigger trigger) {
        std::string text;
        for (const auto& effect : spell.effects) {
            if (effect.trigger != trigger || effect.label.empty()) continue;
            if (text.find(effect.label) != std::string::npos) continue;
            text += (text.empty() ? "" : ", ") + effect.label;
        }
        return text;
    };
    auto tag = [](Ability ability) {
        std::string text = std::string(ability_name(ability)).substr(0, 3);
        for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return text;
    };

    std::string summary;
    switch (spell.resolution) {
        case SpellResolution::Attack:
            summary = std::string(spell.melee_attack ? "melee" : "ranged") + " spell attack, " + damage;
            if (!labels(SpellEffectTrigger::OnHit).empty()) summary += "; on a hit: " + labels(SpellEffectTrigger::OnHit);
            break;
        case SpellResolution::Save:
            summary = tag(spell.save_ability) + " save";
            if (spell.dice_count > 0) summary += std::string(spell.save_halves ? " (half on success), " : ", ") + damage;
            if (!labels(SpellEffectTrigger::OnFailedSave).empty()) {
                summary += "; on a failure: " + labels(SpellEffectTrigger::OnFailedSave);
            }
            break;
        case SpellResolution::AutoHit:
            summary = std::to_string(spell.projectiles) + " × " + damage + ", always hits";
            break;
        case SpellResolution::Heal:
            summary = "heals " + dice + (spell.adds_modifier ? " + modifier" : "");
            break;
        case SpellResolution::Utility:
            summary = "utility (no automated effect yet)";
            break;
        case SpellResolution::Buff: {
            std::string self = labels(SpellEffectTrigger::OnCastSelf);
            std::string others = labels(SpellEffectTrigger::OnCastTarget);
            if (!others.empty()) {
                summary = "buff: " + others + " (" +
                          (spell.max_targets > 1 ? "up to " + std::to_string(spell.max_targets) + " allies" : "1 ally") +
                          ")";
            } else {
                summary = "buff on yourself: " + self;
            }
            break;
        }
        case SpellResolution::Stabilize:
            summary = "a dying creature becomes stable";
            break;
    }
    if (spell.concentration) summary += " (concentration)";
    return summary;
}

bool spell_targets_nobody(const Spell& spell) {
    if (spell.resolution == SpellResolution::Utility) return true;
    if (spell.resolution != SpellResolution::Buff || spell.effects.empty()) return false;
    return std::all_of(spell.effects.begin(), spell.effects.end(),
                       [](const SpellEffectSpec& e) { return e.trigger == SpellEffectTrigger::OnCastSelf; });
}

SpellCastResult cast_spell(const Dice& dice, const Spell& spell, const CasterStats& caster, int slot_level,
                           const std::vector<SpellTarget>& targets) {
    if (spell.level == 0 && slot_level != 0) throw std::invalid_argument("cantrips don't use a spell slot");
    if (spell.level > 0 && (slot_level < spell.level || slot_level > kMaxSpellLevel)) {
        throw std::invalid_argument("slot level must be between the spell's level and 9");
    }

    SpellCastResult result;
    result.slot_level = slot_level;
    if (spell_targets_nobody(spell)) return result;  // nothing to roll or target

    int max_targets = spell_max_targets(spell, caster.level, slot_level);
    if (targets.empty() || static_cast<int>(targets.size()) > max_targets) {
        throw std::invalid_argument("choose between 1 and " + std::to_string(max_targets) + " targets");
    }

    result.projectiles = spell_projectiles(spell, caster.level, slot_level);
    int dice_count = spell_dice_count(spell, caster.level, slot_level);
    int bonus = spell.flat_bonus + (spell.adds_modifier ? caster.spellcasting_modifier : 0);

    if (spell.resolution == SpellResolution::Buff || spell.resolution == SpellResolution::Stabilize) {
        for (size_t i = 0; i < targets.size(); ++i) {
            SpellEffectRoll effect;
            effect.target = static_cast<int>(i);
            effect.hit = true;
            result.effects.push_back(effect);
        }
        return result;
    }

    if (spell.resolution == SpellResolution::Save) {
        // Area damage is rolled once for everyone caught in it.
        result.dice = dice.roll(spell.dice_sides, dice_count);
        int full = std::max(0, Dice::sum(result.dice) + bonus);
        for (size_t i = 0; i < targets.size(); ++i) {
            SpellEffectRoll effect;
            effect.target = static_cast<int>(i);
            const SpellTarget& target = targets[i];
            effect.save_roll = dice.roll_d20(target.save_mode).result;
            int bonus_total = 0;
            for (int sides : target.save_bonus_dice) {
                effect.bonus_rolls.push_back(dice.roll(sides, 1).front());
                bonus_total += effect.bonus_rolls.back();
            }
            effect.auto_failed = target.save_auto_fail;
            effect.saved = !effect.auto_failed &&
                           effect.save_roll + target.save_bonus + bonus_total >= caster.spell_save_dc;
            effect.amount = effect.saved ? (spell.save_halves ? full / 2 : 0) : full;
            result.effects.push_back(effect);
        }
        return result;
    }

    for (int p = 0; p < result.projectiles; ++p) {
        SpellEffectRoll effect;
        effect.target = p % static_cast<int>(targets.size());
        const SpellTarget& target = targets[static_cast<size_t>(effect.target)];

        if (spell.resolution == SpellResolution::Attack) {
            int bonus_total = 0;
            for (int sides : target.attack_bonus_dice) {
                effect.bonus_rolls.push_back(dice.roll(sides, 1).front());
                bonus_total += effect.bonus_rolls.back();
            }
            AttackResult attack = resolve_attack(dice, caster.spell_attack_bonus + bonus_total, target.armor_class,
                                                 dice_count, spell.dice_sides, bonus, target.attack_mode,
                                                 target.critical_on_hit);
            effect.attack_roll = attack.attack_roll;
            effect.hit = attack.hit;
            effect.critical = attack.critical;
            effect.amount = attack.damage_total;
        } else {  // AutoHit, Heal
            effect.hit = true;
            effect.amount = std::max(0, Dice::sum(dice.roll(spell.dice_sides, dice_count)) + bonus);
        }
        result.effects.push_back(effect);
    }
    return result;
}

}  // namespace dnd
