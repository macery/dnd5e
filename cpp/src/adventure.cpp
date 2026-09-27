#include "dnd/adventure.hpp"

namespace dnd {

const std::vector<AdventureTemplate>& adventure_templates() {
    static const std::vector<AdventureTemplate> templates = {
        {
            "Ambush on the Trail",
            "Three goblins burst from the underbrush, blades drawn.",
            "The trail is clear. The goblins won't trouble travelers again.",
            {{"Goblin", 3}},
        },
        {
            "Wolves at the Campfire",
            "Hungry eyes circle your campsite in the dark.",
            "The wolves retreat into the night. You can finally get some rest.",
            {{"Wolf", 2}},
        },
        {
            "The Crypt Awakens",
            "Dust falls from the ceiling as the dead rise to bar your way.",
            "The crypt falls silent again. Whatever disturbed the dead here is gone.",
            {{"Skeleton", 2}, {"Zombie", 1}},
        },
        {
            "Orc Raiding Party",
            "An orc and its goblin scouts have you pinned down.",
            "The raiding party is broken. This stretch of road is safe again.",
            {{"Orc", 1}, {"Goblin", 2}},
        },
    };
    return templates;
}

}  // namespace dnd
