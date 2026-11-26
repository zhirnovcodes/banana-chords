#pragma once

namespace bc {

// Scales are semitone steps from the root, ascending, first step 0.
// The order matches the "scale" choices in params.json. Append only.
struct Scale {
    const char* name;
    int size;
    int steps[12];
};

inline constexpr Scale kScales[] = {
    {"Major",             7, {0, 2, 4, 5, 7, 9, 11}},
    {"Natural Minor",     7, {0, 2, 3, 5, 7, 8, 10}},
    {"Harmonic Minor",    7, {0, 2, 3, 5, 7, 8, 11}},
    {"Melodic Minor",     7, {0, 2, 3, 5, 7, 9, 11}},
    {"Dorian",            7, {0, 2, 3, 5, 7, 9, 10}},
    {"Phrygian",          7, {0, 1, 3, 5, 7, 8, 10}},
    {"Lydian",            7, {0, 2, 4, 6, 7, 9, 11}},
    {"Mixolydian",        7, {0, 2, 4, 5, 7, 9, 10}},
    {"Locrian",           7, {0, 1, 3, 5, 6, 8, 10}},
    {"Phrygian Dominant", 7, {0, 1, 4, 5, 7, 8, 10}},
    {"Hungarian Minor",   7, {0, 2, 3, 6, 7, 8, 11}},
    {"Double Harmonic",   7, {0, 1, 4, 5, 7, 8, 11}},
    {"Major Pentatonic",  5, {0, 2, 4, 7, 9}},
    {"Minor Pentatonic",  5, {0, 3, 5, 7, 10}},
    {"Blues",             6, {0, 3, 5, 6, 7, 10}},
    {"Whole Tone",        6, {0, 2, 4, 6, 8, 10}},
    {"Diminished",        8, {0, 2, 3, 5, 6, 8, 9, 11}},
    {"Hirajoshi",         5, {0, 2, 3, 7, 8}},
    {"In",                5, {0, 1, 5, 7, 8}},
    {"Chromatic",        12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
};

inline constexpr int kScaleCount = static_cast<int>(sizeof(kScales) / sizeof(kScales[0]));

// Pitch classes of `scale` on `root` (0 = C): bit k set = pitch class k is in the scale. For UIs (piano dots).
inline int scalePitchMask(int scale, int root) {
    if (scale < 0 || scale >= kScaleCount) return 0;
    const Scale& s = kScales[scale];
    int mask = 0;
    for (int i = 0; i < s.size; ++i) mask |= 1 << (((root % 12 + 12) % 12 + s.steps[i]) % 12);
    return mask;
}

} // namespace bc
