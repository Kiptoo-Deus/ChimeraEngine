#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace chimera::engine
{
struct WaveformInfo
{
    int id = 0;
    std::string name;
    std::string category;
    std::uint64_t bytes = 0;
    int rootKey = 60;
    int keyLow = 0;
    int keyHigh = 127;
    int velocityLow = 1;
    int velocityHigh = 127;
};

class WaveformBank
{
public:
    explicit WaveformBank(std::uint64_t capacityBytes = 0);

    bool addWaveform(WaveformInfo waveform);
    int waveformCount() const { return static_cast<int>(waveforms.size()); }
    std::uint64_t usedBytes() const { return used; }
    std::uint64_t capacityBytes() const { return capacity; }
    std::uint64_t remainingBytes() const;
    const WaveformInfo* findById(int id) const;
    const std::vector<WaveformInfo>& all() const { return waveforms; }

private:
    std::uint64_t capacity = 0;
    std::uint64_t used = 0;
    std::vector<WaveformInfo> waveforms;
};

class SampleLibrary
{
public:
    static constexpr int factoryWaveformSlots = 3977;
    static constexpr std::uint64_t factoryRomBytes = 741ull * 1024ull * 1024ull;
    static constexpr int userFlashBoards = 2;
    static constexpr std::uint64_t userFlashBoardBytes = 1024ull * 1024ull * 1024ull;

    SampleLibrary();

    bool addFactoryWaveform(WaveformInfo waveform);
    bool addUserWaveform(int boardIndex, WaveformInfo waveform);
    int factoryWaveformCount() const { return factory.waveformCount(); }
    int userWaveformCount(int boardIndex) const;
    std::uint64_t factoryUsedBytes() const { return factory.usedBytes(); }
    std::uint64_t userUsedBytes(int boardIndex) const;
    std::uint64_t totalUserCapacityBytes() const;
    const WaveformInfo* findWaveform(int id) const;

private:
    WaveformBank factory;
    std::array<WaveformBank, userFlashBoards> userBoards;
};
}
