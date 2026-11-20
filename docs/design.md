# Banana Chords · design (v0.0.1)

A MIDI note generator. No input; output is chords as MIDI notes, looped and locked to the host's beat.
This file is the spec as built: every rule here is in `core/`, with the test that checks it.

## Parameters

`params.json` is the only list. Index = engine index. Append only.

| # | id | Label | Kind | Range | Default | Automatable | Regenerates |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | `root` | Note | choice | C..B | C | no | – |
| 1 | `scale` | Scale | choice | 20 scales (below) | Major | no | – |
| 2 | `loop` | Loop | int | 1..32 beats | 8 | no | all |
| 3 | `chordMin` | Chord Length Min | int | 1..32 beats | 1 | yes | all |
| 4 | `chordMax` | Chord Length Max | int | 1..32 beats | 3 | yes | all |
| 5 | `octaveMin` | Octave Min | int | 0..6, ≤ Octave Max | 2 | yes | – |
| 6 | `octaveMax` | Octave Max | int | 0..6, ≥ Octave Min | 3 | yes | – |
| 7 | `shuffle` | Shuffle | trigger | | | yes | all |
| 8 | `drift` | Drift Offset | float | 0..1 | 0 | no | 3 (time table) |
| 9 | `density` | Scale Density | float | 0..1 | 0.8 | yes | – |
| 10 | `notesCount` | Notes Count | int | 1..6 | 3 | yes | 5, 6 |

"Regenerates" fires only when the value really changes (an int param dragged across the same value does nothing).

## Sequences

Each random sequence has its own seed. Regenerating a sequence = a new seed from the engine's master RNG, then
building it from that seed. State saves the five seeds, so a reloaded project plays exactly the same chords.

| # | Sequence | Size | Values | Depends on |
| --- | --- | --- | --- | --- |
| 1 | times | chord count | whole beats in [min, max], sum = loop | loop, chordMin, chordMax |
| 2 | drift | chord count − 1 | [-1, 1) | 1 |
| 3 | time table | loop / cell | chord index per cell | 1, 2, Drift Offset (built, no seed) |
| 4 | notes | chord count | [0, 1) | 1 |
| 5 | chords | chord count × (notes − 1) | [-1, 1) | 1, notesCount |
| 6 | octaves | chord count × notes | [-1, 1) | 1, notesCount |

### 1 · times
Pick lengths one by one, uniformly from the lengths in [min, max] that leave a remainder that can still be split into
chords of min..max. Example: loop 8, min 1, max 3 → `1,2,1,3,1`. If the loop itself cannot be split (loop 5, chords 3..4)
it becomes one chord of the whole loop. Min ≤ Max always, as with the octaves: moving one past the other takes the other along
(an older saved state with min > max still plays, read as swapped).

### 3 · time table and drift
Chord borders live on a 1/4-beat grid ("quanta"). For the border between chord A and chord B, with `d = drift[i] × Drift Offset`:

- `d > 0`: the border moves right by `d × len(B) / 2` (A holds longer, B arrives late).
- `d < 0`: the border moves left by `|d| × len(A) / 2` (B arrives early).
- Rounded to the nearest 1/4 beat. Every chord keeps at least 1/4 beat.

Drift Offset scales every drift value: **0 = straight grid** (default), 1 = the random drift as generated.
The cell size is the largest grid that holds every border (gcd of the borders), so the table has `loop / cell` cells.
Reading: `cell = floor((beat mod loop) / cell size)` → chord index.

### Chord notes (steps 4–6 at play time)
Read whenever a chord is evaluated, so Note, Scale, Density, Octave Min/Max apply without regenerating anything.

1. **Main note**: `degree = floor(notes[i] × scale size)`.
2. **Other notes** (`notesCount − 1`): each value's sign picks the side, its size the distance.
   `v ≥ 0` stacks above the highest note so far, `v < 0` below the lowest. Step = `1 + gap` scale degrees,
   `gap = 1 + floor(|v| × maxGap)`, with `maxGap = 1 + round((1 − density) × 5)`:
   density 1 → gap 1 (stacked thirds), density 0 → gaps up to 6 notes.
   Steps 1–2 decide the chord's **pitch classes** (which notes of the scale).
3. **Octave**: Octave Min/Max say where the notes are. Each note (main and others) is placed in
   `octave = octaveMin + bucket(octaves[i][k])`, uniform buckets over [octaveMin, octaveMax], so every note lies in
   C<min>..B<max>. Octave numbers match the note names (C3 = 60, as IvanDAW names notes): octave 0 = MIDI 24..35,
   octave 6 = MIDI 96..107. Min ≤ Max always: moving one past the other takes the other along.
4. **Duplicates become one note** (two notes with the same pitch class in the same octave).

### Playing
- Chords are evaluated on every 1/4 beat while playing, and at once on Play, a jump or a tempo change.
- A note that is in both the old and the new chord **keeps sounding**; the others stop, then the new ones start (same frame).
- Stop, CC 120/123, suspend, setState, dispose: a note-off for every sounding note.
- Velocity 100, channel 1.

## Answers to the open questions

**Protection from quick note changes?** Built in: notes can only change on the 1/4-beat grid. The time table already
sits on that grid, and parameter changes (automating Density, Scale, Notes Count…) are picked up at the next 1/4 beat
rather than per frame. So the fastest possible change is a 16th note. A drift can make a chord as short as 1/4 beat
when Chord Length Min is 1. If that is still too busy, the next step would be a "min hold" parameter (e.g. 1/2 or 1 beat)
that defers a change until the current notes have held that long. Not in v0.0.1.

**What scales are there?** There is no scale list elsewhere in IvanDAW or Beatality, so this one is new (`core/domain/Scales.h`):

| # | Scale | Steps | # | Scale | Steps |
| --- | --- | --- | --- | --- | --- |
| 0 | Major | 0 2 4 5 7 9 11 | 10 | Hungarian Minor | 0 2 3 6 7 8 11 |
| 1 | Natural Minor | 0 2 3 5 7 8 10 | 11 | Double Harmonic | 0 1 4 5 7 8 11 |
| 2 | Harmonic Minor | 0 2 3 5 7 8 11 | 12 | Major Pentatonic | 0 2 4 7 9 |
| 3 | Melodic Minor | 0 2 3 5 7 9 11 | 13 | Minor Pentatonic | 0 3 5 7 10 |
| 4 | Dorian | 0 2 3 5 7 9 10 | 14 | Blues | 0 3 5 6 7 10 |
| 5 | Phrygian | 0 1 3 5 7 8 10 | 15 | Whole Tone | 0 2 4 6 8 10 |
| 6 | Lydian | 0 2 4 6 7 9 11 | 16 | Diminished (W-H) | 0 2 3 5 6 8 9 11 |
| 7 | Mixolydian | 0 2 4 5 7 9 10 | 17 | Hirajoshi | 0 2 3 7 8 |
| 8 | Locrian | 0 1 3 5 6 8 10 | 18 | In | 0 1 5 7 8 |
| 9 | Phrygian Dominant | 0 1 4 5 7 8 10 | 19 | Chromatic | 0..11 |

"All possible" scales is every set of pitch classes that contains the root: 2¹¹ = 2048 (462 of them have 7 notes),
so a full list is not useful in a selector. More can be appended to the table and to the `scale` choices.

## Where this differs from the brief

| Brief | Built | Why |
| --- | --- | --- |
| Notes Count "slider 0–1" | int 1..6 | Sequences 5 and 6 are sized by it. Automation still sends 0..1 normalized. |
| Drift Offset "adds offset", -1..1 | scales the drift (multiplier), 0..1 | With an additive offset there is no way back to a straight grid. |
| Note checked "each frame" | each 1/4 beat | The quick-change protection above; also keeps output identical for any block size. |
| Octave range -1..1 transpose, 0 = C1 | absolute octaves 0..6 that hold every note | Changed on request. |
