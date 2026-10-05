#include "dnd/adventure.hpp"

#include <utility>

namespace dnd {

namespace {

// A one-fight adventure: a single scene with the adventure's hook as its narration.
AdventureTemplate single(std::string title, std::string hook, std::string victory_text,
                         std::vector<AdventureMonsterGroup> monsters, std::vector<AdventureLoot> loot, int coins_cp,
                         int suggested_level) {
    AdventureTemplate adventure;
    adventure.title = title;
    adventure.hook = hook;
    adventure.victory_text = std::move(victory_text);
    adventure.suggested_level = suggested_level;
    AdventureScene scene;
    scene.title = std::move(title);
    scene.narration = std::move(hook);
    scene.monsters = std::move(monsters);
    scene.loot = std::move(loot);
    scene.coins_cp = coins_cp;
    adventure.scenes.push_back(std::move(scene));
    return adventure;
}

AdventureScene scene(std::string title, std::string narration, std::vector<AdventureMonsterGroup> monsters,
                     std::vector<AdventureLoot> loot, int coins_cp, bool camp_after = false, bool boss = false) {
    AdventureScene result;
    result.title = std::move(title);
    result.narration = std::move(narration);
    result.monsters = std::move(monsters);
    result.loot = std::move(loot);
    result.coins_cp = coins_cp;
    result.camp_after = camp_after;
    result.boss = boss;
    return result;
}

}  // namespace

const std::vector<AdventureTemplate>& adventure_templates() {
    static const std::vector<AdventureTemplate> templates = {
        single(
            "Ambush on the Trail",
            "Goblins burst from the underbrush, blades drawn.",
            "The trail is clear. The goblins won't trouble travelers again.",
            {{"Goblin", 3}},
            {{"Potion of Healing", 1}, {"Rations (1 day)", 2}},
            800,  // 8 gp
            1),  // suggested level
        single(
            "Wolves at the Campfire",
            "Hungry eyes circle your campsite in the dark.",
            "The wolves retreat into the night. You can finally get some rest.",
            {{"Wolf", 2}},
            {{"Potion of Healing", 1}},
            300,  // 3 gp
            1),  // suggested level
        single(
            "The Crypt Awakens",
            "Dust falls from the ceiling as the dead rise to bar your way.",
            "The crypt falls silent again. Whatever disturbed the dead here is gone.",
            {{"Skeleton", 2}, {"Zombie", 1}},
            {{"Potion of Healing", 1}, {"Torch", 3}},
            1500,  // 15 gp
            1),  // suggested level
        single(
            "Orc Raiding Party",
            "An orc and its goblin scouts have you pinned down.",
            "The raiding party is broken. This stretch of road is safe again.",
            {{"Orc", 1}, {"Goblin", 2}},
            {{"Potion of Healing", 2}, {"Hempen Rope (50 feet)", 1}},
            1200,  // 12 gp
            2),  // suggested level
        // Tougher fights for levelled parties (SRD monsters, CR 1/2-5);
        // roughly levels 3 to 7 for a party of three or four.
        single(
            "The Ogre's Larder",
            "Bones crunch somewhere ahead. An ogre and its goblin hangers-on are guarding a cave full of stolen "
            "food.",
            "The ogre topples like a felled tree. The stolen stores can go back to the village.",
            {{"Ogre", 1}, {"Goblin", 2}},
            {{"Potion of Healing", 2}, {"Rations (1 day)", 5}},
            2000,  // 20 gp
            3),  // suggested level
        single(
            "The Ghoul Crypt",
            "The stench hits first: ghouls are feeding in the old family crypt, and their claws can freeze you "
            "where you stand.",
            "The last ghoul falls still. Sunlight finally reaches the crypt floor.",
            {{"Ghoul", 2}, {"Skeleton", 2}},
            {{"Potion of Healing", 2}, {"Healer's Kit", 1}},
            2500,  // 25 gp
            3),  // suggested level
        single(
            "The Bandit Captain's Camp",
            "A bandit captain and hobgoblin mercenaries hold the ford, taking a toll from everyone who "
            "crosses.",
            "The captain's camp is scattered. Travelers can cross the ford for free again.",
            {{"Bandit Captain", 1}, {"Hobgoblin", 2}},
            {{"Potion of Healing", 2}, {"Longsword", 1}, {"Shield", 1}},
            4000,  // 40 gp
            4),  // suggested level
        single(
            "Hounds of the Burning Pass",
            "Smoke curls between the rocks. Hell hounds are hunting the pass, and fire won't touch them.",
            "The pass cools. Whatever summoned the hounds has lost its pack.",
            {{"Hell Hound", 2}},
            {{"Potion of Healing", 3}},
            1500,  // 15 gp
            5),  // suggested level
        single(
            "The Troll Bridge",
            "A troll squats under the old stone bridge. Cut it and it heals; only fire or acid stops it.",
            "The troll finally stays down. The bridge is open again.",
            {{"Troll", 1}},
            {{"Potion of Healing", 3}, {"Chain Mail", 1}},
            5000,  // 50 gp
            6),  // suggested level
        // Multi-scene adventures (docs/adventures-and-currency.md): fights in a row, a camp for a long
        // rest partway, and a boss at the end. Coins are for a level-1 party; they scale with level.
        {
            "The Goblin Warren",
            "Goblins have been raiding the farms below the hills. Their warren is a short climb away, and the "
            "farmers want it cleared.",
            "The warren's boss is down and the goblins scatter into the hills. The farms are safe for now.",
            1,
            {
                scene("The Lookouts", "Two goblins keep a lazy watch at the warren mouth, a hungry wolf at their feet.",
                      {{"Goblin", 2}, {"Wolf", 1}}, {}, 300),
                scene("The Wolf Pens", "Deeper in, the goblins keep their wolves. The pack has caught your scent.",
                      {{"Wolf", 2}}, {{"Rations (1 day)", 2}}, 200, true),
                scene("The Boss's Den", "A hobgoblin has taken charge of the warren. It isn't going to give it "
                      "up without a fight.",
                      {{"Hobgoblin", 1}, {"Goblin", 2}}, {{"Potion of Healing", 2}, {"Cloak of Protection", 1}}, 1500,
                      false, true),
            },
        },
        {
            "Crypt of the Restless",
            "The old barrow on the moor has been opened, and the dead walk out of it at night. Someone has to go "
            "down and put them back.",
            "The ghouls lie still and the barrow is quiet. Whatever stirred the dead has nothing left to command.",
            2,
            {
                scene("The Ossuary", "Bones rattle out of their niches and stand up to meet you.",
                      {{"Skeleton", 3}}, {{"Torch", 2}}, 400),
                scene("The Flooded Tomb", "Knee-deep water, and shapes shambling through it.",
                      {{"Zombie", 2}, {"Skeleton", 1}}, {}, 500, true),
                scene("The Ghouls' Feast", "The smell of the barrow's deepest room is the worst yet. Ghouls look up "
                      "from what they were eating.",
                      {{"Ghoul", 2}},
                      {{"Potion of Greater Healing", 1}, {"Healer's Kit", 1}, {"Mace +1", 1},
                       {"Spell Scroll (Cure Wounds)", 1}},
                      2000, false, true),
            },
        },
        {
            "The Hill Giant's Toll",
            "A hill giant has claimed the mountain road and taken gnolls and ogres into its service. Traders pay "
            "or turn back. You won't do either.",
            "The giant crashes down across its own road. The traders can pass again -- and they're grateful.",
            5,
            {
                scene("Gnoll Scouts", "Gnolls cackle in the rocks above the road.", {{"Gnoll", 3}}, {}, 600),
                scene("The Ogre Gate", "Two ogres guard a crude gate of logs across the road.", {{"Ogre", 2}},
                      {{"Potion of Healing", 2}}, 1000, true),
                scene("The Owlbear Pit", "The giant keeps a pet. It's hungry.", {{"Owlbear", 1}}, {}, 800),
                scene("The Giant's Hall", "The hill giant is waiting in a hall of stolen timber, and it's angry.",
                      {{"Hill Giant", 1}},
                      {{"Potion of Greater Healing", 2}, {"Shield +1", 1}, {"Gauntlets of Ogre Power", 1}}, 5000,
                      false, true),
            },
        },
        // Levels 5-8 (docs/content-5-10.md): monsters that resist plain steel, breathe, cast, and drain.
        {
            "The Barrow-King",
            "Lights move in the old royal barrow, and travelers on the moor road have been found pale and cold. "
            "The barrow-king has risen, and his honor guard with him.",
            "The barrow-king's wraith comes apart in the dawn wind. The moor is only a moor again.",
            5,
            {
                scene("The Barrow Gate", "A wight in rusted mail stands at the door with the dead it commands.",
                      {{"Wight", 1}, {"Skeleton", 3}}, {}, 800),
                scene("The Hall of Kings", "Two of the old king's champions rise from their biers as you pass.",
                      {{"Wight", 2}}, {{"Potion of Greater Healing", 2}}, 1000, true),
                scene("The King's Tomb", "The barrow-king waits above his coffin, a shadow with burning eyes.",
                      {{"Wraith", 1}, {"Wight", 1}},
                      {{"Longsword +2", 1}, {"Ring of Protection", 1}, {"Potion of Superior Healing", 1}}, 3000,
                      false, true),
            },
        },
        {
            "Moonrise at Ashgrove",
            "Ashgrove's sheep are torn apart every full moon, and the villagers won't say why. A cult keeps the secret, "
            "and tonight the moon is full.",
            "The cult is broken and the curse with it. Ashgrove sleeps through its first quiet full moon in years.",
            6,
            {
                scene("The Lane at Dusk", "A robed figure and something that was a farmhand step out of the hedges.",
                      {{"Werewolf", 1}, {"Cult Fanatic", 1}}, {}, 1200),
                scene("The Old Mill", "The mill wheel creaks. Two shapes on the millrace are not wolves, and not men.",
                      {{"Werewolf", 2}}, {{"Potion of Greater Healing", 2}}, 1200, true),
                scene("The Moon Shrine", "The cult's high priest raises a knife to the moon, and his pack answers.",
                      {{"Werewolf", 2}, {"Cult Fanatic", 1}},
                      {{"Warhammer +2", 1}, {"Bracers of Defense", 1}, {"Potion of Superior Healing", 2}}, 4000,
                      false, true),
            },
        },
        {
            "The White Wyrm's Lair",
            "A young white dragon has claimed the high pass, and a renegade mage and a stone giant serve it. The "
            "caravans have stopped.",
            "The wyrm is dead and its hoard is yours. The pass is open again.",
            7,
            {
                scene("The Frozen Pass", "Crossbows click from the rocks: the wyrm's hired swords hold the pass.",
                      {{"Veteran", 2}}, {}, 1500),
                scene("The Quarry", "A stone giant hurls boulders down the quarry face.", {{"Stone Giant", 1}},
                      {{"Potion of Greater Healing", 2}}, 1500, true),
                scene("The Ice Tower", "The renegade mage waits in his tower, his guard beside him.",
                      {{"Mage", 1}, {"Veteran", 1}}, {{"Spell Scroll (Fireball)", 1}}, 2000),
                scene("The Wyrm's Lair", "The young white dragon uncoils from its hoard of frozen gold.",
                      {{"Young White Dragon", 1}},
                      {{"Amulet of Health", 1}, {"Plate Armor +1", 1}, {"Shield +2", 1},
                       {"Potion of Supreme Healing", 1}},
                      6000, false, true),
            },
        },
        single(
            "The Bulette Field",
            "Farmers have been vanishing from their own fields, pulled down into the earth. Something big is "
            "burrowing under the barley.",
            "The landshark is dead. The farmers can plough again.",
            {{"Bulette", 1}},
            {{"Potion of Greater Healing", 2}},
            1500,  // 15 gp
            5),
        single(
            "The Golem's Workshop",
            "A dead wizard's flesh golem still guards his workshop, and nothing nonmagical seems to hurt it.",
            "The golem falls apart at the seams. The workshop and its notes are yours.",
            {{"Flesh Golem", 1}},
            {{"Spell Scroll (Lightning Bolt)", 1}, {"Potion of Greater Healing", 1}},
            2000,  // 20 gp
            6),
        single(
            "The Gorgon's Pasture",
            "A gorgon has taken the high pasture, and the shepherds have become statues.",
            "The gorgon falls. With it dead, the statues are only statues -- but the pasture is safe.",
            {{"Gorgon", 1}},
            {{"Potion of Superior Healing", 1}, {"Shield +1", 1}},
            2000,  // 20 gp
            6),
        single(
            "The Green Wyrm of Thornwood",
            "A young green dragon has made Thornwood its own, and its poison mists creep toward the villages.",
            "The green wyrm is dead and Thornwood's air clears. Its hoard is yours.",
            {{"Young Green Dragon", 1}},
            {{"Longbow +2", 1}, {"Chain Mail +2", 1}, {"Potion of Supreme Healing", 1}},
            8000,  // 80 gp
            8),
    };
    return templates;
}

const AdventureTemplate* find_adventure(const std::string& title) {
    for (const auto& adventure : adventure_templates()) {
        if (adventure.title == title) return &adventure;
    }
    return nullptr;
}

}  // namespace dnd
