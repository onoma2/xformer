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
- **Context (owned by the harmony track):** selectable **Root/Key** + **Scale** (any Performer scale,
  incl. user scales) + **scaleRotate** (the mode — a scale-system rotation, §2f) + a
  **Diatonic ↔ Manual** toggle, all stored in the bias-packed scale group (§2f). The 14 harmonàig
  modes = base scale (**Major** or **H.Minor**, built-in index 2) × `scaleRotate` 0–6 — no bespoke
  "mode" list.
- **Quantise:** `note = scale.noteFromVolts(rawCV)` (`Scale` provides `noteFromVolts` /
  `noteToVolts`). The quantised note is the **root**; its chromatic **degree** relative to the track's
  root indexes the harmonisation (§2).
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

**4 Manual-mode-only extras** (selectable when Diatonic is off; never produced by the LUT):
Sus2 `0 2 7`, Sus4 `0 5 7`, 6th `0 4 7 9`, add9 `0 4 7 14`.

### 2b. Diatonic LUT — two canonical sets, rotated

Each of the 14 modes is one canonical 12-chord chromatic harmonization read from a rotation offset —
*not* a `[14][12]` grid. Index by **chromatic degree** `d = (quantisedNote − root) mod 12` (this
kills the `%7` hack). `quality = CANON[(modeOffset + d) mod 12]`, then apply §2c overrides.

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

The in-scale degrees of each mode fall out of this automatically and match the present code's
correct Ionian column; the off-scale degrees are the manual's chromatic "modal-interchange" chords.

### 2c. Per-mode overrides (3 cells — kept true to hardware)

Two canonical slots are notated as a different chord in specific modes; honoured as printed (not
normalised):
- **Major bank, `CANON_MAJ[8]` slot** → **°7** (G♯dim7) in **Lydian** & **Mixolydian** — i.e.
  `scaleRotate ∈ {3, 4}` (Δ7 elsewhere).
- **Harm-min bank, `CANON_HM[6]` slot** → **ø** (D♯m7♭5) in **Dorian♯4** — i.e. `scaleRotate 3`
  (Δ7 elsewhere).

Store as a 3-entry override keyed on `(bank, scaleRotate, canonical-slot)` — all plain ints now.

### 2d. Manual mode

Diatonic off → quality is fixed to the user's selection (any of the 8, or the 4 extras), regardless
of degree; the root still quantises to the scale.

### 2e. Non-curated scales — generative scale-step stacking

The curated LUT (§2b) covers the 14 harmonàig modes. For **any other scale** — whole-tone,
pentatonic, n-tet, or a 32-degree user scale — Diatonic-auto harmonises by **stacking the
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

---

## 3. Transformation — inversion, voicing, transpose

Octave-shift math on the four 1V/oct values (reuse `HarmonyEngine.h`):

- **Inversion** (Root / 1st / 2nd / 3rd): shift the lowest N notes up an octave.
- **Voicing** (Close / Drop2 / Drop3 / Open): displace specific notes down an octave to spread the
  chord. Drop2 = 2nd-highest down 8ve; Drop3 = 3rd-highest down; Open = Drop2 variant widened.

### 3a. Transpose controls — two (one is just Performer's)

Two offsets, always-on (digital — no Mode toggle or auto-timeout; those are one-fader hardware
affordances). Fine-tune is dropped — a digital track outputs quantised CV, DAC calibration handles tuning.

- **Degree Rotate** — *per-step + routable*. Rotates the harmonised degree over the **12 chromatic
  CANON degrees** → **re-harmonises** to a different chord (incl. off-scale interchange). The
  harmony-specific transpose Performer can't do; this is the per-step recipe field (§5). Named after
  Stochastic's `degreeRotation` (`StochasticSequence.h:235`) — but that rotates modulo *active scale
  notes* (diatonic), whereas harmony rotates over all 12 chromatic degrees.
- **Transpose** — *track-level*. This **is** Performer's diatonic scale-step transpose
  (`evalTransposition`, `NoteTrackEngine.cpp:69`) — the harmonàig "Global Offset" role. Shifts the
  whole output, stays in scale. **Reuse the standard track `transpose`/`octave`**, not a bespoke
  harmony param. (Trade: harmonàig's Global Offset was *chromatic*; this is *diatonic/in-scale*.)

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

- **Per-step record (bit-packed `HarmonySequence::Step`):** Quality Override (6b: `Auto` or an
  absolute quality), Mode (3b), Inversion (2b), Voicing (2b), Degree Rotate (signed — the §3a
  re-harmonise offset over 12 chromatic degrees), Gate (1b, pass/choke), **Rest (1b)**.
  ~22 bits — fits a 32-bit word.
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
modulate harmony live without fighting the step locks.

---

## 7. Outputs

- **Up to 4 CV/Gate voice pairs** (1V/oct). User maps Voice 1–4 → physical CV outputs (generic
  "Voice", not Root/3rd/5th/7th, to accommodate non-tertiary extended/sus chords).
- **Gate:** copy the source/master gate to the voices; the per-step `Gate` bit can choke a step.
- **Strum:** when active, voice updates are pushed into a `SortedQueue` (exists; `TestSortedQueue`)
  at staggered absolute-tick offsets (Up-strum: V1 @ tick+0, V2 @ tick+Δ, …). Bipolar Up/Down +
  amount. A new trigger inside the inhibit window (§4) is dropped, so strums don't pile up.
- Output mapping reuses the existing track-output / `cvOutput` path; no new DAC plumbing.

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
  small engine (quantiser state, prev-note, strum `SortedQueue`) almost certainly fit → ~zero added
  RAM. **Measure** both against the caps in `PROJECT.md`; only excess costs ×8.
- **No ProjectVersion bump.** Per `PROJECT.md` dev rule — adding a `TrackMode` enum value is
  acceptable (old projects read unknown → default); do **not** bump `ProjectVersion.h` or add
  migration. (v0.1's "add to ProjectVersion.h" is struck.)

---

## 10. Engineering punch list

- **Foundation — `docs/scale-rotate-spec.md` — DONE (shipped):** `scaleRotate` +
  `selectedScale(int,int)`→`RotatedScaleView` resolver + bias-packed scale group + `Minor`→`H.Minor`
  swap, all landed. Harmony consumes the as-built API (§2f); mode→offset table in
  `docs/scale-rotate-modes.md`. Six worked precedents now exist to copy — the scale group, the
  resolver migration, and the "Scale Rotate" list-model row (every sequence + Project have them).
- Expand `ChordQuality` 4 → 8 (the §2a set, manual order) + the 4 Manual-only extras; widen
  `ChordIntervalsTable` to match.
- Replace `DiatonicChords[7][7]` with `CANON_MAJ[12]` + `CANON_HM[12]` + the 14 rotation offsets +
  the 3-cell override table (§2b/§2c). Lookup by chromatic degree, not `% 7`.
- Kill the `note % 7` degree hack in `NoteTrackEngine.cpp:967` — compute `(note − root) mod 12`.
- Rip the `HarmonyEngine` instantiation + per-output application out of `NoteTrackEngine.cpp`.
- Remove harmony fields/enums from `NoteSequence.h` (dev-stage break is fine — no migration).
- Create the trio; add `TrackMode::Harmony` to `Config.h` (enum value only).
- Add the `Routing::Target` harmony block.
- Wire `setSequenceEditPage` / `setSequenceView` in `TopPage.cpp`; route stale-page safety.
- Rewrite the existing NoteTrack harmony tests to drive `HarmonyTrackEngine`; add LUT coverage
  (every mode's in-scale degrees + the 3 override cells).

---

## 11. UI surfaces

- **EditPage:** 16-step grid; encoder edits the selected step's Inversion / Voicing / Quality /
  Transpose; per-step Rest + Gate toggles.
- **SequencePage:** list view of the sequence params.
- **TrackPage (setup):** `CV Source` (Track 1–8 / CV In 1–4), Root/Scale/Mode, Diatonic toggle,
  `Trigger Source` (Off=delta-auto / a gate source / Internal counter), `Transpose` (standard
  diatonic, §3a), `Output Map` (Voice→CV), `Strum` (bipolar Up/Down + amount).
  (Degree Rotate is per-step, so it lives on the EditPage, not here.)

---

## 12. Open questions

- **Q-rest:** confirm the v0.2 Rest default (hold-voices + advance-cursor) vs hold-without-advance
  vs gate-off — settle by ear.
- **Q-strum-overlap:** within the inhibit window a new trigger is dropped (v0.2); confirm vs
  cancel-restart by ear.
- **Q-per-pattern-config:** are Root/Scale/Mode/Source per-pattern or track-global? (Default:
  sequence is per-pattern; setup is track-global — confirm.)
- **Q-scale-vs-mode — RESOLVED (hybrid, ungated).** Scale = any Performer scale (incl. 32-degree user
  scales). Harmonisation: **curated CANON LUT** for the 14 harmonàig modes (§2b–2c); **generative
  scale-step stacking** (§2e) for every other scale — augmented (whole-tone), quartal (pentatonic),
  clusters (arbitrary) — ungated (modular: dissonance is a patch away anyway). Manual = fixed quality
  always. The scale-degree page keeps the broad Scale LUT selector.

---

## 13. Deferred to v2

- Algorithmic chord progressions (auto-generated).
- Arpeggiator integration (arpeggiate the chord across the outputs/time).
- Per-step override commands beyond Rest/Gate.
- `External` reset via a routed input.

---

## 14. MVP acceptance

- **A — Compiles & fits.** `HarmonyTrack`/engine fit under the container caps (§9).
- **B — Passthrough.** Source CV in → quantised root out on Voice 1 unaltered (Manual, root-only).
- **C — Harmonise.** C4 in → C4/E4/G4/B4 out across four CVs per the step recipe (diatonic).
- **D — S&H.** Delta-auto trigger fires once per note change on a *glided* source (hysteresis
  proven); assigning a gate source switches to gate-edge triggering.
- **Done gate.** The old NoteTrack harmony tests pass via `HarmonyTrackEngine`.
