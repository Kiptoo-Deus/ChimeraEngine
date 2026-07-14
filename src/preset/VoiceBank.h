#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace chimera::preset
{
enum class VoiceKind
{
    Normal,
    DrumKit
};

enum class VoiceBankId
{
    Preset,
    GeneralMidi,
    User1,
    User2,
    User3,
    User4,
    UserDrum
};

struct VoiceEntry
{
    VoiceBankId bank = VoiceBankId::Preset;
    VoiceKind kind = VoiceKind::Normal;
    int index = 0;
    std::string name;
    std::string category;
    std::string patchPath;
};

class VoiceBank
{
public:
    static constexpr int presetNormalSlots = 1024;
    static constexpr int presetDrumSlots = 64;
    static constexpr int gmNormalSlots = 128;
    static constexpr int gmDrumSlots = 1;
    static constexpr int userBankCount = 4;
    static constexpr int userNormalSlotsPerBank = 128;
    static constexpr int userDrumSlots = 32;
    static constexpr int expansionNormalSlots = 512;
    static constexpr int expansionDrumSlots = 32;
    static constexpr int totalNormalVoiceSlots = presetNormalSlots + gmNormalSlots
        + userBankCount * userNormalSlotsPerBank + expansionNormalSlots;
    static constexpr int totalDrumKitSlots = presetDrumSlots + gmDrumSlots + userDrumSlots + expansionDrumSlots;

    bool setVoice(VoiceEntry entry);
    std::optional<VoiceEntry> getVoice(VoiceBankId bank, VoiceKind kind, int index) const;
    std::vector<VoiceEntry> findByCategory(const std::string& category) const;
    int voiceCount() const { return static_cast<int>(voices.size()); }
    static int capacityFor(VoiceBankId bank, VoiceKind kind);

private:
    std::vector<VoiceEntry> voices;
};
}
