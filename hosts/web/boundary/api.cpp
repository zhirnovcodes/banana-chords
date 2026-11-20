// C ABI over bc::Engine → banana-chords.wasm. Standalone wasm, no Emscripten JS glue.
#include <cstdlib>
#include <new>

#include "Engine.h"

#define BC_EXPORT extern "C" __attribute__((used, visibility("default")))

struct Instance {
    bc::Engine engine;
    bc::OutBuffer out;
    int notes[bc::kMaxNotes] = {};
    Instance(double r, std::uint32_t s) : engine(r, s) {}
};

BC_EXPORT void* bc_alloc(int bytes) { return bytes > 0 ? std::malloc(bytes) : nullptr; }
BC_EXPORT void bc_free(void* p) { std::free(p); }

BC_EXPORT Instance* bc_create(double rate, std::uint32_t seed) {
    if (!(rate > 0)) return nullptr;
    void* m = std::malloc(sizeof(Instance));
    return m ? new (m) Instance(rate, seed) : nullptr;
}
BC_EXPORT void bc_destroy(Instance* i) {
    if (i) {
        i->~Instance();
        std::free(i);
    }
}

BC_EXPORT void bc_set_param(Instance* i, int index, double value) { if (i) i->engine.setParam(index, value); }
BC_EXPORT double bc_param(Instance* i, int index) { return i ? i->engine.param(index) : 0; }

BC_EXPORT void bc_transport(Instance* i, int playing, double bpm, double beat, int num, int den, int frame) {
    if (!i) return;
    bc::Transport t;
    t.playing = playing != 0;
    t.bpm = bpm;
    t.beat = beat;
    t.numerator = num;
    t.denominator = den;
    i->engine.transport(t, frame);
}
BC_EXPORT void bc_midi(Instance* i, int frame, int status, int d1, int d2) {
    if (i) i->engine.midi(frame, static_cast<std::uint8_t>(status), static_cast<std::uint8_t>(d1), static_cast<std::uint8_t>(d2));
}
// Replaces the output buffer with the note-offs.
BC_EXPORT void bc_all_off(Instance* i, int frame) {
    if (!i) return;
    i->out.clear();
    i->engine.allOff(frame, i->out);
}
// Replaces the output buffer with this call's events.
BC_EXPORT void bc_process(Instance* i, int frames, float* left, float* right) {
    if (!i) return;
    i->out.clear();
    i->engine.process(frames, left, right, i->out);
}

BC_EXPORT int bc_out_count(Instance* i) { return i ? i->out.size() : 0; }
BC_EXPORT int bc_out_frame(Instance* i, int k) { return i ? i->out[k].frame : 0; }
BC_EXPORT int bc_out_kind(Instance* i, int k) { return i ? static_cast<int>(i->out[k].kind) : 0; }
BC_EXPORT int bc_out_bytes(Instance* i, int k) {
    if (!i) return 0;
    const bc::OutEvent& e = i->out[k];
    return e.b0 | (e.b1 << 8) | (e.b2 << 16);
}
BC_EXPORT int bc_out_slot(Instance* i, int k) { return i ? i->out[k].slot : 0; }
BC_EXPORT float bc_out_value(Instance* i, int k) { return i ? i->out[k].value : 0; }

// State and UI view.
BC_EXPORT unsigned bc_seed(Instance* i, int which) { return i ? i->engine.seed(which) : 0; }
BC_EXPORT void bc_set_seed(Instance* i, int which, unsigned value) { if (i) i->engine.setSeed(which, value); }
BC_EXPORT int bc_chord_count(Instance* i) { return i ? i->engine.chordCount() : 0; }
BC_EXPORT double bc_chord_start(Instance* i, int chord) { return i ? i->engine.chordStart(chord) : 0; }
BC_EXPORT double bc_chord_length(Instance* i, int chord) { return i ? i->engine.chordLength(chord) : 0; }
// Resolves chord `chord`; read the notes with bc_chord_note(0..count-1).
BC_EXPORT int bc_chord_notes(Instance* i, int chord) { return i ? i->engine.chordNotes(chord, i->notes) : 0; }
BC_EXPORT int bc_chord_note(Instance* i, int k) { return i && k >= 0 && k < bc::kMaxNotes ? i->notes[k] : 0; }
// Pitch classes of the selected scale on the selected root: bit k = pitch class k (0 = C). Piano dots.
BC_EXPORT int bc_scale_mask(Instance* i) { return i ? i->engine.scaleMask() : 0; }
BC_EXPORT int bc_current_chord(Instance* i) { return i ? i->engine.currentChord() : -1; }
