#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace chimera::engine
{
struct ArpStep
{
    int offsetTicks = 0;
    int noteOffset = 0;
    int velocity = 100;
    int gateTicks = 120;
};

struct ArpPattern
{
    int id = 0;
    std::string name;
    std::string category;
    int lengthTicks = 480;
    std::vector<ArpStep> steps;
};

class ArpLibrary
{
public:
    static constexpr int presetSlots = 7881;
    static constexpr int userSlots = 256;
    static constexpr int simultaneousPerformanceArps = 4;

    bool setPreset(ArpPattern pattern);
    bool setUser(int index, ArpPattern pattern);
    std::optional<ArpPattern> getPreset(int id) const;
    std::optional<ArpPattern> getUser(int index) const;
    std::vector<ArpPattern> findByCategory(const std::string& category) const;
    int presetCount() const { return static_cast<int>(presetPatterns.size()); }
    int userCount() const;

private:
    static bool validPattern(const ArpPattern& pattern);

    std::vector<ArpPattern> presetPatterns;
    std::array<std::optional<ArpPattern>, userSlots> userPatterns {};
};
}
