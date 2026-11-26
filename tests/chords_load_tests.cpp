#include <algorithm>
#include <chrono>
#include <vector>

#include "Check.h"
#include "ChordsGenerator.h"

using namespace bc;

namespace {

// Work limits, in ChordsGenerator::lastWork() steps (machine-independent, so CI load cannot flake them).
// Worst measured: initialize 44084 (whole tone, 6 notes, density 1, octaves 0..6), getNext 1024.
// At ~15 ns per step (wasm, desktop) 65536 steps is ~1 ms, a third of a 128-sample block at 48 kHz.
constexpr long long kInitializeWorkLimit = 65536;
constexpr long long kGetNextWorkLimit = 6 * kMaxChordCandidates;   // at most 6 passes over the pool

// Plays a stretch of virtual time and measures the ChordsGenerator work the Engine does on the audio
// thread (Engine::rebuild runs inside process):
//   every `shuffleEveryBlocks` blocks: a new chord set (generate + getNext per chord), like Shuffle
//   every `initializeEveryMs`:         new settings first (initialize), like a knob being turned
class ChordsLoadSimulator {
public:
    struct Config {
        int sampleRate = 48000;
        int blockSize = 128;
        double seconds = 5.0;
        double initializeEveryMs = 100.0;
        int chordsPerSet = kMaxChords;
        int shuffleEveryBlocks = 1;   // 1 = a new set in every block (worse than any real use)
    };

    struct Report {
        int blocks = 0;
        double blockBudgetMs = 0;     // real time one audio block lasts
        double setP99Ms = 0;          // blocks with a chord set only
        double setMaxMs = 0;
        double initBlockMaxMs = 0;    // blocks with initialize + chord set: cost (fastest of 3 runs)
        double initBlockMeanMs = 0;
        int initializes = 0;
        int overruns = 0;             // blocks whose generator work alone took the whole budget
        int emptyChords = 0;
        long long maxBlockWork = 0;   // most lastWork() steps in one block
        long long totalWork = 0;
        double totalMs = 0;
    };

    explicit ChordsLoadSimulator(const std::vector<ChordsSettings>& settings) : settings_(settings) {}

    Report run(const Config& c) {
        using Clock = std::chrono::steady_clock;
        Report r;
        r.blockBudgetMs = 1000.0 * c.blockSize / c.sampleRate;
        r.blocks = static_cast<int>(c.seconds * c.sampleRate / c.blockSize);
        std::vector<double> sets;
        sets.reserve(r.blocks);
        double initTotal = 0;
        double nextInitMs = 0;
        std::uint64_t seed = 1;
        Chord chords[kMaxChords];

        // Warm-up (a host does this while loading): the first wasm calls run unoptimized code.
        for (const ChordsSettings& s : settings_) {
            gen_.initialize(s);
            gen_.generate(0);
            for (int i = 0; i < c.chordsPerSet; ++i) gen_.getNext();
        }

        for (int block = 0; block < r.blocks; ++block) {
            const double nowMs = block * r.blockBudgetMs;
            const bool init = nowMs >= nextInitMs;
            const bool shuffle = init || block % c.shuffleEveryBlocks == 0;
            if (!shuffle) continue;

            // Settings changes are rare and each one is checked against the budget: run those 3 times
            // and keep the fastest, so a preempted run does not count as the cost.
            double ms = 1e9;
            long long work = 0;
            for (int run = 0; run < (init ? 3 : 1); ++run) {
                work = 0;
                const Clock::time_point t = Clock::now();
                if (init) {
                    gen_.initialize(settings_[r.initializes % settings_.size()]);
                    work += gen_.lastWork();
                }
                gen_.generate(seed);
                for (int i = 0; i < c.chordsPerSet; ++i) {
                    chords[i] = gen_.getNext(i == c.chordsPerSet - 1);
                    work += gen_.lastWork();
                }
                ms = std::min(ms, std::chrono::duration<double, std::milli>(Clock::now() - t).count());
            }
            r.maxBlockWork = std::max(r.maxBlockWork, work);
            r.totalWork += work;
            r.totalMs += ms;
            ++seed;

            for (int i = 0; i < c.chordsPerSet; ++i) r.emptyChords += chords[i].count == 0;
            if (ms >= r.blockBudgetMs) ++r.overruns;
            if (init) {
                r.initBlockMaxMs = std::max(r.initBlockMaxMs, ms);
                initTotal += ms;
                ++r.initializes;
                nextInitMs += c.initializeEveryMs;
            } else {
                sets.push_back(ms);
            }
        }

        std::sort(sets.begin(), sets.end());
        if (!sets.empty()) {
            r.setMaxMs = sets.back();
            r.setP99Ms = sets[static_cast<size_t>(sets.size() * 0.99)];
        }
        r.initBlockMeanMs = r.initializes ? initTotal / r.initializes : 0;
        return r;
    }

private:
    std::vector<ChordsSettings> settings_;
    ChordsGenerator gen_;
};

// Settings a user might sweep through, starting with the slowest ones to initialize.
std::vector<ChordsSettings> sweep() {
    std::vector<ChordsSettings> list;
    const int scales[] = {19, 0, 13, 15, 16};   // chromatic, major, minor pentatonic, whole tone, diminished
    const double densities[] = {1.0, 0.66, 0.0};
    for (int scale : scales)
        for (double d : densities)
            for (int notes = kMaxNotes; notes >= 1; --notes) {
                ChordsSettings s;
                s.scale = kScales[scale];
                s.root = (scale * 5) % 12;
                s.density = d;
                s.notesPerChord = notes;
                s.lowNote = 24;     // the widest range the Octave sliders allow: octaves 0..6
                s.highNote = 107;
                list.push_back(s);
            }
    return list;
}

// Every scale, 1..6 notes, 4 densities, 4 octave ranges: each initialize and each getNext stays under its
// step limit, and no chord comes out empty.
void workStaysBounded() {
    ChordsGenerator gen;
    long long maxInit = 0, maxNext = 0;
    int empty = 0;
    const double densities[] = {0.0, 0.33, 0.66, 1.0};
    const int ranges[][2] = {{0, 0}, {0, 6}, {2, 2}, {2, 6}};   // octave min, max (as the sliders)
    for (const Scale& scale : kScales)
        for (double d : densities)
            for (int notes = 1; notes <= kMaxNotes; ++notes)
                for (const auto& range : ranges) {
                    ChordsSettings s;
                    s.scale = scale;
                    s.density = d;
                    s.notesPerChord = notes;
                    s.lowNote = 24 + 12 * range[0];
                    s.highNote = 24 + 12 * range[1] + 11;
                    CHECK(gen.initialize(s));
                    CHECK(gen.lastWork() <= kInitializeWorkLimit);
                    maxInit = std::max(maxInit, gen.lastWork());
                    gen.generate(1);
                    for (int i = 0; i < kMaxChords; ++i) {
                        empty += gen.getNext(i == kMaxChords - 1).count == 0;
                        CHECK(gen.lastWork() <= kGetNextWorkLimit);
                        maxNext = std::max(maxNext, gen.lastWork());
                    }
                }
    CHECK_EQ(empty, 0);
    std::printf("chords work: initialize max %lld steps (limit %lld), getNext max %lld steps (limit %lld)\n",
                maxInit, kInitializeWorkLimit, maxNext, kGetNextWorkLimit);
}

// The block simulation: work per block under the limits; times are printed for a look, not checked
// (a busy machine or CI runner slows them down several times).
void audioThreadKeepsUp() {
    const std::vector<ChordsSettings> settings = sweep();
    ChordsLoadSimulator sim(settings);
    ChordsLoadSimulator::Config config;
    // Long enough for every settings entry to be initialized once.
    config.seconds = (settings.size() + 1) * config.initializeEveryMs / 1000.0;
    const ChordsLoadSimulator::Report r = sim.run(config);

    CHECK_EQ(r.emptyChords, 0);
    CHECK(r.maxBlockWork <= kInitializeWorkLimit + config.chordsPerSet * kGetNextWorkLimit);

    std::printf("chords load: %d blocks of %.2f ms; most work in a block %lld steps (%.1f ns/step here); "
                "new set of %d chords p99 %.3f ms; initialize + set (%d times) mean %.2f ms max %.2f ms; "
                "%d blocks over budget\n",
                r.blocks, r.blockBudgetMs, r.maxBlockWork, r.totalMs * 1e6 / static_cast<double>(r.totalWork),
                config.chordsPerSet, r.setP99Ms, r.initializes, r.initBlockMeanMs, r.initBlockMaxMs, r.overruns);
}

} // namespace

void chordsLoadTests() {
    workStaysBounded();
    audioThreadKeepsUp();
}
