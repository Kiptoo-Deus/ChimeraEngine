#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace chimera::engine
{
struct NoteEvent
{
    int tick = 0;
    int durationTicks = 0;
    int note = 60;
    int velocity = 100;
    int channel = 1;
};

class SequenceTrack
{
public:
    bool addNote(NoteEvent event);
    const std::vector<NoteEvent>& getNotes() const { return notes; }
    int noteCount() const { return static_cast<int>(notes.size()); }

private:
    std::vector<NoteEvent> notes;
};

class Song
{
public:
    static constexpr int trackCount = 16;
    static constexpr int ppq = 480;
    static constexpr int maxNotes = 130000;

    bool addNote(int trackIndex, NoteEvent event);
    int noteCount() const;
    SequenceTrack& track(int index);
    const SequenceTrack& track(int index) const;
    void setTempo(double bpm);
    double getTempo() const { return tempoBpm; }

private:
    static const SequenceTrack emptyTrack;
    std::array<SequenceTrack, trackCount> tracks {};
    double tempoBpm = 120.0;
};

struct PatternSection
{
    std::array<SequenceTrack, 16> tracks {};
    int measureCount = 1;
};

class Pattern
{
public:
    static constexpr int sectionCount = 16;
    static constexpr int maxMeasures = 256;
    static constexpr int phraseSlots = 256;

    PatternSection& section(int index);
    const PatternSection& section(int index) const;
    void setSectionMeasures(int index, int measures);
    int sectionMeasures(int index) const;

private:
    static const PatternSection emptySection;
    std::array<PatternSection, sectionCount> sections {};
};

class Sequencer
{
public:
    static constexpr int songCount = 64;
    static constexpr int patternCount = 64;
    static constexpr int minTempo = 5;
    static constexpr int maxTempo = 300;

    Song& song(int index);
    const Song& song(int index) const;
    Pattern& pattern(int index);
    const Pattern& pattern(int index) const;
    int totalNoteCount() const;

private:
    static const Song emptySong;
    static const Pattern emptyPattern;
    std::array<Song, songCount> songs {};
    std::array<Pattern, patternCount> patterns {};
};
}
