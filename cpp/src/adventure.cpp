#include "dnd/adventure.hpp"

namespace dnd {

const std::vector<AdventureTemplate>& adventure_templates() {
    static const std::vector<AdventureTemplate> templates = {
        {
            "Ambush on the Trail",
            "Three goblins burst from the underbrush, blades drawn.",
            "The trail is clear. The goblins won't trouble travelers again.",
            {{"Goblin", 3}},
            {{"Potion of Healing", 1}, {"Rations (1 day)", 2}},
        },
        {
            "Wolves at the Campfire",
            "Hungry eyes circle your campsite in the dark.",
            "The wolves retreat into the night. You can finally get some rest.",
            {{"Wolf", 2}},
            {{"Potion of Healing", 1}},
        },
        {
            "The Crypt Awakens",
            "Dust falls from the ceiling as the dead rise to bar your way.",
            "The crypt falls silent again. Whatever disturbed the dead here is gone.",
            {{"Skeleton", 2}, {"Zombie", 1}},
            {{"Potion of Healing", 1}, {"Torch", 3}},
        },
        {
            "Orc Raiding Party",
            "An orc and its goblin scouts have you pinned down.",
            "The raiding party is broken. This stretch of road is safe again.",
            {{"Orc", 1}, {"Goblin", 2}},
            {{"Potion of Healing", 2}, {"Hempen Rope (50 feet)", 1}},
        },
        // Tougher fights for levelled parties (SRD monsters, CR 1/2-5);
        // roughly levels 3 to 7 for a party of three or four.
        {
            "The Ogre's Larder",
            "Bones crunch somewhere ahead. An ogre and its goblin hangers-on are guarding a cave full of stolen "
            "food.",
            "The ogre topples like a felled tree. The stolen stores can go back to the village.",
            {{"Ogre", 1}, {"Goblin", 2}},
            {{"Potion of Healing", 2}, {"Rations (1 day)", 5}},
        },
        {
            "The Ghoul Crypt",
            "The stench hits first: ghouls are feeding in the old family crypt, and their claws can freeze you "
            "where you stand.",
            "The last ghoul falls still. Sunlight finally reaches the crypt floor.",
            {{"Ghoul", 2}, {"Skeleton", 2}},
            {{"Potion of Healing", 2}, {"Healer's Kit", 1}},
        },
        {
            "The Bandit Captain's Camp",
            "A bandit captain and a pair of hobgoblin mercenaries hold the ford, taking a toll from everyone who "
            "crosses.",
            "The captain's camp is scattered. Travelers can cross the ford for free again.",
            {{"Bandit Captain", 1}, {"Hobgoblin", 2}},
            {{"Potion of Healing", 2}, {"Longsword", 1}, {"Shield", 1}},
        },
        {
            "Hounds of the Burning Pass",
            "Smoke curls between the rocks. Hell hounds are hunting the pass, and fire won't touch them.",
            "The pass cools. Whatever summoned the hounds has lost its pack.",
            {{"Hell Hound", 2}},
            {{"Potion of Healing", 3}},
        },
        {
            "The Troll Bridge",
            "A troll squats under the old stone bridge. Cut it and it heals; only fire or acid stops it.",
            "The troll finally stays down. The bridge is open again.",
            {{"Troll", 1}},
            {{"Potion of Healing", 3}, {"Chain Mail", 1}},
        },
    };
    return templates;
}

}  // namespace dnd
