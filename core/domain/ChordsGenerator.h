#pragma once
#include <cstdint>

#include "Rng.h"
#include "Scales.h"
#include "Sequences.h"

namespace bc {

// Chord generator with WFC-style adjacency rules: each chord is picked from the chords allowed after
// the previous one. Spec: docs/chords-generator.md. The Engine makes the loop's chords with it (steps 4-6).
//
// Works in scale-degree space, so any scale works (modes, pentatonics, whole-tone, user scales).
// No functional-harmony rules: every rule is stated in scale steps, semitones and pitch classes.
//
// Never allocates, and every call is cheap enough for the audio thread (see tests/chords_load_tests.cpp).
//
//   gen.initialize(settings);                  // when scale / root / range / notes / density change
//   gen.generate(seed);                        // start a new chord set
//   for (int i = 0; i < n; ++i) chords[i] = gen.getNext(i == n - 1);

// Candidate pool cap. More voicings than this are sampled down (fixed seed, so the pool depends
// only on the settings).
inline constexpr int kMaxChordCandidates = 256;
// Voicings are listed in full while that takes at most this many steps; bigger sets are sampled.
inline constexpr long long kEnumerateBudget = 3000;
// Sampling attempts per initialize, when the grid is too big to list.
inline constexpr int kSampleTries = kMaxChordCandidates * 32;

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
struct GapRange {
    int min;
    int max;
};
GapRange chordGapRange(double density, int scaleSize, const ChordRules& rules);

struct ChordsSettings {
    int root = 0;                 // pitch class of the tonic, 0 = C
    Scale scale = kScales[0];     // any scale: size 2..12, steps ascending from 0
    int lowNote = 48;             // MIDI range, both inclusive
    int highNote = 84;
    int notesPerChord = 3;        // 1..kMaxNotes (most notes; fewer when they do not fit, see initialize)
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
    // Builds the candidate voicings. The range and density always hold: when notesPerChord notes do not
    // fit them (e.g. open voicings in one octave), chords get one note fewer, down to 1.
    // False when the settings are invalid or no scale note is in the range (then getNext() returns
    // empty chords).
    bool initialize(const ChordsSettings& settings);

    // Notes per chord after initialize: settings.notesPerChord, or fewer when they did not fit.
    int notesPerChord() const { return poolCount_ > 0 ? pool_[0].count : 0; }

    // Work of the last initialize() or getNext() call, in steps: voicing nodes listed, sampling tries and
    // gap checks, candidates visited. Machine-independent, so tests can bound the cost without a clock.
    long long lastWork() const { return work_; }

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
    void enumerate(Enumeration& e, int depth, int k, int mask);
    void sample(Enumeration& e, int lowIndex, int rootCount);
    bool makeCandidate(const Enumeration& e, Candidate& c) const;
    bool addUnique(const Candidate& c);   // false when the pool already holds it
    int pick(const bool* eligible);
    double score(const Candidate& a, const Candidate& b) const;

    ChordsSettings settings_;
    Candidate pool_[kMaxChordCandidates];
    int poolCount_ = 0;
    long long seen_ = 0;               // voicings offered to the pool sampler
    long long steps_ = 0;              // enumeration steps, against kEnumerateBudget
    long long work_ = 0;               // see lastWork()
    static constexpr int kHashSlots = kMaxChordCandidates * 2;
    std::int16_t hash_[kHashSlots] = {};   // pool index + 1, 0 = empty (open addressing)
    Rng rng_{1};
    Rng poolRng_{1};
    Candidate previous_;
    bool hasPrevious_ = false;
    double weights_[kMaxChordCandidates] = {};
};

} // namespace bc
