---
id: tt2-pattern-shortcuts
schema: plan
title: "feat: TT2 pattern-view keyboard shortcuts (nudges, transpose, length/start/end)"
type: feat
status: completed
date: 2026-06-21
depth: standard
---

# feat: TT2 pattern-view keyboard shortcuts

## Problem frame

The native TT2 pattern view (`src/apps/sequencer/ui/pages/TeletypePatternViewPage.cpp`) implements
only a subset of the original monome Teletype's pattern-mode keyboard editing. Plain `[`/`]` nudge a
cell by ±1, digits type a value, and length/start/end are settable only from the hardware panel
(`Page`+step). The original hardware additionally offers musical value nudges, digit-transpose, and
keyboard length/start/end editing. This plan ports those, matching the original behavior verbatim
where it exists (reference: `master:teletype/module/pattern_mode.c`).

## Scope

Port these original pattern-mode shortcuts onto the TT2 pattern view:
- **Value nudges** (current cell, or the in-progress edit buffer): `Alt+[ ]` = ∓1 semitone,
  `Ctrl+[ ]` = ∓7 (fifth), `Shift+[ ]` = ∓12 (octave). Plain `[ ]` = ∓1 raw stays.
- **Digit transpose** (current cell): `Alt+0–9` up, `Shift+Alt+0–9` down, N semitones.
- **Length/start/end**: `Shift+L/S/E` set the field to the cursor row; `Alt+L/S/E` jump into a new
  numeric field-edit sub-mode for direct entry.

Resolved from the original source (not user decisions):
- **Transpose is cell-only.** Original `note_nudge`/transpose operate on the single cell at
  `base+offset` (or the edit buffer when editing a number), never the whole pattern.
- **"Semitone" is ET-table quantize-and-shift, not a raw ±N.** Original `transpose_n_value`
  quantizes the raw value to the nearest equal-temperament note, shifts by N notes, returns that
  note's raw value (with quantize-to-lower-note when off-grid, and wrap/clamp at note 0..127).

### Deferred to Follow-Up Work
- Other original-Teletype gaps (undo, Help mode, live-mode `~`/dashboard/turtle views, multi-line
  script select/move) — separate efforts, not this plan.

---

## Key technical decisions

- **Reuse the existing ET note table.** `TeletypeNativeOps.cpp` already holds `tt2EtTable[128]`
  (verbatim from teletype `table.c`) plus note→raw and raw→nearest-note conversions used by the QT
  op. The semitone math reuses these rather than duplicating the 128-entry table. A single public
  transpose helper is surfaced for the UI to call (U1).
- **Bracket handler must move off `event.ch()`.** `KeyboardEvent::ch()` returns 0 when Ctrl/Alt is
  held (only Shift yields a shifted char), so the current `event.ch() == '['` path cannot see the
  Ctrl/Alt variants. The handler switches to the bracket **keycodes** (`0x2F` `[`, `0x30` `]`) plus
  explicit modifier checks. Plain and Shift variants are folded into the same keycode dispatch.
- **Nudges respect `_editingNumber`.** Like the original, when a number is mid-entry the nudge
  transforms `_editBuffer`; otherwise it transforms the committed `pat.val[_row]`.
- **Field-edit sub-mode mirrors the existing cell digit-entry.** `Alt+L/S/E` enters a small state
  (which field + a digit buffer) that reuses the page's existing `insertDigit`/`commitEdit` shape,
  committing through `setLength/setStart/setEnd` (or their value-taking variants) instead of a cell.

---

## Implementation units

### U1. Semitone-transpose helper over the ET table

**Goal:** A pure, testable `transpose by N semitones` function the pattern view can call, reusing
the native ET table — the equivalent of the original `transpose_n_value`.
**Dependencies:** none.
**Files:**
- `src/apps/sequencer/engine/TeletypeNativeOps.cpp` / its header — surface a public helper over the
  existing `tt2EtTable` + note↔raw conversions (today static).
- `src/tests/unit/sequencer/TestTeletypeV2Pitch.cpp` (or a new `TestTT2Transpose.cpp`) — tests.
**Approach:** Port the original algorithm: clamp interval to ±127; if the value sits above the top
table entry, index from the top and shift; else find the first note whose raw value ≥ the input,
apply quantize-to-lower-note when the input is between notes and the interval is positive, shift by
the interval with wrap into 0..127, return that note's raw value. Reuse the existing raw→note search
and `tt2EtTable[note]` lookup rather than re-deriving them.
**Patterns to follow:** the existing QT/quantize op in `TeletypeNativeOps.cpp` and the verbatim-port
comments already there; `TestTeletypeV2Pitch.cpp` for test shape.
**Execution note:** Implement test-first — this is pure lookup math with exact expected values.
**Test scenarios:**
- On-grid value, +1 → next note's raw value (e.g. note k → note k+1 raw).
- On-grid value, +7 and +12 → fifth / octave above (raw value of note k+7 / k+12).
- Off-grid value (between notes), +1 → quantize-to-lower-note semantics from the original (lands on
  the expected note, not one too high).
- Negative intervals (−1, −7, −12) → notes below; symmetric to the positive cases.
- Wrap at the ends: shifting below note 0 or above note 127 wraps per the original modulo logic.
- Interval clamp: |interval| > 127 is clamped before lookup.
- Value above the top table entry → the top-anchored branch returns the expected entry.
**Verification:** Unit tests pass with exact ET-table values; no change to existing pitch/QT tests.

### U2. Move the `[`/`]` handler to keycode + modifier dispatch

**Goal:** Refactor the existing bracket handling so it can distinguish plain/Alt/Ctrl/Shift, without
changing current behavior.
**Dependencies:** none (precedes U3).
**Files:** `src/apps/sequencer/ui/pages/TeletypePatternViewPage.cpp` (the `keyboard()` `[`/`]`
branch).
**Approach:** Replace the `event.ch() == '['` / `']'` checks with checks on the bracket keycodes
(`0x2F`/`0x30`) gated by modifier state. Preserve the existing plain-`[ ]` ±1 raw behavior and the
`_editingNumber` branch (nudging `_editBuffer`) exactly. This unit is behavior-neutral scaffolding
for U3.
**Patterns to follow:** the keycode+modifier style already used elsewhere on the page (Ctrl branch,
`event.alt()` arrow handling) and in `TeletypeScriptViewPage`.
**Test scenarios:** `Test expectation: none` — behavior-neutral refactor; covered by U3's behavioral
scenarios and the sim build. Verify plain `[ ]` still does ±1 (raw) in both committed and
editing-number states by inspection / sim.
**Verification:** sim builds; plain `[ ]` unchanged in the running sim (committed cell and mid-entry).

### U3. Value nudges — Alt/Ctrl/Shift + `[ ]`

**Goal:** `Alt+[ ]` ∓1 semitone, `Ctrl+[ ]` ∓7, `Shift+[ ]` ∓12, on the current cell or edit buffer.
**Dependencies:** U1, U2.
**Files:** `src/apps/sequencer/ui/pages/TeletypePatternViewPage.cpp`.
**Approach:** In the U2 keycode dispatch, route each modifier+bracket combo to the U1 transpose
helper with interval ∓1 / ∓7 / ∓12. Mirror `note_nudge`: when `_editingNumber`, transform
`_editBuffer`; else transform and write back `pat.val[_row]`. Consume the event.
**Patterns to follow:** original `note_nudge` (cell vs edit-buffer branch); existing cell-write path
on the page.
**Test scenarios:**
- `Alt+]` on a cell holding an on-grid note → value becomes the next note's raw value.
- `Ctrl+]` / `Shift+]` → fifth / octave up; `Ctrl+[` / `Shift+[` → down.
- Nudge while mid-entry (`_editingNumber`) transforms the edit buffer, not the committed cell, and
  the displayed buffer updates.
- Off-grid cell value nudged → lands on the quantized note (delegates to U1's behavior).
- Plain `[ ]` still ±1 raw (regression guard from U2).
**Verification:** sim build; in the running sim each combo changes the selected cell as specified;
edit-buffer case visibly nudges the in-progress number.

### U4. Digit transpose — Alt+0–9 / Shift+Alt+0–9

**Goal:** Transpose the current cell up (`Alt+0–9`) or down (`Shift+Alt+0–9`) by N semitones.
**Dependencies:** U1.
**Files:** `src/apps/sequencer/ui/pages/TeletypePatternViewPage.cpp`.
**Approach:** Detect alt-only / shift+alt with a digit keycode. Map per the original: N = digit (1–9),
`0` = 10, and `1` = 11 (since Alt+`[ ]` already covers 1 semitone). Call the U1 helper with +N / −N
on the cell or edit buffer (same `_editingNumber` rule as U3). Note: under Alt, `event.ch()` is 0, so
dispatch on the digit **keycodes** (`HID_1`..`HID_0`).
**Patterns to follow:** original `mod_only_alt` / `mod_only_shift_alt` digit branches in
`pattern_mode.c`.
**Test scenarios:**
- `Alt+3` → cell transposed +3 semitones (via U1).
- `Alt+0` → +10; `Alt+1` → +11 (the original's special remap).
- `Shift+Alt+5` → −5.
- Mid-entry (`_editingNumber`) transposes the edit buffer.
- A plain digit (no Alt) still inserts a digit (regression — existing `insertDigit` path).
**Verification:** sim build; each combo transposes the selected cell by the expected amount; plain
digits still enter numbers.

### U5. Length/start/end set-to-cursor — Shift+L/S/E

**Goal:** `Shift+L/S/E` set length/start/end to the current cursor row.
**Dependencies:** none.
**Files:** `src/apps/sequencer/ui/pages/TeletypePatternViewPage.cpp`.
**Approach:** Add `Shift+L/S/E` keycode handlers that call the existing `setLength()` / `setStart()`
/ `setEnd()` (already used by the panel `Page`+step handlers). These set the field from the current
row, so no new state is needed.
**Patterns to follow:** the panel `Page`+step → `setLength/setStart/setEnd` calls already on the page.
**Test scenarios:**
- `Shift+L` with cursor on row r → `pat.len` reflects r (matching the panel `setLength()` result).
- `Shift+S` / `Shift+E` → `pat.start` / `pat.end` set to r.
- Plain `S`/`L`/`E` (no modifier) unaffected (regression — letters were unbound / typed nothing).
**Verification:** sim build; in the running sim `Shift+L/S/E` move the length/start/end markers to the
cursor row, identical to the panel action.

### U6. Field-edit sub-mode — Alt+L/S/E numeric entry

**Goal:** `Alt+L/S/E` jump into a small mode to numerically edit length/start/end directly.
**Dependencies:** U5 (shares the setters).
**Files:** `src/apps/sequencer/ui/pages/TeletypePatternViewPage.cpp` (+ its header for the new state).
**Approach:** Add a field-edit state (which field: none/length/start/end, plus a digit buffer),
mirroring the page's existing `_editingNumber`/`_editBuffer`/`insertDigit`/`commitEdit` shape but
targeting the chosen field. `Alt+L/S/E` selects the field and opens entry; digits accumulate;
Enter/commit writes through the setter (clamped to valid range); Esc cancels. Draw a clear indicator
of which field is being edited (render via ui-preview before finalizing per the OLED-proposal rule).
**Technical design (directional, not implementation spec):**
```
state: editField ∈ {None, Length, Start, End}, fieldBuffer:int
Alt+L/S/E  -> editField = that field; fieldBuffer = current field value; consume
digit      -> if editField != None: fieldBuffer = fieldBuffer*10 + d (clamped); consume
Enter      -> if editField != None: apply via setLength/Start/End(fieldBuffer); editField = None
Esc        -> editField = None (discard)
draw       -> when editField != None, highlight that field + show fieldBuffer
```
*Directional guidance for review, not code to reproduce.*
**Patterns to follow:** the page's own `_editingNumber` cell-entry flow (`insertDigit`,
`commitEdit`, the draw branch at the editing cell); confirm `setLength/Start/End` accept an explicit
value or add value-taking variants.
**Test scenarios:**
- `Alt+L`, type `1`,`6`, Enter → `pat.len` = 16 (clamped to `TT2_PATTERN_LENGTH`).
- `Alt+S` then `Alt+E` switch the active field; the buffer resets to the newly chosen field's value.
- Out-of-range entry (e.g. length beyond max, start > end) clamps per the model's rules.
- Esc mid-entry discards, leaving the field unchanged.
- Digits while in field-edit do NOT also edit the cell value (the two entry modes are exclusive).
**Verification:** sim build; ui-preview render of the field-edit indicator reviewed; in the running
sim each field can be typed and committed, Esc cancels, and cell entry is unaffected.

---

## System-wide impact

Confined to the TT2 pattern view plus one exposed engine helper (U1). No data-model, serialization,
or engine-execution changes — pattern values remain raw `int16` in `pat.val[]`; nudges/transpose
write the same field the existing `[ ]` and digit entry already write. The new U1 helper is additive
(reuses an existing table). Risk is low and contained to one page; the main correctness surface is
U1's ET math, which is unit-tested test-first.

## Risks

- **U1 off-by-one / quantize-direction errors.** The original's quantize-to-lower-note and end-wrap
  are subtle. Mitigated by porting verbatim and testing against exact ET-table values (U1 scenarios).
- **Modifier/keycode handling.** `ch()` nulling under Ctrl/Alt is the known trap; U2 addresses it and
  U3/U4 dispatch on keycodes. Regression guards keep plain `[ ]` and plain digits intact.
- **Field-edit vs cell-edit state confusion.** U6 must keep the two entry modes mutually exclusive
  (test included).

## Deferred / execution-time unknowns

- Exact names of the exposed transpose helper and whether `setLength/Start/End` need value-taking
  variants for U6 — settle when touching the code.
- Final field-edit indicator layout — decide at the ui-preview render step in U6.
