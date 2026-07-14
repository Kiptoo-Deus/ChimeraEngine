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
    std::sort(notes.begin(), notes.end(), [](const auto& a, const auto& b) { return a.tick < b.tick; });
    return true;
}

void SequenceTrack::clear()
{
    notes.clear();
}

bool Song::addNote(int trackIndex, NoteEvent event)
{
    if (trackIndex < 0 || trackIndex >= trackCount || noteCount() >= maxNotes)
        return false;

    return tracks[static_cast<size_t>(trackIndex)].addNote(event);
}

bool Song::recordNote(int trackIndex, int tick, int durationTicks, int note, int velocity, int channel)
{
    return addNote(trackIndex, { tick, durationTicks, note, velocity, channel });
}

void Song::clearTrack(int trackIndex)
{
    if (trackIndex >= 0 && trackIndex < trackCount)
        tracks[static_cast<size_t>(trackIndex)].clear();
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

bool Song::addTempoEvent(TempoEvent event)
{
    if (event.tick < 0 || event.bpm < Sequencer::minTempo || event.bpm > Sequencer::maxTempo)
        return false;

    tempoEvents.push_back(event);
    std::sort(tempoEvents.begin(), tempoEvents.end(), [](const auto& a, const auto& b) { return a.tick < b.tick; });
    return true;
}

bool Song::addSceneEvent(SceneEvent event)
{
    if (event.tick < 0 || event.scene < 0 || event.scene > 127)
        return false;

    sceneEvents.push_back(event);
    std::sort(sceneEvents.begin(), sceneEvents.end(), [](const auto& a, const auto& b) { return a.tick < b.tick; });
    return true;
}

std::vector<PlaybackEvent> Song::collectPlaybackEvents(int startTick, int endTick) const
{
    std::vector<PlaybackEvent> events;
    if (startTick < 0 || endTick <= startTick)
        return events;

    for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex)
    {
        const auto& sequenceTrack = tracks[static_cast<size_t>(trackIndex)];
        for (const auto& note : sequenceTrack.getNotes())
        {
            if (note.tick >= startTick && note.tick < endTick)
                events.push_back({ note.tick, trackIndex, note, true });

            const auto offTick = note.tick + note.durationTicks;
            if (offTick >= startTick && offTick < endTick)
                events.push_back({ offTick, trackIndex, note, false });
        }
    }

    std::sort(events.begin(), events.end(),
              [](const auto& a, const auto& b)
              {
                  if (a.tick != b.tick)
                      return a.tick < b.tick;
                  return a.noteOn && !b.noteOn;
              });
    return events;
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

bool Pattern::addPhraseNote(int sectionIndex, int trackIndex, NoteEvent event)
{
    if (sectionIndex < 0 || sectionIndex >= sectionCount || trackIndex < 0 || trackIndex >= 16)
        return false;

    return sections[static_cast<size_t>(sectionIndex)].tracks[static_cast<size_t>(trackIndex)].addNote(event);
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
