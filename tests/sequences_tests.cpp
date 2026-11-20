#include <initializer_list>

#include "Check.h"
#include "Sequences.h"

using namespace bc;

namespace {

bool fillable(int loop, int mn, int mx) {
    for (int k = 1; k <= loop; ++k) {
        if (k * mn <= loop && loop <= k * mx) return true;
    }
    return false;
}

void timesSumToLoopAndRespectRange() {
    for (int loop = 1; loop <= 32; ++loop) {
        for (int mn = 1; mn <= 8; ++mn) {
            for (int mx = mn; mx <= 9; ++mx) {
                for (std::uint64_t seed = 1; seed <= 20; ++seed) {
                    Rng rng(seed * 7919 + loop * 31 + mn * 7 + mx);
                    Times t;
                    generateTimes(rng, loop, mn, mx, t);
                    int sum = 0;
                    bool inRange = true;
                    for (int i = 0; i < t.count; ++i) {
                        sum += t.beats[i];
                        inRange = inRange && t.beats[i] >= mn && t.beats[i] <= mx;
                    }
                    CHECK_EQ(sum, loop);
                    if (fillable(loop, mn, mx)) CHECK(inRange);
                    else CHECK_EQ(t.count, 1);
                }
            }
        }
    }
}

void timesSwapMinMax() {
    Rng rng(5);
    Times t;
    generateTimes(rng, 8, 3, 1, t);
    int sum = 0;
    for (int i = 0; i < t.count; ++i) {
        sum += t.beats[i];
        CHECK(t.beats[i] >= 1 && t.beats[i] <= 3);
    }
    CHECK_EQ(sum, 8);
}

void driftInRange() {
    Rng rng(9);
    Drift d;
    generateDrift(rng, 10, d);
    for (int i = 0; i < 9; ++i) CHECK(d.value[i] >= -1 && d.value[i] < 1);
    for (int i = 9; i < kMaxChords; ++i) CHECK_EQ(d.value[i], 0.0);
}

Times makeTimes(std::initializer_list<int> beats) {
    Times t;
    for (int b : beats) t.beats[t.count++] = b;
    return t;
}

void tableWithoutDrift() {
    Times t = makeTimes({2, 4, 2});
    Drift d;
    d.value[0] = 1;
    d.value[1] = -1;
    TimeTable tt;
    buildTimeTable(t, d, 0.0, tt);
    CHECK_EQ(tt.border[0], 0);
    CHECK_EQ(tt.border[1], 8);
    CHECK_EQ(tt.border[2], 24);
    CHECK_EQ(tt.border[3], 32);
    CHECK_EQ(tt.cellQuanta, 8);   // 2 beats
    CHECK_EQ(tt.cellCount, 4);
    CHECK_EQ(chordAt(tt, 0), 0);
    CHECK_EQ(chordAt(tt, 7), 0);
    CHECK_EQ(chordAt(tt, 8), 1);
    CHECK_EQ(chordAt(tt, 23), 1);
    CHECK_EQ(chordAt(tt, 24), 2);
    CHECK_EQ(chordAt(tt, 32), 0);   // wraps
    CHECK_EQ(chordAt(tt, -1), 2);   // before the loop start wraps too
}

void tableDriftMovesBorders() {
    Times t = makeTimes({2, 2});
    Drift d;
    TimeTable tt;

    // +1: the first chord grows by half the second (1 beat).
    d.value[0] = 1;
    buildTimeTable(t, d, 1.0, tt);
    CHECK_EQ(tt.border[1], 12);
    CHECK_EQ(chordAt(tt, 11), 0);
    CHECK_EQ(chordAt(tt, 12), 1);

    // -1: the second chord starts half the first (1 beat) early.
    d.value[0] = -1;
    buildTimeTable(t, d, 1.0, tt);
    CHECK_EQ(tt.border[1], 4);

    // The amount scales and flips the drift; the result lands on the 1/4 beat grid.
    d.value[0] = 0.5;
    buildTimeTable(t, d, -1.0, tt);
    CHECK_EQ(tt.border[1], 6);    // -0.5 * 4 quanta / 2 = -1 quantum
    CHECK_EQ(tt.cellQuanta, 2);
    CHECK_EQ(tt.cellCount, 8);
}

void tableKeepsEveryChord() {
    for (std::uint64_t seed = 1; seed <= 300; ++seed) {
        Rng rng(seed);
        Times t;
        generateTimes(rng, 32, 1, 3, t);
        Drift d;
        generateDrift(rng, t.count, d);
        for (double amount : {-1.0, -0.3, 0.0, 0.7, 1.0}) {
            TimeTable tt;
            buildTimeTable(t, d, amount, tt);
            CHECK_EQ(tt.border[0], 0);
            CHECK_EQ(tt.border[t.count], 32 * kQuantaPerBeat);
            for (int i = 0; i < t.count; ++i) {
                CHECK(tt.border[i + 1] > tt.border[i]);
                CHECK_EQ(chordAt(tt, tt.border[i]), i);
            }
            CHECK(tt.cellCount <= kMaxCells);
            CHECK_EQ(tt.cellCount * tt.cellQuanta, 32 * kQuantaPerBeat);
        }
    }
}

} // namespace

void sequencesTests() {
    timesSumToLoopAndRespectRange();
    timesSwapMinMax();
    driftInRange();
    tableWithoutDrift();
    tableDriftMovesBorders();
    tableKeepsEveryChord();
}
