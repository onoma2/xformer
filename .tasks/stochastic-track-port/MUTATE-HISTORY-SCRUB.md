# Mutate History Scrubbing

Per-pattern delta log that lets the user scrub backward through recent
destructive Mutate writes. Locked via `/grillme` session 2026-05-25.

## Goal

Destructive Mutate is a one-way trip today — once a step's pitch / duration /
rest is overwritten, the prior value is gone. Adding a small per-pattern log
of recent mutations lets the user step back through them with the encoder and
hear the loop as it was before the last N mutations, without disturbing the
ongoing mutation train.

## Locked Design

| Aspect | Decision |
|---|---|
| Storage | 8-entry delta ring on `StochasticSequence`, persisted with the pattern |
| Delta encoding | `{step:6b, field:2b (pitch/duration/rest), old, new}` ≈ 6 B/entry → ~48 B/pattern |
| Cursor | `int8_t` offset 0..-(fillCount-1), persisted with the pattern |
| Gesture | Loop page, **hold step 5 + encoder**. No Shift modifier. |
| Encoder direction | CW = forward in time (toward 0 / now), CCW = backward (toward older) |
| Audio swap | At next loop boundary (not mid-loop) |
| Cursor model | **Relative offset from now**, not absolute index. New mutations keep pushing; the user's offset stays put while the train rolls past. Zero the Mutate knob to anchor a historical state. |
| Mutated fields | pitch (degree + octave), duration index, rest flag — three types |
| Combined log | Rhythm and melody mutations share the same 8-entry ring |
| Renew | NewR / NewM clears the buffer, then fires 8 synthetic mutations to refill so scrub is immediately useful |
| Live → Loop capture | Same code path as renew → also clears + refills |
| Chip | Top tiny row, 6th slot alongside PR / PM / MU / JU / SL. Format `MH 0`, `MH -3`, etc. Always visible once first renew has run. |
| Held-step Small label | "MUTATE HISTORY -3" when step 5 is held |

## Scrub Mechanic

```
events[N]                     // current "now" state in the model
deltaRing[8]                  // last 8 mutations, oldest first
cursor                        // int8_t, 0..-(fillCount-1)

audible state at cursor = events[N] with deltaRing[fillCount-1 .. fillCount-|cursor|] un-applied
                          (i.e. the |cursor| most-recent deltas reverted)
```

- Each delta is invertible: `old` restored when reverting, `new` re-applied when re-doing.
- New mutation fires: push to deltaRing; if at depth, drop oldest. Cursor stays at user offset; what was at -k now points at a different (newer) delta.
- Compounding works trivially because each delta carries old + new explicitly.

## Implementation Phases (NOT YET WIRED)

1. **Model** — add `MutationDelta[8]` ring + `_mutationCursor` to
   `StochasticSequence`. Serializer fields. Cleared on `clear()`,
   `setSequenceVersion()`, and renew.
2. **Engine** — hook the destructive-mutation write sites (currently in
   `StochasticGenerator::mutate` / engine mutate path). Capture
   `(step, field, oldValue, newValue)` and push via `seq.pushMutation(...)`.
   Apply cursor at trigger time: the loop-boundary cache rebuild reads
   `events[N]` un-applying deltas newer than `cursor`.
3. **Renew hook** — in `renewRhythm` / `renewMelody`, after regen, clear the
   ring then loop 8× calling the existing mutate routine to pre-populate the
   log. Same in `captureLiveAsLoopRhythm` / `captureLiveAsLoopMelody`.
4. **UI** — Loop page hold-step-5 + encoder adjusts `_mutationCursor`. Held
   Small label shows "MUTATE HISTORY -N". Add 6th tiny chip "MH N" to the top
   row (refit width — currently 5 centered, adding a 6th).
5. **Tests** — delta push/pop round-trip, cursor clamp, renew clears, save /
   load roundtrip, train-rolls-on offset semantics, scrub invariance under
   Mutate=0.

## Open Questions

None — design locked.

## File Touchpoints (planned)

- `src/apps/sequencer/model/StochasticSequence.h` — ring + cursor fields,
  push/clear accessors.
- `src/apps/sequencer/model/StochasticSequence.cpp` — serializer + clear logic.
- `src/apps/sequencer/engine/StochasticTrackEngine.cpp` — mutate write
  capture, renew prefill, cursor-aware playback read.
- `src/apps/sequencer/engine/StochasticGenerator.cpp` — emit old/new from
  mutate routine.
- `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.cpp` —
  `drawLoopPage` 6-chip width refit + held-step branch + `editLoopStep` case 5.
- `tests/unit/sequencer/test_stochastic_*` — new test cases.
