#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "dnd/adventure.hpp"
#include "dnd/npc.hpp"
#include "dnd/progression.hpp"
#include "dnd/repository.hpp"
#include "dnd/scenes.hpp"
#include "dnd/shop.hpp"

namespace {

class ScriptedRandomSource : public dnd::IRandomSource {
public:
    explicit ScriptedRandomSource(std::vector<int> values) : values_(std::move(values)) {}
    int roll_die(int sides) override {
        int value = index_ < values_.size() ? values_[index_++] : values_.back();
        return std::min(value, sides);
    }

private:
    std::vector<int> values_;
    size_t index_ = 0;
};

struct Table {
    dnd::Repository repo{":memory:"};
    int player = repo.add_player("Adam");

    int add(const std::string& name, int charisma = 10) {
        dnd::Character c;
        c.character_name = name;
        c.player_id = player;
        c.class_id = repo.get_classes().front().id;
        c.race = repo.get_races().front().id;
        c.alignment = repo.get_alignments().front().id;
        c.level = 3;
        c.charisma = charisma;
        c.max_hp = 20;
        c.current_hp = 20;
        c.coins = {0, 0, 0, 1000, 0};
        return repo.add_character(c);
    }

    dnd::Character get(int id) {
        for (const auto& c : repo.get_characters()) {
            if (c.character_id == id) return c;
        }
        return {};
    }

    dnd::Npc npc(int campaign, const std::string& key) {
        for (const auto& n : repo.get_npcs(campaign)) {
            if (n.key == key) return n;
        }
        return {};
    }

    int item(const std::string& name) {
        for (const auto& i : repo.get_items()) {
            if (i.name == name) return i.item_id;
        }
        return 0;
    }
};

}  // namespace

TEST_CASE("social_dc follows the DMG's conversation table", "[npcs]") {
    using dnd::Attitude;
    using dnd::SocialRequest;
    REQUIRE(dnd::social_dc(Attitude::Friendly, SocialRequest::NoRisk) == 0);
    REQUIRE(dnd::social_dc(Attitude::Friendly, SocialRequest::MinorRisk) == 10);
    REQUIRE(dnd::social_dc(Attitude::Friendly, SocialRequest::SignificantRisk) == 20);
    REQUIRE(dnd::social_dc(Attitude::Indifferent, SocialRequest::NoHarm) == 0);
    REQUIRE(dnd::social_dc(Attitude::Indifferent, SocialRequest::NoRisk) == 10);
    REQUIRE(dnd::social_dc(Attitude::Indifferent, SocialRequest::MinorRisk) == 20);
    REQUIRE_FALSE(dnd::social_dc(Attitude::Indifferent, SocialRequest::SignificantRisk).has_value());
    REQUIRE(dnd::social_dc(Attitude::Hostile, SocialRequest::NoHarm) == 10);
    REQUIRE(dnd::social_dc(Attitude::Hostile, SocialRequest::NoRisk) == 20);
    REQUIRE_FALSE(dnd::social_dc(Attitude::Hostile, SocialRequest::MinorRisk).has_value());

    dnd::Npc npc;
    npc.attitude = Attitude::Indifferent;
    REQUIRE(dnd::conversation_dc(npc, SocialRequest::NoRisk) == 10);
    npc.known = true;  // knowing what they want and fear
    REQUIRE(dnd::conversation_dc(npc, SocialRequest::NoRisk) == 5);
    REQUIRE(dnd::conversation_dc(npc, SocialRequest::NoHarm) == 0);
    REQUIRE_FALSE(dnd::conversation_dc(npc, SocialRequest::SignificantRisk).has_value());
}

TEST_CASE("attitudes shift a step at a time, and stop at the ends", "[npcs]") {
    using dnd::Attitude;
    REQUIRE(dnd::shift_attitude(Attitude::Indifferent, 1) == Attitude::Friendly);
    REQUIRE(dnd::shift_attitude(Attitude::Indifferent, -1) == Attitude::Hostile);
    REQUIRE(dnd::shift_attitude(Attitude::Friendly, 2) == Attitude::Friendly);
    REQUIRE(dnd::shift_attitude(Attitude::Friendly, -5) == Attitude::Hostile);
    REQUIRE(dnd::attitude_from_name(dnd::attitude_name(Attitude::Hostile)) == Attitude::Hostile);
    REQUIRE(dnd::attitude_from_name("nonsense") == Attitude::Indifferent);
}

TEST_CASE("NPCs are saved per campaign, and met once", "[npcs][repository]") {
    Table t;
    dnd::Campaign campaign;
    campaign.name = "Tuesday";
    int tuesday = t.repo.add_campaign(campaign);

    dnd::Npc tobin;
    tobin.key = "tobin";
    tobin.name = "Old Tobin";
    tobin.wants = "peace";
    dnd::Npc met = dnd::meet_npc(t.repo, tuesday, tobin);
    REQUIRE(met.npc_id != 0);
    REQUIRE(met.campaign_id == tuesday);
    met.attitude = dnd::Attitude::Friendly;
    t.repo.update_npc(met);

    // Met again: as the campaign left him, not as the adventure wrote him.
    REQUIRE(dnd::meet_npc(t.repo, tuesday, tobin).attitude == dnd::Attitude::Friendly);
    REQUIRE(t.repo.get_npcs(tuesday).size() == 1);
    // Another campaign (or none) meets its own Tobin.
    REQUIRE(dnd::meet_npc(t.repo, 0, tobin).attitude == dnd::Attitude::Indifferent);
    REQUIRE(t.repo.get_npcs(0).size() == 1);

    REQUIRE(dnd::shop_merchant(t.repo, tuesday).name == "Hilda Brannock");
    REQUIRE(dnd::temple_priest(t.repo, tuesday).name == "Brother Aldous");
    REQUIRE(t.repo.get_npcs(tuesday).size() == 3);

    t.repo.delete_npc(met.npc_id);
    REQUIRE(t.repo.get_npcs(tuesday).size() == 2);
    t.repo.delete_campaign(tuesday);  // its NPCs go with it
    REQUIRE(t.repo.get_npcs(tuesday).empty());
    REQUIRE(t.repo.get_npcs(0).size() == 1);
}

TEST_CASE("scene outcomes change attitudes: the scene's NPC, the merchant, or the priest", "[npcs][scenes]") {
    Table t;
    int aria = t.add("Aria");
    dnd::AdventureRun run;
    run.title = "The Goblin Warren";
    run.party = {aria};
    ScriptedRandomSource random({10});
    dnd::Dice dice(random);

    dnd::SceneOutcome warmer;
    warmer.attitude_change = 1;
    auto lines = dnd::apply_outcome(t.repo, run, warmer, aria, dice, "snig");
    REQUIRE(t.npc(0, "snig").attitude == dnd::Attitude::Friendly);
    REQUIRE(std::find(lines.begin(), lines.end(), "Snig is now Friendly toward the party.") != lines.end());

    dnd::SceneOutcome merchant;
    merchant.attitude_change = 1;
    merchant.attitude_npc = "merchant";
    dnd::apply_outcome(t.repo, run, merchant, aria, dice, "snig");
    REQUIRE(t.npc(0, "merchant").attitude == dnd::Attitude::Friendly);
    REQUIRE(t.npc(0, "snig").attitude == dnd::Attitude::Friendly);  // unchanged

    // Already as friendly as can be: no change, no line.
    REQUIRE(dnd::apply_outcome(t.repo, run, merchant, aria, dice).empty());
    // A key the adventure doesn't know is ignored.
    dnd::SceneOutcome stranger;
    stranger.attitude_change = -1;
    stranger.attitude_npc = "nobody";
    REQUIRE(dnd::apply_outcome(t.repo, run, stranger, aria, dice).empty());
}

TEST_CASE("reading an NPC with Insight reveals what they want and fear", "[npcs][scenes]") {
    Table t;
    int aria = t.add("Aria");
    dnd::AdventureRun run;
    run.title = "The Goblin Warren";
    run.party = {aria};
    auto rollers = dnd::party_rollers(t.repo, run.party);
    dnd::Npc snig = *dnd::find_run_npc(t.repo, run, "snig");

    ScriptedRandomSource low({2});
    dnd::Dice low_dice(low);
    REQUIRE_FALSE(dnd::read_npc(t.repo, snig, rollers.front(), 10, low_dice).success);
    REQUIRE_FALSE(t.npc(0, "snig").known);

    ScriptedRandomSource high({18});
    dnd::Dice high_dice(high);
    REQUIRE(dnd::read_npc(t.repo, snig, rollers.front(), 10, high_dice).success);
    REQUIRE(t.npc(0, "snig").known);
    REQUIRE(dnd::conversation_dc(snig, dnd::SocialRequest::NoRisk) == 5);
}

TEST_CASE("adventure NPCs: every Social scene's NPC is defined, with unique keys", "[npcs][adventure]") {
    int social = 0;
    for (const auto& adventure : dnd::adventure_templates()) {
        std::vector<std::string> keys;
        for (const auto& npc : adventure.npcs) {
            REQUIRE(std::find(keys.begin(), keys.end(), npc.key) == keys.end());
            REQUIRE_FALSE(npc.name.empty());
            REQUIRE_FALSE(npc.wants.empty());
            REQUIRE_FALSE(npc.fears.empty());
            keys.push_back(npc.key);
        }
        for (const auto& scene : adventure.scenes) {
            if (scene.kind != dnd::SceneKind::Social) continue;
            ++social;
            REQUIRE(std::find(keys.begin(), keys.end(), scene.npc) != keys.end());
            bool any_social = false;
            for (const auto& option : scene.options) any_social = any_social || option.social;
            REQUIRE(any_social);
        }
    }
    REQUIRE(social >= 4);
}

TEST_CASE("haggling: a Persuasion check for 10% off until the next long rest", "[npcs][shop]") {
    Table t;
    int aria = t.add("Aria", 18);  // +4
    int item = t.item("Hempen Rope (50 feet)");  // 1 gp

    SECTION("an Indifferent merchant: DC 20") {
        ScriptedRandomSource random({15});  // 15 + 4 = 19
        dnd::Dice dice(random);
        auto result = dnd::haggle(t.repo, aria, dice);
        REQUIRE_FALSE(result.ok);
        REQUIRE(result.message.find("vs DC 20") != std::string::npos);
        REQUIRE(t.get(aria).haggle == -1);
        REQUIRE(dnd::price_for(t.get(aria), 100) == 100);
        REQUIRE_FALSE(dnd::haggle(t.repo, aria, dice).ok);  // once between long rests
    }
    SECTION("a Friendly merchant: DC 10, and the discount applies to buying") {
        dnd::Npc merchant = dnd::shop_merchant(t.repo, 0);
        merchant.attitude = dnd::Attitude::Friendly;
        t.repo.update_npc(merchant);
        ScriptedRandomSource random({8});  // 8 + 4 = 12
        dnd::Dice dice(random);
        REQUIRE(dnd::haggle(t.repo, aria, dice).ok);
        dnd::Character buyer = t.get(aria);
        REQUIRE(buyer.haggle == 1);
        REQUIRE(dnd::price_for(buyer, 1000) == 900);
        REQUIRE(dnd::price_for(buyer, 15) == 14);  // the discount rounds down
        auto bought = dnd::buy(t.repo, aria, item, 10);  // 10 gp list
        REQUIRE(bought.ok);
        REQUIRE(bought.message.find("9 gp") != std::string::npos);

        dnd::Character rested = t.get(aria);
        dnd::take_long_rest(rested);
        REQUIRE(rested.haggle == 0);
    }
    SECTION("a Hostile merchant won't haggle") {
        dnd::Npc merchant = dnd::shop_merchant(t.repo, 0);
        merchant.attitude = dnd::Attitude::Hostile;
        t.repo.update_npc(merchant);
        ScriptedRandomSource random({20});
        dnd::Dice dice(random);
        auto result = dnd::haggle(t.repo, aria, dice);
        REQUIRE_FALSE(result.ok);
        REQUIRE(result.message.find("Hostile") != std::string::npos);
        REQUIRE(t.get(aria).haggle == 0);
    }
}

TEST_CASE("a Friendly priest waives the temple's fee for raise dead", "[npcs][shop]") {
    Table t;
    int aria = t.add("Aria");
    REQUIRE(dnd::raise_dead_cost_cp(t.repo, 0) == dnd::kRaiseDeadCostCp);
    dnd::Npc priest = dnd::temple_priest(t.repo, 0);
    priest.attitude = dnd::Attitude::Friendly;
    t.repo.update_npc(priest);
    REQUIRE(dnd::raise_dead_cost_cp(t.repo, 0) == dnd::kRaiseDeadFriendlyCostCp);

    dnd::Character dead = t.get(aria);
    dead.dead = true;
    dead.current_hp = 0;
    t.repo.update_character(dead);
    REQUIRE(dnd::raise_dead(t.repo, aria, aria).ok);
    REQUIRE(t.get(aria).coins[3] == 500);  // 1000 gp - 500 gp
}
