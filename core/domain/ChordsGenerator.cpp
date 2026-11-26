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

GapRange chordGapRange(double density, int scaleSize, const ChordRules& r) {
    if (!(density >= 0)) density = 0;
    if (density > 1) density = 1;
    const double s = scaleSize / 7.0;
    const double minRef = r.gapMinDense + (1.0 - density) * (r.gapMinOpen - r.gapMinDense);
    int lo = static_cast<int>(std::lround(minRef * s));
    if (lo < 1) lo = 1;
    int hi = static_cast<int>(std::lround(r.gapMax * s));
    if (hi < lo) hi = lo;
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

    // Note `depth` at scale index k still leaves room for the notes above it at their smallest gaps
    // (pitch grows with the index, so this is exact up to pitch-class repeats).
    bool roomAbove(int depth, int k, int bottom) const {
        const int top = pitchOf(k + (count - 1 - depth) * gaps.min);
        return top <= high && (maxSpan <= 0 || top - bottom <= maxSpan);
    }
};

bool ChordsGenerator::makeCandidate(const Enumeration& e, Candidate& c) const {
    c.count = e.count;
    c.mask = 0;
    for (int i = 0; i < e.count; ++i) {
        c.notes[i] = e.pitchOf(e.index[i]);
        const int bit = 1 << mod12(c.notes[i]);
        if ((c.mask & bit) && !e.allowDoubling) return false;
        c.mask |= bit;
    }
    c.bassPc = mod12(c.notes[0]);
    c.rootDegree = e.index[0] % e.scale->size;
    return true;
}

bool ChordsGenerator::addUnique(const Candidate& c) {
    std::uint32_t h = 2166136261u;   // FNV-1a over the notes
    for (int n = 0; n < c.count; ++n) h = (h ^ static_cast<std::uint32_t>(c.notes[n])) * 16777619u;
    for (int slot = static_cast<int>(h % kHashSlots);; slot = (slot + 1) % kHashSlots) {
        ++work_;
        const int i = hash_[slot] - 1;
        if (i < 0) break;
        const Candidate& p = pool_[i];
        bool same = true;
        for (int n = 0; n < c.count && same; ++n) same = p.notes[n] == c.notes[n];
        if (same) return false;
    }
    int slot = static_cast<int>(h % kHashSlots);
    while (hash_[slot] != 0) slot = (slot + 1) % kHashSlots;
    pool_[poolCount_] = c;
    hash_[slot] = static_cast<std::int16_t>(++poolCount_);
    return true;
}

// Every voicing, reservoir-sampled (algorithm R) down to kMaxChordCandidates.
// Stops once it has taken kEnumerateBudget steps (initialize then samples instead).
// `mask`: pitch classes of the notes so far (a note repeating one is skipped unless doubling is allowed).
void ChordsGenerator::enumerate(Enumeration& e, int depth, int k, int mask) {
    ++work_;
    if (++steps_ > kEnumerateBudget) return;
    e.index[depth] = k;
    mask |= 1 << mod12(e.pitchOf(k));
    if (depth < e.count - 1) {
        const int bottom = e.pitchOf(e.index[0]);
        for (int g = e.gaps.min; g <= e.gaps.max; ++g) {
            ++work_;
            if (!e.roomAbove(depth + 1, k + g, bottom)) break;
            if (!e.allowDoubling && (mask & (1 << mod12(e.pitchOf(k + g))))) continue;
            enumerate(e, depth + 1, k + g, mask);
        }
        return;
    }
    Candidate c;
    if (!makeCandidate(e, c)) return;
    ++seen_;
    if (seen_ <= kMaxChordCandidates) {
        pool_[poolCount_++] = c;
        return;
    }
    const std::uint64_t j = poolRng_.next() % static_cast<std::uint64_t>(seen_);
    if (j < static_cast<std::uint64_t>(kMaxChordCandidates)) pool_[j] = c;
}

// Big grids: random voicings, built note by note from the gaps that still fit; duplicates skipped.
// Bounded work (kSampleTries) whatever the scale and note count.
void ChordsGenerator::sample(Enumeration& e, int lowIndex, int rootCount) {
    for (int t = 0; t < kSampleTries && poolCount_ < kMaxChordCandidates; ++t) {
        ++work_;
        e.index[0] = lowIndex + static_cast<int>(poolRng_.next() % static_cast<std::uint64_t>(rootCount));
        const int bottom = e.pitchOf(e.index[0]);
        int mask = 1 << mod12(bottom);
        bool ok = true;
        for (int d = 1; d < e.count && ok; ++d) {
            // The gaps that keep the note in range and span and its pitch class new; pick one.
            int fits[24];
            int n = 0;
            for (int g = e.gaps.min; g <= e.gaps.max && n < 24; ++g) {
                ++work_;
                if (!e.roomAbove(d, e.index[d - 1] + g, bottom)) break;
                if (!e.allowDoubling && (mask & (1 << mod12(e.pitchOf(e.index[d - 1] + g))))) continue;
                fits[n++] = g;
            }
            if (n == 0) {
                ok = false;
                break;
            }
            e.index[d] = e.index[d - 1] + fits[poolRng_.next() % static_cast<std::uint64_t>(n)];
            mask |= 1 << mod12(e.pitchOf(e.index[d]));
        }
        Candidate c;
        if (ok && makeCandidate(e, c)) addUnique(c);
    }
}

bool ChordsGenerator::initialize(const ChordsSettings& settings) {
    poolCount_ = 0;
    work_ = 0;
    settings_ = settings;
    ChordsSettings& s = settings_;
    if (!validScale(s.scale)) return false;
    s.root = mod12(s.root);
    if (s.notesPerChord < 1) s.notesPerChord = 1;
    if (s.notesPerChord > kMaxNotes) s.notesPerChord = kMaxNotes;
    if (s.lowNote < 0) s.lowNote = 0;
    if (s.highNote > 127) s.highNote = 127;
    if (s.highNote < s.lowNote) return false;

    Enumeration e;
    e.scale = &s.scale;
    e.basePitch = s.lowNote - mod12(s.lowNote - s.root);
    e.high = s.highNote;
    e.gaps = chordGapRange(s.density, s.scale.size, s.rules);
    e.maxSpan = s.rules.maxSpanSemitones;

    // Scale indices of the possible lowest notes: [lowIndex, lowIndex + rootCount).
    int lowIndex = 0;
    while (e.pitchOf(lowIndex) < s.lowNote) {
        ++lowIndex;
        ++work_;
    }
    if (e.pitchOf(lowIndex) > e.high) return false;   // no scale note in the range

    // Range and density hold; the note count gives way (e.g. open voicings in one octave).
    for (int count = s.notesPerChord; count >= 1 && poolCount_ == 0; --count) {
        e.count = count;
        e.allowDoubling = count > s.scale.size;
        seen_ = 0;
        steps_ = 0;
        poolRng_ = Rng(kPoolSeed);
        // Lowest notes that leave room for the whole chord: [lowIndex, lowIndex + rootCount).
        int rootCount = 0;
        while (e.roomAbove(0, lowIndex + rootCount, e.pitchOf(lowIndex + rootCount))) {
            ++rootCount;
            ++work_;
        }
        if (rootCount == 0) continue;
        for (int k = lowIndex; k < lowIndex + rootCount; ++k) enumerate(e, 0, k, 0);
        if (steps_ > kEnumerateBudget) {
            poolCount_ = 0;
            for (std::int16_t& h : hash_) h = 0;
            work_ += kHashSlots;
            sample(e, lowIndex, rootCount);
        }
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
        work_ += poolCount_;
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
        work_ += 2 * poolCount_;
    }
    double r = rng_.unit() * total;
    int chosen = -1;
    for (int i = 0; i < poolCount_; ++i) {
        ++work_;
        if (weights_[i] <= 0) continue;
        chosen = i;   // rounding: falls back to the last positive entry
        r -= weights_[i];
        if (r < 0) break;
    }
    return chosen;
}

Chord ChordsGenerator::getNext(bool last) {
    Chord out;
    work_ = 0;
    if (poolCount_ == 0) return out;

    bool eligible[kMaxChordCandidates];
    int count = 0;
    const int tonicBit = 1 << settings_.root;
    if (last) {
        for (int i = 0; i < poolCount_; ++i) count += eligible[i] = pool_[i].rootDegree == 0;
        work_ += poolCount_;
        if (count == 0) {
            for (int i = 0; i < poolCount_; ++i) count += eligible[i] = (pool_[i].mask & tonicBit) != 0;
            work_ += poolCount_;
        }
    }
    if (count == 0) {
        for (int i = 0; i < poolCount_; ++i) eligible[i] = true;
        work_ += poolCount_;
    }

    const int i = pick(eligible);
    if (i < 0) return out;
    previous_ = pool_[i];
    hasPrevious_ = true;
    out.count = previous_.count;
    for (int n = 0; n < out.count; ++n) out.notes[n] = previous_.notes[n];
    return out;
}

} // namespace bc
