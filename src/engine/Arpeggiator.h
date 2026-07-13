#pragma once

#include <vector>

namespace chimera::engine
{
enum class ArpMode
{
    Up,
    Down,
    UpDown,
    Random,
    AsPlayed,
    Chord
};

class Arpeggiator
{
public:
    void setMode(ArpMode newMode);
    void setHeldNotes(std::vector<int> notes);
    std::vector<int> tick();

private:
    std::vector<int> heldNotes;
    int step = 0;
    ArpMode mode = ArpMode::Up;
};
}
