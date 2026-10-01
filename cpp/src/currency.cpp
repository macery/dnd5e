#include "dnd/currency.hpp"

#include <algorithm>
#include <map>

namespace dnd {

namespace {

constexpr std::array<const char*, kCoinCount> kAbbreviations = {"cp", "sp", "ep", "gp", "pp"};
constexpr std::array<int, kCoinCount> kValues = {1, 10, 50, 100, 1000};

}  // namespace

const char* coin_abbreviation(Coin coin) {
    return kAbbreviations[static_cast<size_t>(coin)];
}

int coin_value_cp(Coin coin) {
    return kValues[static_cast<size_t>(coin)];
}

int total_cp(const Coins& coins) {
    int total = 0;
    for (size_t i = 0; i < coins.size(); ++i) total += coins[i] * kValues[i];
    return total;
}

Coins coins_from_cp(int cp) {
    Coins coins{};
    cp = std::max(0, cp);
    coins[static_cast<size_t>(Coin::Gold)] = cp / 100;
    coins[static_cast<size_t>(Coin::Silver)] = cp % 100 / 10;
    coins[static_cast<size_t>(Coin::Copper)] = cp % 10;
    return coins;
}

void add_coins(Coins& purse, const Coins& more) {
    for (size_t i = 0; i < purse.size(); ++i) purse[i] += more[i];
}

std::string format_coins(const Coins& coins) {
    std::string text;
    for (int i = kCoinCount - 1; i >= 0; --i) {
        int count = coins[static_cast<size_t>(i)];
        if (count == 0) continue;
        if (!text.empty()) text += ", ";
        text += std::to_string(count) + " " + kAbbreviations[static_cast<size_t>(i)];
    }
    return text.empty() ? "none" : text;
}

std::string format_cp(int cp) {
    return format_coins(coins_from_cp(cp));
}

std::string serialize_coins(const Coins& coins) {
    std::string text;
    for (size_t i = 0; i < coins.size(); ++i) {
        if (coins[i] == 0) continue;
        if (!text.empty()) text += ",";
        text += std::string(kAbbreviations[i]) + ":" + std::to_string(coins[i]);
    }
    return text;
}

Coins parse_coins(const std::string& text) {
    Coins coins{};
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find(',', start);
        if (end == std::string::npos) end = text.size();
        std::string part = text.substr(start, end - start);
        size_t colon = part.find(':');
        if (colon != std::string::npos) {
            std::string name = part.substr(0, colon);
            for (size_t i = 0; i < kAbbreviations.size(); ++i) {
                if (name != kAbbreviations[i]) continue;
                try {
                    coins[i] = std::stoi(part.substr(colon + 1));
                } catch (...) {
                    // malformed: ignored
                }
            }
        }
        start = end + 1;
    }
    return coins;
}

std::vector<int> split_cp(int cp, int people) {
    people = std::max(1, people);
    std::vector<int> shares(static_cast<size_t>(people), std::max(0, cp) / people);
    for (int i = 0; i < std::max(0, cp) % people; ++i) ++shares[static_cast<size_t>(i)];
    return shares;
}

int background_purse_gp(const std::string& background) {
    static const std::map<std::string, int> purses = {
        {"Acolyte", 15}, {"Charlatan", 15}, {"Criminal", 15}, {"Entertainer", 15}, {"Folk Hero", 10},
        {"Guild Artisan", 15}, {"Hermit", 5}, {"Noble", 25}, {"Outlander", 10}, {"Sage", 10},
        {"Sailor", 10}, {"Soldier", 10}, {"Urchin", 10},
    };
    auto found = purses.find(background);
    return found == purses.end() ? 0 : found->second;
}

}  // namespace dnd
