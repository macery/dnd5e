#include "dnd/npc.hpp"

#include <algorithm>

#include "dnd/repository.hpp"

namespace dnd {

const char* attitude_name(Attitude attitude) {
    switch (attitude) {
        case Attitude::Hostile: return "Hostile";
        case Attitude::Indifferent: return "Indifferent";
        case Attitude::Friendly: return "Friendly";
    }
    return "Indifferent";
}

Attitude attitude_from_name(const std::string& name) {
    if (name == "Hostile") return Attitude::Hostile;
    if (name == "Friendly") return Attitude::Friendly;
    return Attitude::Indifferent;
}

Attitude shift_attitude(Attitude attitude, int steps) {
    int value = std::clamp(static_cast<int>(attitude) + steps, static_cast<int>(Attitude::Hostile),
                           static_cast<int>(Attitude::Friendly));
    return static_cast<Attitude>(value);
}

const char* social_request_text(SocialRequest request) {
    switch (request) {
        case SocialRequest::NoHarm: return "stop opposing you";
        case SocialRequest::NoRisk: return "help at no risk";
        case SocialRequest::MinorRisk: return "help at a minor risk";
        case SocialRequest::SignificantRisk: return "help at a significant risk";
    }
    return "";
}

std::optional<int> social_dc(Attitude attitude, SocialRequest request) {
    switch (attitude) {
        case Attitude::Friendly:
            switch (request) {
                case SocialRequest::NoHarm:
                case SocialRequest::NoRisk: return 0;
                case SocialRequest::MinorRisk: return 10;
                case SocialRequest::SignificantRisk: return 20;
            }
            break;
        case Attitude::Indifferent:
            switch (request) {
                case SocialRequest::NoHarm: return 0;
                case SocialRequest::NoRisk: return 10;
                case SocialRequest::MinorRisk: return 20;
                case SocialRequest::SignificantRisk: return std::nullopt;
            }
            break;
        case Attitude::Hostile:
            switch (request) {
                case SocialRequest::NoHarm: return 10;
                case SocialRequest::NoRisk: return 20;
                case SocialRequest::MinorRisk:
                case SocialRequest::SignificantRisk: return std::nullopt;
            }
            break;
    }
    return std::nullopt;
}

std::optional<int> conversation_dc(const Npc& npc, SocialRequest request) {
    auto dc = social_dc(npc.attitude, request);
    if (!dc) return dc;
    return std::max(0, *dc - (npc.known && *dc > 0 ? kKnownMotivesBonus : 0));
}

Npc meet_npc(Repository& repo, int campaign_id, const Npc& first_meeting) {
    for (const auto& npc : repo.get_npcs(campaign_id)) {
        if (!first_meeting.key.empty() && npc.key == first_meeting.key) return npc;
    }
    Npc npc = first_meeting;
    npc.campaign_id = campaign_id;
    npc.npc_id = repo.add_npc(npc);
    return npc;
}

Npc shop_merchant(Repository& repo, int campaign_id) {
    Npc merchant;
    merchant.key = "merchant";
    merchant.name = "Hilda Brannock";
    merchant.role = "merchant";
    merchant.description = "A broad, sharp-eyed trader who keeps the shop's ledger in her head and a cudgel under "
                           "the counter.";
    merchant.wants = "steady customers, and safe roads for her wagons";
    merchant.fears = "bandits on the trade road, and being made a fool of";
    return meet_npc(repo, campaign_id, merchant);
}

Npc temple_priest(Repository& repo, int campaign_id) {
    Npc priest;
    priest.key = "priest";
    priest.name = "Brother Aldous";
    priest.role = "priest";
    priest.description = "An old, soft-spoken priest with ink-stained fingers, who has buried more adventurers than "
                         "he has raised.";
    priest.wants = "the restless dead put back in their graves";
    priest.fears = "that the temple's diamonds run out before the dying do";
    return meet_npc(repo, campaign_id, priest);
}

}  // namespace dnd
