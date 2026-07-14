#include "engine/SequencerIO.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace chimera::engine
{
juce::Result exportSongToMidiFile(const Song& song, const juce::File& file)
{
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(Song::ppq);

    for (int trackIndex = 0; trackIndex < Song::trackCount; ++trackIndex)
    {
        juce::MidiMessageSequence sequence;
        if (trackIndex == 0)
            sequence.addEvent(juce::MidiMessage::tempoMetaEvent(static_cast<int>(std::round(60000000.0 / song.getTempo()))), 0);

        for (const auto& note : song.track(trackIndex).getNotes())
        {
            sequence.addEvent(juce::MidiMessage::noteOn(note.channel, note.note, juce::uint8(note.velocity)), note.tick);
            sequence.addEvent(juce::MidiMessage::noteOff(note.channel, note.note), note.tick + note.durationTicks);
        }
        sequence.updateMatchedPairs();
        midiFile.addTrack(sequence);
    }

    juce::FileOutputStream stream(file);
    if (!stream.openedOk())
        return juce::Result::fail("Could not open MIDI file for writing: " + file.getFullPathName());

    if (!midiFile.writeTo(stream))
        return juce::Result::fail("Could not write MIDI file: " + file.getFullPathName());

    return juce::Result::ok();
}

juce::Result importSongFromMidiFile(const juce::File& file, Song& song)
{
    if (!file.existsAsFile())
        return juce::Result::fail("MIDI file does not exist: " + file.getFullPathName());

    juce::FileInputStream stream(file);
    if (!stream.openedOk())
        return juce::Result::fail("Could not open MIDI file: " + file.getFullPathName());

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream))
        return juce::Result::fail("Could not read MIDI file: " + file.getFullPathName());

    for (int trackIndex = 0; trackIndex < Song::trackCount; ++trackIndex)
        song.clearTrack(trackIndex);

    for (int trackIndex = 0; trackIndex < std::min(Song::trackCount, midiFile.getNumTracks()); ++trackIndex)
    {
        const auto* sequence = midiFile.getTrack(trackIndex);
        if (sequence == nullptr)
            continue;

        std::map<std::pair<int, int>, std::vector<std::pair<int, int>>> activeNotes;
        for (int eventIndex = 0; eventIndex < sequence->getNumEvents(); ++eventIndex)
        {
            const auto* event = sequence->getEventPointer(eventIndex);
            if (event == nullptr)
                continue;

            const auto message = event->message;
            const auto tick = static_cast<int>(std::round(message.getTimeStamp()));
            if (message.isTempoMetaEvent())
                song.setTempo(60.0 / message.getTempoSecondsPerQuarterNote());
            else if (message.isNoteOn())
                activeNotes[{ message.getChannel(), message.getNoteNumber() }].push_back({ tick, static_cast<int>(message.getVelocity()) });
            else if (message.isNoteOff())
            {
                auto& starts = activeNotes[{ message.getChannel(), message.getNoteNumber() }];
                if (starts.empty())
                    continue;

                const auto [startTick, velocity] = starts.back();
                starts.pop_back();
                song.addNote(trackIndex,
                             { startTick,
                               std::max(1, tick - startTick),
                               message.getNoteNumber(),
                               std::clamp(velocity, 1, 127),
                               message.getChannel() });
            }
        }
    }

    return juce::Result::ok();
}
}
