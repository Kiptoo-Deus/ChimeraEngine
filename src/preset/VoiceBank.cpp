#include "preset/VoiceBank.h"

#include <algorithm>

namespace chimera::preset
{
namespace
{
bool sameSlot(const VoiceEntry& a, const VoiceEntry& b)
{
    return a.bank == b.bank && a.kind == b.kind && a.index == b.index;
}
}

int VoiceBank::capacityFor(VoiceBankId bank, VoiceKind kind)
{
    switch (bank)
    {
        case VoiceBankId::Preset:
            return kind == VoiceKind::Normal ? presetNormalSlots : presetDrumSlots;
        case VoiceBankId::GeneralMidi:
            return kind == VoiceKind::Normal ? gmNormalSlots : gmDrumSlots;
        case VoiceBankId::User1:
        case VoiceBankId::User2:
        case VoiceBankId::User3:
        case VoiceBankId::User4:
            return kind == VoiceKind::Normal ? userNormalSlotsPerBank : 0;
        case VoiceBankId::UserDrum:
            return kind == VoiceKind::DrumKit ? userDrumSlots : 0;
    }

    return 0;
}

bool VoiceBank::setVoice(VoiceEntry entry)
{
    const auto capacity = capacityFor(entry.bank, entry.kind);
    if (capacity <= 0 || entry.index < 0 || entry.index >= capacity)
        return false;
    if (entry.name.empty() || entry.category.empty())
        return false;

    if (auto existing = std::find_if(voices.begin(), voices.end(),
                                     [&entry](const auto& voice) { return sameSlot(voice, entry); });
        existing != voices.end())
    {
        *existing = std::move(entry);
        return true;
    }

    voices.push_back(std::move(entry));
    return true;
}

std::optional<VoiceEntry> VoiceBank::getVoice(VoiceBankId bank, VoiceKind kind, int index) const
{
    const auto found = std::find_if(voices.begin(), voices.end(),
                                    [bank, kind, index](const auto& voice)
                                    {
                                        return voice.bank == bank && voice.kind == kind && voice.index == index;
                                    });

    if (found == voices.end())
        return std::nullopt;

    return *found;
}

std::vector<VoiceEntry> VoiceBank::findByCategory(const std::string& category) const
{
    std::vector<VoiceEntry> matches;
    for (const auto& voice : voices)
        if (voice.category == category)
            matches.push_back(voice);

    return matches;
}
}
