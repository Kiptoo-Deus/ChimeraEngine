#include "preset/Preset.h"

namespace chimera::preset
{
namespace
{
bool readRange(const juce::var& value, int& low, int& high, int defaultLow, int defaultHigh)
{
    if (!value.isArray() || value.size() != 2)
    {
        low = defaultLow;
        high = defaultHigh;
        return false;
    }

    low = static_cast<int>(value[0]);
    high = static_cast<int>(value[1]);
    return low <= high;
}
}

juce::var toJson(const PresetMetadata& metadata)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("name", metadata.name);
    object->setProperty("category", metadata.category);
    return object;
}

juce::Result loadPatch(const juce::File& file, Patch& patch)
{
    if (!file.existsAsFile())
        return juce::Result::fail("Patch file does not exist: " + file.getFullPathName());

    const auto parsed = juce::JSON::parse(file);
    if (!parsed.isObject())
        return juce::Result::fail("Patch is not a JSON object: " + file.getFullPathName());

    if (parsed.getProperty("format", {}).toString() != "chpatch")
        return juce::Result::fail("Unsupported patch format: " + file.getFullPathName());

    Patch next;
    next.version = static_cast<int>(parsed.getProperty("version", 1));
    next.metadata.name = parsed.getProperty("name", {}).toString();
    next.metadata.category = parsed.getProperty("category", {}).toString();

    if (next.metadata.name.isEmpty() || next.metadata.category.isEmpty())
        return juce::Result::fail("Patch requires name and category: " + file.getFullPathName());

    const auto elements = parsed.getProperty("elements", {});
    if (!elements.isArray() || elements.size() <= 0)
        return juce::Result::fail("Patch requires at least one element: " + file.getFullPathName());

    for (int i = 0; i < elements.size(); ++i)
    {
        const auto elementVar = elements[i];
        if (!elementVar.isObject())
            return juce::Result::fail("Patch element is not an object: " + file.getFullPathName());

        ElementDefinition element;
        element.samplePath = elementVar.getProperty("sample", {}).toString();
        element.rootKey = static_cast<int>(elementVar.getProperty("rootKey", 60));
        readRange(elementVar.getProperty("keyRange", {}), element.keyLow, element.keyHigh, 0, 127);
        readRange(elementVar.getProperty("velocityRange", {}), element.velocityLow, element.velocityHigh, 1, 127);
        element.level = static_cast<float>(elementVar.getProperty("level", 1.0));
        element.pan = static_cast<float>(elementVar.getProperty("pan", 0.0));
        element.ampAttack = static_cast<float>(elementVar.getProperty("ampAttack", 0.0));
        element.ampSustain = static_cast<float>(elementVar.getProperty("ampSustain", 1.0));
        element.ampRelease = static_cast<float>(elementVar.getProperty("ampRelease", 0.0));

        if (element.samplePath.isEmpty())
            return juce::Result::fail("Patch element requires sample path: " + file.getFullPathName());
        if (element.rootKey < 0 || element.rootKey > 127
            || element.keyLow < 0 || element.keyHigh > 127
            || element.velocityLow < 1 || element.velocityHigh > 127
            || element.level < 0.0f || element.level > 2.0f
            || element.pan < -1.0f || element.pan > 1.0f
            || element.ampAttack < 0.0f || element.ampAttack > 10.0f
            || element.ampSustain < 0.0f || element.ampSustain > 1.0f
            || element.ampRelease < 0.0f || element.ampRelease > 20.0f)
            return juce::Result::fail("Patch element MIDI ranges are invalid: " + file.getFullPathName());

        next.elements.push_back(element);
    }

    patch = std::move(next);
    return juce::Result::ok();
}

juce::Result validatePatchSamples(const juce::File& projectRoot, const Patch& patch)
{
    for (const auto& element : patch.elements)
    {
        const auto sample = projectRoot.getChildFile(element.samplePath);
        if (!sample.existsAsFile())
            return juce::Result::fail("Missing sample: " + element.samplePath);
    }

    return juce::Result::ok();
}
}
