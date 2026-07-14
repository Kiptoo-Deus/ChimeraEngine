#include "engine/SampleLibrary.h"

#include <algorithm>

namespace chimera::engine
{
namespace
{
bool validWaveform(const WaveformInfo& waveform)
{
    return waveform.id > 0
        && !waveform.name.empty()
        && waveform.bytes > 0
        && waveform.rootKey >= 0 && waveform.rootKey <= 127
        && waveform.keyLow >= 0 && waveform.keyHigh <= 127 && waveform.keyLow <= waveform.keyHigh
        && waveform.velocityLow >= 1 && waveform.velocityHigh <= 127 && waveform.velocityLow <= waveform.velocityHigh;
}
}

WaveformBank::WaveformBank(std::uint64_t capacityBytes)
    : capacity(capacityBytes)
{
}

bool WaveformBank::addWaveform(WaveformInfo waveform)
{
    if (!validWaveform(waveform))
        return false;
    if (capacity > 0 && waveform.bytes > remainingBytes())
        return false;
    if (findById(waveform.id) != nullptr)
        return false;

    used += waveform.bytes;
    waveforms.push_back(std::move(waveform));
    return true;
}

std::uint64_t WaveformBank::remainingBytes() const
{
    if (capacity == 0 || used >= capacity)
        return capacity == 0 ? 0 : 0;

    return capacity - used;
}

const WaveformInfo* WaveformBank::findById(int id) const
{
    const auto found = std::find_if(waveforms.begin(), waveforms.end(),
                                    [id](const auto& waveform) { return waveform.id == id; });
    return found == waveforms.end() ? nullptr : &*found;
}

SampleLibrary::SampleLibrary()
    : factory(factoryRomBytes),
      userBoards { WaveformBank(userFlashBoardBytes), WaveformBank(userFlashBoardBytes) }
{
}

bool SampleLibrary::addFactoryWaveform(WaveformInfo waveform)
{
    if (factory.waveformCount() >= factoryWaveformSlots)
        return false;

    return factory.addWaveform(std::move(waveform));
}

bool SampleLibrary::addUserWaveform(int boardIndex, WaveformInfo waveform)
{
    if (boardIndex < 0 || boardIndex >= userFlashBoards)
        return false;

    return userBoards[static_cast<size_t>(boardIndex)].addWaveform(std::move(waveform));
}

int SampleLibrary::userWaveformCount(int boardIndex) const
{
    if (boardIndex < 0 || boardIndex >= userFlashBoards)
        return 0;

    return userBoards[static_cast<size_t>(boardIndex)].waveformCount();
}

std::uint64_t SampleLibrary::userUsedBytes(int boardIndex) const
{
    if (boardIndex < 0 || boardIndex >= userFlashBoards)
        return 0;

    return userBoards[static_cast<size_t>(boardIndex)].usedBytes();
}

std::uint64_t SampleLibrary::totalUserCapacityBytes() const
{
    return userFlashBoards * userFlashBoardBytes;
}

const WaveformInfo* SampleLibrary::findWaveform(int id) const
{
    if (const auto* waveform = factory.findById(id))
        return waveform;

    for (const auto& board : userBoards)
        if (const auto* waveform = board.findById(id))
            return waveform;

    return nullptr;
}
}
