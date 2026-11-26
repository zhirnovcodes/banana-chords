# ChordsGenerator · WFC chord progressions

Code: [core/domain/ChordsGenerator.h](../core/domain/ChordsGenerator.h). Tests: [tests/chords_generator_tests.cpp](../tests/chords_generator_tests.cpp).
The Engine plays its chords (steps 4–6 in [design.md](design.md)).

## Implementation notes

API: `initialize(settings)` → `generate(seed)` → `getNext(last)` once per chord. Output is chords only
(MIDI notes, ascending). The Engine calls all three in `rebuild()` and stores the loop's chords.

Differences from the spec below:

- One chord at a time: each pick weighs the candidates after the previous chord (§5), no look-ahead
  (§6's solver is not used). `getNext(true)` stands on the tonic (or contains it, if no voicing stands on it).
  When no candidate passes `hardThreshold` after the previous chord, the threshold is dropped for that pick.
- 1..6 notes. The range and density always hold; when the notes do not fit, chords get fewer notes
  (`notesPerChord()` says how many). No gap widening.
- No pins, no melody, no chord names, no failure report: `initialize` returns false when no scale note is in range.
- Candidate pool cap `kMaxChordCandidates` (256), fixed seed, so the pool depends only on the settings.
  Voicings are listed in full while that takes ≤ `kEnumerateBudget` steps (reservoir-sampled down to 256);
  bigger sets are sampled note by note (only gaps that keep the pitch class new and leave room for the
  notes above), duplicates skipped through a small hash table.
- All weights and gap constants live in `ChordRules`; gap math only in `chordGapRange()`.
- Never allocates, ~13 KB object. Everything runs on the audio thread; `tests/chords_load_tests.cpp`
  checks a new 32-chord set every block and a settings change (initialize + set) against a 128-sample block.

---

# Spec: WFC Chord Progression Generator (scale-agnostic)

Chord-progression generator using a Wave Function Collapse (WFC) style constraint solver over a 1D
sequence of chord slots. It must work with ANY scale (modes, pentatonics, harmonic/melodic minor,
whole-tone, octatonic, exotic/ethnic scales, user-defined scales), so it does NOT rely on
major/minor functional harmony (no hardcoded "V → I" rules). All rules are in scale steps and semitones.

## 1. Inputs

| Param | Type | Description |
|---|---|---|
| `scaleRoot` | int 0–11 | Pitch class of the tonic (0 = C). |
| `scaleIntervals` | int[] | Semitone offsets from root, ascending, starting with 0. Length N can be 2–12. |
| `numChords` | int | Number of chord slots (cells) to generate. |
| `notesPerChord` | int ≥ 2 | Number of notes in every chord. |
| `density` | float 0–1 | How close notes inside a chord may be. 0 = only wide gaps, 1 = adjacent scale notes allowed. |
| `baseOctave` / `range` | MIDI range | Lowest/highest allowed MIDI note, e.g. 48–84. |
| `pins` (optional) | map slot → constraint | Fixed chord, required root degree, or required pitch class(es) in a slot. |
| `melody` (optional) | pitch class per slot | The chord in that slot must contain that pitch class (optionally as its top note). |
| `endOnTonic` (optional, default true) | bool | Last chord's root should be scale degree 0. |
| `seed` | int | RNG seed for reproducibility. |

Chords are generated directly. There is no melody generation step. Melody is only an optional constraint.

## 2. Representation

- A scale has N degrees `0..N-1`. Degree `d` at octave `o` is MIDI `basePitch + 12*o + scaleIntervals[d]`.
- Flat scale index `k = o*N + d`: stepping up by 1 always moves to the next scale note.
- A chord candidate = `rootIndex` (scale index of the lowest note) + `gaps[]` (length
  `notesPerChord-1`, each ≥ 1 scale step).
- Derived: MIDI notes, pitch-class set, bass pitch class, root degree (`rootIndex mod N`).
- The lowest note is the chord's "root" for scoring. The algorithm never depends on an inferred root.

## 3. Note density → allowed gaps

Gaps in scale steps, normalized to a 7-note reference `s = N / 7`:

```
minGap(density) = max(1, round( (1 + (1 - density) * 3) * s ))   // density 0 → 4, 1 → 1 (7-note scale)
maxGap          = max(minGap, round(6 * s))
```

Hard filters: all notes within range; no two notes with the same pitch class unless
`notesPerChord > N`; total span ≤ 2 octaves (configurable).

## 4. Candidate generation (the initial "superposition")

Per slot: all candidates (root indices in range × gap combinations) passing §3, then
`pins[slot]`, `melody[slot]` (and "melody on top"), and for the last slot with `endOnTonic`
root degree 0 (if that empties the domain: candidates that merely contain the tonic pitch class).

## 5. Transition scoring (A → B)

1. Root motion, interval class between bass pitch classes: 5 → +3, 3/4 → +2, 1/2 → +1.5, 6 → +0.5, 0 → −1.
2. Common tones: +1 per shared pitch class; identical pitch-class sets: −3 instead.
3. Voice leading: sorted voicings, note i to note i, `−0.15 * Σ|Δ|`.
4. Tonic gravity: +1 if B contains the tonic pitch class and B is the last slot or a phrase end.

Weight `w(A→B) = exp(score / temperature)`, temperature ≈ 1 ("randomness").
Hard adjacency: A → B allowed only when `score ≥ hardThreshold` (0).

## 6. Solver

WFC on a 1D chain (lowest-entropy slot, weighted pick, arc-consistency propagation, backtracking),
or, recommended, forward filtering / backward sampling:
`α_i(c) = Σ_a α_{i-1}(a) * w(a→c)` over the domains, sample the last slot from `α_n`, then walk
backwards sampling `a ∝ α_{i-1}(a) * w(a→next)`.

Failure handling, in order: 1) `endOnTonic` → "contains tonic", 2) lower `hardThreshold`,
3) widen the gap range by 1 step, 4) report which slot / constraint failed.

## 7. Output

Per slot: MIDI notes (ascending), pitch classes, root degree, best-effort chord name
(pitch-class set against known chord types, else note names, e.g. "C-G-E").

## 8. Test cases

1. C major, 3 notes, density 0.66, melody `A C B A` → each chord contains its melody note.
2. Same scale, density 0 → all internal gaps ≥ 4 scale steps.
3. Same scale, density 1, 4 notes → some chords contain adjacent scale notes.
4. A minor pentatonic, 3 notes, 8 chords → only pentatonic notes, gaps scaled by N/7.
5. Whole-tone → works with no special-casing.
6. `notesPerChord` > N (6 notes in pentatonic) → doubling allowed, no crash.
7. Same seed → same output.
8. Pins: slot 0 = tonic chord, slot 3 fixed → respected, neighbours adapt.
