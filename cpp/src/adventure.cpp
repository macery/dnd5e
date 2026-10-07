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

// --- Non-fight scenes (docs/scenes-and-npcs.md) ---------------------------------------

SceneOutcome says(std::string text) {
    SceneOutcome outcome;
    outcome.text = std::move(text);
    return outcome;
}

SceneOutcome surprises(std::string text) {
    SceneOutcome outcome = says(std::move(text));
    outcome.surprise = Surprise::PartySurprises;
    return outcome;
}

SceneOutcome surprised(std::string text) {
    SceneOutcome outcome = says(std::move(text));
    outcome.surprise = Surprise::PartySurprised;
    return outcome;
}

SceneOutcome hurts(std::string text, int dice, int sides, std::string type, bool everyone = false) {
    SceneOutcome outcome = says(std::move(text));
    outcome.damage_dice = dice;
    outcome.damage_sides = sides;
    outcome.damage_type = std::move(type);
    outcome.damage_everyone = everyone;
    return outcome;
}

SceneOutcome fewer(std::string text, int count, bool inspiration = false) {
    SceneOutcome outcome = says(std::move(text));
    outcome.fewer_monsters = count;
    outcome.inspiration = inspiration;
    return outcome;
}

SceneOutcome more(std::string text, int count) {
    SceneOutcome outcome = says(std::move(text));
    outcome.more_monsters = count;
    return outcome;
}

SceneOption check(std::string label, std::string skill, int dc, SceneOutcome success, SceneOutcome failure,
                  bool group = false) {
    SceneOption option;
    option.label = std::move(label);
    option.check = std::move(skill);
    option.dc = dc;
    option.group = group;
    option.success = std::move(success);
    option.failure = std::move(failure);
    return option;
}

SceneOption pick(std::string label, SceneOutcome outcome) {
    SceneOption option;
    option.label = std::move(label);
    option.success = std::move(outcome);
    return option;
}

AdventureScene challenge(std::string title, std::string narration, std::vector<SceneOption> options) {
    AdventureScene result;
    result.title = std::move(title);
    result.narration = std::move(narration);
    result.kind = SceneKind::Challenge;
    result.options = std::move(options);
    return result;
}

AdventureScene trap(std::string title, std::string narration, SceneTrap details) {
    AdventureScene result;
    result.title = std::move(title);
    result.narration = std::move(narration);
    result.kind = SceneKind::Trap;
    result.trap = std::move(details);
    return result;
}

SceneTrap hazard(std::string name, int spot_dc, std::string disarm, int disarm_dc, int save_dc, int dice, int sides,
                 std::string type, bool everyone = false) {
    SceneTrap t;
    t.name = std::move(name);
    t.spot_dc = spot_dc;
    t.disarm_check = std::move(disarm);
    t.disarm_dc = disarm_dc;
    t.save_dc = save_dc;
    t.damage_dice = dice;
    t.damage_sides = sides;
    t.damage_type = std::move(type);
    t.everyone = everyone;
    return t;
}

// --- People and conversations ----------------------------------------------------------

Npc person(std::string key, std::string name, std::string role, Attitude attitude, std::string description,
           std::string wants, std::string fears, std::string stat_block = "") {
    Npc npc;
    npc.key = std::move(key);
    npc.name = std::move(name);
    npc.role = std::move(role);
    npc.attitude = attitude;
    npc.description = std::move(description);
    npc.wants = std::move(wants);
    npc.fears = std::move(fears);
    npc.stat_block = std::move(stat_block);
    return npc;
}

// The outcome, and someone's attitude moving: the scene's NPC, or `who`.
SceneOutcome sways(SceneOutcome outcome, int steps, std::string who = "") {
    outcome.attitude_change = steps;
    outcome.attitude_npc = std::move(who);
    return outcome;
}

// The outcome, and an NPC fighting beside the party in the next fight.
SceneOutcome joins(SceneOutcome outcome, std::string who) {
    outcome.ally_joins = std::move(who);
    return outcome;
}

SceneOutcome inspires(SceneOutcome outcome) {
    outcome.inspiration = true;
    return outcome;
}

// Asking an NPC for something: the DC comes from their attitude.
SceneOption ask(std::string label, SocialRequest request, SceneOutcome success, SceneOutcome failure,
                std::vector<std::string> approaches = {}) {
    SceneOption option;
    option.label = std::move(label);
    option.social = true;
    option.request = request;
    option.approaches = std::move(approaches);
    option.success = std::move(success);
    option.failure = std::move(failure);
    return option;
}

AdventureScene social(std::string title, std::string narration, std::string npc, std::vector<SceneOption> options,
                      int insight_dc = 12) {
    AdventureScene result;
    result.title = std::move(title);
    result.narration = std::move(narration);
    result.kind = SceneKind::Social;
    result.npc = std::move(npc);
    result.options = std::move(options);
    result.insight_dc = insight_dc;
    return result;
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
                challenge("The Approach", "The warren mouth is a dark gash in the hillside. You could creep up on it, "
                          "or simply walk in.",
                          {check("Creep up on the lookouts", "Stealth", 12,
                                 surprises("You're on the lookouts before they look up."),
                                 surprised("A stone skitters away under someone's boot. The wolf's head comes up."),
                                 true),
                           pick("Walk straight up the path", says("You make no secret of it."))}),
                scene("The Lookouts", "Two goblins keep a lazy watch at the warren mouth, a hungry wolf at their feet.",
                      {{"Goblin", 2}, {"Wolf", 1}}, {}, 300),
                scene("The Wolf Pens", "Deeper in, the goblins keep their wolves. The pack has caught your scent.",
                      {{"Wolf", 2}}, {{"Rations (1 day)", 2}}, 200, true),
                trap("The Snare Corridor", "A low passage leads deeper in. The floor is suspiciously clean.",
                     hazard("a tripwire hung with rusty blades", 12, "Sleight of Hand", 12, 11, 2, 4, "piercing")),
                social("The Captive", "A goblin sits tied up in a side room -- a deserter, by the look of it. "
                       "It eyes you, and the door to the boss's den.",
                       "snig",
                       {ask("Get it to say where the boss's guards sleep", SocialRequest::NoRisk,
                            fewer("Snig babbles where the guards sleep. One won't be waking up.", 1),
                            more("Snig shrieks for help before you can stop it.", 1)),
                        ask("Get it to lead you in by the back way", SocialRequest::MinorRisk,
                            joins(sways(surprises("Snig leads you through a crawlway into the den, grinning, and "
                                                  "draws a rusty knife. The boss never sees you coming."),
                                        1),
                                  "snig"),
                            says("Snig just glares at you.")),
                        pick("Leave it tied up", says("You leave it for the farmers to deal with."))},
                       10),
                scene("The Boss's Den", "A hobgoblin has taken charge of the warren. It isn't going to give it "
                      "up without a fight.",
                      {{"Hobgoblin", 1}, {"Goblin", 2}}, {{"Potion of Healing", 2}, {"Cloak of Protection", 1}}, 1500,
                      false, true),
            },
            {person("snig", "Snig", "goblin deserter", Attitude::Indifferent,
                    "A scrawny goblin tied up with its own belt, one ear bitten off.",
                    "to get out of the warren alive", "the hobgoblin boss finding out it talked", "Goblin")},
        },
        {
            "Crypt of the Restless",
            "The old barrow on the moor has been opened, and the dead walk out of it at night. Someone has to go "
            "down and put them back.",
            "The ghouls lie still and the barrow is quiet. Whatever stirred the dead has nothing left to command.",
            2,
            {
                social("The Gravedigger", "An old man leans on his spade at the edge of the moor, watching the "
                       "barrow. He doesn't look round.",
                       "tobin",
                       {ask("Ask what he's seen", SocialRequest::NoRisk,
                            surprises("\"They come out through the ossuary, and they don't look up.\" You'll be "
                                      "waiting above the door."),
                            says("He spits, and goes back to his digging.")),
                        ask("Ask him to guide you down", SocialRequest::SignificantRisk,
                            inspires(fewer("He knows every niche of the place, and leads you round the worst of "
                                           "the dead.",
                                           1)),
                            says("\"Not for any money.\"")),
                        pick("Promise to see his wife's grave left in peace",
                             sways(says("He nods slowly. \"Then I'll tell the temple you came.\""), 1, "priest"))},
                       12),
                scene("The Ossuary", "Bones rattle out of their niches and stand up to meet you.",
                      {{"Skeleton", 3}}, {{"Torch", 2}}, 400),
                trap("The False Floor", "The passage beyond the ossuary is paved with old, uneven slabs.",
                     hazard("a pit under a pivoting slab", 13, "Athletics", 12, 12, 2, 6, "bludgeoning")),
                scene("The Flooded Tomb", "Knee-deep water, and shapes shambling through it.",
                      {{"Zombie", 2}, {"Skeleton", 1}}, {}, 500, true),
                challenge("The Sealed Door", "A stone door blocks the deepest chamber, carved with warding runes.",
                          {check("Read the runes and speak the old words", "Religion", 13,
                                 surprises("The door swings open in silence. Nothing inside heard you coming."),
                                 hurts("The wards flare: radiant fire licks across your hands.", 1, 6, "radiant")),
                           check("Force it open", "Athletics", 15,
                                 says("Stone grinds, and the door gives."),
                                 surprised("It takes three tries, and the noise echoes through the barrow."))}),
                scene("The Ghouls' Feast", "The smell of the barrow's deepest room is the worst yet. Ghouls look up "
                      "from what they were eating.",
                      {{"Ghoul", 2}},
                      {{"Potion of Greater Healing", 1}, {"Healer's Kit", 1}, {"Mace +1", 1},
                       {"Spell Scroll (Cure Wounds)", 1}},
                      2000, false, true),
            },
            {person("tobin", "Old Tobin", "gravedigger", Attitude::Indifferent,
                    "A stooped old man with a spade, who has dug every grave on the moor for forty years.",
                    "his wife's grave in the barrow left in peace", "the dead walking into the village")},
        },
        {
            "The Hill Giant's Toll",
            "A hill giant has claimed the mountain road and taken gnolls and ogres into its service. Traders pay "
            "or turn back. You won't do either.",
            "The giant crashes down across its own road. The traders can pass again -- and they're grateful.",
            5,
            {
                scene("Gnoll Scouts", "Gnolls cackle in the rocks above the road.", {{"Gnoll", 3}}, {}, 600),
                social("The Stranded Caravan", "Past the gnolls' rocks, a dwarf sits on an overturned wagon, "
                       "binding a cut on her arm. Her guards look as battered as she does.",
                       "dagna",
                       {ask("Ask for supplies from her wagons", SocialRequest::NoRisk,
                            [] {
                                SceneOutcome given = says("\"Take them. You've earned them already.\"");
                                given.loot = {{"Potion of Healing", 2}};
                                return given;
                            }(),
                            says("She shakes her head.")),
                        ask("Ask her to come up the road with you", SocialRequest::SignificantRisk,
                            joins(says("Dagna shoulders her axe. \"Lead on. That gate's been in my way long "
                                       "enough.\""),
                                  "dagna"),
                            says("\"My wagons won't guard themselves.\"")),
                        pick("Promise to clear the road for her",
                             sways(says("\"Do that, and every merchant in the valley will hear of it.\""), 1,
                                   "merchant"))},
                       12),
                challenge("The Scree Slope", "The road climbs a slope of loose rock under the giant's lookout.",
                          {check("Climb carefully, together", "Athletics", 13,
                                 says("Everyone makes it up without starting a slide."),
                                 hurts("The scree gives way. Everyone is battered by a rockslide.", 2, 6, "bludgeoning",
                                       true),
                                 true)}),
                challenge("The Gatekeepers", "Two ogres lounge at the gate, gnawing bones. They don't look clever.",
                          {check("Convince them you're the giant's guests", "Deception", 15,
                                 [] {
                                     SceneOutcome passed = says("They wave you through, grinning. The gate fight is "
                                                                "avoided.");
                                     passed.skip_next_fight = true;
                                     return passed;
                                 }(),
                                 surprised("They don't believe a word, and they're on you before you're ready.")),
                           pick("Attack the gate", says("Steel out, you charge."))}),
                scene("The Ogre Gate", "Two ogres guard a crude gate of logs across the road.", {{"Ogre", 2}},
                      {{"Potion of Healing", 2}}, 1000, true),
                scene("The Owlbear Pit", "The giant keeps a pet. It's hungry.", {{"Owlbear", 1}}, {}, 800),
                scene("The Giant's Hall", "The hill giant is waiting in a hall of stolen timber, and it's angry.",
                      {{"Hill Giant", 1}},
                      {{"Potion of Greater Healing", 2}, {"Shield +1", 1}, {"Gauntlets of Ogre Power", 1}}, 5000,
                      false, true),
            },
            {person("dagna", "Dagna Ironfoot", "caravan master", Attitude::Friendly,
                    "A dwarf with a scorched beard and a cut arm, whose wagons the gnolls overturned.",
                    "her wagons over the pass before winter", "the giant's toll taking her last coin", "Veteran")},
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
                trap("The Collapsing Passage", "The barrow's spine is a low passage of stacked stone, and it creaks.",
                     hazard("a passage that comes down on whoever disturbs its keystone", 14, "Athletics", 14, 13, 3, 6,
                            "bludgeoning", true)),
                scene("The Hall of Kings", "Two of the old king's champions rise from their biers as you pass.",
                      {{"Wight", 2}}, {{"Potion of Greater Healing", 2}}, 1000, true),
                challenge("The King's Epitaph", "Above the tomb door, a long inscription names the king and his oath.",
                          {check("Read the history of the king", "History", 15,
                                 fewer("You speak his name and oath aloud. His honor guard kneels and does not rise.", 1,
                                       true),
                                 says("The words mean nothing to you.")),
                           check("Recognize the funeral rites", "Religion", 13,
                                 fewer("You perform the rites. His honor guard lies back down.", 1),
                                 says("You can't make out which rites these are."))}),
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
                social("The Miller's Wife", "The villagers clam up when you ask about the full moon, all but the "
                       "miller's wife, who can't meet your eyes.",
                       "wenna",
                       {ask("Win her trust: when does the cult gather?", SocialRequest::MinorRisk,
                            sways(surprises("She tells you: the old mill, at moonrise. You'll be there first."), 1),
                            surprised("She runs to the mill the moment you're gone. They're warned."),
                            {"Persuasion", "Deception"}),
                        ask("Lean on her until she talks", SocialRequest::MinorRisk,
                            sways(surprises("She breaks: the old mill, at moonrise. She won't forgive you."), -1),
                            more("She screams for her husband, and someone runs to warn the mill.", 1),
                            {"Intimidation"})},
                       14),
                scene("The Old Mill", "The mill wheel creaks. Two shapes on the millrace are not wolves, and not men.",
                      {{"Werewolf", 2}}, {{"Potion of Greater Healing", 2}}, 1200, true),
                trap("The Shrine Path", "The path to the shrine winds through dark woods, and something has dug here.",
                     hazard("a covered pit lined with stakes", 14, "Survival", 13, 13, 3, 6, "piercing")),
                scene("The Moon Shrine", "The cult's high priest raises a knife to the moon, and his pack answers.",
                      {{"Werewolf", 2}, {"Cult Fanatic", 1}},
                      {{"Warhammer +2", 1}, {"Bracers of Defense", 1}, {"Potion of Superior Healing", 2}}, 4000,
                      false, true),
            },
            {person("wenna", "Wenna Hask", "the miller's wife", Attitude::Indifferent,
                    "A tired woman with flour to her elbows, who keeps her back to you while she works.",
                    "her husband out of the cult before the moon takes him too", "the cult's high priest")},
        },
        {
            "The White Wyrm's Lair",
            "A young white dragon has claimed the high pass, and a renegade mage and a stone giant serve it. The "
            "caravans have stopped.",
            "The wyrm is dead and its hoard is yours. The pass is open again.",
            7,
            {
                challenge("The Blizzard", "Snow comes sideways off the peaks, and the trail disappears.",
                          {check("Keep the party on the trail", "Survival", 14,
                                 says("You find the way through the whiteout."),
                                 [] {
                                     SceneOutcome lost = says("You spend a freezing night lost on the mountain.");
                                     lost.exhaustion = 1;
                                     return lost;
                                 }(),
                                 true)}),
                scene("The Frozen Pass", "Crossbows click from the rocks: the wyrm's hired swords hold the pass.",
                      {{"Veteran", 2}}, {}, 1500),
                scene("The Quarry", "A stone giant hurls boulders down the quarry face.", {{"Stone Giant", 1}},
                      {{"Potion of Greater Healing", 2}}, 1500, true),
                challenge("The Ice Bridge", "A bridge of blue ice spans the chasm to the tower. It's thin.",
                          {check("Cross first and anchor a rope", "Acrobatics", 15,
                                 says("You make it across and tie off the rope; the rest follow safely."),
                                 hurts("The ice cracks. You hang on, but the cold bites deep.", 3, 6, "cold")),
                           check("Find another way down and up", "Athletics", 14,
                                 says("A long climb, but nobody falls."),
                                 hurts("A handhold breaks. You slide down the ice.", 2, 6, "bludgeoning"))}),
                scene("The Ice Tower", "The renegade mage waits in his tower, his guard beside him.",
                      {{"Mage", 1}, {"Veteran", 1}}, {{"Spell Scroll (Fireball)", 1}}, 2000),
                challenge("The Sleeping Wyrm", "Through the ice you can see the dragon, coiled and still. Asleep?",
                          {check("Creep in while it sleeps", "Stealth", 15,
                                 surprises("It's still asleep when you strike."),
                                 surprised("A blue eye opens. It was never asleep."),
                                 true),
                           pick("Wake it with a war cry", says("No tricks. You stride in."))}),
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
