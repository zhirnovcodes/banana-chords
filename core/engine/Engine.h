#pragma once
#include <cstdint>

#include "Harmony.h"
#include "Rng.h"
#include "Sequences.h"

namespace bc {

inline constexpr std::uint8_t kVelocity = 100;

// Index = position in params.json. Append only.
enum Param : int {
    kRoot = 0,
    kScale,
    kLoop,
    kChordMin,
    kChordMax,
    kOctaveMin,
    kOctaveMax,
    kShuffle,
    kDrift,
    kDensity,
    kNotesCount,
    kParamCount
};

// The random sequences. Each has its own seed, so a saved state replays exactly.
// The time table (step 3) is built from times + drift and has no seed.
enum SeqSeed : int { kSeedTimes = 0, kSeedDrift, kSeedNotes, kSeedChords, kSeedOctaves, kSeedCount };

struct Transport {
    bool playing = false;
    double bpm = 120;
    double beat = 0;
    int numerator = 4;
    int denominator = 4;
};

struct OutEvent {
    int frame;
    enum Kind : std::uint8_t { Midi, Control } kind;
    std::uint8_t b0, b1, b2;
    int slot;
    float value;
};

class OutBuffer {
public:
    static constexpr int kCapacity = 512;
    bool push(const OutEvent& e) {
        if (size_ >= kCapacity) return false;
        events_[size_++] = e;
        return true;
    }
    int size() const { return size_; }
    const OutEvent& operator[](int i) const { return events_[i]; }
    void clear() { size_ = 0; }

private:
    OutEvent events_[kCapacity];
    int size_ = 0;
};

class Engine {
public:
    Engine(double sampleRate, std::uint32_t seed);

    // value = plain (min..max). Out of range index: ignored.
    void setParam(int index, double value);
    double param(int index) const;

    // Apply at `frame` of the next process() call. Call in frame order.
    void transport(const Transport& t, int frame);
    void midi(int frame, std::uint8_t status, std::uint8_t d1, std::uint8_t d2);

    void process(int frames, float* left, float* right, OutBuffer& out);

    // Note-offs for every sounding note at `frame`, queued events dropped.
    void allOff(int frame, OutBuffer& out);

    // Sequence seeds, for state. setSeed rebuilds that sequence (and the ones built from it) from the seed.
    std::uint32_t seed(int which) const;
    void setSeed(int which, std::uint32_t value);

    // Read-only view for UIs and tests. Rebuilds pending sequences first.
    int chordCount();
    double chordStart(int chord);      // beats, after drift
    double chordLength(int chord);     // beats, after drift
    int chordNotes(int chord, int out[kMaxNotes]);
    int currentChord() const { return currentChord_; }   // -1 when nothing plays

private:
    enum Dirty : unsigned { kDirtyTimes = 1, kDirtyDrift = 2, kDirtyTable = 4, kDirtyNotes = 8,
                            kDirtyChords = 16, kDirtyOctaves = 32 };

    struct Pending {
        int frame;
        bool isTransport;
        Transport t;
        std::uint8_t status, d1, d2;
    };
    static constexpr int kMaxPending = 64;

    void reseed(unsigned dirtyMask);
    void rebuild();
    HarmonyParams harmony() const;
    int intParam(int index) const;

    double beatAtAbs(long long absFrame) const;
    long long absFrameOfBeat(double beat) const;
    void renderSegment(int from, int to, OutBuffer& out);
    void evaluate(int frame, long long quanta, OutBuffer& out);
    void applyPending(const Pending& p, OutBuffer& out);
    void releaseAll(int frame, OutBuffer& out);

    double sampleRate_;
    Rng master_;
    double params_[kParamCount];
    std::uint32_t seeds_[kSeedCount];
    unsigned dirty_ = 0;

    Times times_;
    Drift drift_;
    TimeTable table_;
    Notes notes_;
    Chords chords_;
    Octaves octaves_;

    // Transport: beat at absolute frame f = anchorBeat_ + (f - anchorFrame_) * bpm / (60 * rate).
    // Absolute frames make the timing identical for every block size.
    bool playing_ = false;
    double bpm_ = 120;
    double anchorBeat_ = 0;
    long long anchorFrame_ = 0;
    long long blockStart_ = 0;     // absolute frame of the current block's frame 0

    Pending pending_[kMaxPending];
    int pendingCount_ = 0;

    bool sounding_[128] = {};
    int currentChord_ = -1;
    bool shuffleHigh_ = false;
};

} // namespace bc
