# Scale Rotate spec — Version 0.3

A scale-system feature: **modal rotation of any scale**, selectable by every scale-bearing track
through its existing scale slot. Modes (Dorian, Phrygian, …) are a base scale rotated to a different
degree — so they belong in the scale layer, not in any one track.

> v0.3 corrects v0.2 after adversarial + feasibility review: bias-encoded bitpack, `noteName` octave
> rule, the full caller-migration set, `supportsRotation` as a defaulted virtual, the Stochastic /
> routed-Scale interactions, and the `Minor`-swap blast radius.

## 1. What

Add **`scaleRotate`** as a sibling field to the existing `scale` + `rootNote` selection.

- `scaleRotate = 0` → scale unchanged (identity).
- `scaleRotate = N` → degree N becomes the tonic. **Major + 1 = Dorian**, **Major + 5 = Aeolian**.
- Effective value is `storedRotate mod notesPerOctave` at resolve time (§4); stored sentinel `−1` =
  inherit Project.

`rootNote` is **not** part of this — it stays exactly where it is today (§2).

## 2. Mechanism & API

`Scale::noteToVolts(int)` / `noteFromVolts(float)` take **no root** (`Scale.h:28-29`); consumers add
the chromatic root offset *after* `noteToVolts` (`NoteTrackEngine.cpp:101`). Unchanged.

Rotation is a **`RotatedScaleView(const Scale &base, int rotate)`** adapter implementing the `Scale`
interface through the base's **public** methods (no access to `NoteScale`'s private `_notes[]`):

```
view.noteToVolts(i)   = base.noteToVolts(N + i) − base.noteToVolts(N)        // re-base degree N to 0V
view.noteFromVolts(v) = base.noteFromVolts(v + base.noteToVolts(N)) − N
```

**`noteName` (the subtle one).** `NoteScale::noteName` derives the chromatic name from
`_notes[index]/128 + rootNote` and the octave from `note/_noteCount` with a `while (noteIndex ≥ 12)`
wrap (`Scale.h:62-70`, name line `:66`). The view must **not** raw-delegate `base.noteName(N+i)` (Major
+ 5, root C, degree 0 would mis-print **A**). Instead derive both name **and octave** from the view's
own **rebased volts** (`view.noteToVolts(i)`): chromatic scales → name `(rebased-semitone + root)` with
octave from the rebased value (so Major + 5, root C prints `C D D# F G G# A# C+1` — degree 7 wraps to
`C+1`, *not* the base octave); non-chromatic → degree `i+1` against the rebased index. The `Short2`
octave-only display path (`NoteSequenceEditPage.cpp:328`) consumes exactly that octave.

## 3. Behaviour per scale class

| Class | Rotation |
|-------|----------|
| `NoteScale` (Major/Minor/…/Whole-Tone, incl. non-chromatic n-tet) | rotates the interval table |
| `UserScale` in `Mode::Chromatic` | rotates |
| `VoltScale` ("Voltage") | **no-op** |
| `UserScale` in `Mode::Voltage` | **no-op** |

Rotatability is a **defaulted** virtual `Scale::supportsRotation()` — **`true` in the base**
(`Scale.h:13-41`), overridden **`false`** in `VoltScale` (`Scale.h:125`) and in `UserScale` when
`_mode == Voltage` (`UserScale.h:150`). Pure-virtual would force-touch every subclass — use a default.
It is queried **at resolve time**, so it tracks the *live* UserScale mode (mode is editable —
`UserScaleListModel.h:99`): a stored `scaleRotate` on a user scale silently activates/deactivates as
the user toggles Chromatic↔Voltage, and the stored value **persists** (reactivates if toggled back).
That is intended, not an error.

## 4. Resolver, persistence, RAM

### Resolver — fold rotation into `selectedScale()`

~50 call sites (engine eval + UI display) already route through `sequence.selectedScale(...)`. Change
it to return a **`RotatedScaleView` by value**, with the view's base **always
`Scale::get(effectiveIndex)`** — a long-lived static, **never a wrapped temporary or passed
`const Scale&`**. Resolve from *indices*:

```
effectiveIndex  = sequenceScale  == −1 ? projectScaleIndex  : sequenceScale
effectiveRotate = ((sequenceRotate == −1 ? projectRotate : sequenceRotate) % notesPerOctave + …)
return RotatedScaleView(Scale::get(effectiveIndex), effectiveRotate)
```

**All resolvers change signature to `selectedScale(int projectScale, int projectRotate)`** — they need
`projectRotate` to resolve an inherited (`_scaleRotate == −1`) rotate, which *none* can do today:
- **Int-based**, currently `selectedScale(int defaultScale)` (scale only — **must add `projectRotate`**):
  `NoteSequence.h:409`, `StochasticSequence.h:30`, `PhaseFluxSequence.h:322`, `Project.h:174`.
- **Ref-based**, currently `selectedScale(const Scale&)` (**change to the two ints** — also kills the
  dangle): `DiscreteMapSequence.h:365`, `IndexedSequence.h:638`.
- **Missing** — `TuesdaySequence` has none; add `selectedScale(int, int)`.

So **every** `selectedScale(...)` call site gains a `project.scaleRotate()` argument (~50 sites), not
only the 13 ref-based ones.

### Migration (explicit — enumerated)

1. **All six resolvers gain `projectRotate`**; **every** `selectedScale(...)` caller adds
   `project.scaleRotate()`. The **13 ref-based callers** additionally switch their arg from
   `_project.selectedScale()` to `project.scale(), project.scaleRotate()` ints (wrapping a by-value
   project view as `const Scale&` would dangle): `DiscreteMapTrackEngine.cpp:622`;
   `IndexedTrackEngine.cpp:374,442`; `DiscreteMapSequencePage.cpp:335,808,1015`;
   `DiscreteMapStagesPage.cpp:72`; `IndexedStepsPage.cpp:217`;
   `IndexedSequenceEditPage.cpp:408,434,762,1092,1589`. The int-based callers (e.g.
   `NoteTrackEngine.cpp:424`) just append the second arg.
2. **Non-const bindings** won't compile against return-by-value: `auto &scale` → `const auto &scale` —
   **7 sites** in `StochasticSequenceEditPage.cpp` (853,1147,1239,1264,1474,1484,1586).
3. **Member-captured refs** (compile fine, dangle at runtime): `IndexedSequenceBuilder` stores
   `const Scale &_scale` (`:167`) from `selectedScale()` (`IndexedSequenceEditPage.cpp:1092`). Safe today
   only by synchronous use. Grep for `const Scale &` **members** fed from the resolver and store a view
   *by value* (or the base index + rotate), never a reference.
4. **Tuesday bypass**: `TuesdayTrackEngine.cpp:1657` resolves manually (`project.selectedScale()` /
   `Scale::get()`). Add `TuesdaySequence::selectedScale(idx, rotate)` and migrate that site.

### Owners & persistence

- **`Project`** — global default. `uint8_t _scale`/`_rootNote`, no `−1` sentinel (`Project.h:596,161,180`).
  Gains a `scaleRotate` global default (no sentinel).
- **Six sequences** — `int8_t _scale` with `−1`=inherit. Each gains `_scaleRotate` whose inherit mirrors
  `scale`: `NoteSequence`, `StochasticSequence`, `IndexedSequence`, `DiscreteMapSequence`,
  `PhaseFluxSequence`, `TuesdaySequence`. **Not `CurveSequence`** (no scale/root). (`DiscreteMapSequence`
  root has no inherit — `0..11`, `:215` — but `scaleRotate` keys off `scale`, which does inherit.)
- **RAM — bias-encoded bitpack (this is the corrected arithmetic).** Signed fields can't hold the
  values: a signed 5-bit holds −16..15 (not +23), signed 4-bit −8..7 (not +11). Store **+1-biased
  unsigned**: `scale+1` 0..24 (`Scale::Count≈24`, `Scale.cpp:72`) → **5 b**; `rootNote+1` 0..12 → **4 b**;
  `scaleRotate+1` 0..32 (`−1`..31; magnitude up to 31 for a 32-degree user scale, `CONFIG_USER_SCALE_SIZE
  =32`) → **6 b**. Total **15 b → one `uint16`** (1 spare), replacing the two `int8_t` → **zero added
  bytes**. No compile-time container gate exists (only `Routing.cpp:242` for track bits) — **verify with
  `sizeof` before/after**; don't rely on a guard.
- **Serialization** — write the **three logical values** (bias-decoded) via temporaries; the `uint16`
  packing is an **in-RAM concern only**, not the on-disk form. Unconditional read/write next to `scale`;
  **no `ProjectVersion` bump, no migration** (dev policy). (Note: `IndexedSequence` writes `_rootNote`
  as **two bytes** by design — a historical `Routable<int8_t>` mirror; the read side consumes both
  (`:614-615`). Leave it as-is; just don't replicate the 2-byte pattern for `scaleRotate`.)
- **Not routable** — explicit product decision; `scaleRotate` is set directly, never a `Routing::Target`.

## 5. Built-in scale change — `Minor` → Harmonic Minor

Change `minorScale` from natural minor `0 2 3 5 7 8 10` to **harmonic minor `0 2 3 5 7 8 11`**
(`Scale.cpp:12`; 7th entry `1280`→`1408` in /128 units). Natural minor = `Major + scaleRotate 5`. Silent
dev change. **Blast radius the swap touches (enumerate, don't hand-wave):**
- `Project.cpp:141` and `:197` call `setScale(2) // Minor` as init defaults — the **default/example
  project tonality changes** and those comments go stale. Decide: update the defaults, or accept HM as
  the new default. (Index 2 = `minorScale` slot.)
- `TT2ScaleTables.cpp:14` carries an **independent** `{0 2 3 5 7 8 10}` "Natural Minor" table (Teletype
  path) that does **not** change. After the swap the firmware holds two different "minor" definitions —
  intended (different subsystems), but call it out so it isn't read as an inconsistency.

## 6. Interactions

- **`scaleRotate` is not routable, but `Scale` *is*** (`Routing.h:83`) and `_scale` resolves the routed
  value per tick (`NoteSequence.h:390`, clamped `0..23`, which **`== Scale::Count-1` today** so user
  scales *are* reachable — a brittle duplicated literal, also at `Routing.cpp:381`, that the plan
  fixes; not a live exclusion). So a fixed `scaleRotate` composes on
  **whichever scale resolves this tick**; modulo-at-resolve (§4) keeps it valid as the routed scale's
  degree count changes. Defined behavior, just stated.
- **Stochastic already rotates degrees.** `degreeRotation` (`StochasticSequence.h:234`) rotates the
  *ticket selection* modulo `activeNotes`, derived engine-side as `scale.notesPerOctave()`
  (`StochasticTrackEngine.cpp:368`). Composition order: **`scaleRotate` sets the scale/degree set
  first** (and since `RotatedScaleView` delegates `notesPerOctave()` to the base, rotation reorders
  degrees without resizing `activeNotes`), **then `degreeRotation` rotates selection within it** — not
  independent, composed.

## 7. Scope — out

- `VoltScale` / `UserScale::Voltage` don't rotate (§3 no-op).
- Per-degree masking / ticket rotation = Stochastic's `degreeRotation` (distinct axis; §6 defines the
  composition).
- `CurveSequence` — no scale/root, not a consumer.
- **TT2 output quantize** — `TT2OutputShaping.h:24` quantises **per CV output** via
  `Scale::get(quantizeScale)` (`TeletypeProgram::cvOutputQuantizeScale[]`, set on `TT2IoConfigPage`), a
  *parallel* per-output scale subsystem, never `selectedScale()`. **Deferred** (TT2 is its own
  subsystem) — *not* silently excluded: it should get a `cvOutputScaleRotate[]` companion later for
  consistency. Implementation sketch in `docs/plans/2026-06-23-scale-resolution-normalization.md`
  (Deferred section). It's the only direct-`Scale::get` consumer outside the resolver bodies and Tuesday.
- **Harmony** — its quantisation path is **independent of `selectedScale()`**: `NoteTrackEngine.cpp:969-986`
  reads `harmonyScale()`, builds a `HarmonyEngine`, and harmonises in **MIDI semitones** with its own
  mode enum (serialised at `NoteSequence.cpp:343-355`). So rotation does not reach it today. (The future
  Harmony track, `docs/harmony-spec.md` §2f, will read `(base, scaleRotate)` deliberately.) — *Not* "no
  landed harmony"; the harmony path exists, it just doesn't route through this resolver.

## 8. Acceptance

- `Major + scaleRotate 5` = the *old* built-in natural `Minor` pitch set.
- `scaleRotate 0` bit-identical to today **except the deliberate `Minor`→HM change** (§5).
- `Major + 1` quantises Dorian; **`noteName` prints the rotated tonic incl. the octave wrap** — Major + 5,
  root C: degree 0 → `C`, degree 7 → `C+1` (not `A`, not base octave). Verified in UI display paths
  (`NoteSequenceEditPage`), not just engine eval.
- `VoltScale` / `UserScale::Voltage` with any `scaleRotate` == `scaleRotate 0`; toggling a UserScale's
  mode flips rotate active/no-op live, stored value preserved.
- Selecting a smaller scale keeps `scaleRotate` valid (mod `notesPerOctave`), no stored mutation.
- Stochastic with both `scaleRotate` and `degreeRotation` set produces the composed pitch set per §6.
- Compiles clean: all 7 `StochasticSequenceEditPage` `auto&` converted; all 13 ref-caller sites updated;
  no member `const Scale&` fed from the resolver remains; `sizeof` of each owner sequence unchanged.
- Tuesday rotates via its new `selectedScale()` resolver.
