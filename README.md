# Banana Chords

A MIDI chord generator. It takes no input. From a few settings (root, scale, loop length, chord lengths, density,
notes per chord, octave range) it generates a looping chord progression and plays it as MIDI notes, locked to the
host's beat. **Shuffle** rolls a new progression.

How the generation works: [docs/design.md](docs/design.md).

| Version | Scope |
| --- | --- |
| **0.0.1** (this) | Default design, IvanDAW web plugin, dev page with a sine test synth, CI/CD |
| 0.1.0 | VST3 / AU MIDI FX / Standalone (JUCE) |
| 1.0.0 | Final design |

## Try it

Needs emscripten 6.0.12, CMake ≥ 3.25, Ninja, Node 24, pnpm 10.

```
emcmake cmake -S . -B build-wasm -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm
node build-wasm/bc_tests.js                    # core tests
pnpm install
pnpm --filter @banana-chords/web dev           # http://localhost:5190
```

On the dev page: **Start audio**, then **Play** (or Space). The timeline shows each chord's notes and highlights the
one playing; the log lists every note the plugin emits. Turn on **Click** to hear the beat against the chord changes.

What to listen for:

- Chords change exactly on the timeline borders, in time with the click.
- With **Drift Offset** at 0 every change is on a beat; moving it away from 0 pushes changes early or late by 1/4-beat steps.
- A note shared by two neighbouring chords keeps sounding through the change.
- **Stop** leaves no note hanging. **Shuffle** while playing changes the progression from the next 1/4 beat.

## IvanDAW

Each merge to `master` publishes release `daw-build-<n>` with `banana-chords-daw.zip`. In ivan-daw, add
`plugins/banana-chords/package.json`:

```json
{
  "name": "@ivandaw/banana-chords",
  "version": "0.0.0",
  "private": true,
  "description": "Banana Chords, prebuilt by its CI. dist/ is downloaded by scripts/fetch-plugins.mjs, never built here.",
  "ivandaw": {
    "id": "com.ivandaw.banana-chords",
    "repo": "zhirnovcodes/banana-chords",
    "asset": "banana-chords-daw.zip",
    "release": "daw-build-1"
  }
}
```

Wire: Banana Chords `out` → any instrument `in` → Master. Press Play.

## Known limits

- The plugin renders 100 ms ahead (IvanDAW's lookahead). A tempo change or loop jump that arrives later than that
  takes effect up to 100 ms late. Stop is immediate: notes already booked past the stop get their note-off at the stop.
- Parameter changes are heard from the next 1/4 beat, plus up to 100 ms of lookahead.
