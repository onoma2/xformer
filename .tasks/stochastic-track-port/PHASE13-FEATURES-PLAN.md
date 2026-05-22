# Phase 13 — Stochastic feature plan

_Drafted 2026-05-22. Updated 2026-05-22 after source/product review. Status: parked behind adversarial-review remediation._

Survey of candidate features (from `vcv-sequencers.md` + gap audit vs Note/Curve/Indexed tracks), with feasibility notes against the existing stochastic engine, CV pipeline, and UI scaffolding.

This is not a request to add generic Eurorack stochastic features by name. The current track already has the core evolving-loop product:

- `Mutate` is already implemented from the Marbles-style loop mutation path and then extended. Positive values permute existing Loop material; negative values destructively regenerate one Loop event.
- Rhythm and melody can already be split between `Loop` and `Live`.
- Patience already evolves Loop material independently for rhythm and melody.
- Mask/Tilt already implement loop thinning and duration-biased survival.
- Repeat already freezes/clusters emitted events at playback time.
- Lock is already a hard runtime evaluated-output freeze, but its runtime/cache contract still needs cleanup.

Phase 13 must preserve that product model. Improvements should make the existing instrument more truthful and faster to use before adding new color knobs.

---

## Ownership contract (Batch 0 prerequisite)

Spec written 2026-05-22 ahead of Patch 1.

**State owners:**

- **`StochasticSequence`** — pattern material: source loop tape (`events()`), ticket weights (`_degreeTickets`, `_durationTickets`), Marbles bell knobs, masks, rotations, sequence-level validity flags (`_rhythmValid`, `_melodyValid`), seeds. Persisted with the project.
- **`StochasticTrack`** — track-global controls only: Lock toggle, Divisor, Clock Mult, Reset Measure, Scale, RootNote, Octave, Transpose, CV Update mode, Slide Time, Fill Mode, PlayMode. Persisted with the project (Lock excepted — runtime-only, see review #6).
- **`StochasticTrackEngine`** — runtime playback state: gate/CV queues, locked evaluated history (`_lockedParents`), repeat cache (`_lastEvent`), patience counters, jump register, sleep counter, pattern index, mask/tilt cache fields. **Not persisted.**

**Naming, going forward (replaces the ambiguous "events" everywhere):**

- **"Source loop tape"** = `StochasticSequence::events()` — the source material the engine reads when playing.
- **"Evaluated history"** = the final CV / rest / legato / children that came out of `triggerStep` after mask + jump + root + scale + burst evaluation.
- **"Lock capture"** = the frozen evaluated history stored in `_lockedParents` for lock-mode replay.

**Write boundaries:**

- The **engine has the right to mutate** the sequence (Loop tape regeneration, mutation/permutation, validity flags, seed bumps) and the track where applicable. This is now explicit: `_stochasticTrack` and `_sequence` are non-const references. **No `const_cast` in the stochastic engine.**
- **Validity flags** (`_rhythmValid`, `_melodyValid`) belong with source tape regeneration. They are sequence-level, written by engine at cycle boundaries (patience) or generation calls, never mid-audio-event.
- **Live mode writeback** (the actual race risk from review #1) currently writes `events()[readIndex]` mid-audio. Patch 1 makes the ownership boundary explicit; Patch 3 (optional) will move this to a separate engine-owned shadow if the UI needs tear-free truth.

**Stage gates:**

- **Patch 1 (landed 2026-05-22) — const ownership only, no behavior change.** Dropped all 9 `const_cast` sites in `StochasticTrackEngine.cpp` (5 sequence + 4 track). Stored `_stochasticTrack` as `StochasticTrack&` and `_sequence` as `StochasticSequence*`. STM32 release builds clean (`.text=888,972`, +1,760 B vs prior baseline from const-folding loss); four stochastic test suites pass.
- **Patch 2 (landed 2026-05-22) — helper extraction, no behavior change.** Added local accessors `sequence()` / `stochasticTrack()` (mutable + const overloads) and pulled the writeback paths into intent-named private helpers: `ensureLoopSources(scale, rootNote)`, `writeLiveRhythmShadow(readIndex, rhythm)`, `writeLiveMelodyShadow(readIndex, melody)`, `captureLockedParent(readIndex, finalCv, durationTicks, isRest, isLegato, isSlide, evalChildren, activeNotes, scale, rootNote)`. `triggerStep` now opens with a single `ensureLoopSources` call instead of an inlined regen block; Live writeback is one helper call per domain; lock capture is one helper call. Engine code reads as a story. STM32 release builds clean (`.text=889,080`, +108 B helper-call overhead); four stochastic test suites pass.
- **Patch 3 (optional, later) — Direct hero + event-history truth source.** If we want a tear-free walker that reads truth from the engine instead of the (potentially racy) source tape, add a tiny `LastEvaluatedEvent` accessor and have the walker read it. Do not overload `StochasticSourceEvent` for evaluated reality.

**Acceptance criteria for each patch:**

- No `const_cast` in stochastic engine.
- No source material writes hidden behind const references.
- Commented contract near `_sequence`, `_lockedParents`, and `sequence.events()` usage sites.
- STM32 `make sequencer` passes.
- Patches 1 & 2: no intended sound/behavior changes.

---

## Phase 13 gate — repair product truth first

Do these before new feature work. They come from `docs/stoch-review.md` and from rereading the live model/engine/UI source.

1. **Resolve model-vs-engine ownership for generated loop tape.** _(Patch 1: const ownership landed 2026-05-22; Patches 2/3 pending.)_
   `StochasticTrackEngine::triggerStep()` previously cast away constness and wrote generated Live/Loop material into `StochasticSequence::events()` during playback. Patch 1 made the ownership explicit by storing the track/sequence as non-const references on the engine — no `const_cast` remains. Patches 2 and 3 will pull the writeback paths into intent-named helpers and optionally relocate the Live writeback to an engine-owned shadow.

2. **Make Lock pattern-safe and explicit.**
   The current lock cache is engine-local, heap allocated, and keyed by event index only. It should be cleared/scoped on pattern change, or have pattern identity. Decide whether the `Lock` flag itself is performance-only or persistent. If performance-only, document/label it as runtime state; if persistent, serialize it.

3. **Add stochastic page type guards.**
   `StochasticSequenceEditPage` directly calls `selectedTrack().stochasticTrack()` in draw/input/LED paths. TopPage remapping covers the normal path, but the page should locally guard type-specific accessors or the navigation layer must guarantee replacement for every selected-track/mode mutation path.

4. **Hide or implement stochastic `PlayMode`.**
   `StochasticTrack` exposes/persists PlayMode and the config page shows it, but `StochasticTrackEngine::tick()` never branches on it. Until real Aligned/Free timing exists, the UI should not imply different behavior.

5. **Audit exposed-but-unused track controls.**
   `FillMode` exists on `StochasticTrack` and is shown by config plumbing, but the stochastic engine does not appear to consume it. Treat it like PlayMode: implement it, hide it, or mark it explicitly as reserved.

6. **Add lock allocation feedback.**
   If the heap allocation for evaluated-output lock cache fails, Lock silently behaves as unlocked. This is unacceptable on a RAM-critical firmware path.

---

## Streamlining before new features

These are product/UI changes that should land before or alongside Phase 13 feature work.

### 1. Make Mutate self-explanatory

Do not add a separate mutation/freezing feature by another module name. `Mutate` already owns loop mutation.

Improve held-step/list feedback:
- `MUT +NN PERM` for positive values.
- `MUT -NN REGEN` for negative values.
- `MUT 0 HOLD` or `MUT 0 OFF` for zero.

This makes the current bipolar behavior legible without adding state or a duplicate knob.

### 2. Make source state visible everywhere

Direct and Loop pages already expose rhythm/melody source buttons. Keep that, but make the same state obvious on all stochastic editing surfaces:

- `R:L M:L`, `R:V M:L`, etc. in compact headers/footers where possible.
- Show when a Loop domain is invalid/stale or due to regenerate if the engine exposes that safely.
- Keep `Refresh` distinct from `Patience`: Refresh is manual loop repopulation; Patience is automatic loop evolution.

### 3. Make Lock semantics visible

If Lock stays runtime-only, do not let users infer it is saved project state. Label/feedback should make it feel like a performance latch, not a persisted sequence property.

### 4. Make Direct visualization reflect evaluated events

The Direct page reads live CV/gate from the engine, but some rest/burst visual state is still inferred from parameters/page timing rather than the last evaluated stochastic event. Expose a small read-only event summary from `StochasticTrackEngine` after the ownership fix:

- rest/pass state
- parent duration slot
- child count
- slide/legato flags
- active read index

Then draw the actual emitted event, not a parameter proxy.

---

## Reference: stochastic CV pipeline as it stands

`StochasticTrackEngine::triggerStep` is where everything lands:

1. Pick `note` from scale degree + octave + jumpRegister + track.octave + track.transpose
2. `finalCv = scale.noteToVolts(note) + (chromatic ? rootNote/12 : 0)` — **quantization happens here**, single point of contact with the Scale
3. Push `{tick, finalCv, slide}` onto `_cvQueue`
4. Push `{tick, gateOn}` and `{tick+gateLen, gateOff}` onto `_gateQueue`
5. For burst: `evaluateChildren` computes per-child `tickOffset = (i+1) * spacing`, `gateTicks`, and a re-quantized child CV via `scale.noteToVolts(childNote)`

Then in `tick(...)`:
- Pop `_cvQueue` when `tick >= ev.tick` → `_cvOutputTarget = ev.cv; _slideActive = ev.slide`
- Pop `_gateQueue` → `_gateOutput = ev.gate`

Then in `update(dt)` (every audio tick, ~1 kHz):
- If `_slideActive && slideTime > 0`: `_cvOutput += (target - _cvOutput) * dt/(slideTime/1000+1)` (linear approximation, NOT the shared `Slide::applySlide` helper used by every other track)
- Else: `_cvOutput = _cvOutputTarget` instantly

**Two divergences from other tracks worth noting:**
- Stochastic's `update(dt)` rolls its own slide math instead of calling `Slide::applySlide` (RC-shaped exponential). Other tracks (Note, Curve, Indexed, MidiCv, Teletype) all use the helper. Fixing this is essentially free and would harmonize the slide shape across track types.
- Quantization is committed at queue-push time (parent + each child). Anything that wants to bypass quantization (intentional detune / wobble) must run **after** queue pop, i.e. on `_cvOutput` in `update(dt)`.

---

## Feature A — Wobble (post-quant pitch drift)

**Source:** post-quant drift/wow feature candidate from the VCV survey, not related to the existing `Mutate` implementation.

**What:** Continuous random micro-modulation added to the emitted CV between events. Adds tape-style pitch drift.

**Where it lands:** `update(dt)` — **after** the Slide step, **after** queue pop. Bypasses scale quantization deliberately.

**Sketch:**
```cpp
void StochasticTrackEngine::update(float dt) {
    if (_slideActive && _stochasticTrack.slideTime() > 0) {
        _cvOutput = Slide::applySlide(_cvOutput, _cvOutputTarget, _stochasticTrack.slideTime(), dt);
    } else {
        _cvOutput = _cvOutputTarget;
    }
    if (sequence.wobble() > 0) {
        _wobblePhase += dt * 0.5f;                 // ~2 Hz LFO base
        float lfo = std::sin(_wobblePhase * float(M_PI * 2.f));
        float noise = (float(_rng.nextRange(1024)) / 512.f) - 1.f;
        float amount = sequence.wobble() / 100.f;
        _cvOutput += amount * (0.6f * lfo + 0.4f * noise) * 0.04f;  // ±0.04V at full
    }
}
```

**Model:** one new `_wobble` knob (0–100) on `StochasticSequence`. Routable.

**State:** `float _wobblePhase = 0.f` on engine. ~4 B.

**Cost:** ~25 lines engine, ~10 lines model, ~6 lines UI binding.

**Knob slot:** Loop hero (currently has 5/6/7/12/13 free) — slot 12 (`WOBL`) reads cleanly.

**Quantization note:** Wobble is intentionally **post-quant**. A 100% wobble at C4 will not snap to C# but float between. That's the entire feel — track Lock + Wobble → "tape with character".

**Priority note:** do after truth/UX cleanup and after Burst Strum. Wobble is a color knob; it should not land before the core loop/timing contract is honest.

---

## Feature B — Per-event glide shape (Phaseque EXPR_CURVE)

**Source:** ZZC Phaseque.

**What:** Right now `slide` is binary (on/off). Add a shape selector: Linear / Exp / Log / S-curve. Applies between consecutive non-rest events.

**Where it lands:** `update(dt)` — replace the current ad-hoc slide math with a shape-aware glider. The `Slide::applySlide` helper is RC-style exponential by default; expanding it to a shape arg is the minimal-touch path.

**Sketch:**
```cpp
// Slide.h
enum class Shape : uint8_t { RC, Linear, Exp, Log };
static float applyShapedSlide(float current, float target, int slideTime, float dt, Shape shape, float &progress);
```

Stochastic stores per-event slide shape via a 2-bit field in `StochasticSourceEvent` bytes[4..5] (currently has spare bits) OR as one global shape per sequence (simpler; matches per-sequence slideTime).

**Sequence-level shape is the right starting point:**
- One enum field on `StochasticSequence` (or `StochasticTrack` alongside `slideTime`).
- Zero per-event storage cost.
- UI: footer toggle on Loop page or a Config-page entry.

**Per-event shape later** if the global version proves limiting — requires extending `StochasticSourceEvent` (need to verify bit budget; struct is currently 6 bytes packed tight).

**Cost:** ~30 lines `Slide.h` (new shaped variants) + ~5 lines per-track update() call switch + 1 model field.

**Quantization note:** Doesn't touch quantization. Slide ramps between two already-quantized targets, regardless of shape.

---

## Feature C — Burst strum

**Source:** OrangeLine Gator + Phaseque SHIFT.

**What:** Currently burst children land at uniform `spacing = durationTicks / spacingDenom`. Add a knob that progressively skews child timing — positive = back-loaded (accelerando into the end), negative = front-loaded (decelerando), zero = uniform (current behavior).

**Where it lands:** `StochasticGenerator::evaluateChildren` — modify the `offset = (i+1) * spacing` line.

**Sketch:**
```cpp
// Strum: bend the per-child timing along a quadratic curve.
// strum = -100 → front-loaded; 0 → uniform; +100 → back-loaded.
int strum = sequence.burstStrum();
for (int i = 0; i < count; ++i) {
    float t = float(i + 1) / float(count);                // 0..1
    float curve = t + (strum / 100.f) * (t * t - t);     // bend along t^2
    uint32_t offset = uint32_t(curve * count * spacing);
    ...
}
```

**Model:** one new `_burstStrum` field on `StochasticSequence` (bipolar, -100..+100). Routable.

**State:** none — pure calculation per event.

**Cost:** ~10 lines generator, ~6 lines model, ~6 lines UI binding.

**Knob slot:** DIRECT page burst sub-set is already at 4-7 (BURS/BCNT/BRAT/BPIT, all 4 burst slots filled). Strum needs to bump into the Loop page or borrow a Direct slot. Cleanest path: **add to LOOP page** (slot 12 or 13) since LOOP is already the "advanced rhythm" page.

**Quantization note:** Timing only — children are still quantized in pitch by the existing `scale.noteToVolts(childNote)` call. Strum doesn't touch CV.

**Eval-time clip:** existing `offset + gate >= durationTicks → break` already handles the case where back-loaded strum pushes children off the end of the parent. No extra guard needed.

**Priority note:** this is the best new musical feature candidate after remediation. It extends an already implemented burst system instead of creating a parallel concept.

---

## Feature D — Playback direction (RunMode)

**Source:** existing `Types::RunMode` enum already used by Note, Curve, Indexed; also matches dbRack Uno PENDULUM and many others.

**What:** Replace stochastic's hardcoded `_patternIndex++; if (>last) → first` with `SequenceState::advanceFree(runMode, first, last, rng)`. Adds Forward (current), Backward, Pendulum, PingPong, Random, RandomWalk for free.

**Where it lands:** `StochasticTrackEngine::triggerStep` — the `_patternIndex++ / wrap` block at line 463-466.

**Sketch:**
```cpp
// Replace:
//   _patternIndex++;
//   if (_patternIndex > sequence.last()) {
//       _patternIndex = sequence.first();
//       _patternCycleEnded = true;
//       ...post-cycle hooks...
//   }
//
// With:
uint32_t prevIter = _sequenceState.iteration();
_sequenceState.advanceFree(sequence.runMode(), sequence.first(), sequence.last(), _rng);
_patternIndex = _sequenceState.step();
if (_sequenceState.iteration() != prevIter) {
    _patternCycleEnded = true;
    // ...existing post-cycle hooks (jump, sleep, patience, mutate)...
}
```

**Model:** one new `_runMode` field on `StochasticSequence` (or a re-derived view of the unused `_playMode` slot — but that's `Aligned/Free` semantics, not direction; should stay separate).

**State:** `SequenceState _sequenceState;` on engine. 7 bytes packed.

**Reset semantics:** `resetMeasure()` should call `_sequenceState.reset()` alongside `_patternIndex = first()`.

**Cost:** ~20 lines engine, ~6 lines model, ~6 lines UI binding (LOOP page slot or footer Fn).

**Interaction notes:**
- Rotate (`sequence.rotate()`) already runs on `readIndex` inside `triggerStep`. Keep that mechanism; RunMode just drives which raw index is the "current step".
- Direction reversal interacts with mutation window — mutation already targets `[first, last]` by random pick (windowSize), not by direction, so no change needed there.
- Backward + Loop mode: replays the stored events in reverse. Works.
- Random + Live mode: stochastic-on-stochastic. Works, but probably musically pointless — Random already comes from the rhythm/melody generator. Worth a footer hint at design time.

**Naming note:** RunMode is playback direction/order. It is not `PlayMode`. Do not reuse the existing `PlayMode` field for this; Aligned/Free timing and Forward/Backward/Pendulum direction are different concepts.

**Priority note:** useful, but it should not bypass the older V5 warning that random playback order can be illegible. Start with Forward/Backward/Pendulum/PingPong. Add Random/RandomWalk only if the UI can explain the result.

---

## Generic gaps vs Note/Curve/Indexed tracks

Audit of what stochastic is **missing** relative to the established track type conventions.

### 1. Context menu (Init/Copy/Paste/Dupl/Gen)

**Other tracks:** NoteTrack has 5 items (INIT, COPY, PASTE, DUPL, GEN), all wired to clipboard ops on the sequence.

**Stochastic:** 3 items (INIT, EVEN, RANDOM) — and the hero pages explicitly skip them (`Hero pages don't yet have INIT/EVEN/RAND semantics — no-op for now`).

**What's missing:**
- **COPY / PASTE** for the whole stochastic sequence (knobs + tickets + degree masks + duration tickets). Cross-pattern movement is unreachable without scratch projects.
- **DUPL** (duplicate current pattern to next pattern slot). Same value as on NoteTrack.
- **GEN** — would launch the Generator page if stochastic had a Generator backend. Stochastic generates itself, so GEN could be "RENEW" (already exists as F-button) or "INIT from preset". Lower priority.

**Cost:** ~80 lines (clipboard ops + context menu wiring). The clipboard pattern is already established in `NoteSequenceClipboard` — add a `StochasticSequenceClipboard` next to it.

**Hero-page INIT semantics:** the hero pages currently no-op on INIT/EVEN/RAND. The cleanest fix: hero-page INIT runs through the same clipboard `init()` for the full sequence; EVEN/RAND stay on ticket pages where they make sense.

### 2. Quick edits (Page + step shortcut)

**Other tracks:** NoteTrack binds 8 step buttons (slots 8-15 with `Key::Page` held) to `NoteSequenceListModel` rows — FirstStep, LastStep, RunMode, DivisorX, ResetMeasure, Scale, RootNote. Open via `_manager.pages().quickEdit.show(_listModel, row)`.

**Stochastic:** `key.isQuickEdit()` exists on the ticket pages (Pitch/Duration), and there it dispatches to INIT/EVEN/RAND only. **No quick-edit binding for sequence-level settings.** All track-level config (Divisor, ClockMult, Scale, RootNote, etc.) requires the Setup page detour.

**What's missing:** Add an 8-slot quickEditItems[] for stochastic hero pages, pointing at `StochasticConfigListModel` rows. Candidates (already in the config list model):
- `Divisor`
- `ClockMult`
- `ResetMeasure`
- `Scale`
- `RootNote`
- `Octave`
- `Transpose`
- `SlideTime`

**Cost:** ~25 lines (table + binding in `keyPress`, LED hint in `updateLeds`).

### 3. USB keyboard support

**Other tracks:** NoteTrack/CurveTrack/IndexedTrack each override `keyboard(KeyboardEvent &event)` to handle note entry, arrow navigation, function keys, and per-track shortcuts.

**Stochastic:** **no override.** Inherits `BasePage::keyboard()` only — F1-F5, Tab, arrows, Esc. No degree entry from keyboard, no quick-tab to ticket banks, no note picker.

**What's missing:**
- A-Z note keys → set degree/transpose at selected slot. The unified picker uses degree tickets, so keyboard input could nudge `degreeTickets[i]` for the pressed note's degree.
- Number keys 1-8 → select duration ticket slot directly (mirror IndexedTrack's number-key step jump).
- Page-Up/Page-Down → swap between hero pages (NEXT shortcut without F5).
- Modifier + digit → set bank/transpose octave.

**Cost:** ~60-100 lines — match the verbosity of IndexedSequenceEditPage::keyboard (~120 lines covering all of the above).

**Priority:** lower than context menu / quick edits. USB keyboard is power-user territory; the two above are visible to every user.

---

## Implementation order

Ranked by current product risk first, then `(musical-impact × user-visibility) / engineering-cost`:

| # | Feature | Cost | Visibility | Notes |
|---|---|---|---|---|
| 0 | Review remediation gate | medium | high | Fix model/engine ownership, lock cache identity, page guards, PlayMode truth, FillMode truth, lock allocation feedback. No new features before this. |
| 1 | Mutate/Lock/source-state clarity | small | high | Rename/readout work only. Makes the existing Marbles-derived loop mutation and runtime lock behavior legible. |
| 2 | Context menu (Copy/Paste/Dupl) | medium | high | Required for moving stochastic content between patterns. This is a product workflow gap, not a new engine feature. |
| 3 | Quick edits | small | medium | Standard track-type convention; closes the Setup-page detour for common timing/scale controls. |
| 4 | Harmonize slide path to `Slide::applySlide` | trivial | invisible | Should land before glide-shape work. Current ad-hoc math is the odd one out across track types. |
| 5 | Burst strum | small | high | Best new musical control. Extends existing burst behavior with timing feel. |
| 6 | Playback direction (RunMode) | small/medium | high | Useful, but distinct from PlayMode. Start with deterministic directions before Random/RandomWalk. |
| 7 | Per-event glide shape (global first) | small | medium | Cleanest as a sequence-level Shape enum; per-event later if bit budget and UI need justify it. |
| 8 | Wobble | small | medium | Post-quant color knob. Good late-stage character feature; not core contract. |
| 9 | USB keyboard support | medium | low (power users) | Worth doing last, when the surface stabilizes. |

**Suggested batching:**
- **Batch 0 (correctness):** item 0. No feature work. STM32 build + targeted stochastic tests after changes.
- **Batch 1 (clarity/workflow):** items 1, 2, 3. Mostly UI/list/clipboard; should not alter stochastic generation sound.
- **Batch 2 (engine feel):** items 4, 5, 6. Slide helper, Burst Strum, then RunMode/direction. Hardware-audible.
- **Batch 3 (long tail):** items 7, 8, 9. Glide shape, Wobble, USB keyboard.

## Out of scope (Phase 14+)

From the VCV survey, candidates that don't land here:

- **Euclidean rhythm overlay** — promising but interacts with the existing rest knob + window logic in non-obvious ways. Defer until the simpler features above settle.
- **Per-event chance re-roll** (Tracker CHANCE) — small, but overlaps with existing Patience / Mutate semantics. Risk of three knobs that all "destabilize Loop content".
- **Mother-style scale weight RNG presets** — already covered by `degreeTickets`, would just be a UI shortcut. Lives in the context menu / generator question, not the engine.
