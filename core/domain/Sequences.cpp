#include "Sequences.h"

namespace bc {

namespace {

int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

int gcd(int a, int b) {
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a < 0 ? -a : a;
}

int roundAway(double v) { return static_cast<int>(v < 0 ? v - 0.5 : v + 0.5); }

} // namespace

void generateTimes(Rng& rng, int loop, int minLen, int maxLen, Times& out) {
    loop = clampInt(loop, 1, kMaxChords);
    minLen = clampInt(minLen, 1, 32);
    maxLen = clampInt(maxLen, 1, 32);
    if (maxLen < minLen) {
        int t = minLen;
        minLen = maxLen;
        maxLen = t;
    }

    // r beats can be split into chords of min..max: some k chords with k*min <= r <= k*max.
    auto fillable = [&](int r) {
        if (r == 0) return true;
        int k = (r + maxLen - 1) / maxLen;   // fewest chords that can cover r
        return k * minLen <= r;
    };

    out.count = 0;
    int remaining = loop;
    while (remaining > 0 && out.count < kMaxChords) {
        int options[32];
        int optionCount = 0;
        for (int len = minLen; len <= maxLen && len <= remaining; ++len) {
            if (fillable(remaining - len)) options[optionCount++] = len;
        }
        // No option only when the loop itself cannot be split (e.g. loop 5, chords 3..4): one chord takes the rest.
        int len = optionCount > 0 ? options[rng.range(0, optionCount - 1)] : remaining;
        out.beats[out.count++] = len;
        remaining -= len;
    }
}

void generateDrift(Rng& rng, int chordCount, Drift& out) {
    for (int i = 0; i < kMaxChords; ++i) out.value[i] = 0;
    for (int i = 0; i + 1 < chordCount; ++i) out.value[i] = rng.signedUnit();
}

void buildTimeTable(const Times& times, const Drift& drift, double amount, TimeTable& out) {
    const int n = times.count;
    if (amount < -1) amount = -1;
    if (amount > 1) amount = 1;

    int len[kMaxChords];
    out.border[0] = 0;
    for (int i = 0; i < n; ++i) {
        len[i] = times.beats[i] * kQuantaPerBeat;
        out.border[i + 1] = out.border[i] + len[i];
    }
    const int total = out.border[n];

    // Move each inner border. d > 0: the first chord grows into the second by up to half the second.
    // d < 0: the second chord starts early by up to half the first.
    for (int i = 1; i < n; ++i) {
        double d = drift.value[i - 1] * amount;
        int shift = d >= 0 ? roundAway(d * len[i] * 0.5) : roundAway(d * len[i - 1] * 0.5);
        out.border[i] += shift;
    }
    // Every chord keeps at least one quantum.
    for (int i = 1; i < n; ++i) {
        if (out.border[i] < out.border[i - 1] + 1) out.border[i] = out.border[i - 1] + 1;
    }
    for (int i = n - 1; i >= 1; --i) {
        if (out.border[i] > out.border[i + 1] - 1) out.border[i] = out.border[i + 1] - 1;
    }

    int cell = total;
    for (int i = 1; i < n; ++i) cell = gcd(cell, out.border[i]);
    if (cell <= 0) cell = 1;
    out.cellQuanta = cell;
    out.cellCount = total / cell;

    int chord = 0;
    for (int c = 0; c < out.cellCount; ++c) {
        int q = c * cell;
        while (chord + 1 < n && q >= out.border[chord + 1]) ++chord;
        out.chord[c] = static_cast<std::uint8_t>(chord);
    }
}

int chordAt(const TimeTable& table, long long quanta) {
    if (table.cellCount <= 0) return 0;
    const long long total = static_cast<long long>(table.cellCount) * table.cellQuanta;
    long long pos = quanta % total;
    if (pos < 0) pos += total;
    return table.chord[pos / table.cellQuanta];
}

} // namespace bc
