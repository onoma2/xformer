# Harmony Track spec — Version 0.3 (chord LUT locked)

A **track-hosted Harmonàig**: a diatonic harmonic quantizer (the stateless "music-theory
processor" from Instruō's harmonàig) wrapped in Performer's Track/Sequence/Engine shell so it
gains a sequencer identity — per-step chord *recipes*, per-pattern progressions, and Tuesday-style
routable params. It reads a **raw CV source**, quantizes it to a selectable root/scale, harmonises
it into up to four voices, and re-samples on a Rings-style sample-and-hold trigger.

**Decision (v0.2):** this is a **Track**, not a global engine. The instrument is a sequencer;
harmony belongs as a track. Reuse `model/HarmonyEngine.h`'s inversion/voicing/interval-apply math;
its quality enum + diatonic table are a reduced stub (see §2) and get **replaced**, not reused. The
track is the shell that adds sequencing, patterns, and routing. (Engine-vs-track was weighed; RAM
does not separate them — see §9. The track wins on instrument identity + per-step recipe locks +
per-pattern progressions.)

**Reference:** `../Four-poly/ref/harmonaig.md` (the harmonàig functional breakdown — input stage,
8 chord qualities, mode→quality tables, inversion/voicing math) and Mutable Rings `cv_scaler`
(the S&H / note-change trigger logic). Reuse, don't re-derive.

---

## 1. Input — raw CV to a selectable root (harmonàig-exact)

The harmony track does **not** read a "note" from anywhere. It takes a **raw CV voltage** and
quantizes it against its **own** harmonic context — exactly the harmonàig input stage.

- **Source:** any raw-volt signal — another track's `cvOutput()` *or* a physical CV-input jack
  (both are `float channel(i)`, 1V/oct). Selectable: `Track 1–8 | CV In 1–4`.
- **Context — two orthogonal axes (§2g):** **Root/Key** + a **named scale + `scaleRotate`** (the
  *chord preset* — the mode picks the CANON quality table, §2b/§2f) and a **chromatic note mask**
  (the *quantise note-set* — which of the tuning's chromatic steps CV may land on, §2g). The 14
  harmonàig modes = base scale (**Major** or **H.Minor**, built-in index 2) × `scaleRotate` 0–6.
- **Quantise:** snap the raw CV to the **nearest enabled chromatic step** (§2g mask), over the
  tuning's full chromatic axis (12 for 12-TET, `notesPerOctave()` for n-TET). The landed step is the
  **root**; its chromatic **degree** relative to the track root indexes the chord preset (§2).
- Works with any source, quantised or not — the track always quantises into its own scale (no
  "assumes source pre-quantised" hedge from v0.1).

---

## 2. Harmonic engine — chord qualities + diatonic LUT (LOCKED)

The present `model/HarmonyEngine.h` is a reduced stub: 4 qualities, `DiatonicChords[7][7]`
(diatonic degrees only), and a `note % 7` degree hack (`NoteTrackEngine.cpp:967`, comment "for
simplicity"). Keep its inversion/voicing/interval-apply helpers; **replace** the quality enum and
the lookup with the data below — note-derived from the harmonàig manual's staff (its roman-numeral
labels carry many typos) and confirmed by ear.

### 2a. Chord qualities

**8 diatonic qualities** (intervals from root) — the manual's official set, in its order:

| idx | quality | symbol | intervals |
|-----|---------|--------|-----------|
| 0 | Minor-Major 7 | −Δ7 | 0 3 7 11 |
| 1 | Diminished 7 | °7 | 0 3 6 9 |
| 2 | Half-diminished 7 | ø | 0 3 6 10 |
| 3 | Minor 7 | −7 | 0 3 7 10 |
| 4 | Dominant 7 | 7 | 0 4 7 10 |
| 5 | Major 7 | Δ7 | 0 4 7 11 |
| 6 | Augmented-Major 7 | +Δ7 | 0 4 8 11 |
| 7 | Augmented-Dominant 7 | +7 | 0 4 8 10 |

**5 extra qualities** (selectable per step; never produced by `Auto`):
Sus2 `0 2 7`, Sus4 `0 5 7`, 6th `0 4 7 9`, add9 `0 4 7 14`, **Q4 (open 4ths)** `0 5 10 15` — a quartal
"So What" stack, also reachable generatively on pentatonic scales (§2e).

### 2b. Diatonic LUT — two canonical sets, rotated

**Storage = CANON + rotation + alt.** Index by **chromatic degree** `d = (quantisedNote − root) mod 12`
(kills the `%7` hack). Resolve in two steps: `quality = CANON[(modeOffset + d) mod 12]` gives the
**diatonic backbone** — verified: every mode's *in-scale* chords are a clean rotation of one canon per
bank — then **`alt[mode][d]` overrides the off-scale (chromatic-passing) chord** wherever the manual
hand-picks it (§2c). Not a `[14][12]` grid: one CANON per bank + the offsets + a sparse alt table.

**CANON_MAJ** (major bank — quality symbol per degree 0…11):
`Δ7 · Δ7 · −7 · Δ7 · −7 · Δ7 · ø · 7 · Δ7 · −7 · Δ7 · ø`

**CANON_HM** (harmonic-minor bank — degree 0…11):
`−Δ7 · Δ7 · ø · +Δ7 · Δ7 · −7 · Δ7 · 7 · Δ7 · ø · 7 · °7`

**Rotation offsets (semitones into the CANON)** — these are the base scale's degree positions, so
`scaleRotate` (degree 0–6, §2f) maps straight to them. The canonical index→mode→offset table is the
as-built reference **`docs/scale-rotate-modes.md`** — cite it rather than re-deriving:
- Major: Ionian 0 · Dorian 2 · Phrygian 4 · Lydian 5 · Mixolydian 7 · Aeolian 9 · Locrian 11.
- H.Minor: Harmonic-Minor 0 · Locrian♮6 2 · Ionian♯5 3 · Dorian♯4 5 · Phrygian-Dominant 7 · Lydian♯2 8 ·
  Ultralocrian 11.

The in-scale degrees of each mode fall out of the rotation automatically (`CANON` is the base mode's
column — Ionian for MAJ, Harmonic-Minor for HM). The **off-scale (chromatic-passing) degrees are
hand-picked per mode** and are *not* the rotation's prediction — the manual reaches for °7 / −7 / ø
where a pure rotation gives Δ7. Those live in the alt table (§2c).

### 2c. Alt — per-mode off-scale overrides

The chromatic-passing chords are authored per mode, not derivable from the rotation. Each mode
overrides **~2–4** of its off-scale slots vs the CANON rotation (measured so far: Phrygian 3, Lydian 2,
Aeolian 3, Locrian 4) — so the major bank alone is **~20 overrides, not the 3** first assumed. Store as
a sparse `alt` table keyed on `(bank, scaleRotate, degree) → quality`; at resolve time it replaces the
CANON-rotation result at that cell.

> **‼️ EXACT ALT VALUES ARE TO BE ADDED.** Transcribe them cell-by-cell from the manual's **parallel**
> per-mode tables (every example transposed to Middle C — this is the read that finally matched).
> Have: 5/7 major (Ionian, Phrygian, Lydian, Aeolian, Locrian) + 2/7 HM (Harmonic-Minor, Ionian♯5).
> Still need: major **Dorian, Mixolydian**; HM modes **2/4/5/6/7**. Then validate against rachim's
> `giHarmonyQual` (the prior decode). **Do not implement the LUT until `alt` is complete.**

### 2d. Per-step quality — `Auto` or absolute

Each step's **Quality** (§5) is either `Auto` — the §2b LUT (or §2e generative) picks the chord for the
quantised degree — or an **absolute quality** (any of the 8 canon + 5 extras) applied regardless of
degree. The root still quantises to the scale in both cases.

### 2e. Non-curated scales — generative scale-step stacking

The curated LUT (§2b) covers the 14 harmonàig modes. For **any other scale** — whole-tone,
pentatonic, n-tet, or a 32-degree user scale — `Auto` harmonises by **stacking the
1st/3rd/5th/7th scale-tones** from the quantised degree (the same skip-one rule that derives the
diatonic in-scale chords). No table needed. Results are the scale's native harmony: augmented triads
(whole-tone), open quartal voicings (pentatonic), dense clusters (arbitrary user scales). **Ungated**
— dissonant stacks are a feature in a modular context, not something to prevent.

### 2f. `scaleRotate` (dependency — LANDED)

Modes are scale rotations and live in the scale layer. The dependency is **built and shipped**
(`docs/scale-rotate-spec.md`): `scaleRotate` + the `RotatedScaleView` adapter + the `Minor`→`H.Minor`
swap (built-in index 2). Harmony just *consumes* the as-built API:
- **Resolve** `(base scale, scaleRotate)` via `selectedScale(int projectScale, int projectRotate)` →
  returns a `RotatedScaleView` by value (base always `Scale::get(idx)`); same `-1`=inherit sentinel as
  every sequence, so harmony inherits the project scale/rotate for free.
- **Storage** is the bias-packed scale group (`scale`+`rootNote`+`scaleRotate` in one `uint16`) — copy
  the pattern from any sequence (e.g. `NoteSequence.h`).
- `scaleRotate` is **non-routable** (fixed `-1..31`, set on the setup page) — the *mode* is a set
  field; harmony's routable re-harmonisation axis is **Degree Rotate** (§3a), which is separate.

Banks are exactly **Major** and **H.Minor** (built-in index 2); pick `CANON_MAJ` vs `CANON_HM` by
whether the resolved base scale is index 2. Natural-minor harmony is **not** a third bank — it's
`CANON_MAJ` at `scaleRotate 5` (Aeolian).

### 2g. Degree availability — the chromatic note mask (keyboard)

The harmonic context is **two orthogonal axes** — conflating them is the trap:

- **Named scale + `scaleRotate` (§2f) = the chord preset.** The mode selects the CANON quality table
  (§2b) — *what chord* is built on each degree. It does **not** decide which notes are playable.
- **The note mask = the quantise note-set.** A bitmask over the tuning's **full chromatic** axis —
  12 steps for 12-TET, `notesPerOctave()` for an n-TET `VoltScale`, up to `CONFIG_USER_SCALE_SIZE`
  (32). Incoming CV snaps to the nearest **enabled** step.

The keyboard always shows **every** chromatic step; lit = enabled. The mask is the only thing that
dims a step — the named scale never does. A fully-chromatic mask lights all 12. Enabling an
off-diatonic step makes it a **passing** degree: it lands, and its chord comes from the preset's
passing / `alt` entry (12-TET, §2c) or generative stacking (n-TET, §2e).

The named NoteScales (`majorScale` = 7 notes, `majorPentatonicScale` = 5) are **chord presets only**,
never the note axis — the keyboard axis is always the full chromatic, so their reduced
`notesPerOctave()` is irrelevant to playability.

**Storage:** a 32-bit `noteMask` on the harmony track (covers every tuning's chromatic width).
**Tuning change:** clamp mask + cursor to the new chromatic width.

---

## 3. Transformation — inversion, voicing, transpose

Octave-shift math on the four 1V/oct values (reuse `HarmonyEngine.h`):

- **Inversion** (Root / 1st / 2nd / 3rd): shift the lowest N notes up an octave.
- **Voicing** (Close / Drop2 / Drop3 / Open): displace specific notes down an octave to spread the
  chord. Drop2 = 2nd-highest down 8ve; Drop3 = 3rd-highest down; Open = Drop2 variant widened.

### 3a. Transpose controls — three (one is just Performer's)

Three offsets, always-on (digital — no Mode toggle or auto-timeout; those are one-fader hardware
affordances). Fine-tune is dropped — a digital track outputs quantised CV, DAC calibration handles tuning.

- **Degree Rotate** — *per-step + routable*. Rotates the harmonised degree over the **12 chromatic
  CANON degrees** → **re-harmonises** to a different chord (incl. off-scale interchange). The
  harmony-specific transpose Performer can't do; this is the per-step recipe field (§5). Named after
  Stochastic's `degreeRotation` (`StochasticSequence.h:235`) — but that rotates modulo *active scale
  notes* (diatonic), whereas harmony rotates over all 12 chromatic degrees.
- **Transpose** — *track-level*. Performer's diatonic scale-step transpose (`evalTransposition`,
  `NoteTrackEngine.cpp:69`) — the harmonàig "Global Offset" role. Shifts the whole output, stays in
  scale. Reuse the standard track `transpose`. (Trade: harmonàig's Global Offset was *chromatic*;
  this is *diatonic/in-scale*.)
- **Octave** — *per-step*. A signed octave shift in the recipe (§5), **not** the track octave — lets a
  progression leap registers per chord.

### 3b. Tuning — ET / JI

A per-track **Tuning** setup field: `ET` (default) or `JI`. In `JI`, the four output voices are
retuned to just intervals against the sounding bass. Note *selection* stays equal-tempered (all of §2's
table work is ET); JI is a **tuning layer on the final CV** — orthogonal to the chord LUT, so it does
**not** depend on the `alt` table being complete.

**Layout-resolved** (lay the chord out in ET, then retune what's actually sounding — the approach that
tunes real intervals, not abstract ones):
1. Build the chord and apply Inversion/Voicing (§3) in ET.
2. Voice 1 (lowest after voicing) is the ET **anchor** — it carries the quantised root.
3. For each upper voice: split its interval above the anchor into `octaves + semitone (0–11)`, look up
   the semitone's just ratio, retune `volts = anchorVolts + octaves + log2(ratio)`. Octaves stay pure
   2/1; the within-octave interval goes just.
4. Emit as CV (§7).

**JI ratio table** (semitone above bass → ratio; 5-limit + septimal tritone):

| semis | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| ratio | 16/15 | 9/8 | 6/5 | 5/4 | 4/3 | 7/5 | 3/2 | 8/5 | 5/3 | 9/5 | 15/8 |

`0`/`12` = `1/1`. **Precompute** the eleven `log2(ratio)` values as volt offsets at init — runtime is a
table lookup + add per voice, **no per-sample math, no runtime `log`**. Four voices retuned once per
trigger; cost is negligible on the F405 FPU.

**Source:** `Nebulae2/my-good/rachim_hw.instr` — `JIIntervalRatio` (the table) + `ResolveJIMidiFromBass`
(the retune). Layout-resolved is chosen over the member-stable variant (`HarmonyVoicesJIMemberStable`,
which tunes pure from the root *before* voicing): voicing then displaces notes by octaves and smears
those pure intervals, whereas retuning from the sounding bass tunes the intervals you actually hear.

**Deferred:** selectable temperaments (other ratio tables) — ship the 5-limit set first.

---

## 4. Sample-and-hold trigger model (Rings-grounded)

The engine **holds** the last chord until a trigger says "re-sample CV → re-quantise root → advance
recipe → re-voice." Trigger handling mirrors Rings' `cv_scaler` — **patch-driven, minimal knobs**.

- **One assignable trigger source.** Default **unassigned**.
  - **Unassigned →** internal **delta**: fire when the *hysteretic* quantised note changes
    (auto-trigger). This is Rings' "unpatched jack → auto-strum on input change."
  - **Assigned →** an **external gate** (a track's gate output, or a CV-in gate jack — rising edge),
    **or** the **internal counter** (the track's own clock divisor, free-running).
  - Mutually exclusive by assignment, like Rings' trigger-jack **normalization** — no `TriggerMode`
    enum, no OR-arbitration. Assigning a gate *is* the patch; clearing it falls back to delta.
- **Hysteresis (baked, hidden).** The root quantise uses **directional hysteresis ≈ ±0.3 semitone**
  (Rings `cv_scaler.cc` transpose pattern): the note must move ~⅓ semitone past the boundary to
  re-trigger. Glide/slew/noise on the source can't chatter. Fixed constant, not a param.
- **Inhibit / debounce (baked, hidden).** After any trigger, suppress re-triggers for a fixed
  window (Rings `inhibit_strum_`). Triggers inside the window are **dropped** (not queued, not
  cancel-restart) — kills glide-ramp and gate-bounce double-fires and caps strum overlap.
- **Idle-hold.** Between triggers, hold the last voices. No trigger → no output change.

---

## 5. Recipe sequence + advance

A standard 16-step grid, but each step is a harmony **recipe**, not a note. The S&H trigger (§4)
advances the recipe cursor **accumulator-style** (the advance is event-driven by the trigger, *not*
a free clock unless the internal-counter source is assigned).

- **Per-step record (bit-packed `HarmonySequence::Step`):** Quality (6b: `Auto`, or one of the 8 canon
  + 5 extras — §2), Inversion (2b), Voicing (2b), Degree Rotate (signed — the §3a re-harmonise offset
  over 12 chromatic degrees), Strum (direction up/down/alt + time + curve, ~7b — §7), Octave (signed,
  ~4b — §3a), Gate (1b, pass/choke), **Rest (1b)**. ~29 bits — fits a 32-bit word.
- **Advance:** on a trigger, the cursor steps to the next recipe and applies it to the freshly
  quantised root.
- **Rest semantics (v0.2 default):** a Rest step **holds the previous chord's voices through this
  trigger** (no re-voice) and the cursor **still advances** — i.e., it inserts a "stay put" beat in
  the progression while a root change passes underneath. (Adjustable later: hold-without-advancing,
  or gate-off, are the alternatives.)
- Standard shift/rotate transforms apply to the grid.

---

## 6. Routable params (Tuesday-style)

Add a `Routing::Target` block (`HarmonyFirst..HarmonyLast`) mirroring `TuesdayFirst..TuesdayLast`:
**Inversion, Voicing, Quality, Degree Rotate, Strum** (the overall Transpose rides the
standard track-transpose routing, not this block). A routed value **offsets the per-step base** (same convention as other tracks), so a CurveTrack/CV can
modulate harmony live without fighting the step locks. **Quality** offsets as a wrapping index into
the ordered quality enum (`Auto` + 8 canon + 5 extras = 14, §2a) — a routed sweep walks
`Auto → canon → extras`.

---

## 7. Outputs

- **Per-voice output — up to 4 voices, each its own CV + gate** (1V/oct). Voices are generic
  ("Voice", not Root/3rd/5th/7th) to accommodate non-tertiary extended/sus chords. The engine exposes
  the **final** voiced/inverted/JI-retuned chord as indexed `cvOutput(v)` / `gateOutput(v)` (`v`=0..3).
- **Voice→jack mapping is the existing CV/Gate-output routing.** Point physical CV/gate jacks at the
  harmony track on the standard output routing (`Project::setCvOutputTrack` / `setGateOutputTrack`); the
  engine's per-track output index fans voices onto them in **ascending jack order** (voice 0 = lowest
  routed jack). Voicing/Inversion rotate voices *inside* the engine, so a jack tracks a **slot** and its
  note moves when the voicing changes — consistent with the layout-resolved JI (§3b). No new DAC plumbing.
- **Per-voice gate:** each voice gates independently — strum staggers them, and a chord with fewer than
  four notes leaves the unused voices' gates low. The per-step `Gate` bit chokes the whole step.
- **Gate length (global, §11):** how long each voice gate stays high. `0` = **"T"** — a fixed
  wall-clock trig (the Length-0 trig path), gap-independent. `1–100` = % of the **last measured
  inter-trigger gap** (covers every trigger mode — internal divisor, external gate, delta-auto). The
  first trigger after start/reset has no prior gap → **hold** the gate until the next trigger
  (idle-hold, §4); % applies from the second trigger on. One value for all voices, mirroring
  DiscreteMap's `gateLength` (`DiscreteMapSequence`, 0 → "T"). Strum sets onset *spacing*; gate length
  sets each onset's *duration*; the per-step `Gate` bit still chokes.
- **Strum (per-step, §5):** one trigger fans the voice onsets out over time. **Three core controls:**
  - **direction** — up (V1→VN) / down (VN→V1) / alternate (flip each trigger);
  - **strum-time** — the delay between onsets, **wall-clock ms** via `os::ticks()` (tempo-independent,
    like a real guitar; reuses the Length-0 trig path);
  - **curve** — the onset *spacing* shape (signed: 0 = linear, + accelerate toward the end, −
    decelerate) — a real strum isn't evenly spaced.

  Voice onsets are scheduled on per-voice **wall-clock deadlines**, evaluated in `update()` (the
  trig-sentinel pattern) — **not** the musical-tick `_gateQueue`, since the strum feel must not scale
  with tempo. A new trigger inside the inhibit window (§4) is dropped, so strums don't pile up. Models
  **NYSTHI Strummer** (VCV): trigger → sequential voice gates, up/down/alternate, strum-time 10 ms
  default; a tapped subset ≈ a sub-four-voice chord.
- **MIDI out** (polyphonic + microtuned chord) is handled by the **global** MIDI-output layer — see
  `docs/midi-output-spec.md` (Phase 1 microtuning, Phase 2 polyphony). The chord is a poly source; no
  bespoke harmony MIDI.

---

## 8. Performer integration

- **Mute:** force 0V or hold last, per setting.
- **Snapshot/Pattern:** participates as a track — switching patterns switches the active
  `HarmonySequence`, so chord progressions are per-pattern.
- **Borrow pattern:** follow `Stochastic`; copy `_playMode` / `_fillMode` from `NoteTrack`.
- **Tick ordering (correctness gate):** the harmony engine reads its source's *settled* output, so
  it must run **after** the source track each tick. Honor the cross-track ordering rules in
  `docs/performer-architecture.md`; edge-detection (`prev quantised note`) assumes a settled source.

---

## 9. Class shape, storage, RAM

- **Trio:** `model/HarmonyTrack.h`, `model/HarmonySequence.h`, `engine/HarmonyTrackEngine.h` —
  standard NoteTrack/CurveTrack precedent. Reuses `model/HarmonyEngine.h` for the math.
- **RAM (corrected from v0.1 — it is a *fit-check*, not an automatic ×8):** the model/engine
  containers are sized to their *largest* variant, so a new TrackMode adds RAM **only for bytes it
  exceeds the cap by**. Fit checks: `sizeof(HarmonySequence) ≤ NoteSequence` (model cap ≈ 9544 B)
  and `sizeof(HarmonyTrackEngine) ≤ largest engine variant (≈ 912 B)`. A 16-step recipe grid + a
  small engine (quantiser state, prev-note, per-voice strum onset deadlines) almost certainly fit → ~zero added
  RAM. **Measure** both against the caps in `PROJECT.md`; only excess costs ×8.
- **No ProjectVersion bump.** Per `PROJECT.md` dev rule — adding a `TrackMode` enum value is
  acceptable (old projects read unknown → default); do **not** bump `ProjectVersion.h` or add
  migration. (v0.1's "add to ProjectVersion.h" is struck.)

---

## 10. Engineering punch list

### Legacy teardown — remove the old NoteTrack cross-track harmony

The current harmony is a **cross-track master/follower** system on NoteTrack: one NoteTrack is the
`HarmonyMaster`; other tracks set `harmonyRole` to `FollowerRoot/3rd/5th/7th` and read the master's
active sequence (`masterTrackIndex`) to play their assigned chord-tone, with a per-step role override
and a dedicated page. The self-contained harmony track replaces it whole. Remove:

- **`model/NoteSequence.h`:** the `HarmonyRole` enum, `HarmonyRoleOverride` / `HarmonyRoleOverrideType`,
  the `harmonyRole()` / `masterTrackIndex()` fields + accessors + their serialize, and the per-step
  `harmonyRoleOverride` layer (dev break is fine — no migration).
- **`engine/NoteTrackEngine.cpp`:** `applyHarmony()` (~`:929`) + its three call sites
  (~`:758/:823/:844`), the `#include "model/HarmonyEngine.h"`, and the `% 7` degree hack (~`:967`).
- **`ui/pages/NoteSequenceEditPage.cpp`** + **`ui/model/NoteSequenceListModel.h`:** the
  `Layer::HarmonyRoleOverride` row + rendering.
- **`ui/pages/HarmonyPage.{h,cpp}`** + its wiring (`Pages.h:37,85`, PageManager/TopPage) — the
  master/follower setup page.
- **Tests:** delete the follower-behaviour suites (`TestHarmonyIntegration`,
  `TestHarmonyInversionBug`, `TestHarmonyInversionIssue`); **port** the math suites
  (`TestHarmonyEngine`, `TestHarmonyVoicing`) to the harmony track — don't lose the coverage.

**Keep:** `model/HarmonyEngine.{h,cpp}` inversion/voicing/interval math — the harmony track reuses it
(§9); only its quality enum + `DiatonicChords` table are replaced (§2). `model/Model.h`'s reference
follows whatever `HarmonyEngine` becomes.

- **Foundation — `docs/scale-rotate-spec.md` — DONE (shipped):** `scaleRotate` +
  `selectedScale(int,int)`→`RotatedScaleView` resolver + bias-packed scale group + `Minor`→`H.Minor`
  swap, all landed. Harmony consumes the as-built API (§2f); mode→offset table in
  `docs/scale-rotate-modes.md`. Six worked precedents now exist to copy — the scale group, the
  resolver migration, and the "Scale Rotate" list-model row (every sequence + Project have them).
- Expand `ChordQuality` 4 → 8 (the §2 canon set, manual order) + the 5 extras (Sus2/Sus4/6th/add9/Q4);
  widen `ChordIntervalsTable` to match.
- Replace `DiatonicChords[7][7]` with `CANON_MAJ[12]` + `CANON_HM[12]` + the 14 rotation offsets +
  the sparse `alt` override table (§2b/§2c). Lookup by chromatic degree, not `% 7`.
- Legacy teardown of the old NoteTrack harmony — see the subsection above.
- Create the trio; add `TrackMode::Harmony` to `Config.h` (enum value only).
- Add the 32-bit chromatic `noteMask` (§2g) + the ScalePage keyboard (§11) — quantise snaps to the
  nearest enabled step.
- Add the `Routing::Target` harmony block.
- Wire `setSequenceEditPage` / `setSequenceView` in `TopPage.cpp`; route stale-page safety.
- LUT coverage for the ported harmony tests: every mode's in-scale degrees + the `alt` override cells.

---

## 11. UI surfaces

- **HarmonySequenceEditPage — hero pages, F5 = NEXT** (Stochastic pattern: internal `Page` enum,
  `nextPage()` wraps `% Page::Count`, F5 footer cycles; F1–F4 page-local):
  - **Recipe** — 16-step grid; layer-select F-keys edit the selected step's Quality / Inversion /
    Voicing / Degree Rotate / Octave / Strum; per-step Rest + Gate toggles.
  - **Scale** — the chromatic keyboard mask (§2g): the tuning's steps (12 / n) as a reflowing bar row,
    reuse `StochasticSequenceEditPage::drawPitchPage` (3-state Bright/Medium/Low = sounding / enabled /
    off, 32-bit `noteMask`, cursor clamp). Named-scale select seeds it.
  - **Chord** — live readout of the sounding chord / per-voice monitor.
- **SequencePage (list, per-pattern):** Root / Scale / Mode (`scaleRotate`), **CV Source** (Track 1–8 /
  CV In 1–4), **Trigger Source** (delta-auto / gate / internal counter), **Gate Length** (`0` = T,
  `1–100%`), divisor, runMode, first/last step.
- **TrackPage (setup, track-global):** `Transpose` (standard diatonic, §3a), `Tuning` (ET/JI, §3b).
  Voice→CV uses the standard CV/Gate-output routing (§7).

---

## 12. Open questions

- **Q-rest:** confirm the v0.2 Rest default (hold-voices + advance-cursor) vs hold-without-advance
  vs gate-off — settle by ear.
- **Q-strum-overlap:** within the inhibit window a new trigger is dropped (v0.2); confirm vs
  cancel-restart by ear.
- **Q-per-pattern-config — RESOLVED.** Per-pattern (SequencePage): Root/Scale/Mode, CV Source,
  Trigger Source, Gate Length. Track-global (TrackPage): Transpose, Tuning. Per-step: Octave + the recipe.
- **Q-scale-vs-mode — RESOLVED (hybrid, ungated).** Scale = any Performer scale (incl. 32-degree user
  scales). Harmonisation: **curated CANON LUT** for the 14 harmonàig modes (§2b–2c); **generative
  scale-step stacking** (§2e) for every other scale — augmented (whole-tone), quartal (pentatonic),
  clusters (arbitrary) — ungated (modular: dissonance is a patch away anyway). A step may override the
  harmonisation with an absolute quality (§2d). The scale-degree page keeps the broad Scale LUT selector.

---

## 13. Deferred to v2

- Algorithmic chord progressions (auto-generated).
- Arpeggiator integration (arpeggiate the chord across the outputs/time).
- Per-step override commands beyond Rest/Gate.
- `External` reset via a routed input.
- Strum **humanize** (random timing/onset jitter) and **velocity ramp / accent** across the strum
  (MIDI-out only — the CV/gate path has no velocity). Core strum is direction + time + curve (§7).

---

## 14. MVP acceptance

- **A — Compiles & fits.** `HarmonyTrack`/engine fit under the container caps (§9).
- **B — Passthrough.** Source CV in → quantised root out on Voice 1 unaltered (root-only: one voice, no harmonisation).
- **C — Harmonise.** C4 in → C4/E4/G4/B4 out across four CVs per the step recipe (diatonic).
- **D — S&H.** Delta-auto trigger fires once per note change on a *glided* source (hysteresis
  proven); assigning a gate source switches to gate-edge triggering.
- **Done gate.** The old NoteTrack harmony tests pass via `HarmonyTrackEngine`.
