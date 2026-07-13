#include "engine/Arpeggiator.h"
#include <algorithm>

namespace chimera::engine
{
void Arpeggiator::setMode(ArpMode newMode)
{
    mode = newMode;
    step = 0;
}

void Arpeggiator::setHeldNotes(std::vector<int> notes)
{
    heldNotes = std::move(notes);
    if (mode != ArpMode::AsPlayed)
        std::sort(heldNotes.begin(), heldNotes.end());
    step = 0;
}

std::vector<int> Arpeggiator::tick()
{
    if (heldNotes.empty())
        return {};

    if (mode == ArpMode::Chord)
        return heldNotes;

    auto index = 0;
    if (mode == ArpMode::Down)
        index = static_cast<int>(heldNotes.size()) - 1 - (step % static_cast<int>(heldNotes.size()));
    else if (mode == ArpMode::UpDown && heldNotes.size() > 1)
    {
        const auto span = static_cast<int>(heldNotes.size() * 2 - 2);
        const auto pos = step % span;
        index = pos < static_cast<int>(heldNotes.size()) ? pos : span - pos;
    }
    else
        index = step % static_cast<int>(heldNotes.size());

    ++step;
    return { heldNotes[static_cast<size_t>(index)] };
}
}
