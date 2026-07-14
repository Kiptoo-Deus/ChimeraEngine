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

std::vector<juce::String> readStringArray(const juce::var& value)
{
    std::vector<juce::String> strings;
    if (!value.isArray())
        return strings;

    for (int i = 0; i < value.size(); ++i)
        if (value[i].isString())
            strings.push_back(value[i].toString());

    return strings;
}

EnvelopeDefinition readEnvelope(const juce::var& value, EnvelopeDefinition defaults)
{
    if (!value.isObject())
        return defaults;

    defaults.attack = static_cast<float>(value.getProperty("attack", defaults.attack));
    defaults.decay1 = static_cast<float>(value.getProperty("decay1", defaults.decay1));
    defaults.decay2 = static_cast<float>(value.getProperty("decay2", defaults.decay2));
    defaults.sustain = static_cast<float>(value.getProperty("sustain", defaults.sustain));
    defaults.release = static_cast<float>(value.getProperty("release", defaults.release));
    defaults.depth = static_cast<float>(value.getProperty("depth", defaults.depth));
    return defaults;
}

std::vector<ModSlotDefinition> readModSlots(const juce::var& value)
{
    std::vector<ModSlotDefinition> slots;
    if (!value.isArray())
        return slots;

    for (int i = 0; i < value.size(); ++i)
    {
        const auto slotVar = value[i];
        if (!slotVar.isObject())
            continue;

        ModSlotDefinition slot;
        slot.source = slotVar.getProperty("source", "velocity").toString();
        slot.destination = slotVar.getProperty("destination", "amp").toString();
        slot.depth = static_cast<float>(slotVar.getProperty("depth", 0.0));
        slot.enabled = static_cast<bool>(slotVar.getProperty("enabled", true));
        slots.push_back(std::move(slot));
    }

    return slots;
}

bool validEnvelope(const EnvelopeDefinition& envelope, float minDepth, float maxDepth)
{
    return envelope.attack >= 0.0f && envelope.attack <= 10.0f
        && envelope.decay1 >= 0.0f && envelope.decay1 <= 10.0f
        && envelope.decay2 >= 0.0f && envelope.decay2 <= 10.0f
        && envelope.sustain >= 0.0f && envelope.sustain <= 1.0f
        && envelope.release >= 0.0f && envelope.release <= 20.0f
        && envelope.depth >= minDepth && envelope.depth <= maxDepth;
}

bool validModSource(const juce::String& source)
{
    const auto id = source.trim().toLowerCase();
    return id == "velocity" || id == "modwheel" || id == "aftertouch" || id == "lfo1" || id == "lfo2"
        || id == "pitcheg" || id == "filtereg" || id == "ampeg";
}

bool validModDestination(const juce::String& destination)
{
    const auto id = destination.trim().toLowerCase();
    return id == "pitch" || id == "cutoff" || id == "amp" || id == "pan";
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
    next.voiceMode = parsed.getProperty("voiceMode", "poly").toString().trim().toLowerCase();
    next.portamentoTime = static_cast<float>(parsed.getProperty("portamentoTime", 0.0));
    next.pitchBendRange = static_cast<int>(parsed.getProperty("pitchBendRange", 2));

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
        element.alternateSamples = readStringArray(elementVar.getProperty("alternates", {}));
        element.releaseSamples = readStringArray(elementVar.getProperty("releaseSamples", {}));
        element.alternateMode = elementVar.getProperty("alternateMode", "off").toString().trim().toLowerCase();
        element.rootKey = static_cast<int>(elementVar.getProperty("rootKey", 60));
        readRange(elementVar.getProperty("keyRange", {}), element.keyLow, element.keyHigh, 0, 127);
        readRange(elementVar.getProperty("velocityRange", {}), element.velocityLow, element.velocityHigh, 1, 127);
        element.level = static_cast<float>(elementVar.getProperty("level", 1.0));
        element.pan = static_cast<float>(elementVar.getProperty("pan", 0.0));
        element.tuningCents = static_cast<float>(elementVar.getProperty("tuningCents", 0.0));
        element.filterType = elementVar.getProperty("filterType", "lowPass12").toString();
        element.ampAttack = static_cast<float>(elementVar.getProperty("ampAttack", 0.0));
        element.ampDecay1 = static_cast<float>(elementVar.getProperty("ampDecay1", 0.05));
        element.ampDecay2 = static_cast<float>(elementVar.getProperty("ampDecay2", 0.05));
        element.ampSustain = static_cast<float>(elementVar.getProperty("ampSustain", 1.0));
        element.ampRelease = static_cast<float>(elementVar.getProperty("ampRelease", 0.0));
        element.pitchEnvelope = readEnvelope(elementVar.getProperty("pitchEnvelope", {}), element.pitchEnvelope);
        element.filterEnvelope = readEnvelope(elementVar.getProperty("filterEnvelope", {}), element.filterEnvelope);
        element.lfo1RateHz = static_cast<float>(elementVar.getProperty("lfo1RateHz", 0.0));
        element.lfo1CutoffDepth = static_cast<float>(elementVar.getProperty("lfo1CutoffDepth", 0.0));
        element.lfo2RateHz = static_cast<float>(elementVar.getProperty("lfo2RateHz", 0.0));
        element.lfo2AmpDepth = static_cast<float>(elementVar.getProperty("lfo2AmpDepth", 0.0));
        element.lfo2PanDepth = static_cast<float>(elementVar.getProperty("lfo2PanDepth", 0.0));
        element.modSlots = readModSlots(elementVar.getProperty("modSlots", {}));

        if (element.samplePath.isEmpty())
            return juce::Result::fail("Patch element requires sample path: " + file.getFullPathName());
        if ((next.voiceMode != "poly" && next.voiceMode != "mono" && next.voiceMode != "legato")
            || next.portamentoTime < 0.0f || next.portamentoTime > 10.0f
            || next.pitchBendRange < 0 || next.pitchBendRange > 48
            || (element.alternateMode != "off" && element.alternateMode != "roundrobin" && element.alternateMode != "random"))
            return juce::Result::fail("Patch voice mode is invalid: " + file.getFullPathName());
        if (element.rootKey < 0 || element.rootKey > 127
            || element.keyLow < 0 || element.keyHigh > 127
            || element.velocityLow < 1 || element.velocityHigh > 127
            || element.level < 0.0f || element.level > 2.0f
            || element.pan < -1.0f || element.pan > 1.0f
            || element.tuningCents < -1200.0f || element.tuningCents > 1200.0f
            || element.ampAttack < 0.0f || element.ampAttack > 10.0f
            || element.ampDecay1 < 0.0f || element.ampDecay1 > 10.0f
            || element.ampDecay2 < 0.0f || element.ampDecay2 > 10.0f
            || element.ampSustain < 0.0f || element.ampSustain > 1.0f
            || element.ampRelease < 0.0f || element.ampRelease > 20.0f
            || !validEnvelope(element.pitchEnvelope, -4800.0f, 4800.0f)
            || !validEnvelope(element.filterEnvelope, -1.0f, 1.0f)
            || element.lfo1RateHz < 0.0f || element.lfo1RateHz > 40.0f
            || element.lfo1CutoffDepth < 0.0f || element.lfo1CutoffDepth > 1.0f
            || element.lfo2RateHz < 0.0f || element.lfo2RateHz > 40.0f
            || element.lfo2AmpDepth < 0.0f || element.lfo2AmpDepth > 1.0f
            || element.lfo2PanDepth < 0.0f || element.lfo2PanDepth > 1.0f)
            return juce::Result::fail("Patch element MIDI ranges are invalid: " + file.getFullPathName());

        if (element.modSlots.size() > 8)
            return juce::Result::fail("Patch element has too many modulation slots: " + file.getFullPathName());
        for (const auto& slot : element.modSlots)
            if (!validModSource(slot.source) || !validModDestination(slot.destination) || slot.depth < -4.0f || slot.depth > 4.0f)
                return juce::Result::fail("Patch element modulation slot is invalid: " + file.getFullPathName());

        next.elements.push_back(element);
    }

    patch = std::move(next);
    return juce::Result::ok();
}

juce::Result validatePatchSamples(const juce::File& projectRoot, const Patch& patch)
{
    for (const auto& element : patch.elements)
    {
        std::vector<juce::String> samples { element.samplePath };
        samples.insert(samples.end(), element.alternateSamples.begin(), element.alternateSamples.end());
        samples.insert(samples.end(), element.releaseSamples.begin(), element.releaseSamples.end());
        for (const auto& samplePath : samples)
        {
            const auto sample = projectRoot.getChildFile(samplePath);
            if (!sample.existsAsFile())
                return juce::Result::fail("Missing sample: " + samplePath);
        }
    }

    return juce::Result::ok();
}
}
