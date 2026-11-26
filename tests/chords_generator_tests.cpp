#include "Check.h"
#include "ChordsGenerator.h"

using namespace bc;

namespace {

int pcOf(int pitch) { return ((pitch % 12) + 12) % 12; }

// Flat scale index (octave * N + degree) of a MIDI pitch, -1 when the pitch is not in the scale.
int scaleIndex(int pitch, int root, const Scale& s) {
    const int rel = pitch - root + 120;
    const int octave = rel / 12, pc = rel % 12;
    for (int d = 0; d < s.size; ++d)
        if (s.steps[d] == pc) return octave * s.size + d;
    return -1;
}

int maskOf(const Chord& c) {
    int m = 0;
    for (int i = 0; i < c.count; ++i) m |= 1 << pcOf(c.notes[i]);
    return m;
}

// One chord set the way the Engine will make it: generate(seed), then getNext per chord.
void makeSet(ChordsGenerator& gen, std::uint64_t seed, int n, Chord* out) {
    gen.generate(seed);
    for (int i = 0; i < n; ++i) out[i] = gen.getNext(i == n - 1);
}

// Every chord: right count, ascending, in range, in the scale.
void checkWellFormed(const Chord* chords, int n, const ChordsSettings& s) {
    for (int i = 0; i < n; ++i) {
        const Chord& c = chords[i];
        CHECK_EQ(c.count, s.notesPerChord);
        for (int k = 0; k < c.count; ++k) {
            CHECK(c.notes[k] >= s.lowNote && c.notes[k] <= s.highNote);
            CHECK(scaleIndex(c.notes[k], s.root, s.scale) >= 0);
            if (k > 0) CHECK(c.notes[k] > c.notes[k - 1]);
        }
    }
}

void gapRanges() {
    ChordRules r;
    GapRange g = chordGapRange(0.0, 7, r);
    CHECK_EQ(g.min, 4);
    CHECK_EQ(g.max, 6);
    g = chordGapRange(0.66, 7, r);
    CHECK_EQ(g.min, 2);
    g = chordGapRange(1.0, 7, r);
    CHECK_EQ(g.min, 1);
    g = chordGapRange(0.0, 5, r);   // pentatonic: scaled by 5/7
    CHECK_EQ(g.min, 3);
    CHECK_EQ(g.max, 4);
    g = chordGapRange(1.0, 7, r, 1);
    CHECK_EQ(g.min, 1);
    CHECK_EQ(g.max, 7);
}

// Gaps (in scale steps) inside the density range; last chord stands on the tonic.
void gapsFollowDensity(const Scale& scale, int root, double density, int notes) {
    ChordsGenerator gen;
    ChordsSettings s;
    s.scale = scale;
    s.root = root;
    s.density = density;
    s.notesPerChord = notes;
    CHECK(gen.initialize(s));
    const GapRange g = chordGapRange(density, scale.size, s.rules);
    for (std::uint64_t seed = 1; seed <= 10; ++seed) {
        Chord chords[kMaxChords];
        makeSet(gen, seed, kMaxChords, chords);
        checkWellFormed(chords, kMaxChords, s);
        for (const Chord& c : chords)
            for (int k = 1; k < c.count; ++k) {
                const int gap = scaleIndex(c.notes[k], root, scale) - scaleIndex(c.notes[k - 1], root, scale);
                CHECK(gap >= g.min && gap <= g.max);
            }
        CHECK_EQ(pcOf(chords[kMaxChords - 1].notes[0]), root);
    }
}

// Density 1, 4 notes → some chords hold neighbouring scale notes.
void clustersAtFullDensity() {
    ChordsGenerator gen;
    ChordsSettings s;
    s.density = 1;
    s.notesPerChord = 4;
    CHECK(gen.initialize(s));
    int clusters = 0;
    for (std::uint64_t seed = 1; seed <= 10; ++seed) {
        Chord chords[8];
        makeSet(gen, seed, 8, chords);
        checkWellFormed(chords, 8, s);
        for (const Chord& c : chords)
            for (int k = 1; k < c.count; ++k)
                if (scaleIndex(c.notes[k], 0, s.scale) - scaleIndex(c.notes[k - 1], 0, s.scale) == 1) ++clusters;
    }
    CHECK(clusters > 0);
}

// Every built-in scale, 2..6 notes, any density: no special cases, no empty chords.
// Covers whole-tone and 6 notes in a 5-note scale (doubled pitch classes).
void everyScaleWorks() {
    ChordsGenerator gen;
    const double densities[] = {0.0, 0.66, 1.0};
    for (const Scale& scale : kScales)
        for (double d : densities)
            for (int notes = 2; notes <= kMaxNotes; ++notes) {
                ChordsSettings s;
                s.scale = scale;
                s.root = 4;
                s.density = d;
                s.notesPerChord = notes;
                CHECK(gen.initialize(s));
                Chord chords[kMaxChords];
                makeSet(gen, 7, kMaxChords, chords);
                checkWellFormed(chords, kMaxChords, s);
            }
}

// Neighbouring chords never repeat a pitch-class set (C major, plenty of candidates).
void noRepeatedNeighbours() {
    ChordsGenerator gen;
    ChordsSettings s;
    CHECK(gen.initialize(s));
    for (std::uint64_t seed = 1; seed <= 10; ++seed) {
        Chord chords[kMaxChords];
        makeSet(gen, seed, kMaxChords, chords);
        for (int i = 1; i < kMaxChords; ++i) CHECK(maskOf(chords[i]) != maskOf(chords[i - 1]));
    }
}

// Same seed → same set, also from another generator and after re-initializing.
void sameSeedSameOutput() {
    ChordsGenerator a, b;
    ChordsSettings s;
    CHECK(a.initialize(s));
    CHECK(b.initialize(s));
    Chord r1[kMaxChords], r2[kMaxChords], r3[kMaxChords];
    makeSet(a, 42, kMaxChords, r1);
    makeSet(b, 42, kMaxChords, r2);
    CHECK(a.initialize(s));
    makeSet(a, 42, kMaxChords, r3);
    bool same = true;
    for (int i = 0; i < kMaxChords; ++i)
        for (int k = 0; k < r1[i].count; ++k)
            same = same && r1[i].notes[k] == r2[i].notes[k] && r1[i].notes[k] == r3[i].notes[k];
    CHECK(same);

    makeSet(a, 43, kMaxChords, r2);
    bool different = false;
    for (int i = 0; i < kMaxChords; ++i)
        for (int k = 0; k < r1[i].count; ++k) different = different || r1[i].notes[k] != r2[i].notes[k];
    CHECK(different);
}

void badInput() {
    ChordsGenerator gen;
    ChordsSettings s;
    s.scale.size = 1;
    CHECK(!gen.initialize(s));
    gen.generate(1);
    CHECK_EQ(gen.getNext().count, 0);

    ChordsSettings narrow;
    narrow.lowNote = 61;   // C#..C# holds no C major note
    narrow.highNote = 61;
    CHECK(!gen.initialize(narrow));
    CHECK_EQ(gen.getNext().count, 0);
}

} // namespace

void chordsGeneratorTests() {
    gapRanges();
    gapsFollowDensity(kScales[0], 0, 0.0, 3);    // C major, open voicings
    gapsFollowDensity(kScales[0], 0, 0.66, 3);   // C major, tertian
    gapsFollowDensity(kScales[13], 9, 0.66, 3);  // A minor pentatonic
    clustersAtFullDensity();
    everyScaleWorks();
    noRepeatedNeighbours();
    sameSeedSameOutput();
    badInput();
}
