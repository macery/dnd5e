#pragma once

#include <random>

namespace dnd {

class IRandomSource {
public:
    virtual ~IRandomSource() = default;

    // Returns a value in [1, sides], inclusive.
    virtual int roll_die(int sides) = 0;
};

class SystemRandomSource final : public IRandomSource {
public:
    SystemRandomSource() : engine_(std::random_device{}()) {}

    int roll_die(int sides) override {
        std::uniform_int_distribution<int> dist(1, sides);
        return dist(engine_);
    }

private:
    std::mt19937 engine_;
};

}  // namespace dnd
