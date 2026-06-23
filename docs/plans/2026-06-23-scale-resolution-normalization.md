# Scale Resolution Normalization + Scale Rotate Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Unify the scale/rootNote resolution subsystem across all six track sequences + Project onto one resolver signature, one inherit convention, and one storage layout — then land `scaleRotate` (modal rotation) and the `Minor`→Harmonic-Minor swap on the clean base.

**Architecture:** Today scale/root handling is inconsistent (two resolver signatures, two storage types, mismatched inherit sentinels, a brittle hardcoded `23` scale clamp duplicated across seven sites, and two manual bypasses — Tuesday and TT2). We normalize by routing every consumer through one `selectedScale(int projectScale, int projectRotate)` resolver that returns a `RotatedScaleView` by value (base always `Scale::get(index)`, never a wrapped temporary). Modal rotation is a scale-layer feature delivered through that resolver, so every track gets modes for free. Design reference: `docs/scale-rotate-spec.md` v0.3 — **read it before starting.**

**Tech Stack:** C++ (STM32F405 firmware + host sim), custom `UnitTest.h` framework, CMake. Unit tests in `src/tests/unit/sequencer/`. No `ProjectVersion` bump, no migration code (dev-branch policy). TDD where logic is non-trivial; characterization (existing tests stay green) for mechanical migrations.

**Build/test commands (executor runs these):**
- Unit test build + run: `make -C build <TestName>` then `./build/src/tests/unit/sequencer/<TestName>`
- Sim build (compile gate): `make -C build/sim/debug sequencer`
- STM32 release (RAM/flash gate): `make -C build/stm32/release sequencer`

**Discipline (hard rule, learned this session):** before asserting any codebase fact in a task, `grep`/read to confirm it. Cite only `file:line` you've opened. Enumerate full sets (all callers, all resolvers), never a sample.

**Commits (local repo rule):** do **NOT** `git add` / `git commit`. Each task ends at a **checkpoint** — stop, report the diff, and ask the user to commit (or commit only if they explicitly request it). The "Checkpoint" steps below carry a suggested message only.

**Scope boundaries:**
- IN: the 6 sequence resolvers (Note, Stochastic, PhaseFlux, DiscreteMap, Indexed, Tuesday) + Project; `scaleRotate` field + `RotatedScaleView` + `supportsRotation()`; **the UI rows to set it** (Phase F); the `Minor`→HM swap.
- OUT: `CurveSequence` (no scale/root). **TT2 output quantize — DEFERRED** (its own per-output scale subsystem; implementation sketch in the Deferred section below — *not* a silent exclusion). `scaleRotate` is **not** routable. Full-header bitpack beyond the scale group.

---

## Phase A — `supportsRotation()` + `RotatedScaleView` (pure, no wiring)

### Task A1: Add `supportsRotation()` as a defaulted virtual

**Files:**
- Modify: `src/apps/sequencer/model/Scale.h` (base class ~`:13-41`; `VoltScale` ~`:125`; `NoteScale` if it must differ — it does not)
- Modify: `src/apps/sequencer/model/UserScale.h` (~`:150`, gate on `_mode`)
- Test: `src/tests/unit/sequencer/TestScale.cpp`

**Step 1: Write the failing test** (append to `TestScale.cpp`)

```cpp
CASE("supportsRotation: note scales rotate, Voltage built-in does not") {
    expectTrue(Scale::get(1).supportsRotation(), "Major rotates");        // index 1 = Major
    // Find the built-in by NAME (exact — don't rely on a fragile index, and don't let a
    // chromatic note scale satisfy the assertion vacuously).
    int voltIdx = -1;
    for (int i = 0; i < Scale::Count; ++i)
        if (strcmp(Scale::name(i), "Voltage") == 0) { voltIdx = i; break; }
    expectTrue(voltIdx >= 0, "Voltage built-in present");
    expectTrue(!Scale::get(voltIdx).supportsRotation(), "Voltage scale does not rotate");
}
```

Ensure `#include <cstring>` is present in `TestScale.cpp` (for `strcmp`).

**Step 2: Run to verify it fails** — `make -C build TestScale && ./build/src/tests/unit/sequencer/TestScale` → FAIL: `supportsRotation` not a member.

**Step 3: Implement**

In `Scale.h`, base class public section:
```cpp
virtual bool supportsRotation() const { return true; }
```
In `VoltScale` (`Scale.h:~125`):
```cpp
bool supportsRotation() const override { return false; }
```
In `UserScale` (`UserScale.h`):
```cpp
bool supportsRotation() const override { return _mode != Mode::Voltage; }
```

**Step 4: Run to verify pass** — same command → PASS.

**Step 5: Checkpoint** — stop; report the diff, ask the user to commit. Suggested message: `feat(scale): add supportsRotation() defaulted virtual`.

---

### Task A2: `RotatedScaleView` adapter

**Files:**
- Create: `src/apps/sequencer/model/RotatedScale.h`
- Test: `src/tests/unit/sequencer/TestRotatedScale.cpp` + register in `src/tests/unit/sequencer/CMakeLists.txt`

**Step 1: Write failing tests** (`TestRotatedScale.cpp`)

```cpp
#include "UnitTest.h"
#include "apps/sequencer/model/Scale.h"
#include "apps/sequencer/model/RotatedScale.h"
#include "core/utils/StringBuilder.h"
#include <cstring>

UNIT_TEST("RotatedScale") {

CASE("rotate 0 is identity vs base") {
    const Scale &major = Scale::get(1);
    RotatedScaleView v(major, 0);
    for (int n = 0; n < 7; ++n)
        expectEqual(v.noteToVolts(n), major.noteToVolts(n), "identity at rotate 0");
}

CASE("Major rotate 5 equals natural-minor interval set") {
    const Scale &major = Scale::get(1);
    RotatedScaleView v(major, 5);            // Aeolian
    // natural minor degrees in semitones: 0 2 3 5 7 8 10
    const int expect[7] = {0,2,3,5,7,8,10};
    for (int n = 0; n < 7; ++n)
        expectEqual(int(v.noteToVolts(n) * 12.f + 0.5f), expect[n], "aeolian semitone");
}

CASE("noteFromVolts round-trips through rotate") {
    const Scale &major = Scale::get(1);
    RotatedScaleView v(major, 2);            // Dorian
    for (int n = 0; n < 7; ++n)
        expectEqual(v.noteFromVolts(v.noteToVolts(n)), n, "roundtrip");
}

CASE("voltage scale ignores rotate (no-op)") {
    VoltScale volt("V", 0.1f);                 // VoltScale ctor is public (used at Scale.cpp:38)
    expectTrue(!volt.supportsRotation(), "voltage scale not rotatable");
    RotatedScaleView v(volt, 3);
    for (int n = -3; n < 5; ++n)
        expectEqual(v.noteToVolts(n), volt.noteToVolts(n), "rotate is a no-op on voltage scale");
}

CASE("rotate wraps modulo on smaller scale") {
    const Scale &major = Scale::get(1);      // 7 notes
    RotatedScaleView v(major, 9);            // 9 mod 7 == 2 == Dorian
    RotatedScaleView d(major, 2);
    for (int n = 0; n < 7; ++n)
        expectEqual(v.noteToVolts(n), d.noteToVolts(n), "9 wraps to 2");
}

CASE("noteName names rebased tonic with octave wrap (Major+5 root C)") {
    const Scale &major = Scale::get(1);
    RotatedScaleView v(major, 5);
    FixedStringBuilder<8> s0; v.noteName(s0, 0, 0, Scale::Long);  // degree 0
    expectEqual(strncmp((const char *)s0, "C", 1), 0, "degree 0 names C, not A");
    FixedStringBuilder<8> s7; v.noteName(s7, 7, 0, Scale::Long);  // degree 7 wraps an octave
    expectTrue(strncmp((const char *)s7, "C", 1) == 0 && strstr((const char *)s7, "+1") != nullptr,
               "degree 7 names C+1");
}

}
```

API note: `StringBuilder` exposes `operator const char *()` (`StringBuilder.h:37`); `FixedStringBuilder<N>` is the concrete type (`:50`). There is **no** `startsWith`/`contains` — use `strncmp`/`strstr` on the `const char *` conversion (hence `<cstring>`).

**Step 2: Run to verify fail** — register then `make -C build TestRotatedScale` → FAIL (no header).

CMakeLists addition (after `:34`):
```cmake
register_sequencer_test(TestRotatedScale TestRotatedScale.cpp)
```

**Step 3: Implement `RotatedScale.h`**

```cpp
#pragma once
#include "Scale.h"
#include "core/utils/StringBuilder.h"

// A modal-rotation view over a base Scale: degree N becomes the tonic.
// Works entirely through the base's public methods; base must outlive the view
// (always constructed from Scale::get(), a long-lived static).
class RotatedScaleView : public Scale {
public:
    RotatedScaleView(const Scale &base, int rotate)
        : Scale("rot")
        , _base(base)
        , _rotate(normalize(base, rotate))
    {}

    bool isChromatic() const override { return _base.isChromatic(); }
    int notesPerOctave() const override { return _base.notesPerOctave(); }
    bool supportsRotation() const override { return _base.supportsRotation(); }

    float noteToVolts(int note) const override {
        if (_rotate == 0) return _base.noteToVolts(note);
        return _base.noteToVolts(note + _rotate) - _base.noteToVolts(_rotate);
    }

    int noteFromVolts(float volts) const override {
        if (_rotate == 0) return _base.noteFromVolts(volts);
        return _base.noteFromVolts(volts + _base.noteToVolts(_rotate)) - _rotate;
    }

    void noteName(StringBuilder &str, int note, int rootNote, Format format) const override {
        if (_rotate == 0) { _base.noteName(str, note, rootNote, format); return; }
        // Name from the REBASED pitch, not base degree (note + _rotate).
        if (isChromatic()) {
            // rebased volts -> absolute chromatic semitone, then name+octave like NoteScale.
            float volts = noteToVolts(note);
            int totalSemis = int(volts * 12.f + (volts < 0 ? -0.5f : 0.5f)) + rootNote;
            int octave = (totalSemis >= 0 ? totalSemis : totalSemis - 11) / 12;
            int idx = totalSemis - octave * 12;
            if (format == Short1 || format == Long) Types::printNote(str, idx);
            if (format == Short2 || format == Long) str("%+d", octave);
        } else {
            int n = notesPerOctave();
            int octave = (note >= 0 ? note : note - (n - 1)) / n;
            int degree = note - octave * n + 1;
            if (format == Short1 || format == Long) str("%d", degree);
            if (format == Short2 || format == Long) str("%+d", octave);
        }
    }

private:
    static int normalize(const Scale &base, int rotate) {
        if (!base.supportsRotation()) return 0;
        int n = base.notesPerOctave();
        return ((rotate % n) + n) % n;
    }
    const Scale &_base;
    int _rotate;
};
```

(Reference only — adjust `printNote`/`StringBuilder` calls to the real APIs found in step 1/2. The octave-from-rebased-volts branch is the load-bearing part; the test pins its behavior.)

**Step 4: Run to verify pass** — `make -C build TestRotatedScale && ./build/src/tests/unit/sequencer/TestRotatedScale` → PASS.

**Step 5: Checkpoint** — stop; report the diff, ask the user to commit. Suggested message: `feat(scale): RotatedScaleView modal rotation adapter`.

---

## Phase B — `scaleRotate` storage (bias-packed) + serialization

### Task B1: Bias-packed scale group in each owner

**Files (each gets the same transform):**
- Modify: `model/NoteSequence.h` (`_scale`/`_rootNote` ~`:786-787`), `model/StochasticSequence.h` (~`:890-891`), `model/PhaseFluxSequence.h` (~`:588-589`), `model/DiscreteMapSequence.h` (~`:413-414`), `model/IndexedSequence.h` (~`:684-685`), `model/TuesdaySequence.h` (~`:544`), `model/Project.h` (`uint8_t` ~`:596-597`)
- Test: `src/tests/unit/sequencer/TestScaleGroupStorage.cpp` (+ register)

**Encoding (from spec §4):** store `scale+1` (5b, 0..24), `rootNote+1` (4b, 0..12), `scaleRotate+1` (6b, 0..32) in a `uint16` bitfield, replacing the two `int8_t`. `-1` = inherit (sequences); Project has no `-1`.

**Step 1: Failing test** — round-trip get/set incl. `-1` sentinel; `sizeof` unchanged:
```cpp
CASE("scaleRotate get/set roundtrip incl inherit sentinel") {
    NoteSequence s;
    s.setScaleRotate(5); expectEqual(s.scaleRotate(), 5, "set 5");
    s.setScaleRotate(-1); expectEqual(s.scaleRotate(), -1, "inherit sentinel");
}
CASE("scale group: zero size growth, all owners + container") {
    // Constants are PRE-MEASURED in Step 0 below (paste exact bytes). Assert ==, not <=.
    expectEqual(int(sizeof(Project)),             PRE_PROJECT,   "Project unchanged");
    expectEqual(int(sizeof(NoteSequence)),        PRE_NOTE,      "NoteSequence unchanged");
    expectEqual(int(sizeof(StochasticSequence)),  PRE_STOCH,     "Stochastic unchanged");
    expectEqual(int(sizeof(PhaseFluxSequence)),   PRE_PHASEFLUX, "PhaseFlux unchanged");
    expectEqual(int(sizeof(DiscreteMapSequence)), PRE_DMAP,      "DiscreteMap unchanged");
    expectEqual(int(sizeof(IndexedSequence)),     PRE_INDEXED,   "Indexed unchanged");
    expectEqual(int(sizeof(TuesdaySequence)),     PRE_TUESDAY,   "Tuesday unchanged");
    expectEqual(int(sizeof(Track)),               PRE_TRACK,     "Track variant unchanged");
}
```

**Step 0 (before any edit): measure the baseline.** Add a throwaway test (or `printf`) that prints
`sizeof` for `Project`, all six sequences, and `Track` (sequences live inside `Track::_container`, so
the variant size is the real RAM gate). Record the exact byte values and paste them as the `PRE_*`
constants above. The gate is **`==` the measured baseline** — a `<=` or placeholder would pass under
growth and is not a gate.

**Step 2: Run to verify fail.**

**Step 3: Implement** — introduce a `BitField<uint16_t,…>` group (mirror the per-step `BitField` idiom in `NoteSequence.h:352-367`); accessors apply the ±1 bias. `setScale`/`scale` keep their existing clamps; add `scaleRotate()`/`setScaleRotate(int)` clamped to a **fixed generous `-1..31`** — **never** to `notesPerOctave-1`. The live scale may be `-1` (inherit) or change after the rotate is set; clamping to a scale's degree count at store/edit time would mutate the stored value against the live scale, which §5/acceptance forbid ("no stored mutation"). Range validity is handled by **modulo-at-resolve** (Task C1). **Alignment:** place the `uint16` at an even offset — e.g. `StochasticSequence.h:889-891`'s `_scale` begins at an *odd* offset (after `int8_t _trackIndex`), so a naive `uint16` forces a pad byte; reorder the scale group to even-aligned in each owner.

**Step 4: Run to verify pass** — also `make -C build/stm32/release sequencer` and confirm model size didn't grow. **Zero-growth is the hypothesis the `==` gate falsifies, not a given.** If the gate fails (alignment pad), reorder the scale group to an even offset and re-measure; only accept a `+N` after confirming `sizeof(Track)` is unchanged (pad may be absorbed by existing Track slack). Task B does not proceed past a failed gate without one of those resolutions.

**Step 5: Checkpoint** — stop; report the diff + the sizeof results, ask the user to commit. Suggested message: `refactor(model): bias-pack scale/rootNote/scaleRotate into uint16 per owner`.

### Task B2: Serialization (3 logical values)

**Files:** `model/NoteSequence.cpp:322-323/359-360`, and the matching write/read in StochasticSequence, PhaseFluxSequence, DiscreteMapSequence, IndexedSequence, TuesdaySequence, Project. **Do NOT touch `IndexedSequence.h:590-591`** — the 2-byte `_rootNote` write is a *deliberate* historical `Routable<int8_t>` mirror whose read side consumes both bytes (`:614-615`); changing it desyncs every field after it on load. (Earlier review mislabeled this a "bug" — it isn't.) Just don't replicate the 2-byte pattern for `scaleRotate`: write/read it as a single value.

**Steps:** write/read three logical values via temporaries (`int t = scale(); writer.write(int8_t(t));` … decode on read). TDD: a serialize→deserialize round-trip test per owner asserting scale/root/rotate survive. No version bump. Checkpoint: stop and ask the user to commit (per owner, or one "fix serialization for scale group" change).

---

## Phase C — Unified resolver + migrate every call site

### Task C1: One resolver signature

**Files:** the 6 sequence resolvers + Project + new Tuesday one:
- `NoteSequence.h:409`, `StochasticSequence.h:30`, `PhaseFluxSequence.h:322` — change `selectedScale(int defaultScale)` → `selectedScale(int projectScale, int projectRotate)` returning `RotatedScaleView`.
- `DiscreteMapSequence.h:365`, `IndexedSequence.h:638` — change `selectedScale(const Scale&)` → same int-based signature.
- `Project.h:174` — `selectedScale()` → returns `RotatedScaleView` (its own scale+rotate).
- `TuesdaySequence.h` — **add** `selectedScale(int projectScale, int projectRotate)`.

**Body (uniform):**
```cpp
RotatedScaleView selectedScale(int projectScale, int projectRotate) const {
    int idx    = scale()       < 0 ? projectScale  : scale();
    int rotate = scaleRotate()  < 0 ? projectRotate : scaleRotate();
    return RotatedScaleView(Scale::get(idx), rotate);   // base is always a static
}
```

**TDD:** resolver returns identity when rotate resolves to 0; returns rotated when set; inherits project rotate when `-1`. Test on NoteSequence + DiscreteMap (the two signature families).

**Checkpoint** — stop; ask the user to commit.

### Task C2: Migrate all ~50 call sites

**Pattern:** every `sequence.selectedScale(project.scale())` → `sequence.selectedScale(project.scale(), project.scaleRotate())`; `auto&` → `const auto&`.

**Enumerated sites (from review — verify each still present before editing):**
- **Int-based callers (append 2nd arg):** `engine/NoteTrackEngine.cpp:424,724,793,811,893`; `engine/StochasticTrackEngine.cpp` (6); `engine/PhaseFluxTrackEngine.cpp:308,676` (both `const Scale&` bindings — just append the arg, no `const auto&` change needed); plus their UI pages.
- **13 ref-based callers (switch `_project.selectedScale()` → `project.scale(), project.scaleRotate()`):** `engine/DiscreteMapTrackEngine.cpp:622`; `engine/IndexedTrackEngine.cpp:374,442`; `ui/pages/DiscreteMapSequencePage.cpp:335,808,1015`; `ui/pages/DiscreteMapStagesPage.cpp:72`; `ui/pages/IndexedStepsPage.cpp:217`; `ui/pages/IndexedSequenceEditPage.cpp:408,434,762,1092,1589`.
- **7 non-const bindings → `const auto&`:** `ui/pages/StochasticSequenceEditPage.cpp:853,1147,1239,1264,1474,1484,1586`.
- **Member-ref capture → store by value:** `engine/generators/IndexedSequenceBuilder.h:167` (`const Scale &_scale` from `IndexedSequenceEditPage.cpp:1092`) — change member to hold a `RotatedScaleView` by value (or base index + rotate).
- **Tuesday manual site:** `engine/TuesdayTrackEngine.cpp:1657` → use the new `TuesdaySequence::selectedScale(...)`.
- **Launchpad:** `ui/controllers/launchpad/LaunchpadController.cpp:830`.
- **Model-side caller:** `StochasticSequence::printNote` (`model/StochasticSequence.h:480`) calls
  `selectedScale(defaultScale)` — its signature gains a `defaultScaleRotate` param (alongside the
  existing `defaultRootNote`/`defaultScale`); migrate it **and its caller**
  `model/StochasticSequenceListModel.h:73` (`printNote(str, step.note(), _project->rootNote(),
  _project->scale())` → append `, _project->scaleRotate()`).
- **Existing test that breaks (outside engine/ui/model scope):**
  `src/tests/unit/sequencer/TestRouteGetterMigration.cpp:102` asserts
  `expectEqual(&seq.selectedScale(0), &Scale::get(8), …)` — single-arg **and** address-of-return. A
  by-value `RotatedScaleView` makes `&…selectedScale(0,0)` the address of a *temporary* (always-false +
  `-Waddress-of-temporary`). Rewrite to compare **pitch output** (`noteToVolts`), not pointer identity,
  and pass the 2-arg form.

**Verification (this task is characterization, not new behavior):** with all `scaleRotate` defaulting to inherit/0, `make -C build/sim/debug sequencer` compiles clean and **every existing unit test stays green** (`make -C build && ctest` or run each Test* binary). Behavior is identical at rotate 0. Prove no caller was missed with a **single-arg detector** (the
old `grep -v ", project"` is wrong — migrated calls like `selectedScale(project.scale(),
project.scaleRotate())` contain no literal `, project`):

First **delete the stale untracked cruft** `ui/pages/DiscreteMapSequencePage.cpp.backup` and
`.bak2` — both contain old `selectedScale(_project.selectedScale())` (`:181`) that would poison the
detector and tempt edits to dead files. Then:

```
rg -n 'selectedScale\([^,)]*\)' src/apps/sequencer/engine src/apps/sequencer/ui src/apps/sequencer/model src/tests -g '!*.backup' -g '!*.bak2'
```

Scanning `model/` **and `src/tests/`** is required — the earlier engine+ui-only scope missed both the
model-side `printNote` caller (`:480`) and the `TestRouteGetterMigration` assertion, violating
"enumerate the full set". This matches
`selectedScale(x)` and `selectedScale()` but **not** the migrated two-arg form. Classify by hand:
legitimate matches are **`Project::selectedScale()` no-arg call sites** and the **resolver definitions**
(`model/*Sequence.h` / `Project.h`); every other hit is an unmigrated caller and must be fixed.

**Checkpoint** — stop; ask the user to commit (may split: engine, ui, generators).

### Task C3: Fix routed-read clamp + confirm TT2 exclusion

- **Routed `Scale` range — the magic `23` lives in SEVEN places, fix all.** The
  `routedValueInt(ParamKey::Scale, …, 0, 23)` literal is in **all six** sequence getters —
  `NoteSequence.h:390`, `StochasticSequence.h:28`, `PhaseFluxSequence.h:320`, `DiscreteMapSequence.h:197`,
  `IndexedSequence.h:320`, `TuesdaySequence.h:319` — **and** the central routing metadata
  (`Routing.cpp:381` `[Target::Scale] = {0, 23, 0, 23, 1}`). With the current config
  (`BuiltinCount≈20 + CONFIG_USER_SCALE_COUNT=4` → `Scale::Count=24`), `23 == Count-1` — user scales
  *are* reachable today; this is a **brittle duplicated literal**, not a live exclusion. Make both track
  the count. `Scale::Count` is a runtime `static int` (`Scale.cpp:72`) so it **cannot** go in
  `Routing.cpp`'s static initializer — add a compile-time ceiling `Scale::MaxCount` (declared in
  `Scale.h`) = builtin count + `CONFIG_USER_SCALE_COUNT`, and use it in both spots. **Drift guard
  (required):** the built-in count derives from the private `scales[]` array (`Scale.cpp:40`,
  `BuiltinCount = sizeof(scales)/sizeof(Scale*)` `:69`), so a hand-maintained `MaxCount` can silently
  drift — routed scale would emit invalid indexes or hide newly-added scales. Add, in `Scale.cpp` where
  `scales[]` is visible, `static_assert(Scale::MaxCount == int(sizeof(scales)/sizeof(scales[0])) +
  CONFIG_USER_SCALE_COUNT, "MaxCount drifted from scales[]")`. TDD: at runtime the routed `Scale` max
  equals `Scale::Count-1` and reaches the highest user-scale index.
- `engine/TT2OutputShaping.h:24` — **no change in this plan**; TT2 output-quantize rotation is **deferred** (see Deferred section). Leave a one-line comment that its quantize scale is intentionally unrotated *for now*.

**Checkpoint** — stop; ask the user to commit.

---

## Phase D — `Minor` → Harmonic Minor swap

### Task D1: Swap intervals + handle defaults

**Files:** `model/Scale.cpp:12` (`minorScale`), `model/Project.cpp:141,197` (`setScale(2)`), reference `engine/TT2ScaleTables.cpp:14`.

**Step 1: Failing test** — `Major + scaleRotate 5` produces the *old* natural-minor pitch set; `Minor + 0` now produces harmonic minor:
```cpp
CASE("minor builtin is now harmonic minor; natural minor via Major+5") {
    const int harmMinor[7] = {0,2,3,5,7,8,11};
    const Scale &minor = Scale::get(2);
    for (int n=0;n<7;++n) expectEqual(int(minor.noteToVolts(n)*12+0.5f), harmMinor[n], "HM");
    RotatedScaleView nat(Scale::get(1), 5);
    const int natMinor[7] = {0,2,3,5,7,8,10};
    for (int n=0;n<7;++n) expectEqual(int(nat.noteToVolts(n)*12+0.5f), natMinor[n], "natural via Major+5");
}
```

**Step 2: Fail. Step 3:** change `NOTE_SCALE(minorScale, "Minor", true, 0, 256, 384, 640, 896, 1024, 1280)` → last value `1408` (`…, 1024, 1408`) = `0 2 3 5 7 8 11`. **Step 4: pass.**

**Decision to surface to user (not silently pick):** `Project.cpp:141,197 setScale(2) // Minor` now seeds harmonic minor as the default/example tonality, and the comments go stale. Options: (a) leave (HM default), (b) repoint defaults to `Major + rotate 5` for natural-minor default. Flag in done-notes; do not change default semantics without confirmation. `TT2ScaleTables.cpp:14` "Natural Minor" is a separate subsystem table — leave it; note the intentional divergence.

**Step 5: Checkpoint** — stop; report the diff + the default-tonality decision, ask the user to commit. Suggested message: `feat(scale): Minor builtin -> Harmonic Minor; natural minor = Major+rotate5`.

---

## Phase E — Stochastic composition + acceptance

### Task E1: `scaleRotate` × `degreeRotation` composition

`StochasticSequence` has `degreeRotation` (`:234`) rotating ticket selection mod `activeNotes`, derived engine-side as `int activeNotes = scale.notesPerOctave();` (`StochasticTrackEngine.cpp:368`). Order: `scaleRotate` selects the scale/degree set → `degreeRotation` rotates selection within it. Since `RotatedScaleView` delegates `notesPerOctave()` to the base, rotation reorders degrees without resizing `activeNotes` — so likely **no code change**, just confirm the engine feeds the now-rotated `RotatedScaleView` into that line, plus a test composing both.

### Task E2: Acceptance suite (`TestScaleRotateAcceptance.cpp`)

Encode spec §8: Major+5==old Minor; rotate-0 identical except Minor swap; Dorian intervals + `noteName` octave wrap; Voltage no-op + live-mode-toggle restability; smaller-scale modulo; Stochastic double-rotation; **MIDI-record between-degree round-trip** (`noteFromVolts` of an arbitrary input volt landing *between* rotated degrees, exercising octave-wrap on the live record path `NoteTrackEngine.cpp:893-899` — not just exact-degree inverse); **rotated `noteName` on a non-chromatic n-tet scale** (e.g. 19-tet, where `notesPerOctave` is large and octave-division rounding is least obvious); sim compiles clean with no stray single-arg `selectedScale(` callers (incl. `src/tests`); `sizeof` of each owner unchanged. Checkpoint: stop; ask the user to commit.

---

## Phase F — UI: a way to set `scaleRotate`

Without this the feature has storage + engine behavior but **no edit path** — a user can't change it. Add a "Scale Rotate" row to every scale-bearing list model + Project, showing `Default` for the `-1` inherit sentinel.

### Task F1: NoteSequenceListModel reference row

**Files:** `src/apps/sequencer/ui/model/NoteSequenceListModel.h` (enum `:25`; methods `itemName()` ~`:95`, `formatValue()` ~`:120`, `editValue()` ~`:169` — not `name()`/`printValue()`, those don't exist) **and `NoteSequenceListModel.cpp`** (the row-visibility arrays — see "Row visibility" below).

**Gate:** list-model UI isn't unit-tested here; the gate is `make -C build/sim/debug sequencer` + the F2 acceptance walkthrough (model get/set is already covered by Phase B).

**Implement — add the row:**
```cpp
// enum Item, after RootNote (:26):
ScaleRotate,
// itemName(), after the RootNote case:
case ScaleRotate: return "Scale Rotate";
// formatValue() — Default for the inherit sentinel:
case ScaleRotate: {
    int r = _sequence->scaleRotate();
    if (r < 0) str("Default"); else str("%d", r);
    break;
}
// editValue():
case ScaleRotate: _sequence->editScaleRotate(value, shift); break;
// indexedCountValue() / indexedValue() (both return int):
case ScaleRotate: return -1;   // free-edit row, not an indexed-scale row
// setIndexedValue() (returns void):
case ScaleRotate: return;
```
**Row visibility — Note is array-driven (this is the easy-to-miss part).** Note rows do **not** follow enum order; they come from three mode arrays in `NoteSequenceListModel.cpp`: `linearItems[]` (`:3`), `reneItems[]` (`:16`), `ikraItems[]` (`:32`), each listing `Scale`/`RootNote` (`:11-12` / `:27-28` / `:43-44`). **Insert `ScaleRotate` beside `Scale`/`RootNote` in all three arrays** — otherwise the enum+switch additions compile but the row never appears. (Note is the *only* array-driven list model; F2's six models are enum-driven — `rows()` returns `Last`/`LastItem` — so for those, enum+switches are sufficient and no `.cpp` array exists.)

`scaleRotate()`/`editScaleRotate()` come from Phase B (`editScaleRotate` clamps the **fixed `-1..31`**, never `notesPerOctave` — see B1; `-1`=Default). Scale-rotate is **not** an indexed-scale row, but the build is `-Wall` (`-Wswitch`, `src/CMakeLists.txt:28-29`) and these models have **no `default:`** — so the new enumerator must still appear in the three indexed switches: `indexedCountValue()` (~`:218`) and `indexedValue()` (~`:249`) both return `int` → `case ScaleRotate: return -1;`; `setIndexedValue()` (~`:285`) returns **void** → `case ScaleRotate: return;` (a bare `return`, *not* `return -1`). (No `-Werror`, so omission is warning-noise, not a failure — but add the cases to stay clean.)

**Checkpoint** — suggested message `feat(ui): scale-rotate row in NoteSequenceListModel`.

### Task F2: apply to the other five sequences + Project

**Files (same transform as F1):** `DiscreteMapSequenceListModel.h`, `IndexedSequenceListModel.h`, `PhaseFluxSequenceListModel.h`, `TuesdaySequenceListModel.h`, **`StochasticConfigListModel.h`**, and `ProjectListModel.h`. Two corrections vs a blind "six models" sweep:
- **Stochastic:** the row goes in **`StochasticConfigListModel.h`** (Scale `:16` / RootNote `:17`, cases at `:50/:66/:87/:105`), **not** `StochasticSequenceListModel.h` — that one is the per-step content editor (Gate/Note/Length…) with no Scale/RootNote.
- **Tuesday:** `TuesdaySequenceListModel.h` already has a `"Rotate"` row (`:117`, the loop-step `_rotate`, unrelated). The new `"Scale Rotate"` sits beside it — two rotation rows by design; keep the distinct label so they're not confused.
- **Project:** has **no** inherit — print the raw number, never "Default". **Not** `CurveSequenceListModel.h` (no scale).

**Acceptance (the gate the whole plan was missing):** on every scale-bearing track page **and** the Project page, the encoder moves a "Scale Rotate" row, the value changes, "Default" shows at -1 (sequences only), and rotation audibly takes effect — a Major track at Scale Rotate 5 plays natural minor.

**Checkpoint** — suggested message `feat(ui): scale-rotate row across sequence list models + Project`.

---

## Deferred — TT2 output-quantize rotation (its own beast)

TT2 quantizes **per CV output**, not per sequence: `TeletypeProgram::cvOutputQuantizeScale[i]` /
`cvOutputRootNote[i]` (`model/TeletypeProgram.h:105`), set on `ui/pages/TT2IoConfigPage.cpp:124`,
applied in `Tt2OutputShaping::shapeCv` → `Scale::get(quantizeScale)` (`engine/TT2OutputShaping.h:24`,
called from `engine/TT2TrackEngine.h:118-119`). This is a **parallel per-output scale subsystem**,
separate from the sequence resolver — TT2 is its own beast and is **deferred** out of this plan, so
its output quantize is unrotated until picked up. Phases A–E do not touch it.

**When scheduled, the add is small and self-contained (rides on `RotatedScaleView` from Phase A):**
- `model/TeletypeProgram.h` — add `int8_t cvOutputScaleRotate[TT2_CV_OUTPUT_COUNT]` (default `0`)
  beside `cvOutputQuantizeScale`/`cvOutputRootNote`; init `0` where the others init (`:137`).
- `engine/TT2OutputShaping.h` — add a `scaleRotate` param to `shapeCv`; replace
  `const Scale &scale = Scale::get(quantizeScale)` with
  `RotatedScaleView scale(Scale::get(quantizeScale), scaleRotate)` and quantize through it.
- `engine/TT2TrackEngine.h:118-119` — pass `p.cvOutputScaleRotate[index]`.
- `ui/pages/TT2IoConfigPage.cpp` — add a rotate column (clamp `0..notesPerOctave-1`) next to the
  quantize-scale column (mirror the `:124` editing pattern; render mirror of `:196`).
- `engine/TT2SceneSerializer.cpp` — write/read the new field (mirror `:92` / `:211`).

No reverse dependency: this plan ships without it; TT2 rotation can land any time after Phase A.

## Done-notes to report back
- Final `sizeof` of each owner sequence (prove zero RAM growth) + STM32 `.bss`/`.text` delta.
- The `Project.cpp` default-tonality decision (a/b) for the user to confirm.
- Confirmation greps: (a) no single-arg `selectedScale(` caller remains across **engine + ui + model + tests**
  (C2 detector); (b) no direct `Scale::get()` in engine/UI note-evaluation consumers **except** TT2
  (intentional, `TT2OutputShaping.h:24`) and the resolver implementations/tests (which legitimately call
  `Scale::get(idx)` to build the view). The grep is scoped to that exception set, not a bare
  `Scale::get(` count.
