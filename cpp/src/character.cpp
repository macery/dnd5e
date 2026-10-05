#include "dnd/character.hpp"

#include <algorithm>

namespace dnd {

std::vector<std::string> class_option(const Character& character, const std::string& key) {
    auto found = character.class_options.find(key);
    return found == character.class_options.end() ? std::vector<std::string>{} : found->second;
}

bool has_class_option(const Character& character, const std::string& key, const std::string& value) {
    auto values = class_option(character, key);
    return std::find(values.begin(), values.end(), value) != values.end();
}

std::string format_class_options(const std::map<std::string, std::vector<std::string>>& options) {
    std::string text;
    for (const auto& [key, values] : options) {
        if (values.empty()) continue;
        if (!text.empty()) text += ";";
        text += key + "=";
        for (size_t i = 0; i < values.size(); ++i) text += (i ? "|" : "") + values[i];
    }
    return text;
}

std::map<std::string, std::vector<std::string>> parse_class_options(const std::string& text) {
    std::map<std::string, std::vector<std::string>> options;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find(';', start);
        if (end == std::string::npos) end = text.size();
        std::string part = text.substr(start, end - start);
        size_t equals = part.find('=');
        if (equals != std::string::npos) {
            auto& values = options[part.substr(0, equals)];
            std::string rest = part.substr(equals + 1);
            size_t at = 0;
            while (at <= rest.size()) {
                size_t bar = rest.find('|', at);
                if (bar == std::string::npos) bar = rest.size();
                if (bar > at) values.push_back(rest.substr(at, bar - at));
                at = bar + 1;
            }
        }
        start = end + 1;
    }
    return options;
}

}  // namespace dnd
