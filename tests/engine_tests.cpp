#include <algorithm>
#include <cmath>
#include <vector>

#include "Check.h"
#include "Engine.h"

using namespace bc;

namespace {

constexpr double kRate = 48000;

struct Ev {
    long long frame;
    int b0, b1, b2;
    bool operator==(const Ev& o) const { return frame == o.frame && b0 == o.b0 && b1 == o.b1 && b2 == o.b2; }
};

void collect(const OutBuffer& out, long long blockStart, std::vector<Ev>& events) {
    for (int i = 0; i < out.size(); ++i) {
        events.push_back({blockStart + out[i].frame, out[i].b0, out[i].b1, out[i].b2});
    }
}

// Plays `totalFrames` from beat 0 at `bpm`, in blocks of `block`. Stops at `stopFrame` (absolute) if >= 0.
std::vector<Ev> render(Engine& e, long long totalFrames, int block, double bpm = 120, long long stopFrame = -1) {
    std::vector<Ev> events;
    OutBuffer out;
    Transport play;
    play.playing = true;
    play.bpm = bpm;
    e.transport(play, 0);
    for (long long start = 0; start < totalFrames; start += block) {
        int frames = static_cast<int>(std::min<long long>(block, totalFrames - start));
        if (stopFrame >= start && stopFrame < start + frames) {
            Transport stop;
            stop.playing = false;
            stop.bpm = bpm;
            stop.beat = static_cast<double>(stopFrame) * bpm / (60.0 * kRate);
            e.transport(stop, static_cast<int>(stopFrame - start));
        }
        out.clear();
        e.process(frames, nullptr, nullptr, out);
        collect(out, start, events);
    }
    return events;
}

// Every note-on is closed by a note-off; no pitch is started twice while it sounds.
void checkBalanced(const std::vector<Ev>& events, bool mustEndSilent) {
    bool on[128] = {};
    for (const Ev& ev : events) {
        if ((ev.b0 & 0xF0) == 0x90 && ev.b2 > 0) {
            CHECK(!on[ev.b1]);
            CHECK(ev.b2 >= 1 && ev.b2 <= 127);
            on[ev.b1] = true;
        } else if ((ev.b0 & 0xF0) == 0x80) {
            CHECK(on[ev.b1]);
            on[ev.b1] = false;
        }
    }
    if (mustEndSilent) {
        for (bool b : on) CHECK(!b);
    }
}

void sameOutputForEveryBlockSize() {
    const long long total = static_cast<long long>(kRate * 8);   // 4 bars at 120 BPM
    Engine reference(kRate, 42);
    reference.setParam(kDrift, 0.8);
    std::vector<Ev> expected = render(reference, total, 4096);
    CHECK(!expected.empty());
    for (int block : {1, 64, 128, 512, 4096, 1000}) {
        Engine e(kRate, 42);
        e.setParam(kDrift, 0.8);
        std::vector<Ev> got = render(e, total, block);
        CHECK_EQ(got.size(), expected.size());
        CHECK(got == expected);
    }
}

void chordsChangeOnTheirBorders() {
    Engine e(kRate, 7);
    e.setParam(kLoop, 8);
    e.setParam(kChordMin, 1);
    e.setParam(kChordMax, 3);
    const int count = e.chordCount();
    CHECK(count >= 3 && count <= 8);
    double sum = 0;
    for (int i = 0; i < count; ++i) sum += e.chordLength(i);
    CHECK_EQ(sum, 8.0);

    std::vector<Ev> events = render(e, static_cast<long long>(kRate * 4), 256);   // 8 beats = one loop
    // Every event lands on a chord start, and the first ones on frame 0.
    CHECK(!events.empty() && events[0].frame == 0);
    for (const Ev& ev : events) {
        double beat = ev.frame * 2.0 / kRate;
        bool onBorder = false;
        for (int i = 0; i < count; ++i) onBorder = onBorder || std::fabs(e.chordStart(i) - beat) < 1e-6;
        CHECK(onBorder);
    }
    // The chord at each start sounds right after it.
    for (int i = 0; i < count; ++i) {
        long long frame = static_cast<long long>(std::llround(e.chordStart(i) * kRate / 2.0));
        int notes[kMaxNotes];
        int n = e.chordNotes(i, notes);
        bool on[128] = {};
        for (const Ev& ev : events) {
            if (ev.frame > frame) break;
            if ((ev.b0 & 0xF0) == 0x90) on[ev.b1] = true;
            if ((ev.b0 & 0xF0) == 0x80) on[ev.b1] = false;
        }
        int sounding = 0;
        for (bool b : on) sounding += b ? 1 : 0;
        CHECK_EQ(sounding, n);
        for (int k = 0; k < n; ++k) CHECK(on[notes[k]]);
    }
    checkBalanced(events, false);
}

void stopReleasesEverything() {
    Engine e(kRate, 3);
    const long long stopAt = 70001;
    std::vector<Ev> events = render(e, static_cast<long long>(kRate * 4), 128, 120, stopAt);
    checkBalanced(events, true);
    bool sawOffAtStop = false;
    for (const Ev& ev : events) {
        CHECK(ev.frame <= stopAt);
        sawOffAtStop = sawOffAtStop || (ev.frame == stopAt && (ev.b0 & 0xF0) == 0x80);
    }
    CHECK(sawOffAtStop);
}

void allNotesOffCc() {
    Engine e(kRate, 3);
    OutBuffer out;
    Transport play;
    play.playing = true;
    e.transport(play, 0);
    e.process(256, nullptr, nullptr, out);
    CHECK(out.size() > 0);
    out.clear();
    e.midi(10, 0xB0, 123, 0);
    e.process(256, nullptr, nullptr, out);
    CHECK(out.size() > 0);
    for (int i = 0; i < out.size(); ++i) {
        CHECK_EQ(out[i].frame, 10);
        CHECK_EQ(out[i].b0 & 0xF0, 0x80);
    }
}

void seedsReplay() {
    Engine a(kRate, 11);
    a.setParam(kChordMax, 4);
    a.setParam(kShuffle, 1);
    a.setParam(kShuffle, 0);
    a.setParam(kNotesCount, 4);

    Engine b(kRate, 999);
    for (int i = 0; i < kParamCount; ++i) b.setParam(i, a.param(i));
    for (int s = 0; s < kSeedCount; ++s) b.setSeed(s, a.seed(s));

    CHECK_EQ(a.chordCount(), b.chordCount());
    for (int i = 0; i < a.chordCount(); ++i) {
        CHECK_EQ(a.chordStart(i), b.chordStart(i));
        int na[kMaxNotes], nb[kMaxNotes];
        int ca = a.chordNotes(i, na), cb = b.chordNotes(i, nb);
        CHECK_EQ(ca, cb);
        for (int k = 0; k < ca && k < cb; ++k) CHECK_EQ(na[k], nb[k]);
    }
}

void regenerationFollowsDependencies() {
    Engine e(kRate, 5);
    std::uint32_t before[kSeedCount];
    for (int s = 0; s < kSeedCount; ++s) before[s] = e.seed(s);

    // Root, scale, density, octaves: no sequence changes.
    e.setParam(kRoot, 5);
    e.setParam(kScale, 3);
    e.setParam(kDensity, 0.1);
    e.setParam(kOctaveMin, 1);
    e.setParam(kDrift, 0.5);
    for (int s = 0; s < kSeedCount; ++s) CHECK_EQ(e.seed(s), before[s]);

    // Notes count: chords and octaves only.
    e.setParam(kNotesCount, 5);
    CHECK_EQ(e.seed(kSeedTimes), before[kSeedTimes]);
    CHECK_EQ(e.seed(kSeedDrift), before[kSeedDrift]);
    CHECK_EQ(e.seed(kSeedNotes), before[kSeedNotes]);
    CHECK(e.seed(kSeedChords) != before[kSeedChords]);
    CHECK(e.seed(kSeedOctaves) != before[kSeedOctaves]);

    // The same value again changes nothing.
    std::uint32_t chords = e.seed(kSeedChords);
    e.setParam(kNotesCount, 5);
    CHECK_EQ(e.seed(kSeedChords), chords);

    // Loop: everything.
    for (int s = 0; s < kSeedCount; ++s) before[s] = e.seed(s);
    e.setParam(kLoop, 16);
    for (int s = 0; s < kSeedCount; ++s) CHECK(e.seed(s) != before[s]);

    // Shuffle fires once per rise and reads back 0.
    for (int s = 0; s < kSeedCount; ++s) before[s] = e.seed(s);
    e.setParam(kShuffle, 1);
    std::uint32_t afterFirst = e.seed(kSeedTimes);
    e.setParam(kShuffle, 1);
    CHECK(afterFirst != before[kSeedTimes]);
    CHECK_EQ(e.seed(kSeedTimes), afterFirst);
    CHECK_EQ(e.param(kShuffle), 0.0);
}

void paramsClampAndRound() {
    Engine e(kRate, 1);
    e.setParam(kLoop, 100);
    CHECK_EQ(e.param(kLoop), 32.0);
    e.setParam(kNotesCount, 2.6);
    CHECK_EQ(e.param(kNotesCount), 3.0);
    e.setParam(kDensity, -3);
    CHECK_EQ(e.param(kDensity), 0.0);
    e.setParam(kParamCount, 1);   // ignored
    e.setParam(-1, 1);            // ignored
}

void octaveMinNeverAboveMax() {
    Engine e(kRate, 1);
    e.setParam(kOctaveMin, 2);
    e.setParam(kOctaveMax, 3);
    e.setParam(kOctaveMin, 5);   // pushes max up
    CHECK_EQ(e.param(kOctaveMin), 5.0);
    CHECK_EQ(e.param(kOctaveMax), 5.0);
    e.setParam(kOctaveMax, 1);   // pushes min down
    CHECK_EQ(e.param(kOctaveMin), 1.0);
    CHECK_EQ(e.param(kOctaveMax), 1.0);
    e.setParam(kOctaveMax, 6);   // no push needed
    CHECK_EQ(e.param(kOctaveMin), 1.0);
    e.setParam(kOctaveMin, -3);  // clamps to the slider range
    CHECK_EQ(e.param(kOctaveMin), 0.0);
    e.setParam(kDrift, -0.5);    // drift slider is 0..1
    CHECK_EQ(e.param(kDrift), 0.0);
}

void tempoChangeFollowsTheGrid() {
    Engine e(kRate, 21);
    OutBuffer out;
    Transport t;
    t.playing = true;
    t.bpm = 120;
    e.transport(t, 0);
    e.process(24000, nullptr, nullptr, out);   // beat 0 .. 1
    // Tempo jumps to 60 at beat 1.
    t.bpm = 60;
    t.beat = 1;
    e.transport(t, 0);
    std::vector<Ev> events;
    for (long long start = 24000; start < 24000 + 48000 * 8; start += 512) {
        out.clear();
        e.process(512, nullptr, nullptr, out);
        collect(out, start, events);
    }
    for (const Ev& ev : events) {
        double beat = 1 + (ev.frame - 24000) / 48000.0;   // 1 beat per second now
        double q = beat * kQuantaPerBeat;
        CHECK(std::fabs(q - std::round(q)) * 48000.0 / kQuantaPerBeat < 1.0);   // within one frame of the 1/4 grid
    }
}

} // namespace

void engineTests() {
    sameOutputForEveryBlockSize();
    chordsChangeOnTheirBorders();
    stopReleasesEverything();
    allNotesOffCc();
    seedsReplay();
    regenerationFollowsDependencies();
    paramsClampAndRound();
    octaveMinNeverAboveMax();
    tempoChangeFollowsTheGrid();
}
