#include <catch2/catch_test_macros.hpp>

#include <string>

#include "dnd/campaign.hpp"
#include "dnd/repository.hpp"
#include "dnd/shop.hpp"

namespace {

struct Table {
    dnd::Repository repo{":memory:"};
    int adam = repo.add_player("Adam");
    int beth = repo.add_player("Beth");

    int add(const std::string& name, int player_id) {
        dnd::Character c;
        c.character_name = name;
        c.class_id = repo.get_classes().front().id;
        c.race = repo.get_races().front().id;
        c.alignment = repo.get_alignments().front().id;
        c.player_id = player_id;
        c.max_hp = 10;
        c.current_hp = 10;
        c.coins = {0, 0, 0, 100, 0};
        return repo.add_character(c);
    }

    int campaign(const std::string& name) {
        dnd::Campaign c;
        c.name = name;
        return repo.add_campaign(c);
    }

    dnd::Character get(int id) {
        for (const auto& c : repo.get_characters()) {
            if (c.character_id == id) return c;
        }
        FAIL("no character " << id);
        return {};
    }
};

}  // namespace

TEST_CASE("campaigns: create, rename, and choose the active one", "[campaign]") {
    Table t;
    REQUIRE(t.repo.active_campaign_id() == 0);
    int mines = t.campaign("Lost Mine");
    int dragon = t.campaign("Dragon Heist");
    REQUIRE(t.repo.get_campaigns().size() == 2);
    REQUIRE(t.repo.get_campaigns().front().name == "Dragon Heist");  // by name

    t.repo.set_active_campaign(mines);
    REQUIRE(t.repo.active_campaign_id() == mines);
    REQUIRE(dnd::active_campaign_name(t.repo) == "Lost Mine");
    t.repo.set_active_campaign(dragon);  // only one at a time
    REQUIRE(t.repo.active_campaign_id() == dragon);
    t.repo.set_active_campaign(0);
    REQUIRE(t.repo.active_campaign_id() == 0);
    REQUIRE(dnd::active_campaign_name(t.repo).empty());

    auto renamed = t.repo.get_campaigns().back();
    renamed.name = "Lost Mine of Phandelver";
    renamed.notes = "Session 1: the goblin ambush.";
    t.repo.update_campaign(renamed);
    bool found = false;
    for (const auto& c : t.repo.get_campaigns()) found = found || (c.name == "Lost Mine of Phandelver" && !c.notes.empty());
    REQUIRE(found);
}

TEST_CASE("campaigns: rosters scope who's in play", "[campaign]") {
    Table t;
    int clark = t.add("Clark", t.adam);
    int phi = t.add("Phi", t.beth);
    int solo = t.add("Solo", t.adam);
    int mines = t.campaign("Lost Mine");
    t.repo.set_character_campaign(clark, mines);
    t.repo.set_character_campaign(phi, mines);

    SECTION("no campaign active: everyone") {
        REQUIRE(dnd::characters_in_play(t.repo).size() == 3);
        REQUIRE(dnd::in_play(t.repo, t.get(solo)));
    }
    SECTION("a campaign active: its roster, across players") {
        t.repo.set_active_campaign(mines);
        auto members = dnd::characters_in_play(t.repo);
        REQUIRE(members.size() == 2);
        REQUIRE_FALSE(dnd::in_play(t.repo, t.get(solo)));
        REQUIRE(t.get(clark).campaign_id == mines);
    }
    SECTION("update_character keeps the campaign") {
        auto c = t.get(clark);
        c.campaign_id = 0;  // ignored: only set_character_campaign moves characters
        c.current_hp = 5;
        t.repo.update_character(c);
        REQUIRE(t.get(clark).campaign_id == mines);
    }
    SECTION("trading stays inside the active campaign") {
        t.repo.set_active_campaign(mines);
        REQUIRE(dnd::give_coins(t.repo, clark, phi, {0, 0, 0, 5, 0}).ok);
        auto refused = dnd::give_coins(t.repo, clark, solo, {0, 0, 0, 5, 0});
        REQUIRE_FALSE(refused.ok);
        REQUIRE(refused.message == "Solo isn't in this campaign.");
        auto dead = t.get(phi);
        dead.dead = true;
        t.repo.update_character(dead);
        REQUIRE_FALSE(dnd::raise_dead(t.repo, phi, solo).ok);
    }
    SECTION("deleting a campaign keeps its characters") {
        t.repo.set_active_campaign(mines);
        t.repo.delete_campaign(mines);
        REQUIRE(t.repo.active_campaign_id() == 0);
        REQUIRE(t.get(clark).campaign_id == 0);
        REQUIRE(t.repo.get_characters().size() == 3);
    }
}

TEST_CASE("campaigns: adventure runs belong to the campaign they started in", "[campaign]") {
    Table t;
    int mines = t.campaign("Lost Mine");
    dnd::AdventureRun open_run;
    open_run.title = "The Goblin Warren";
    t.repo.add_adventure_run(open_run);
    dnd::AdventureRun campaign_run;
    campaign_run.title = "Crypt of the Restless";
    campaign_run.campaign_id = mines;
    t.repo.add_adventure_run(campaign_run);

    REQUIRE(dnd::runs_in_play(t.repo, true).size() == 1);
    REQUIRE(dnd::runs_in_play(t.repo, true).front().title == "The Goblin Warren");
    t.repo.set_active_campaign(mines);
    REQUIRE(dnd::runs_in_play(t.repo, true).size() == 1);
    REQUIRE(dnd::runs_in_play(t.repo, true).front().title == "Crypt of the Restless");

    t.repo.delete_campaign(mines);  // its runs fall back to "no campaign"
    REQUIRE(dnd::runs_in_play(t.repo, true).size() == 2);
}
