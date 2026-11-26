#include "ChordsGenerator.h"

#include <cmath>

namespace bc {

namespace {

// The pool sample depends only on the settings, never on the generate() seed.
constexpr std::uint64_t kPoolSeed = 0x42414E414E41ull;

int mod12(int x) { return ((x % 12) + 12) % 12; }

int popcount(int m) {
    int c = 0;
    for (; m; m &= m - 1) ++c;
    return c;
}

bool validScale(const Scale& s) {
    if (s.size < 2 || s.size > 12 || s.steps[0] != 0) return false;
    for (int i = 1; i < s.size; ++i)
        if (s.steps[i] <= s.steps[i - 1] || s.steps[i] >= 12) return false;
    return true;
}

} // namespace

GapRange chordGapRange(double density, int scaleSize, const ChordRules& r, int widen) {
    if (!(density >= 0)) density = 0;
    if (density > 1) density = 1;
    const double s = scaleSize / 7.0;
    const double minRef = r.gapMinDense + (1.0 - density) * (r.gapMinOpen - r.gapMinDense);
    int lo = static_cast<int>(std::lround(minRef * s));
    if (lo < 1) lo = 1;
    int hi = static_cast<int>(std::lround(r.gapMax * s));
    if (hi < lo) hi = lo;
    lo -= widen;
    if (lo < 1) lo = 1;
    hi += widen;
    return {lo, hi};
}

// ---------------------------------------------------------------------------------------------
// Candidates: every voicing = lowest note (scale index) + gaps in scale steps.

struct ChordsGenerator::Enumeration {
    const Scale* scale;
    int basePitch;   // pitch of scale index 0: the tonic at or below lowNote
    int high;
    int count;
    GapRange gaps;
    int maxSpan;     // semitones, 0 = no limit
    bool allowDoubling;
    int index[kMaxNotes];

    int pitchOf(int k) const { return basePitch + 12 * (k / scale->size) + scale->steps[k % scale->size]; }
};

void ChordsGenerator::enumerate(Enumeration& e, int depth, int k) {
    e.index[depth] = k;
    if (depth < e.count - 1) {
        const int bottom = e.pitchOf(e.index[0]);
        for (int g = e.gaps.min; g <= e.gaps.max; ++g) {
            const int pitch = e.pitchOf(k + g);
            if (pitch > e.high) break;
            if (e.maxSpan > 0 && pitch - bottom > e.maxSpan) break;
            enumerate(e, depth + 1, k + g);
        }
        return;
    }
    Candidate c;
    c.count = e.count;
    for (int i = 0; i < e.count; ++i) {
        c.notes[i] = e.pitchOf(e.index[i]);
        const int bit = 1 << mod12(c.notes[i]);
        if ((c.mask & bit) && !e.allowDoubling) return;
        c.mask |= bit;
    }
    c.bassPc = mod12(c.notes[0]);
    c.rootDegree = e.index[0] % e.scale->size;

    // Reservoir sampling (algorithm R): a uniform sample of at most kMaxChordCandidates voicings.
    ++seen_;
    if (seen_ <= kMaxChordCandidates) {
        pool_[poolCount_++] = c;
        return;
    }
    const std::uint64_t j = poolRng_.next() % static_cast<std::uint64_t>(seen_);
    if (j < static_cast<std::uint64_t>(kMaxChordCandidates)) pool_[j] = c;
}

bool ChordsGenerator::initialize(const ChordsSettings& settings) {
    poolCount_ = 0;
    settings_ = settings;
    ChordsSettings& s = settings_;
    if (!validScale(s.scale)) return false;
    s.root = mod12(s.root);
    if (s.notesPerChord < 2) s.notesPerChord = 2;
    if (s.notesPerChord > kMaxNotes) s.notesPerChord = kMaxNotes;
    if (s.lowNote < 0) s.lowNote = 0;
    if (s.highNote > 127) s.highNote = 127;
    if (s.highNote < s.lowNote) return false;

    Enumeration e;
    e.scale = &s.scale;
    e.basePitch = s.lowNote - mod12(s.lowNote - s.root);
    e.high = s.highNote;
    e.count = s.notesPerChord;
    e.allowDoubling = s.notesPerChord > s.scale.size;

    // Nothing fits the density (e.g. 6 open-voiced notes in 2 octaves): widen gaps, drop the span limit.
    for (int widen = 0; widen <= 1 && poolCount_ == 0; ++widen) {
        e.gaps = chordGapRange(s.density, s.scale.size, s.rules, widen);
        e.maxSpan = widen > 0 ? 0 : s.rules.maxSpanSemitones;
        seen_ = 0;
        poolRng_ = Rng(kPoolSeed);
        for (int k = 0; e.pitchOf(k) <= e.high; ++k)
            if (e.pitchOf(k) >= s.lowNote) enumerate(e, 0, k);
    }
    return poolCount_ > 0;
}

// ---------------------------------------------------------------------------------------------
// Transition score A → B (scale-agnostic: semitones and pitch classes only).

double ChordsGenerator::score(const Candidate& a, const Candidate& b) const {
    const ChordRules& r = settings_.rules;
    const int d = mod12(b.bassPc - a.bassPc);
    const int ic = d > 6 ? 12 - d : d;
    double s = 0;
    switch (ic) {
    case 0: s = r.rootSame; break;
    case 1:
    case 2: s = r.rootStep; break;
    case 3:
    case 4: s = r.rootThird; break;
    case 5: s = r.rootFifth; break;
    default: s = r.rootTritone; break;
    }
    if (a.mask == b.mask)
        s += r.identicalChord;
    else
        s += r.commonTone * popcount(a.mask & b.mask);
    // Both voicings ascending: note i moves to note i (counts differ only after a re-initialize).
    const int n = a.count < b.count ? a.count : b.count;
    int movement = 0;
    for (int i = 0; i < n; ++i) movement += a.notes[i] > b.notes[i] ? a.notes[i] - b.notes[i] : b.notes[i] - a.notes[i];
    s += r.voiceLeading * movement;
    return s;
}

// ---------------------------------------------------------------------------------------------
// Sequence

void ChordsGenerator::generate(std::uint64_t seed) {
    rng_ = Rng(seed);
    hasPrevious_ = false;
}

int ChordsGenerator::pick(const bool* eligible) {
    const double t = settings_.randomness < 0.05 ? 0.05 : settings_.randomness;
    double total = 0;
    if (!hasPrevious_) {
        for (int i = 0; i < poolCount_; ++i) total += weights_[i] = eligible[i] ? 1.0 : 0.0;
    } else {
        // w = exp(score / T), shifted by the best score so nothing overflows. Candidates under the
        // hard threshold are out, unless that leaves none (then every eligible one is in).
        double best = 0;
        bool anyAllowed = false, any = false;
        for (int i = 0; i < poolCount_; ++i) {
            if (!eligible[i]) continue;
            const double s = score(previous_, pool_[i]);
            weights_[i] = s;
            const bool allowed = s >= settings_.rules.hardThreshold;
            if (!any || s > best) best = s;
            any = true;
            anyAllowed = anyAllowed || allowed;
        }
        for (int i = 0; i < poolCount_; ++i) {
            const bool in = eligible[i] && (!anyAllowed || weights_[i] >= settings_.rules.hardThreshold);
            weights_[i] = in ? std::exp((weights_[i] - best) / t) : 0.0;
            total += weights_[i];
        }
    }
    double r = rng_.unit() * total;
    int chosen = -1;
    for (int i = 0; i < poolCount_; ++i) {
        if (weights_[i] <= 0) continue;
        chosen = i;   // rounding: falls back to the last positive entry
        r -= weights_[i];
        if (r < 0) break;
    }
    return chosen;
}

Chord ChordsGenerator::getNext(bool last) {
    Chord out;
    if (poolCount_ == 0) return out;

    bool eligible[kMaxChordCandidates];
    int count = 0;
    const int tonicBit = 1 << settings_.root;
    if (last) {
        for (int i = 0; i < poolCount_; ++i) count += eligible[i] = pool_[i].rootDegree == 0;
        if (count == 0)
            for (int i = 0; i < poolCount_; ++i) count += eligible[i] = (pool_[i].mask & tonicBit) != 0;
    }
    if (count == 0)
        for (int i = 0; i < poolCount_; ++i) eligible[i] = true;

    const int i = pick(eligible);
    if (i < 0) return out;
    previous_ = pool_[i];
    hasPrevious_ = true;
    out.count = previous_.count;
    for (int n = 0; n < out.count; ++n) out.notes[n] = previous_.notes[n];
    return out;
}

} // namespace bc
