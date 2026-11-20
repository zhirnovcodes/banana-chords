#include "Engine.h"

#include <cmath>

#include "ParamsJson.h"   // written by CMake from params.json

namespace bc {

static_assert(sizeof(kParams) / sizeof(kParams[0]) == kParamCount, "params.json and enum Param differ in length");

namespace {

bool rounds(const ParamSpec& p) { return p.kind[0] == 'i' || p.kind[0] == 'c' || p.kind[0] == 'b'; }

constexpr double kEps = 1e-7;

} // namespace

Engine::Engine(double sampleRate, std::uint32_t seed)
    : sampleRate_(sampleRate > 0 ? sampleRate : 48000), master_(seed) {
    for (int i = 0; i < kParamCount; ++i) params_[i] = kParams[i].def;
    reseed(kDirtyTimes | kDirtyDrift | kDirtyNotes | kDirtyChords | kDirtyOctaves);
    rebuild();
}

int Engine::intParam(int index) const { return static_cast<int>(std::lround(params_[index])); }

double Engine::param(int index) const {
    if (index < 0 || index >= kParamCount) return 0;
    return params_[index];
}

void Engine::setParam(int index, double value) {
    if (index < 0 || index >= kParamCount || !std::isfinite(value)) return;
    const ParamSpec& spec = kParams[index];

    if (index == kShuffle) {
        // Trigger: fires once per rise past 0.5, reads back 0.
        bool high = value > 0.5;
        if (high && !shuffleHigh_) reseed(kDirtyTimes | kDirtyDrift | kDirtyNotes | kDirtyChords | kDirtyOctaves);
        shuffleHigh_ = high;
        return;
    }

    if (value < spec.min) value = spec.min;
    if (value > spec.max) value = spec.max;
    if (rounds(spec)) value = std::round(value);
    const double old = params_[index];
    params_[index] = value;
    if (old == value) return;

    switch (index) {
    case kLoop:
    case kChordMin:
    case kChordMax:
        reseed(kDirtyTimes | kDirtyDrift | kDirtyNotes | kDirtyChords | kDirtyOctaves);
        break;
    case kDrift:
        dirty_ |= kDirtyTable;
        break;
    // Min <= max: moving one past the other takes the other along.
    case kOctaveMin:
        if (params_[kOctaveMax] < value) params_[kOctaveMax] = value;
        break;
    case kOctaveMax:
        if (params_[kOctaveMin] > value) params_[kOctaveMin] = value;
        break;
    case kNotesCount:
        reseed(kDirtyChords | kDirtyOctaves);
        break;
    default:
        break;   // root, scale, octaves, density: read when a chord is played
    }
}

void Engine::reseed(unsigned mask) {
    if (mask & kDirtyTimes) seeds_[kSeedTimes] = master_.next32();
    if (mask & kDirtyDrift) seeds_[kSeedDrift] = master_.next32();
    if (mask & kDirtyNotes) seeds_[kSeedNotes] = master_.next32();
    if (mask & kDirtyChords) seeds_[kSeedChords] = master_.next32();
    if (mask & kDirtyOctaves) seeds_[kSeedOctaves] = master_.next32();
    dirty_ |= mask;
}

std::uint32_t Engine::seed(int which) const {
    return which >= 0 && which < kSeedCount ? seeds_[which] : 0;
}

void Engine::setSeed(int which, std::uint32_t value) {
    static constexpr unsigned kBits[kSeedCount] = {kDirtyTimes, kDirtyDrift, kDirtyNotes, kDirtyChords, kDirtyOctaves};
    if (which < 0 || which >= kSeedCount) return;
    seeds_[which] = value;
    dirty_ |= kBits[which];
}

void Engine::rebuild() {
    if (dirty_ == 0) return;
    // Times sets the chord count, so everything built on it follows, each from its own seed.
    if (dirty_ & kDirtyTimes) dirty_ |= kDirtyDrift | kDirtyNotes | kDirtyChords | kDirtyOctaves;
    if (dirty_ & kDirtyDrift) dirty_ |= kDirtyTable;

    const int notesCount = intParam(kNotesCount);
    if (dirty_ & kDirtyTimes) {
        Rng r(seeds_[kSeedTimes]);
        generateTimes(r, intParam(kLoop), intParam(kChordMin), intParam(kChordMax), times_);
    }
    if (dirty_ & kDirtyDrift) {
        Rng r(seeds_[kSeedDrift]);
        generateDrift(r, times_.count, drift_);
    }
    if (dirty_ & kDirtyTable) buildTimeTable(times_, drift_, params_[kDrift], table_);
    if (dirty_ & kDirtyNotes) {
        Rng r(seeds_[kSeedNotes]);
        generateNotes(r, times_.count, notes_);
    }
    if (dirty_ & kDirtyChords) {
        Rng r(seeds_[kSeedChords]);
        generateChords(r, times_.count, notesCount, chords_);
    }
    if (dirty_ & kDirtyOctaves) {
        Rng r(seeds_[kSeedOctaves]);
        generateOctaves(r, times_.count, notesCount, octaves_);
    }
    dirty_ = 0;
}

HarmonyParams Engine::harmony() const {
    HarmonyParams h;
    h.root = intParam(kRoot);
    h.scale = intParam(kScale);
    h.density = params_[kDensity];
    h.notesCount = intParam(kNotesCount);
    h.octaveMin = intParam(kOctaveMin);
    h.octaveMax = intParam(kOctaveMax);
    return h;
}

// ---- view -------------------------------------------------------------------------------------

int Engine::chordCount() {
    rebuild();
    return times_.count;
}

double Engine::chordStart(int chord) {
    rebuild();
    if (chord < 0 || chord >= times_.count) return 0;
    return table_.border[chord] / static_cast<double>(kQuantaPerBeat);
}

double Engine::chordLength(int chord) {
    rebuild();
    if (chord < 0 || chord >= times_.count) return 0;
    return (table_.border[chord + 1] - table_.border[chord]) / static_cast<double>(kQuantaPerBeat);
}

int Engine::chordNotes(int chord, int out[kMaxNotes]) {
    rebuild();
    if (chord < 0 || chord >= times_.count) return 0;
    return resolveChord(notes_, chords_, octaves_, chord, harmony(), out);
}

// ---- time -------------------------------------------------------------------------------------

double Engine::beatAtAbs(long long absFrame) const {
    return anchorBeat_ + static_cast<double>(absFrame - anchorFrame_) * bpm_ / (60.0 * sampleRate_);
}

long long Engine::absFrameOfBeat(double beat) const {
    double frames = (beat - anchorBeat_) * 60.0 * sampleRate_ / bpm_;
    return anchorFrame_ + static_cast<long long>(std::ceil(frames - kEps));
}

void Engine::transport(const Transport& t, int frame) {
    if (pendingCount_ >= kMaxPending) return;
    Pending& p = pending_[pendingCount_++];
    p.frame = frame;
    p.isTransport = true;
    p.t = t;
}

void Engine::midi(int frame, std::uint8_t status, std::uint8_t d1, std::uint8_t d2) {
    if (pendingCount_ >= kMaxPending) return;
    Pending& p = pending_[pendingCount_++];
    p.frame = frame;
    p.isTransport = false;
    p.status = status;
    p.d1 = d1;
    p.d2 = d2;
}

void Engine::process(int frames, float* left, float* right, OutBuffer& out) {
    (void)left;
    (void)right;
    if (frames < 0) frames = 0;
    rebuild();
    int cursor = 0;
    for (int i = 0; i < pendingCount_; ++i) {
        int f = pending_[i].frame;
        if (f < cursor) f = cursor;
        if (f > frames) f = frames;
        renderSegment(cursor, f, out);
        pending_[i].frame = f;
        applyPending(pending_[i], out);
        cursor = f;
    }
    pendingCount_ = 0;
    renderSegment(cursor, frames, out);
    blockStart_ += frames;
}

void Engine::applyPending(const Pending& p, OutBuffer& out) {
    if (!p.isTransport) {
        const bool cc = (p.status & 0xF0) == 0xB0;
        if (cc && (p.d1 == 120 || p.d1 == 123)) releaseAll(p.frame, out);
        return;
    }
    const Transport& t = p.t;
    if (t.bpm > 0 && std::isfinite(t.bpm)) bpm_ = t.bpm;
    anchorFrame_ = blockStart_ + p.frame;
    anchorBeat_ = std::isfinite(t.beat) ? t.beat : 0;
    if (!t.playing) {
        playing_ = false;
        releaseAll(p.frame, out);
        return;
    }
    playing_ = true;
    // Start, jump or tempo change: play the chord under the playhead now, then follow the grid.
    evaluate(p.frame, static_cast<long long>(std::floor(anchorBeat_ * kQuantaPerBeat + kEps)), out);
}

// Chords are checked on every 1/4 beat. That grid is the shortest time between two note changes,
// whatever the parameters do, and it holds every chord border (drift moves borders on the same grid).
void Engine::renderSegment(int from, int to, OutBuffer& out) {
    if (!playing_ || to <= from) return;
    const long long absFrom = blockStart_ + from;
    const long long absTo = blockStart_ + to;
    long long k = static_cast<long long>(std::floor(beatAtAbs(absFrom) * kQuantaPerBeat)) - 1;
    for (;; ++k) {
        long long f = absFrameOfBeat(static_cast<double>(k) / kQuantaPerBeat);
        if (f >= absTo) break;
        if (f >= absFrom) evaluate(static_cast<int>(f - blockStart_), k, out);
    }
}

void Engine::evaluate(int frame, long long quanta, OutBuffer& out) {
    rebuild();
    const int chord = chordAt(table_, quanta);
    int notes[kMaxNotes];
    const int n = resolveChord(notes_, chords_, octaves_, chord, harmony(), notes);
    bool wanted[128] = {};
    for (int i = 0; i < n; ++i) wanted[notes[i]] = true;

    // A note in both chords keeps sounding. Others stop first, then the new ones start.
    for (int pitch = 0; pitch < 128; ++pitch) {
        if (sounding_[pitch] && !wanted[pitch]) {
            out.push({frame, OutEvent::Midi, 0x80, static_cast<std::uint8_t>(pitch), 0, 0, 0});
            sounding_[pitch] = false;
        }
    }
    for (int i = 0; i < n; ++i) {
        const int pitch = notes[i];
        if (!sounding_[pitch]) {
            out.push({frame, OutEvent::Midi, 0x90, static_cast<std::uint8_t>(pitch), kVelocity, 0, 0});
            sounding_[pitch] = true;
        }
    }
    currentChord_ = chord;
}

void Engine::releaseAll(int frame, OutBuffer& out) {
    for (int pitch = 0; pitch < 128; ++pitch) {
        if (sounding_[pitch]) {
            out.push({frame, OutEvent::Midi, 0x80, static_cast<std::uint8_t>(pitch), 0, 0, 0});
            sounding_[pitch] = false;
        }
    }
    currentChord_ = -1;
}

void Engine::allOff(int frame, OutBuffer& out) {
    pendingCount_ = 0;
    releaseAll(frame, out);
}

} // namespace bc
