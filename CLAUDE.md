# BananaChords: rules for agents

BananaChords is a MIDI note generator (chords) built from one C++ core:
an IvanDAW web plugin (wasm, Plugin API v2) now, a JUCE plugin (VST3, AU MIDI FX, Standalone) from v0.1.0.

Guide: ivan-daw `docs/plugin-dev/` (README, 01-architecture, types/midi-generator.md, 03-web-host, 04-vst-host, 05-ci-cd).
Reference implementation: zhirnovcodes/Beatality. Behaviour spec: [docs/design.md](docs/design.md).

## Names

| Thing | Value |
| --- | --- |
| Repo / wasm / zip | `banana-chords` / `banana-chords.wasm` / `banana-chords-daw.zip` |
| IvanDAW id | `com.ivandaw.banana-chords` |
| JUCE target | `BananaChords`, `BananaChordsMidiFx` (v0.1.0) |
| C ABI prefix / namespace | `bc_` / `bc::` |
| AU codes | manufacturer `Ivdw`, plugin `Bc01`, MIDI FX `Bcx1` (v0.1.0; check against Beatality's before use) |

## Folder map

| Folder | Holds | Uses |
| --- | --- | --- |
| `core/domain/` | scales, sequences, harmony, RNG | C++20 std only; no exceptions, RTTI, I/O, allocation in `process` |
| `core/engine/` | `Engine`: params, transport, note output | core/domain |
| `hosts/web/` | C ABI (`boundary/api.cpp`), wasm loader, lookahead driver, `PluginFactory` | core via C ABI, ivandaw-core |
| `apps/web/` | Vite build of the IvanDAW plugin, dev page with sine synth, the skin in `src/ui/` | – |
| `tests/` | C++ test executable `bc_tests` | core |
| `params.json` | the only parameter list | both hosts, CMake (generates `ParamsJson.h`) |

## Commands

```
# wasm + core tests (no native compiler needed: tests run under Node)
emcmake cmake -S . -B build-wasm -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm
node build-wasm/bc_tests.js
# native core tests (CI)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build --target bc_tests && ./build/bc_tests
# IvanDAW plugin → apps/web/dist-daw/
pnpm install
pnpm --filter @banana-chords/web build:daw
pnpm --filter @banana-chords/web dev        # http://localhost:5190
```

## Rules

- All behaviour lives in `core/`. Hosts translate; they never decide musical behaviour.
- `params.json` is append-only. Ids and indexes never change. `enum Param` in `Engine.h` follows it.
- The same input gives the same output for any block size. Core tests check sizes 1, 64, 128, 512, 4096.
- Every note-on gets a note-off (stop, CC 120/123, `setState`, suspend, dispose).
- Random sequences come from per-sequence seeds; state saves the seeds.
- Web: musical timing uses `createTicker` / `LOOKAHEAD` from ivandaw-core, never `setTimeout` / `setInterval`.
- The release contract (tag `daw-build-<n>`, asset `banana-chords-daw.zip`, `index.js` at zip root) never changes without a matching ivan-daw change.
- You cannot hear. Every check says exactly what Ivan should listen for.

## Workflow

- Branch, PR to `master`. CI: `daw-plugin.yml` (PR artifact; release on `master`), `core-tests.yml`.
- Before pushing: `node build-wasm/bc_tests.js` and `build:daw` pass locally.
- PR description ends with a **Ready for IvanDAW** section (release, port / param / state changes, what to listen for).
- ivan-daw is changed only in its own session (pin in `plugins/banana-chords/package.json`).
