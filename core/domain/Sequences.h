#pragma once
#include <cstdint>

#include "Rng.h"

namespace bc {

inline constexpr int kMaxChords = 32;        // loop 32 beats / chord min 1 beat
inline constexpr int kMaxNotes = 6;          // notes in one chord
inline constexpr int kQuantaPerBeat = 4;     // drift moves chord borders on a 1/4 beat grid
inline constexpr int kMaxCells = 32 * kQuantaPerBeat;

// 1 · times: chord lengths in whole beats. Sum == loop, each in [min, max] where the loop allows.
struct Times {
    int count = 0;
    int beats[kMaxChords] = {};
};

// 2 · drift: one value in [-1, 1) per neighbouring chord pair (count - 1 values).
struct Drift {
    double value[kMaxChords] = {};
};

// 3 · time table: loop position (in cells) → chord index. Built, not random.
struct TimeTable {
    int cellQuanta = kQuantaPerBeat;     // cell size in quanta (1/4 beats)
    int cellCount = 0;
    std::uint8_t chord[kMaxCells] = {};
    int border[kMaxChords + 1] = {};     // chord i plays on [border[i], border[i+1]) quanta, after drift
};

// 4 · notes: one value in [0, 1) per chord, the main note's scale degree.
struct Notes {
    double value[kMaxChords] = {};
};

// 5 · chords: per chord, notes count - 1 values in [-1, 1): where each extra chord note goes.
struct Chords {
    double value[kMaxChords][kMaxNotes - 1] = {};
};

// 6 · octaves: per chord, notes count values in [-1, 1): octave transpose of each note.
struct Octaves {
    double value[kMaxChords][kMaxNotes] = {};
};

void generateTimes(Rng& rng, int loop, int minLen, int maxLen, Times& out);
void generateDrift(Rng& rng, int chordCount, Drift& out);
// amount in [-1, 1] scales every drift value (the Drift Offset parameter).
void buildTimeTable(const Times& times, const Drift& drift, double amount, TimeTable& out);
void generateNotes(Rng& rng, int chordCount, Notes& out);
void generateChords(Rng& rng, int chordCount, int notesCount, Chords& out);
void generateOctaves(Rng& rng, int chordCount, int notesCount, Octaves& out);

// Chord index at a loop position in quanta (any integer; wraps by the loop).
int chordAt(const TimeTable& table, long long quanta);

} // namespace bc
