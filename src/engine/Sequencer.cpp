#include "engine/Sequencer.h"

#include <algorithm>

namespace chimera::engine
{
namespace
{
bool validNoteEvent(const NoteEvent& event)
{
    return event.tick >= 0
        && event.durationTicks > 0
        && event.note >= 0 && event.note <= 127
        && event.velocity >= 1 && event.velocity <= 127
        && event.channel >= 1 && event.channel <= 16;
}
}

const SequenceTrack Song::emptyTrack {};
const PatternSection Pattern::emptySection {};
const Song Sequencer::emptySong {};
const Pattern Sequencer::emptyPattern {};

bool SequenceTrack::addNote(NoteEvent event)
{
    if (!validNoteEvent(event))
        return false;

    notes.push_back(event);
    return true;
}

bool Song::addNote(int trackIndex, NoteEvent event)
{
    if (trackIndex < 0 || trackIndex >= trackCount || noteCount() >= maxNotes)
        return false;

    return tracks[static_cast<size_t>(trackIndex)].addNote(event);
}

int Song::noteCount() const
{
    auto total = 0;
    for (const auto& sequenceTrack : tracks)
        total += sequenceTrack.noteCount();

    return total;
}

SequenceTrack& Song::track(int index)
{
    if (index < 0 || index >= trackCount)
        return const_cast<SequenceTrack&>(emptyTrack);

    return tracks[static_cast<size_t>(index)];
}

const SequenceTrack& Song::track(int index) const
{
    if (index < 0 || index >= trackCount)
        return emptyTrack;

    return tracks[static_cast<size_t>(index)];
}

void Song::setTempo(double bpm)
{
    tempoBpm = std::clamp(bpm, static_cast<double>(Sequencer::minTempo), static_cast<double>(Sequencer::maxTempo));
}

PatternSection& Pattern::section(int index)
{
    if (index < 0 || index >= sectionCount)
        return const_cast<PatternSection&>(emptySection);

    return sections[static_cast<size_t>(index)];
}

const PatternSection& Pattern::section(int index) const
{
    if (index < 0 || index >= sectionCount)
        return emptySection;

    return sections[static_cast<size_t>(index)];
}

void Pattern::setSectionMeasures(int index, int measures)
{
    if (index < 0 || index >= sectionCount)
        return;

    sections[static_cast<size_t>(index)].measureCount = std::clamp(measures, 1, maxMeasures);
}

int Pattern::sectionMeasures(int index) const
{
    return section(index).measureCount;
}

Song& Sequencer::song(int index)
{
    if (index < 0 || index >= songCount)
        return const_cast<Song&>(emptySong);

    return songs[static_cast<size_t>(index)];
}

const Song& Sequencer::song(int index) const
{
    if (index < 0 || index >= songCount)
        return emptySong;

    return songs[static_cast<size_t>(index)];
}

Pattern& Sequencer::pattern(int index)
{
    if (index < 0 || index >= patternCount)
        return const_cast<Pattern&>(emptyPattern);

    return patterns[static_cast<size_t>(index)];
}

const Pattern& Sequencer::pattern(int index) const
{
    if (index < 0 || index >= patternCount)
        return emptyPattern;

    return patterns[static_cast<size_t>(index)];
}

int Sequencer::totalNoteCount() const
{
    auto total = 0;
    for (const auto& sequenceSong : songs)
        total += sequenceSong.noteCount();

    return total;
}
}
