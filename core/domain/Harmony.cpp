#include "Harmony.h"

#include "Scales.h"

namespace bc {

int maxGap(double density) {
    if (!(density >= 0)) density = 0;
    if (density > 1) density = 1;
    return 1 + static_cast<int>((1.0 - density) * 5.0 + 0.5);
}

int octaveFor(double value, int octaveMin, int octaveMax) {
    if (octaveMax < octaveMin) {
        int t = octaveMin;
        octaveMin = octaveMax;
        octaveMax = t;
    }
    const int buckets = octaveMax - octaveMin + 1;
    double u = (value + 1.0) * 0.5;      // 0..1
    int k = static_cast<int>(u * buckets);
    if (k < 0) k = 0;
    if (k >= buckets) k = buckets - 1;
    return octaveMin + k;
}

int resolveChord(const Notes& notes, const Chords& chords, const Octaves& octaves, int chord,
                 const HarmonyParams& p, int out[kMaxNotes]) {
    if (chord < 0 || chord >= kMaxChords) return 0;
    int scaleIndex = p.scale < 0 ? 0 : (p.scale >= kScaleCount ? kScaleCount - 1 : p.scale);
    const Scale& scale = kScales[scaleIndex];
    int count = p.notesCount < 1 ? 1 : (p.notesCount > kMaxNotes ? kMaxNotes : p.notesCount);
    const int root = ((p.root % 12) + 12) % 12;

    // Main note: a degree of the scale, inside one octave above the root.
    int mainDegree = static_cast<int>(notes.value[chord] * scale.size);
    if (mainDegree >= scale.size) mainDegree = scale.size - 1;
    if (mainDegree < 0) mainDegree = 0;

    // Extra notes stack above the highest note so far (value >= 0) or below the lowest (value < 0).
    int degrees[kMaxNotes];
    degrees[0] = mainDegree;
    int lo = mainDegree, hi = mainDegree;
    const int gapMax = maxGap(p.density);
    for (int k = 1; k < count; ++k) {
        double v = chords.value[chord][k - 1];
        double a = v < 0 ? -v : v;
        int gap = 1 + static_cast<int>(a * gapMax);
        if (gap > gapMax) gap = gapMax;
        int step = gap + 1;
        if (v >= 0) {
            hi += step;
            degrees[k] = hi;
        } else {
            lo -= step;
            degrees[k] = lo;
        }
    }

    int n = 0;
    for (int k = 0; k < count; ++k) {
        // The scale and density pick the pitch class; the octaves sequence puts it in an octave in [min, max].
        int pitchClass = ((root + degreeToSemitones(scale, degrees[k])) % 12 + 12) % 12;
        int octave = octaveFor(octaves.value[chord][k], p.octaveMin, p.octaveMax);
        int pitch = kOctaveZero + 12 * octave + pitchClass;
        while (pitch < 0) pitch += 12;
        while (pitch > 127) pitch -= 12;
        bool seen = false;
        for (int j = 0; j < n; ++j) seen = seen || out[j] == pitch;
        if (!seen) out[n++] = pitch;
    }
    // Ascending (insertion sort, n <= 6).
    for (int i = 1; i < n; ++i) {
        int v = out[i], j = i - 1;
        while (j >= 0 && out[j] > v) {
            out[j + 1] = out[j];
            --j;
        }
        out[j + 1] = v;
    }
    return n;
}

} // namespace bc
