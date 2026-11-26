// The wasm module's exports (hosts/web/boundary/api.cpp) and how to load it.

export interface Core {
  bc_create(rate: number, seed: number): number;
  bc_destroy(engine: number): void;
  bc_set_param(engine: number, index: number, value: number): void;
  bc_param(engine: number, index: number): number;
  bc_transport(engine: number, playing: number, bpm: number, beat: number, num: number, den: number, frame: number): void;
  bc_midi(engine: number, frame: number, status: number, d1: number, d2: number): void;
  bc_all_off(engine: number, frame: number): void;
  bc_process(engine: number, frames: number, left: number, right: number): void;
  bc_out_count(engine: number): number;
  bc_out_frame(engine: number, k: number): number;
  bc_out_kind(engine: number, k: number): number;
  bc_out_bytes(engine: number, k: number): number;
  bc_chord_count(engine: number): number;
  bc_chord_start(engine: number, chord: number): number;
  bc_chord_length(engine: number, chord: number): number;
  bc_chord_notes(engine: number, chord: number): number;
  bc_chord_note(engine: number, k: number): number;
  bc_current_chord(engine: number): number;
  /** Pitch classes of the selected scale on the selected root: bit k = pitch class k (0 = C). */
  bc_scale_mask(engine: number): number;
}

// STANDALONE_WASM imports only this one function (checked with WebAssembly.Module.imports).
const imports = { env: { emscripten_notify_memory_growth() {} } };

export async function instantiateCore(bytes: BufferSource): Promise<Core> {
  const { instance } = await WebAssembly.instantiate(bytes, imports);
  (instance.exports as { _initialize?: () => void })._initialize?.();
  return instance.exports as unknown as Core;
}

const loaded = new Map<string, Promise<Core>>();

/** One wasm instance per URL, shared by every plugin instance (each gets its own engine). */
export function loadCore(url: URL): Promise<Core> {
  const key = url.href;
  let core = loaded.get(key);
  if (!core) {
    core = fetch(url)
      .then((r) => {
        if (!r.ok) throw new Error(`banana-chords: could not load ${url.href} (${r.status})`);
        return r.arrayBuffer();
      })
      .then(instantiateCore);
    core.catch(() => loaded.delete(key));
    loaded.set(key, core);
  }
  return core;
}
