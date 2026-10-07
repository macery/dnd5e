#include "dnd/scenes.hpp"

#include <algorithm>

#include "dnd/character_creation.hpp"
#include "dnd/currency.hpp"
#include "dnd/magic_items.hpp"
#include "dnd/npc.hpp"

namespace dnd {

namespace {

Character* find(std::vector<Character>& characters, int id) {
    for (auto& c : characters) {
        if (c.character_id == id) return &c;
    }
    return nullptr;
}

std::string class_of(const Repository& repo, const Character& character) {
    for (const auto& entry : repo.get_classes()) {
        if (entry.id == character.class_id) return entry.name;
    }
    return "";
}

// Takes damage outside combat: HP down to 0 at the least (unconscious and
// stable there); temporary HP first.
std::string hurt(Character& character, int amount) {
    int absorbed = std::min(character.temp_hp, amount);
    character.temp_hp -= absorbed;
    amount -= absorbed;
    character.current_hp = std::max(0, character.current_hp - amount);
    return character.character_name + " takes " + std::to_string(amount + absorbed) + " damage (now " +
           std::to_string(character.current_hp) + "/" + std::to_string(character.max_hp) + " HP)" +
           (character.current_hp == 0 ? ", and falls unconscious." : ".");
}

}  // namespace

std::vector<const SceneOption*> available_options(const AdventureScene& scene, const AdventureRun& run) {
    std::vector<const SceneOption*> options;
    for (const auto& option : scene.options) {
        bool unlocked = option.requires_flag.empty() ||
                        std::find(run.flags.begin(), run.flags.end(), option.requires_flag) != run.flags.end();
        if (unlocked) options.push_back(&option);
    }
    return options;
}

std::vector<CheckRoller> party_rollers(const Repository& repo, const std::vector<int>& party) {
    std::vector<CheckRoller> rollers;
    for (const auto& character : repo.get_characters()) {
        bool in = std::find(party.begin(), party.end(), character.character_id) != party.end();
        if (in && !character.dead && character.current_hp > 0) {  // the unconscious sit it out
            rollers.push_back({character, class_of(repo, character), repo.get_inventory(character.character_id)});
        }
    }
    return rollers;
}

std::vector<std::string> apply_outcome(Repository& repo, AdventureRun& run, const SceneOutcome& outcome,
                                       int roller_id, const Dice& dice, const std::string& scene_npc) {
    std::vector<std::string> lines;
    if (!outcome.text.empty()) lines.push_back(outcome.text);
    auto everyone = repo.get_characters();
    std::vector<Character*> party;
    for (int id : run.party) {
        if (Character* c = find(everyone, id); c && !c->dead) party.push_back(c);
    }
    Character* roller = find(everyone, roller_id);
    std::vector<Character*> changed;
    auto touch = [&](Character* c) {
        if (std::find(changed.begin(), changed.end(), c) == changed.end()) changed.push_back(c);
    };

    if (outcome.damage_dice > 0) {
        int rolled = Dice::sum(dice.roll(outcome.damage_sides, outcome.damage_dice));
        std::vector<Character*> hit = outcome.damage_everyone ? party : std::vector<Character*>{};
        if (!outcome.damage_everyone && roller) hit.push_back(roller);
        for (Character* c : hit) {
            lines.push_back(hurt(*c, rolled) + (outcome.damage_type.empty() ? "" : " (" + outcome.damage_type + ")"));
            touch(c);
        }
    }
    if (outcome.exhaustion > 0) {
        for (Character* c : party) {
            c->exhaustion = std::min(6, c->exhaustion + outcome.exhaustion);
            lines.push_back(c->character_name + " gains a level of exhaustion (now " + std::to_string(c->exhaustion) +
                            ").");
            touch(c);
        }
    }
    if (outcome.inspiration && roller) {
        roller->inspiration = true;
        lines.push_back(roller->character_name + " gains inspiration: advantage on one attack or check.");
        touch(roller);
    }
    if (outcome.coins_cp > 0 && !party.empty()) {
        auto shares = split_cp(outcome.coins_cp, static_cast<int>(party.size()));
        for (size_t i = 0; i < party.size(); ++i) {
            add_coins(party[i]->coins, coins_from_cp(shares[i]));
            touch(party[i]);
        }
        lines.push_back("The party finds " + format_cp(outcome.coins_cp) + ".");
    }
    for (Character* c : changed) repo.update_character(*c);

    Character* finder = roller ? roller : (party.empty() ? nullptr : party.front());
    if (finder) {
        auto items = repo.get_items();
        for (const auto& loot : outcome.loot) {
            auto item = std::find_if(items.begin(), items.end(), [&](const Item& i) { return i.name == loot.item_name; });
            if (item == items.end()) continue;
            repo.add_to_inventory(finder->character_id, item->item_id, loot.quantity);
            lines.push_back(finder->character_name + " takes " + std::to_string(loot.quantity) + " × " + loot.item_name +
                            ".");
        }
    }

    if (!outcome.unlock.empty() &&
        std::find(run.flags.begin(), run.flags.end(), outcome.unlock) == run.flags.end()) {
        run.flags.push_back(outcome.unlock);
    }
    if (outcome.attitude_change != 0) {
        std::string key = outcome.attitude_npc.empty() ? scene_npc : outcome.attitude_npc;
        if (auto npc = find_run_npc(repo, run, key)) {
            Attitude before = npc->attitude;
            npc->attitude = shift_attitude(before, outcome.attitude_change);
            if (npc->attitude != before) {
                repo.update_npc(*npc);
                lines.push_back(npc->name + " is now " + attitude_name(npc->attitude) + " toward the party.");
            }
        }
    }
    if (!outcome.ally_joins.empty()) {
        const std::string& key = outcome.ally_joins;
        auto npc = find_run_npc(repo, run, key);
        if (npc && !npc->dead && npc->attitude != Attitude::Hostile && !npc->stat_block.empty()) {
            if (std::find(run.allies.begin(), run.allies.end(), key) == run.allies.end()) run.allies.push_back(key);
            lines.push_back(npc->name + " will fight beside the party in the next fight.");
        }
    }
    if (outcome.skip_next_fight) run.skip_next_fight = true;
    run.fewer_monsters += outcome.fewer_monsters;
    run.more_monsters += outcome.more_monsters;
    if (outcome.surprise != Surprise::None) run.surprise = outcome.surprise;
    return lines;
}

std::optional<Npc> find_run_npc(Repository& repo, const AdventureRun& run, const std::string& key) {
    if (key == "merchant") return shop_merchant(repo, run.campaign_id);
    if (key == "priest") return temple_priest(repo, run.campaign_id);
    if (const AdventureTemplate* adventure = find_adventure(run.title)) {
        for (const auto& npc : adventure->npcs) {
            if (!key.empty() && npc.key == key) return meet_npc(repo, run.campaign_id, npc);
        }
    }
    return std::nullopt;
}

std::vector<std::string> social_approaches(const SceneOption& option) {
    if (!option.approaches.empty()) return option.approaches;
    return {"Persuasion", "Deception", "Intimidation"};
}

std::vector<FightAlly> next_fight_allies(Repository& repo, const AdventureRun& run,
                                         const std::vector<Monster>& monsters) {
    std::vector<FightAlly> allies;
    for (const auto& key : run.allies) {
        auto npc = find_run_npc(repo, run, key);
        if (!npc || npc->dead || npc->attitude == Attitude::Hostile) continue;
        auto block = std::find_if(monsters.begin(), monsters.end(),
                                  [&](const Monster& m) { return m.name == npc->stat_block; });
        if (block != monsters.end()) allies.push_back({*npc, *block});
    }
    return allies;
}

void mark_npcs_dead(Repository& repo, int campaign_id, const std::vector<int>& npc_ids) {
    for (auto npc : repo.get_npcs(campaign_id)) {
        if (std::find(npc_ids.begin(), npc_ids.end(), npc.npc_id) != npc_ids.end() && !npc.dead) {
            npc.dead = true;
            repo.update_npc(npc);
        }
    }
}

CheckResult read_npc(Repository& repo, Npc& npc, const CheckRoller& roller, int dc, const Dice& dice, bool helped,
                     bool inspired) {
    CheckResult result = ability_check(dice, roller, "Insight", dc, helped, inspired);
    if (result.success && !npc.known) {
        npc.known = true;
        repo.update_npc(npc);
    }
    return result;
}

BestPassive best_passive(const std::vector<CheckRoller>& party, const std::string& what) {
    BestPassive best;
    best.score = -100;
    for (const auto& roller : party) {
        int score = passive_score(roller, what);
        if (score > best.score) best = {roller.character.character_id, roller.character.character_name, score};
    }
    return best;
}

std::vector<std::string> spring_trap(Repository& repo, const SceneTrap& trap, const std::vector<int>& party,
                                     int victim_id, const Dice& dice) {
    std::vector<std::string> lines;
    auto everyone = repo.get_characters();
    int rolled = Dice::sum(dice.roll(trap.damage_sides, trap.damage_dice));
    for (auto& c : everyone) {
        bool in = std::find(party.begin(), party.end(), c.character_id) != party.end();
        if (!in || c.dead || (!trap.everyone && c.character_id != victim_id)) continue;
        // Exhaustion 3+ means disadvantage on saves; items (Cloak of Protection...) count.
        bool exhausted = c.exhaustion >= 3;
        int roll = dice.roll_d20(exhausted ? RollMode::Disadvantage : RollMode::Normal).result;
        int bonus = saving_throw_bonus(c, trap.save_ability, repo.get_inventory(c.character_id)) - c.revival_penalty;
        bool saved = roll + bonus >= trap.save_dc;
        std::string tag = ability_name(trap.save_ability);
        lines.push_back(c.character_name + ": " + tag.substr(0, 3) + " save " + (exhausted ? "(disadvantage) " : "") +
                        "d20(" + std::to_string(roll) + ") " +
                        (bonus >= 0 ? "+" : "") + std::to_string(bonus) + " vs DC " + std::to_string(trap.save_dc) +
                        " -> " + (saved ? "half. " : "failed. ") + hurt(c, saved ? rolled / 2 : rolled));
        repo.update_character(c);
    }
    return lines;
}

Surprise shape_next_fight(const AdventureRun& run, AdventureScene& scene, bool& skip) {
    skip = run.skip_next_fight;
    for (int n = 0; n < run.fewer_monsters; ++n) {
        int total = 0;
        for (const auto& group : scene.monsters) total += group.count;
        if (total <= 1) break;
        for (auto group = scene.monsters.rbegin(); group != scene.monsters.rend(); ++group) {
            if (group->count > 0 && (group->count > 1 || scene.monsters.size() > 1)) {
                --group->count;
                break;
            }
        }
        scene.monsters.erase(std::remove_if(scene.monsters.begin(), scene.monsters.end(),
                                            [](const AdventureMonsterGroup& g) { return g.count <= 0; }),
                             scene.monsters.end());
    }
    if (!scene.monsters.empty()) scene.monsters.front().count += run.more_monsters;
    return run.surprise;
}

}  // namespace dnd
