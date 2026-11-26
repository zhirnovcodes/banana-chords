#include <algorithm>
#include <chrono>
#include <vector>

#include "Check.h"
#include "ChordsGenerator.h"

using namespace bc;

namespace {

// Plays a stretch of virtual time and measures how long ChordsGenerator calls take on each thread:
//   audio thread:   every `shuffleEveryBlocks` blocks, a new chord set (generate + getNext per chord)
//   message thread: every `initializeEveryMs`, initialize with the next settings from a list
// The core has no threads, so both run here one after another; each is measured against its own budget.
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
        double audioMaxMs = 0;        // slowest block's generator work
        double audioP99Ms = 0;
        int audioOverruns = 0;        // blocks whose generator work alone took the whole budget
        int emptyChords = 0;
        int initializes = 0;
        double initMaxMs = 0;
        double initMeanMs = 0;
    };

    explicit ChordsLoadSimulator(const std::vector<ChordsSettings>& settings) : settings_(settings) {}

    Report run(const Config& c) {
        using Clock = std::chrono::steady_clock;
        auto msSince = [](Clock::time_point t) {
            return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
        };

        Report r;
        r.blockBudgetMs = 1000.0 * c.blockSize / c.sampleRate;
        r.blocks = static_cast<int>(c.seconds * c.sampleRate / c.blockSize);
        std::vector<double> audio;
        audio.reserve(r.blocks);
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
            // Message thread: settings changed.
            if (nowMs >= nextInitMs) {
                const ChordsSettings& s = settings_[r.initializes % settings_.size()];
                const Clock::time_point t = Clock::now();
                gen_.initialize(s);
                const double ms = msSince(t);
                r.initMaxMs = std::max(r.initMaxMs, ms);
                initTotal += ms;
                ++r.initializes;
                nextInitMs += c.initializeEveryMs;
            }
            // Audio thread: one process block.
            double ms = 0;
            if (block % c.shuffleEveryBlocks == 0) {
                const Clock::time_point t = Clock::now();
                gen_.generate(seed++);
                for (int i = 0; i < c.chordsPerSet; ++i) chords[i] = gen_.getNext(i == c.chordsPerSet - 1);
                ms = msSince(t);
                for (int i = 0; i < c.chordsPerSet; ++i) r.emptyChords += chords[i].count == 0;
            }
            audio.push_back(ms);
            if (ms >= r.blockBudgetMs) ++r.audioOverruns;
        }

        std::sort(audio.begin(), audio.end());
        r.audioMaxMs = audio.back();
        r.audioP99Ms = audio[static_cast<size_t>(audio.size() * 0.99)];
        r.initMeanMs = r.initializes ? initTotal / r.initializes : 0;
        return r;
    }

private:
    std::vector<ChordsSettings> settings_;
    ChordsGenerator gen_;
};

// Settings a user might sweep through, ending with the slowest one to initialize.
std::vector<ChordsSettings> sweep() {
    std::vector<ChordsSettings> list;
    const int scales[] = {19, 0, 13, 15, 16};   // chromatic, major, minor pentatonic, whole tone, diminished   // major, minor pentatonic, whole tone, diminished, chromatic
    const double densities[] = {0.0, 0.66, 1.0};
    for (int scale : scales)
        for (double d : densities)
            for (int notes = 2; notes <= kMaxNotes; ++notes) {
                ChordsSettings s;
                s.scale = kScales[scale];
                s.root = (scale * 5) % 12;
                s.density = d;
                s.notesPerChord = notes;
                list.push_back(s);
            }
    return list;
}

void audioAndMessageThreadsKeepUp() {
    const std::vector<ChordsSettings> settings = sweep();
    ChordsLoadSimulator sim(settings);
    ChordsLoadSimulator::Config config;
    // Long enough for every settings entry to be initialized once.
    config.seconds = (settings.size() + 1) * config.initializeEveryMs / 1000.0;
    const ChordsLoadSimulator::Report r = sim.run(config);

    CHECK_EQ(r.emptyChords, 0);
    // Audio thread: a new 32-chord set every block stays well inside the block. The generator
    // shares the block with the rest of the DAW, so it gets a quarter of it at most.
    CHECK(r.audioP99Ms < r.blockBudgetMs * 0.25);
    // Preemption on a busy machine can stretch a single block; allow 1 in 1000.
    CHECK(r.audioOverruns <= r.blocks / 1000);
    // Message thread: initialize every 100 ms stays under half the interval.
    CHECK(r.initMaxMs < config.initializeEveryMs * 0.5);

    std::printf("chords load: %d blocks of %.2f ms, set of %d chords p99 %.3f ms max %.3f ms (%d overruns); "
                "%d initializes every %.0f ms, mean %.2f ms max %.2f ms\n",
                r.blocks, r.blockBudgetMs, config.chordsPerSet, r.audioP99Ms, r.audioMaxMs, r.audioOverruns,
                r.initializes, config.initializeEveryMs, r.initMeanMs, r.initMaxMs);
}

} // namespace

void chordsLoadTests() {
    audioAndMessageThreadsKeepUp();
}
