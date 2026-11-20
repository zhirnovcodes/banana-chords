#pragma once

#include "Sequences.h"

namespace bc {

// MIDI note of C in octave 0. Octave numbers match note names in the C3 = 60 naming IvanDAW and
// Ableton use: octave n holds C<n>..B<n>, so octave 0 = MIDI 24..35, octave 6 = MIDI 96..107.
inline constexpr int kOctaveZero = 24;

struct HarmonyParams {
    int root = 0;          // 0..11
    int scale = 0;         // index into kScales
    double density = 0.8;  // 0..1
    int notesCount = 3;    // 1..kMaxNotes
    int octaveMin = 2;     // every note lands in an octave in [octaveMin, octaveMax]
    int octaveMax = 3;
};

// Scale steps between neighbouring chord notes is 1 + gap, gap in [1, maxGap(density)].
// density 1 → gap 1 (stacked thirds in a 7-note scale), density 0 → gap up to 6.
int maxGap(double density);

// Octave for a sequence value in [-1, 1): uniform buckets over [octaveMin, octaveMax].
int octaveFor(double value, int octaveMin, int octaveMax);

// The MIDI notes of chord `chord`, ascending, without duplicates. Returns how many (0..kMaxNotes).
int resolveChord(const Notes& notes, const Chords& chords, const Octaves& octaves, int chord,
                 const HarmonyParams& p, int out[kMaxNotes]);

} // namespace bc
