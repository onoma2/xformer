# Scale Rotate — mode reference

`scaleRotate` rotates a base scale's degree set so that **degree _N_ becomes the new tonic**, where
_N_ = the rotate index. The pitch class set is unchanged; only the starting point (and therefore the
interval pattern from the root) moves. For 7-note scales the rotations are the classic named modes.

- Rotate is applied at resolve time, modulo `notesPerOctave` — so on a 7-note scale `rotate 9 == rotate 2`.
- Voltage / continuous scales don't rotate (`supportsRotation() == false`); rotate is a no-op there.
- Only 7-note diatonic bases have standard mode names. Pentatonic (5), whole-tone (6), and the
  n-tet scales rotate too, but their rotations have no conventional names — pick by ear.
- Natural minor is **not** a separate built-in: it's `Major + rotate 5` (Aeolian). Built-in index 2 is
  Harmonic Minor (`H.Minor`).

## Base: Major (Ionian) — `0 2 4 5 7 9 11`

| Rotate | Mode        | Semitones from root      | Also known as            |
|:------:|-------------|--------------------------|--------------------------|
| 0      | Ionian      | `0 2 4 5 7 9 11`         | Major                    |
| 1      | Dorian      | `0 2 3 5 7 9 10`         | —                        |
| 2      | Phrygian    | `0 1 3 5 7 8 10`         | —                        |
| 3      | Lydian      | `0 2 4 6 7 9 11`         | —                        |
| 4      | Mixolydian  | `0 2 4 5 7 9 10`         | Dominant                 |
| 5      | Aeolian     | `0 2 3 5 7 8 10`         | Natural minor            |
| 6      | Locrian     | `0 1 3 5 6 8 10`         | —                        |

## Base: Harmonic Minor (`H.Minor`, built-in index 2) — `0 2 3 5 7 8 11`

| Rotate | Mode                | Semitones from root      | Also known as                       |
|:------:|---------------------|--------------------------|-------------------------------------|
| 0      | Harmonic Minor      | `0 2 3 5 7 8 11`         | Aeolian ♯7                          |
| 1      | Locrian ♮6          | `0 1 3 5 6 9 10`         | —                                   |
| 2      | Ionian ♯5           | `0 2 4 5 8 9 11`         | Augmented major                     |
| 3      | Dorian ♯4           | `0 2 3 6 7 9 10`         | Ukrainian Dorian / Romanian minor   |
| 4      | Phrygian Dominant   | `0 1 4 5 7 8 10`         | Spanish / Freygish                  |
| 5      | Lydian ♯2           | `0 3 4 6 7 9 11`         | —                                   |
| 6      | Ultralocrian        | `0 1 3 4 6 8 9`          | Super Locrian ♭♭7 / Altered dim     |
