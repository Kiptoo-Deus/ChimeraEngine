#include "preset/Preset.h"

namespace chimera::preset
{
juce::var toJson(const PresetMetadata& metadata)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("name", metadata.name);
    object->setProperty("category", metadata.category);
    return object;
}
}
