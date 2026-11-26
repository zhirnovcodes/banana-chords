#pragma once
#include <cstdint>

#include "Rng.h"
#include "Scales.h"
#include "Sequences.h"

namespace bc {

// Chord generator with WFC-style adjacency rules: each chord is picked from the chords allowed after
// the previous one. Spec: docs/chords-generator.md. Not wired into the Engine yet.
//
// Works in scale-degree space, so any scale works (modes, pentatonics, whole-tone, user scales).
// No functional-harmony rules: every rule is stated in scale steps, semitones and pitch classes.
//
// Never allocates. initialize() enumerates voicings (up to ~10 ms for 6-note chromatic chords):
// keep it off the audio thread. generate() and getNext() are a few µs: safe on the audio thread.
//
//   gen.initialize(settings);                  // when scale / root / range / notes / density change
//   gen.generate(seed);                        // start a new chord set
//   for (int i = 0; i < n; ++i) chords[i] = gen.getNext(i == n - 1);

// Candidate pool cap. More voicings than this are sampled down (fixed seed, so the pool depends
// only on the settings).
inline constexpr int kMaxChordCandidates = 256;

// Every tunable number of the rules. Defaults follow the spec; tune per genre later.
struct ChordRules {
    // Gaps between neighbouring chord notes, in scale steps of a 7-note scale (scaled by N / 7).
    double gapMinOpen = 4.0;      // smallest gap at density 0
    double gapMinDense = 1.0;     // smallest gap at density 1
    double gapMax = 6.0;          // largest gap at any density
    int maxSpanSemitones = 24;    // lowest to highest chord note; 0 = no limit

    // Transition score A → B.
    double rootFifth = 3.0;       // bass interval class 5
    double rootThird = 2.0;       // interval class 3 or 4
    double rootStep = 1.5;        // interval class 1 or 2
    double rootTritone = 0.5;     // interval class 6
    double rootSame = -1.0;       // interval class 0
    double commonTone = 1.0;      // per shared pitch class
    double identicalChord = -3.0; // same pitch-class set (replaces the common tone bonus)
    double voiceLeading = -0.15;  // per semitone of total voice movement
    double hardThreshold = 0.0;   // A → B is allowed only when score >= this
};

// Allowed gap range in scale steps. All gap math lives here so the units can be swapped.
// `widen` grows the range by that many steps on both sides.
struct GapRange {
    int min;
    int max;
};
GapRange chordGapRange(double density, int scaleSize, const ChordRules& rules, int widen = 0);

struct ChordsSettings {
    int root = 0;                 // pitch class of the tonic, 0 = C
    Scale scale = kScales[0];     // any scale: size 2..12, steps ascending from 0
    int lowNote = 48;             // MIDI range, both inclusive
    int highNote = 84;
    int notesPerChord = 3;        // 2..kMaxNotes
    double density = 0.66;        // 0 = open voicings, 1 = clusters allowed
    double randomness = 1.0;      // temperature of the transition weights
    ChordRules rules;
};

struct Chord {
    int count = 0;
    int notes[kMaxNotes] = {};    // MIDI, ascending
};

class ChordsGenerator {
public:
    // Builds the candidate voicings. False when the settings are invalid or no chord fits the range
    // (then getNext() returns empty chords). When nothing fits the density, the gap range is widened
    // by one step and the span limit dropped.
    bool initialize(const ChordsSettings& settings);

    // Starts a new chord sequence: the next getNext() picks a first chord with no predecessor.
    void generate(std::uint64_t seed);

    // The chord after the previous one. `last`: stand on the tonic (contain it, if no voicing stands on it).
    // When no candidate is allowed after the previous chord, the hard threshold is dropped for this pick.
    Chord getNext(bool last = false);

private:
    struct Candidate {
        int count = 0;
        int notes[kMaxNotes] = {};
        int mask = 0;        // pitch classes
        int bassPc = 0;
        int rootDegree = 0;  // scale degree of the lowest note
    };

    struct Enumeration;
    void enumerate(Enumeration& e, int depth, int k);
    int pick(const bool* eligible);
    double score(const Candidate& a, const Candidate& b) const;

    ChordsSettings settings_;
    Candidate pool_[kMaxChordCandidates];
    int poolCount_ = 0;
    long long seen_ = 0;               // voicings offered to the pool sampler
    Rng rng_{1};
    Rng poolRng_{1};
    Candidate previous_;
    bool hasPrevious_ = false;
    double weights_[kMaxChordCandidates] = {};
};

} // namespace bc
