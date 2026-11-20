#include "Check.h"
#include "Harmony.h"
#include "ParamsJson.h"
#include "Scales.h"

using namespace bc;

namespace {

void scaleTableMatchesParams() {
    // params.json "scale" is index 1.
    CHECK_EQ(static_cast<int>(kParams[1].max) + 1, kScaleCount);
    for (const Scale& s : kScales) {
        CHECK_EQ(s.steps[0], 0);
        for (int i = 1; i < s.size; ++i) CHECK(s.steps[i] > s.steps[i - 1] && s.steps[i] < 12);
    }
    CHECK_EQ(degreeToSemitones(kScales[0], 7), 12);
    CHECK_EQ(degreeToSemitones(kScales[0], -1), -1);
    CHECK_EQ(degreeToSemitones(kScales[0], -7), -12);
    CHECK_EQ(degreeToSemitones(kScales[0], 9), 16);
}

void gapsAndOctaves() {
    CHECK_EQ(maxGap(1.0), 1);
    CHECK_EQ(maxGap(0.0), 6);
    CHECK_EQ(maxGap(0.5), 4);
    CHECK_EQ(octaveFor(-1.0, -1, 1), -1);
    CHECK_EQ(octaveFor(0.0, -1, 1), 0);
    CHECK_EQ(octaveFor(0.99, -1, 1), 1);
    CHECK_EQ(octaveFor(0.5, 1, -1), 1);   // swapped bounds
    CHECK_EQ(octaveFor(0.5, 0, 0), 0);
}

void cMajorTriad() {
    Notes notes;
    Chords chords;
    Octaves octaves;
    notes.value[0] = 0.0;    // degree 0
    chords.value[0][0] = 0.1;
    chords.value[0][1] = 0.9;
    HarmonyParams p;
    p.density = 1;           // thirds only
    p.notesCount = 3;
    p.octaveMin = 3;
    p.octaveMax = 3;
    int out[kMaxNotes];
    int n = resolveChord(notes, chords, octaves, 0, p, out);
    CHECK_EQ(n, 3);
    CHECK_EQ(out[0], 60);   // C3
    CHECK_EQ(out[1], 64);   // E3
    CHECK_EQ(out[2], 67);   // G3

    // Below the main note, A minor: root A, notes stack downward to D F A, all inside octave 0.
    p.root = 9;
    p.scale = 1;
    p.octaveMin = 0;
    p.octaveMax = 0;
    chords.value[0][0] = -0.1;
    chords.value[0][1] = -0.1;
    n = resolveChord(notes, chords, octaves, 0, p, out);
    CHECK_EQ(n, 3);
    CHECK_EQ(out[0], 26);   // D0
    CHECK_EQ(out[1], 29);   // F0
    CHECK_EQ(out[2], 33);   // A0

    // Octave values spread the notes over [min, max]: lowest value → min, highest → max.
    p.octaveMin = 1;
    p.octaveMax = 5;
    octaves.value[0][0] = -1.0;
    octaves.value[0][1] = 0.0;
    octaves.value[0][2] = 0.99;
    n = resolveChord(notes, chords, octaves, 0, p, out);
    CHECK_EQ(n, 3);
    CHECK_EQ(out[0], 24 + 12 * 1 + 9);   // A1 (main note, octave min)
    CHECK_EQ(out[1], 24 + 12 * 3 + 5);   // F3 (middle)
    CHECK_EQ(out[2], 24 + 12 * 5 + 2);   // D5 (octave max)
}

void notesStayInScaleAndAreUnique() {
    for (std::uint64_t seed = 1; seed <= 500; ++seed) {
        Rng rng(seed);
        Notes notes;
        Chords chords;
        Octaves octaves;
        generateNotes(rng, 1, notes);
        generateChords(rng, 1, kMaxNotes, chords);
        generateOctaves(rng, 1, kMaxNotes, octaves);
        HarmonyParams p;
        p.root = rng.range(0, 11);
        p.scale = rng.range(0, kScaleCount - 1);
        p.density = rng.unit();
        p.notesCount = rng.range(1, kMaxNotes);
        p.octaveMin = rng.range(0, 6);
        p.octaveMax = rng.range(p.octaveMin, 6);
        int out[kMaxNotes];
        int n = resolveChord(notes, chords, octaves, 0, p, out);
        CHECK(n >= 1 && n <= p.notesCount);
        const Scale& s = kScales[p.scale];
        for (int i = 0; i < n; ++i) {
            // Every note inside the octaves [min, max].
            CHECK(out[i] >= kOctaveZero + 12 * p.octaveMin);
            CHECK(out[i] < kOctaveZero + 12 * (p.octaveMax + 1));
            if (i > 0) CHECK(out[i] > out[i - 1]);
            int pc = ((out[i] - p.root) % 12 + 12) % 12;
            bool inScale = false;
            for (int k = 0; k < s.size; ++k) inScale = inScale || s.steps[k] == pc;
            CHECK(inScale);
        }
    }
}

} // namespace

void harmonyTests() {
    scaleTableMatchesParams();
    gapsAndOctaves();
    cMajorTriad();
    notesStayInScaleAndAreUnique();
}
