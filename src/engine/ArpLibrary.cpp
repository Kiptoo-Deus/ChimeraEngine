#include "engine/ArpLibrary.h"

#include <algorithm>

namespace chimera::engine
{
bool ArpLibrary::validPattern(const ArpPattern& pattern)
{
    if (pattern.id <= 0 || pattern.name.empty() || pattern.category.empty())
        return false;
    if (pattern.lengthTicks <= 0 || pattern.steps.empty())
        return false;

    return std::all_of(pattern.steps.begin(), pattern.steps.end(),
                       [&pattern](const auto& step)
                       {
                           return step.offsetTicks >= 0
                               && step.offsetTicks < pattern.lengthTicks
                               && step.velocity >= 1 && step.velocity <= 127
                               && step.gateTicks > 0;
                       });
}

bool ArpLibrary::setPreset(ArpPattern pattern)
{
    if (!validPattern(pattern) || pattern.id > presetSlots)
        return false;

    if (auto existing = std::find_if(presetPatterns.begin(), presetPatterns.end(),
                                     [&pattern](const auto& entry) { return entry.id == pattern.id; });
        existing != presetPatterns.end())
    {
        *existing = std::move(pattern);
        return true;
    }

    presetPatterns.push_back(std::move(pattern));
    return true;
}

bool ArpLibrary::setUser(int index, ArpPattern pattern)
{
    if (index < 0 || index >= userSlots || !validPattern(pattern))
        return false;

    userPatterns[static_cast<size_t>(index)] = std::move(pattern);
    return true;
}

std::optional<ArpPattern> ArpLibrary::getPreset(int id) const
{
    const auto found = std::find_if(presetPatterns.begin(), presetPatterns.end(),
                                    [id](const auto& pattern) { return pattern.id == id; });
    if (found == presetPatterns.end())
        return std::nullopt;

    return *found;
}

std::optional<ArpPattern> ArpLibrary::getUser(int index) const
{
    if (index < 0 || index >= userSlots)
        return std::nullopt;

    return userPatterns[static_cast<size_t>(index)];
}

std::vector<ArpPattern> ArpLibrary::findByCategory(const std::string& category) const
{
    std::vector<ArpPattern> matches;
    for (const auto& pattern : presetPatterns)
        if (pattern.category == category)
            matches.push_back(pattern);
    for (const auto& pattern : userPatterns)
        if (pattern.has_value() && pattern->category == category)
            matches.push_back(*pattern);

    return matches;
}

int ArpLibrary::userCount() const
{
    return static_cast<int>(std::count_if(userPatterns.begin(), userPatterns.end(),
                                          [](const auto& pattern) { return pattern.has_value(); }));
}
}
