# Route Reordering Spec

## Principle

Signal-flow order: what the user touches first appears first. Transport → project → sequence structure → pitch → gate/timing → probability → output routing → module-specific → infrastructure.

## New Target Order

```
None                         0

--- Transport ---
Play                         1
PlayToggle                   2
Record                       3
RecordToggle                 4
TapTempo                     5
Run                          6
Reset                        7
Mute                         8
Fill                         9
FillAmount                  10
Pattern                     11

--- Project ---
Tempo                       12
Swing                       13

--- Sequence Structure ---
FirstStep                   14
LastStep                    15
RunMode                     16
Divisor                     17
ClockMult                   18

--- Pitch ---
Scale                       19
RootNote                    20
Octave                      21
Transpose                   22
Offset                      23

--- Gate / Timing ---
Rotate                      24
GateOffset                  25
GateLength                  26
SlideTime                   27

--- Probability / Bias ---
GateProbabilityBias         28
RetriggerProbabilityBias    29
LengthBias                  30
NoteProbabilityBias         31
ShapeProbabilityBias        32

--- Output Routing ---
CvOutputRotate              33
GateOutputRotate            34

--- Tuesday ---
Algorithm                   35
Flow                        36
Ornament                    37
Power                       38
Glide                       39
Trill                       40
StepTrill                   41

--- Chaos ---
ChaosAmount                 42
ChaosRate                   43
ChaosParam1                 44
ChaosParam2                 45

--- Wavefolder ---
Fold                        46
FoldGain                    47
DJFilter                    48
CurveRate                   49

--- DiscreteMap ---
DMapInput                   50
DMapScanner                 51
DMapSync                    52
DMapRangeHigh               53
DMapRangeLow                54

--- Indexed ---
IndexedA                    55
IndexedB                    56

--- Bus ---
BusCv1                      57
BusCv2                      58
BusCv3                      59
BusCv4                      60

--- Infrastructure ---
CvRouteScan                 61
CvRouteRoute                62
```

## What Moved

| Target | Old Group | Old Value | New Group | New Value |
|--------|-----------|-----------|-----------|-----------|
| Mute | PlayState | 10 | Transport | 8 |
| Fill | PlayState | 11 | Transport | 9 |
| FillAmount | PlayState | 12 | Transport | 10 |
| Pattern | PlayState | 13 | Transport | 11 |
| GateOffset | Tuesday | 42 | Gate/Timing | 25 |
| GateLength | Tuesday | 43 | Gate/Timing | 26 |
| Scale | Sequence | 33 | Pitch | 19 |
| RootNote | Sequence | 34 | Pitch | 20 |
| CvRouteScan | Project | 8 | Infrastructure | 61 |
| CvRouteRoute | Project | 9 | Infrastructure | 62 |

## Internal Changes Required

### `Routing.h` — `enum class Target`
- Rearrange all constants to new order above.
- Each `GroupFirst`/`GroupLast` sentinel pair must bound the new group's range.
- No `= N` explicit assignments needed — enum auto-advance produces new values.

### `Routing.h` — `isXxxTarget()` range checks
- `isTrackTarget()`, `isSequenceTarget()`, `isTuesdayTarget()`, etc. guard on `>= First && <= Last`.
- Since all targets are now in contiguous groups, these checks work with new sentinel values. No logic change, just the sentinel values move.

### `Routing.cpp` — `targetInfos[int(Last)]`
- Designated-initializer indices must update to match new enum values.
- E.g. `[int(Target::Mute)]` stays `[int(Target::Mute)]` — no change needed for name-based access.
- But array order in the source follows the new values if list is maintained sorted.

### `Routing.cpp` — `routedSet[size_t(Target::Last)]`
- Size unchanged (still 63 entries). No change needed.

### `Routing.h` — `targetSerialize()`
- No change. Maps constant names to frozen byte values. Enum renumbering is invisible.

### `Routing.cpp` — `Route::read()`
- No change. Uses `reader.readEnum(_target, targetSerialize)` which reverse-maps from frozen bytes.

### `Routing.cpp` — `Route::write()`
- No change. Writes `targetSerialize(_target)` which returns frozen bytes.

## Non-goals
- No version bump.
- No migration table.
- No display-order array separate from enum.
- No user-facing behavior change in any page.
- No new Target entries.
- No removal of existing Target entries.

## Verification
1. `build/stm32/release && make sequencer` — compiles.
2. `targetInfos[int(Target::Last)]` — every `0..Last-1` has a designated initializer; no zero-initialized gaps.
3. `targetSerialize()` — every old `targetSerialize(x)` returns the same byte as before. Confirm with grep: output bytes are frozen, not `static_cast<uint8_t>(target)`.
