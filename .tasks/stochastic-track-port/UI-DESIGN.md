# Enhanced Stochastic UI Design

## Goal

Design the Stochastic UI as an XFORMER-native track surface, not a copy of Vinx's Stochastic page. The UI must keep the common Performer page grammar used by Note, Curve, Indexed, DiscreteMap, and Tuesday:

- `WindowPainter::drawHeader(...)` for page identity and transport/track context.
- `WindowPainter::drawActiveFunction(...)` for the current layer, mode, or sub-view.
- `WindowPainter::drawFooter(...)` for the five function-key labels and highlighted active control.
- Context menu for destructive or bulk actions.
- Page+Step quick access for view jumps and macros where it already fits local convention.

The design should minimize list-model dependence. List pages are acceptable for pattern selection and fallback settings, but the main Stochastic experience should be visual and performance-oriented.

## Design Principles

1. **One live track, several focused surfaces.** Stochastic has too many dimensions for one grid. Use separate pages for step programming, pitch distribution, Marbles shaping, and locked-loop performance.
2. **Keep the footer semantic.** Function keys should select editing axes or page slots, not open long lists.
3. **Show probability as shape, not rows of text.** Bars, dots, fences, and cursors are better than list values for probability music.
4. **Separate source programming from captured playback.** Step pages edit model probabilities. Lock pages inspect and transform captured evaluated events.
5. **Keep the MVP mono.** Do not reserve UI space for split locks, drift, reverse, or ghost voices until those become post-MVP work.

## Existing UI References

- **Note/Curve:** 16-step section grid, function-key layer selection, loop markers, active playhead, selection state, context actions.
- **Curve:** alternate detail surfaces for special domains such as wavefolder/chaos, and context submenus for transforms.
- **Indexed:** timeline/section display, direct manipulation of structured step data, Page+Step macro shortcuts.
- **DiscreteMap:** distribution visualization with threshold bars, active cursor, selected markers, and macro context menus.
- **Tuesday:** four-column parameter console plus compact live status box, F5 as `NEXT`, page indicator, quick copy/paste/randomize shortcuts.

## External UI References

The following reference implementations informed the alternative approaches below. Full file paths are given for direct access.

### Norns Script Visual Patterns

- **`temp-ref/norns/delinquencer/delinquencer.lua`** — 8x8 binary sequencer grid (5x5px cells, 7px spacing). Probability shown as four erased corner pixels on cells with <100% chance. Cell intensity mapped to status (On=15, Rest=3, Skip=1, Ctl=8). Current step as 2px dot, selected cell as 4px rect. Horizontal/vertical modulation controls along bottom and right edges. 4-page menu system with page dots. 25 named probability patterns.
  - Key drawing: `draw_a_cell()` at line 1197 — corner pixel removal for sub-100% probability. `draw_cells()` at line 920 — 8x8 grid layout. `build_probgrid()` at line 22 — per-cycle probability regeneration.

- **`temp-ref/norns/delinquencer/lib/probability.lua`** — Simple probability engine: `math.random(1,100) <= cell_prob` gate.

- **`temp-ref/norns/luck/luck.lua`** — 7-node ring topology. Screen shows radial node graph: 7 circles (radius=7) at 2pi/7 angular divisions around center. Bezier connection curves between nodes, brightness scaled by probability weight (0-7). Active node pulse. Probability editing mode (hold K3) shows cursor ring. Weighted random selection via list-repetition method (`get_next_node()` at line 61).
  - Key: `redraw()` at line 169 — pure geometric node graph, no text grid.

- **`temp-ref/norns/kreislauf/lib/core/stueck.lua`** — Concentric ring wedge sequencer. 4 rings (16 segments each) drawn as 4-point filled polygon wedges via trig in `Stueck.plot()` (4 points: outer-1, outer-2, inner-2, inner-1). Bright wedge = active beat (level=15), dim = inactive (level=2). Page marker badge (`"P1"`), label groups, metro/speaker icons.
  - `temp-ref/norns/kreislauf/lib/core/ui.lua` — Reusable UI toolkit: `page_marker()`, `highlight()`, `signal()` (3px circle indicator), `metro_icon()`, `recording()`, `tape_icon()`, `speaker_icon()`, `draw_param()`.

- **`temp-ref/norns/spirals/spirals.lua`** — Spiral point sequencer. Notes spread around a circle, points spiral outward (radius+0.2/step, angle rotates by 2pi*rotation). Each point = filled 1px-radius circle. Arc visualization for rotation. Scale overlay: radial lines from center with note names. Sliding option panel enters from right. Lock icon (padlock). Euclidean rest distribution.
  - Key drawing: lines 328-332 — point rendering. `draw_scale()` at line 411 — radial note lines. `draw_angle()` at line 390 — arc + rotation indicator.

- **`temp-ref/norns/less-concepts/less-concepts.lua`** — Cellular automaton sequencer. Left panel: 8-bit seed as 5x4px filled rectangles. Large 24px note name center. Right panel: parameter text (octave, low/high, time, gate prob%, tran prob%, cycle mode, preset duration). 17 snapshots as 2x8 colored rect grid at bottom. Dense information display.
  - Key drawing: lines 1247-1342 — dense text layout, 8-bit seed rects, preset grid.

- **`temp-ref/norns/skylines/skylines.lua`** — M-185 "city skyline" bar chart. Step height = repetition count. Voice 1 in foreground (bright), voice 2 background (dim). Vertical line segments: height = reps*5px, width = 4px per step. 7 trig modes including random (50% per stage). Grid has 8x10 repetition matrix. Docs include `sequence_screen.png`, `sequencer_grid.png`, `preset_screen.png`.

- **`temp-ref/norns/qfwfq/qfwfq.lua`** — ASCII character ribbon "password cracking" sequencer. Two rows of 16 characters: password (top) and brute-force guess (bottom, sliding). Step dot + cursor dot indicators. K2 randomizes password. Solved positions lock and play notes (ASCII->frequency). Gimmick but unique "solving" animation.

- **`temp-ref/norns/constellations/constellations.lua`** — Star-field density sequencer. Stars appear from top at random (x, brightness, size). Crosshair cursor. Scrolling background dust. Probability parameter, density parameter. Stars generated by `math.random(100) <= scaled_density`. Star note from y-position.

### O_C (Ornament & Crime) Visual Patterns

- **`temp-ref/o_c/O_C-Phazerville/software/src/applets/ProbabilityMelody.h`** — 12 semitone weight bars + piano keyboard. Vertical dotted lines at each semitone x-offset with horizontal tick at weight height (0-10). 8 white key outlines + 6 black key inverted rects. Active note shown as UP_TRI_L_HALF / UP_TRI_R_HALF icons (59, y). Octave indicators as small rects at screen right. Value animation popup: inverted rect showing note name + weight during edit. Negative weight = "X" (excluded from scale mask).
  - Key drawing: `DrawKeyboard()` at line 342 — keyboard rendering. `DrawParams()` at line 364 — weight bar rendering. Line 230: x-positions table for 12 semitones. Line 231: black/white key pattern array.
  - Architecture: `probMeloD` icon in `PhzIcons.h`. Docs at `docs/_applets/ProbMeloD.md`. Screenshot at `docs/images/ProbMeloD.png`.

- **`temp-ref/o_c/O_C-Phazerville/software/src/applets/ProbabilityDivider.h`** — 4 horizontal sliders with dotted lines + 2x8 rect knob. Division labels (`/1`, `/2`, `/4`, `/8`). Flash animation (invertRect) on triggered division. Loop icon (8px arrow-circle) + length number. Reseed flash on loop icon, Reset flash showing "R". `DrawSlider()` from HemisphereApplet base.
  - Key drawing: `DrawInterface()` at line 244 — slider + flash + loop display. `GetNextWeightedDiv()` at line 275 — weighted random selection.

- **`temp-ref/o_c/O_C-Phazerville/software/src/applets/Shredder.h`** — Cartesian 4x4 grid (5x5px frames, 8px spacing, `gfxFrame()` at line 298). Dotted crosshair lines through active cell row/column (`gfxDottedLine()` at lines 303-304). Filled rect at active cell. Progressive imprint animation revealing newly randomized cells (line 308-314). Horizontal bar meters showing +/- voltage from center zero (line 279-293). Parameter labels for range, quantize, bipolar.
  - Key: `DrawGrid()` at line 296 — grid + crosshair + animation.

- **`temp-ref/o_c/O_C-Phazerville/software/src/applets/RndWalk.h`** — Horizontal inverted bar meters showing random walk output value per channel: `gfxInvert(31, 48+ch*8, w, 7)` — width proportional to output, centered. Parameter text list (Range, Step, Smoothness). Cursor underlines at values.

- **`temp-ref/o_c/O_C-Phazerville/software/src/applets/ShiftReg.h`** — 16-bit register as vertical bars at screen bottom: 3x14px filled rects for 1-bits, empty for 0-bits. Probability display `"p=XX"`. Icons: LOOP_ICON, LOCK_ICON, SCALE_ICON, NOTE4_ICON.

- **`temp-ref/o_c/O_C-Phazerville/software/src/applets/SequenceX.h`** — 8 vertical slider steps. Dotted vertical line per step with 5x3 rect indicator at value. Dotted horizontal zero line at midpoint. Up arrow for play position. Mute checkmark icons. Value number popup in edit mode.

- **`temp-ref/o_c/O_C-Phazerville/software/src/APP_AUTOMATONNETZ.h`** — 5x5 cell grid with transform names. Pitch class circle (28px radius, 12 positions, filled dots + triangle connecting lines). Grid trail screensaver: dots + connecting lines showing path history through cells. Clock fraction display. Cell event masks include random transform/transpose/inversion.

- **`temp-ref/o_c/O_C-Phazerville/software/src/OC_menus.cpp`** — `visualize_pitch_classes()` at line 65: draws circle (radius 28), up to 3 filled dots at pitch positions, connecting lines forming chord triangle. Uses precomputed `circle_pos_lut`.

- **`temp-ref/o_c/O_C-Phazerville/software/src/HemisphereApplet.h`** — Base graphics API: `gfxCursor()` (underline + blink), `gfxSpicyCursor()` (thicker/throbbing), `DrawSlider()` (dotted line + rect knob), `gfxSkyline()` (bilateral bar display), `gfxHeader()`, `gfxPrintCV()`, `gfxIcon()` for icon rendering.

### Squares-and-Circles (Alternate O_C Firmware) — The Name is the Interface

A complete alternate firmware for O_C (Teensy 4.0) by Eduard Heidt. Its entire visual philosophy is named after the two primitives: **squares for deterministic/active state, circles for probabilistic/fluid state.** This is the single most relevant external reference for the stochastic track's visual language. Full README at `temp-ref/o_c/squares-and-circles/README.md`. Screenshots in `doc/*.png`.

**Graphics API** (`temp-ref/o_c/squares-and-circles/src/squares-and-circles-api.h:497-524`):
- Draw primitives: `drawCircle`, `fillCircle`, `clearCircle`, `drawRect`, `fillRect`, `clearRect`, `invertRect`, `drawLine`, `setPixel`, `drawXbm` (bitmap glyphs).
- Text: `drawString(x,y,s,font)`, `drawStringEx`, `drawStringCenter`.
- Specialized: `drawPiano(x,y,note)`, `drawScope(y, frame, x_scale, y_scale)`, `drawWaveform(waveform)`, `drawMessageBox(title,msg)`.
- Dedicated display buffer access: `displayBuffer()` (raw `uint8_t*`).

**Euclidean Circle Sequencer** (`temp-ref/o_c/squares-and-circles/src/SEQ/EuclidRythm.cpp:99-175`, duplicated in `EuclidArp.cpp:118-194`):
- `draw_eclid_cyrcle()` — the centerpiece. Steps arranged trigonometrically around a 21px radius circle.
- Outer guide circle: `gfx::drawCircle(xx+3, 31+3, 21)` (21px radius).
- Step positions computed with fixed-point sine/cosine: `fpsin(a)/194`, `-fpcos(a)/194`.
- Steps rendered with the **square/circle binary**: filled `gfx::fillRect(x+2, y+2, 3, 3)` = active step (square/deterministic). Hollow `gfx::drawCircle(x+3, y+3, 3)` = inactive step (circle/probabilistic).
- Rotation offset: current rotation start marked with `gfx::drawRect(x, y, 7, 7)` — hollow 7x7 rect.
- Playhead (cursor): `gfx::drawRect(x+1, y+1, 5, 5)` — 5x5 hollow rect over the active step.
- Swing visualization: odd-indexed steps angularly offset by `(-128 + swing) * (INT16_MAX / len) / 6` — visible as uneven spacing in the circle.
- Lower left: large gate indicator — `fillRect` for active gate, `drawRect` for rest, `fillCircle`/`drawCircle` for right-side (screensaver mode, lines 158-174).
- Two independent circles drawn side-by-side at `draw()` lines 309-310, offset by 49px.
- Screenshots: `doc/seq_euclidrythm.png`, `doc/seq_euclidarp.png`.

Key insight: when `len` is too short to display meaningfully (less than ~7 steps), `fpsin((INT16_MAX / len)) / 194 <= 6`, the outer guide circle is suppressed. The circle visualization has a minimum radius threshold — small patterns render as simple step dots.

**303 Acid Patterns** (`temp-ref/o_c/squares-and-circles/src/SEQ/303-Patterns.cpp:185-234`):
- 8-column x 4-row grid of circles and squares for 16-32 step 303 patterns.
- **Filled circle** = `gfx::fillCircle(x+r, y+r, r)` where r=4 — active note.
- **Hollow circle** = `gfx::drawCircle(x+r, y+r, r)` — slide step.
- **Filled rect** = `gfx::fillRect(x+1, y+r-1, r+r-1, 3)` — rest (3px tall horizontal bar).
- Playhead: `gfx::drawCircle(x+r, y+r, r-1)` — ring overlay on the active step's circle.
- Bottom area: `gfx::drawPiano(24, y, voct)` — mini keyboard showing current note position.
- Note/rest glyphs via `gfx::drawXbm()`: `xmb_note_6x8` (musical note bitmap) and `xmb_rest_6x8` (rest symbol bitmap).
- Slide/accent indicators: `drawString()` with `-S` and `-A` labels.
- Grid layout: 10px horizontal spacing, 10px vertical spacing, starting at x=24, y=13. When len <= 8, vertical offset +5 centers the single row.

**Turing Machine** (`temp-ref/o_c/squares-and-circles/src/SEQ/TuringMachine.cpp:303-319`):
- 8-bit register displayed as 8 large **12x12px squares** at 16px spacing across the screen width (fits 8 × 16 = 128px exactly). Filled = bit=1, hollow = bit=0. `gfx::fillRect(2+bit*16, 46, 12, 12)` / `gfx::drawRect(...)`.
- Left/right output status: two 2px-radius circles at top-center of screen. `gfx::drawCircle(44, 30, 2)` for inactive, `gfx::fillCircle(...)` for active.
- Screensaver: same 8-bit bars, vertically centered (`y=32` instead of `y=46`). `gfx::clearRect(0, 0, 128, 64)` full clear.
- Parameters: Prob (0-100%), Length (0-16), Out1 mode, Out2 mode — shown as compact text in the app's parameter view.

**Shared Bitmap Glyphs** (defined identically in `EuclidRythm.cpp`, `EuclidArp.cpp`, `303-Patterns.cpp`):
- `xmb_note_6x8[8]` — musical note bitmap (6×8 pixels).
- `xmb_rest_6x8[8]` — rest symbol bitmap (6×8 pixels).
- `xmb_arrow_up_5x6[6]` / `xmb_arrow_dn_5x6[6]` — up/down triangle indicators.
- `xmb_circle_7x7[7]` — 7×7 circle outline. `xmb_filled_circle_7x7[7]` — 7×7 filled circle.
- `xbm_play_11x11[]` — 11×11 play triangle icon.
- `xbm_stop_7x11[]` — 7×11 stop square icon.

**Design Philosophy** (from README):
- Four fully configurable engine instances (like Monomachine/Machinedrum).
- UI constraint: "two buttons, two encoders and a 128x64 display. Sixteen I/O ports. A tricky task to design a simple UI logic, not get lost in menu diving."
- The square/circle binary is not decorative — it *is* the information architecture. Squares hold (active, deterministic, on). Circles suggest (probabilistic, potential, cycling). This maps directly onto the stochastic track's core tension between locked and live.

- **`temp-ref/o_c/O_C-Phazerville/software/src/drivers/weegfx.h`** — Low-level primitives: `setPixel`, `drawRect`, `clearRect`, `invertRect`, `drawFrame`, `drawLine(pattern=1/2/3 for solid/dotted/dash-dot)`, `drawHLine`, `drawVLinePattern`, `drawBitmap8`, `drawCircle`, `print/printf` (6x8 font).

- **`temp-ref/o_c/O_C-Phazerville/software/src/PhzIcons.h`** — Stochastic icons: `probMeloD` (note + probability bar), `probDiv` (clock division), `brancher` (branching paths), `rndWalk` (zigzag), `shredder` (grid).

- **`temp-ref/o_c/O_C-Phazerville/software/src/HSicons.h`** — Shared icons: `RANDOM_ICON` (die face), `BURST_ICON`, `TOSS_ICON` (coin toss), `SHUFFLE_ICON` (crossing arrows), `SCALE_ICON`.

- **`temp-ref/o_c/O_C-Phazerville/software/src/enigma/TuringMachineState.h`** — Two register visualizations: `DrawAt()` draws 16 filled 3px-wide rects (19px tall) for 1-bits with top/bottom borders. `DrawSmallAt()` draws single pixel dots per bit for compact mode.

## Proposed Pages (Primary Approach)

### 1. `StochasticSequenceEditPage` — Step Surface

Primary source-programming page. This replaces a Vinx-style all-layer page with a Note/Curve-like 16-step grid plus Stochastic-specific overlays.

Header:
- `STOCH STEPS` or `STOCH %d/4` if section identity needs to be visible in the header.

Active function:
- Current step layer name: `GATE`, `NOTE`, `LEN`, `RTRG`, `COND`, etc.

Footer:
- `GATE`
- `NOTE`
- `LEN`
- `RTRG`
- `NEXT`

`NEXT` cycles to secondary layer banks:
- Bank A: `GATE`, `NOTE`, `LEN`, `RTRG`
- Bank B: `GPROB`, `NPROB`, `LPROB`, `RPROB`
- Bank C: `OCT`, `OFFSET`, `SLIDE`, `COND`
- Bank D: `REPEAT`, `MODE`, `REST`, `LOOP`

Main drawing:
- 16 visible steps with 4 sections of 16, matching Note/Curve.
- Gate square plus active playhead.
- Mini probability bar under each step for probability layers.
- Note text or pitch-class glyph on note layers.
- Retrigger ticks on retrigger layers.
- Condition abbreviation on condition layer.
- Loop start/end markers as Note/Curve already do.

Interaction:
- Step keys select/toggle/edit the selected layer.
- Shift+Step extends selection.
- Encoder edits selected layer value for selected steps.
- Page+Step switches section or jumps to common macros.
- Context menu: `INIT`, `COPY`, `PASTE`, `DUPL`, `TIE`, `GEN` if generator integration is later accepted.

Why this page exists:
- It keeps the familiar Note/Curve grid while making probability visible in-place.
- It avoids a giant layer list.

### 2. `StochasticDicePage` — Probability Matrix

Dedicated probability overview for "what can happen" across the 64-step sequence.

Header:
- `STOCH DICE`

Active function:
- Current probability axis: `GATE`, `NOTE`, `OCT`, `LEN`, `RTRG`, `REST`

Footer:
- `GATE`
- `NOTE`
- `OCT`
- `LEN`
- `NEXT`

`NEXT` cycles to `RTRG`, `REST`, `COND`, `BIAS` if needed.

Main drawing:
- 64-step compressed strip using 4 rows of 16.
- Each cell is a probability brightness/bar, not a full step box.
- Current playhead is a bright outline.
- Selected steps are medium-bright.
- If a step is disabled, the cell is dim or hollow.

Interaction:
- Step keys select cells in the active 16-step bank.
- Encoder edits probability for selected cells.
- Page+Step selects one of the four 16-step banks.
- Context menu: `FILL`, `RAMP`, `RAND`, `INV`, `CLEAR`.

Why this page exists:
- Step edit pages are good for local editing; probability composition needs a wider overview.
- This borrows the compressed overview idea from Indexed and DiscreteMap rather than hiding probability in list rows.

### 3. `StochasticPitchPage` — Pitch Tickets

Global pitch constraint page for signed PWT tickets, scale-mask exclusion, Semi/Mask rotation, Linearity, range, and scale relationship.

Reference:
- `temp-ref/o_c/O_C-Phazerville/software/src/applets/ProbabilityMelody.h`
- `temp-ref/o_c/O_C-Phazerville/docs/_applets/ProbMeloD.md`

Header:
- `STOCH PITCH`

Active function:
- `TICKETS`, `SEMI`, `MASK`, `LINEAR`, `RANGE`, or `ROOT/SCALE` depending on selected slot.

Footer:
- `TIX`
- `ROT`
- `RANGE`
- `LIN`
- `NEXT`

`NEXT` cycles secondary footer banks:
- Bank A: `TIX`, `ROT`, `RANGE`, `LIN`
- Bank B: `SCALE`, `ROOT`, `EVEN`, `NEXT`

Ticket semantics:
- `-1` = excluded from the scale mask. Draw as `X` or no bar.
- `0` = included in the scale mask but zero raffle tickets. Draw as hollow/empty bar.
- `>0` = raffle tickets. Draw as a filled vertical bar.
- Use "tickets" in UI messages instead of "weight" where space allows.

Rotation semantics:
- **Semi** rotates all 12 ticket slots and represents key movement.
- **Mask** rotates only included scale degrees while excluded `-1` notes stay fixed.
- Manual context actions may destructively rotate stored tickets.
- Live/routed rotation should use non-destructive `semiRotation` and `maskRotation` offsets.

Footer bank A:
- `TIX`
- `SEMI`
- `MASK`
- `LIN`
- `NEXT`

Main drawing:
- 12 vertical bars for pitch tickets, arranged like a compact keyboard/chromatic histogram.
- Excluded notes draw as `X` or blank masked keys.
- Included zero-ticket notes draw as hollow keys/bars.
- Current generated pitch cursor above the active semitone.
- Min/Max note range fences across the pitch field.
- Root/scale markers shown as small ticks or brighter pitch labels.
- Linearity shown as a center-pull arc or distance falloff bar from the last note.
- Semi/Mask rotation offsets shown as small signed values near the footer/page indicator.

Interaction:
- F1 `TIX`: Step keys 1-12 select semitone bars; encoder edits `-1..max`.
- F2 `SEMI`: encoder edits non-destructive semitone rotation; context menu can bake rotation into stored tickets.
- F3 `MASK`: encoder edits non-destructive masked scale-degree rotation.
- F4 `LIN`: encoder edits linearity; view shows tighter/wider distance curve.
- F5 `NEXT`: jump to Marbles page.
- Context menu: `EVEN`, `MAJ`, `MIN`, `PENTA`, `CLEAR`, `RAND`, `BAKE`, `SEMI+`, `MASK+`.

Why this page exists:
- PWT should feel like a playable ticket/mask field, not twelve list entries.
- It borrows DiscreteMap's range/fence display and Tuesday's focused parameter slots.
- It borrows ProbMeloD's important distinction between excluded notes and zero-ticket included notes.

### 4. `StochasticMarblesPage` — Distribution Shape

Marbles-style spread/bias/steps control surface.

Header:
- `STOCH SHAPE`

Active function:
- `SPREAD`, `BIAS`, `STEPS`, or `MODE`

Footer:
- `SPRD`
- `BIAS`
- `STEP`
- `MODE`
- `PITCH`

Main drawing:
- Left side: distribution preview curve or 12-bin histogram of the shaped pitch-choice result.
- Center: bias marker and spread envelope.
- Right side: compact live status box with last chosen slot, note name, CV, and lock state, following the Tuesday status-box idiom.
- Steps value shown as bucket divisions over the distribution, not just a number.

Interaction:
- F1/F2/F3 select Spread/Bias/Steps; encoder edits.
- F4 toggles Marbles mode: `OFF`, `SOFT`, `HARD` or equivalent compact enum.
- F5 returns to `PITCH`.
- Context menu: `CENTER`, `WIDE`, `EDGE`, `RAND`, `COPY`, `PASTE`.

Why this page exists:
- Marbles controls are a shape, not generic settings. The user should see the distribution move.
- Keeping it separate prevents the pitch page from becoming crowded.

### 5. `StochasticLockPage` — Captured Loop Surface

Performance page for the compact captured-event buffer.

Header:
- `STOCH LOCK`

Active function:
- `LOCKED`, `LIVE`, `WINDOW`, or `ROTATE`

Footer:
- `LOCK`
- `FIRST`
- `LAST`
- `ROT`
- `CLEAR`

Main drawing:
- 64-event captured buffer strip as 4 rows of 16.
- Event cells show captured gate/rest, pitch height, and retrigger marker.
- Window first/last fences.
- Rotation offset marker.
- Current replay cursor.
- Edited source-step indicator if the source step differs from the captured event, but do not live-update the captured event.

Interaction:
- F1 toggles lock/capture.
- F2/F3 edit first/last.
- F4 edits rotation.
- F5 clears or asks confirmation if destructive.
- Step keys select captured events for inspection only; edits should be explicit transforms, not accidental source edits.
- Context menu: `CAPTURE`, `CLEAR`, `COPY`, `PASTE`, `REALIGN`.

Why this page exists:
- It makes the locked-loop invariant visible: captured performance is separate from current source step data.
- It avoids hiding lock behavior in a list item.

### 6. `StochasticTrackPage` — Compact Track Console

Small Tuesday-style console for remaining track-level controls that do not deserve full pages.

Header:
- `STOCH TRACK`

Footer:
- Four current parameter labels plus `NEXT`, exactly like Tuesday.

Parameter pages:
- Page 1: `OCT`, `TRANS`, `DIV`, `RESET`
- Page 2: `GBIAS`, `NBIAS`, `LBIAS`, `RBIAS`
- Page 3: `ACC`, `LEG`, `CV`, `FILL`
- Page 4: `LOWO`, `HIO`, `REST`, `LENMOD`

Main drawing:
- Four 51px parameter columns with value text and bars, following Tuesday.
- Right-side live status box: current step, current pitch, gate state, lock state.

Interaction:
- F1-F4 select parameter.
- Encoder edits selected parameter.
- F5 next page.
- Context menu: `INIT`, `RESEED`, `RAND`, `COPY`, `PASTE`.

Why this page exists:
- Some controls are track-global and do not belong in the step grid.
- This replaces long list-model pages with an existing compact Performer idiom.

## Access Model

Primary access should be direct and shallow:

- Track selected + Sequence page opens `StochasticSequenceEditPage`.
- F5/`NEXT` on step page cycles layer banks, not major pages.
- Context menu on step page includes page jumps only if needed.
- Page+Step shortcuts:
    - Page+Step 1: `STEPS`
    - Page+Step 2: `DICE`
    - Page+Step 3: `PITCH`
    - Page+Step 4: `SHAPE`
    - Page+Step 5: `LOCK`
    - Page+Step 6: `TRACK`
- Pattern selection remains a simple `StochasticSequencePage` list, matching Note/Curve/Tuesday.

Do not make users traverse a long `StochasticTrackListModel` for normal performance edits. A list model can exist for fallback/system consistency, but it should not be the primary surface.

## Page Count Recommendation

MVP pages:
- `StochasticSequencePage` — pattern/snapshot list, minimal.
- `StochasticSequenceEditPage` — step grid.
- `StochasticPitchPage` — PWT, Linearity, Range.
- `StochasticMarblesPage` — Spread/Bias/Steps.
- `StochasticLockPage` — captured loop.
- `StochasticTrackPage` — compact track console.

Optional after first pass:
- `StochasticDicePage` if probability overview feels necessary after the step-grid probability layers are implemented.

This keeps the MVP at five visual edit pages plus one list page. If RAM or page count becomes a problem, merge `Pitch` and `Marbles` only after proving the combined page remains readable.

---

## Alternative Visual Approaches

The following alternatives depart from the primary Note/Curve grid idiom. Each is a distinct visual philosophy — tradeoffs are documented. References to external files are concrete paths in `temp-ref/`.

---

### Alternative A: Cartesian Crosshair Grid (Shredder-derived)

Replace the 16-step Note/Curve grid with a 4x4 or 8x8 Cartesian cell grid inspired by Shredder.

**Visual system:**
- 16 cells drawn as 5x5px frames with 8px spacing (`gfxFrame(1 + (8*(s%4)), 26 + (8*(s/4)), 5, 5)`) — from `temp-ref/o_c/O_C-Phazerville/software/src/applets/Shredder.h:298`.
- Active step shown with dotted crosshair lines through row and column (`gfxDottedLine`) — lines 303-304.
- Selected cell filled as inverted rect — line 305.
- Progressive imprint animation reveals newly generated cells by sequential rect fills — lines 308-314.
- Right-side bar meters show probability weights per cell as inverted bars — `DrawMeters()` at lines 279-293.

**When this works:**
- Probability is shown by cell brightness rather than separate bars.
- Crosshair lines give immediate positional context for the active step.
- The 4x4 grid maps naturally to 16-step banks.
- The imprint animation makes generation visible as a process, not a static result.

**When this fails:**
- 4x4 is too small for 64 steps without bank switching.
- No room for per-layer display (gate, note, length simultaneously).
- Less direct editing than Note/Curve grid.

**Mapping to pages:**
- `StochasticSequenceEditPage`: 4x4 Cartesian grid with crosshair. F1-F4 select layer. F5 banks. Context menu for imprint/generate.
- `StochasticDicePage`: 8x8 Cartesian grid (smaller cells). Corner brightness encodes probability.

---

### Alternative B: Concentric Ring Probability Wheel (Kreislauf-derived)

Replace the linear step grid with concentric circular rings divided into radial segments.

**Visual system:**
- 4 concentric rings, each representing a probability layer (GATE, NOTE, LEN, RTRG) — outer ring = GATE, inner = RTRG.
- Each ring divided into 16 wedges. Wedges drawn as 4-point filled polygons: `Stueck.plot(x, y, r_outer, r_inner, angle_start, angle_end)` from `temp-ref/norns/kreislauf/lib/core/stueck.lua:10-35`.
- Bright wedge = active/high probability (level=15). Dim wedge = inactive/low (level=2). Stroke-only = zero probability — line 49-58.
- Active playhead as a bright radial cursor line sweeping through all rings.
- Page marker badge (`"P1"`) top-left — `UI:page_marker()` at `temp-ref/norns/kreislauf/lib/core/ui.lua:89-100`.
- Label groups at ring edges showing layer name and value — `UI:draw_param()` at line 123-138.

**When this works:**
- All four probability layers visible simultaneously — no bank switching.
- Circular layout maps well to cyclic/polyrhythmic perception.
- Radial symmetry matches the periodic nature of probability (wrapping end-around).
- Wedge area encodes probability more directly than bar height.

**When this fails:**
- 128x64 OLED is small for 4 concentric rings with 16 segments each.
- Note names and pitch values hard to read in wedges.
- 4 rings × 16 wedges = 64 steps needs splitting (or use just 16-step sections).
- No clear place for playhead across multiple sections.
- Reading precise values from wedge angles is harder than from linear bars.

**Mapping to pages:**
- `StochasticSequenceEditPage` (Alt): 4 concentric rings → GATE/NOTE/LEN/RTRG. Edit prob by selecting wedge with encoder + step key. Selected wedge inverts.
- `StochasticDicePage` (Alt): Single wide ring. 64 segments in one ring (very thin but readable by brightness only).
- `PitchPage` (Alt): 12-segment pitch ring — semitones as wedges, weight = brightness. Active note as bright radial cursor.

---

### Alternative C: Piano + Weight Bars (ProbMeloD-derived)

Full pitch-probability keyboard for the Pitch Page, drawn directly from ProbabilityMelody.

**Visual system:**
- 8 white key outlines + 6 black key inverted rects — `DrawKeyboard()` at `temp-ref/o_c/O_C-Phazerville/software/src/applets/ProbabilityMelody.h:342-362`.
- 12 vertical dotted lines above the keyboard keys, one per semitone — lines 372-384.
- Horizontal tick at top of each dotted line: y-position = weight value (0-10px). Short horizontal line at `gfxLine(x-1, y+10-w, x+1, y+10-w)` — line 383.
- Negative weight shown as "X" marker instead of tick — line 450.
- Active note shown as left/right triangles from `UP_TRI_L_HALF` / `UP_TRI_R_HALF` icons at y=59 — line 388.
- Octave position as small rects at screen right — lines 434-437.
- Value animation popup: inverted rect showing note name + weight on edit — lines 439-453.
- Lower/Upper range fences as dotted lines with octave.semitone readout — `gfxPrintOctDotSemi()` at lines 456-460.
- Range offset as DOWN_BTN_ICON / UP_BTN_ICON at top of parameter area — line 417-418.

**When this works:**
- Immediately readable pitch probability — weight bars over keyboard is a natural mapping.
- No label needed for semitones — keyboard is the legend.
- Weight rotation animation makes scale mask shifting visually clear.
- Two-channel display with L/R triangles and octave markers.

**When this fails:**
- 8 white keys (7 octaves) in 64px width means very thin keys (8px per white key).
- No room for Marbles controls on the same page.
- 0-10 weight range is small; 128x64 can show 0-63 with finer bars.
- Keyboard takes ~32px of vertical space, leaving only ~32px for parameter rows.

**Mapping to pages:**
- `StochasticPitchPage`: Full ProbMeloD keyboard + weight bars. F1=PWT (weight editing), F2=ROTATE (scale mask rotation), F3=RANGE, F4=CV mode. Remove two-channel complexity; mono mode simplifies to one triangle and octave rect.
- Add Linearity as a horizontal slider bar below the keyboard (space permitting), or via F2 slot.

**Source file reference:**
- Full drawing pipeline: `ProbabilityMelody.h` lines 342-468.
- Pitch class circle (alternative to keyboard): `OC_menus.cpp` lines 65-81 (`visualize_pitch_classes`).

---

### Alternative D: Register Bars + Compact Layout (ShiftReg-derived)

Replace multi-layer step editing with a single register-bar view showing all 64 step-gates at once.

**Visual system:**
- 64 vertical bars (1-2px wide each) in 8 rows of 8, or 4 rows of 16 — inspired by `TuringMachineState::DrawAt()` at `temp-ref/o_c/O_C-Phazerville/software/src/enigma/TuringMachineState.h:97-113`.
- 1-bit = filled 3px-wide rect (19px tall). 0-bit = empty (stroke only).
- Probability shown by rect height: full height = 100%, half = 50%, empty = 0%.
- Brightness encodes secondary dimension: layer (GATE/NOTE/LEN/RTRG) shown by gray level.
- Current playhead as inverted full-height bar — flash animation like ProbDiv (`pulse_animation` at `ProbabilityDivider.h:252-254`).
- Below the bars: compact parameter row with icons (LOOP_ICON, LOCK_ICON, SCALE_ICON from `HSicons.h`) and `"p=XX"` probability display.

**When this works:**
- All 64 steps visible at once — no section switching.
- Extremely compact — fits in bottom 24px of the screen, leaving top 40px for detail editing.
- Good for performance monitoring: see the entire probability landscape as a horizon.

**When this fails:**
- No per-step value editing from this view (requires companion page).
- 64 bars at 2px each = 128px → exactly the screen width; no margins.
- Cannot show note names, length values, or condition types in this view.
- Single dimension only: a register can show gate, but not gate+note+length simultaneously.

**Mapping to pages:**
- `StochasticSequenceEditPage` (Alt): Top 40px = 8x8 register grid (larger bars, readable). Bottom 24px = selected step detail values + icons. Edit mode expands focus: step keys select bar, encoder changes probability, invertRect shows value.
- `StochasticDicePage`: 64 bars at 1px wide across full screen width. Brightness = probability value. Playhead = single bright column.

**Hybrid option:**
- 4 rows of 16 bars, similar to `delinquencer`'s 8x8 grid but vertical register bars instead of square cells. Each row = one layer. 4 rows = GATE/NOTE/LEN/RTRG simultaneously.
- Bar height = probability. Bar brightness = value (note number, length, etc).
- This is the most density-efficient approach for showing all layers at once.

---

### Alternative E: Star-Field Constellation (Constellations-derived)

Replace the deterministic step grid with a free-form point visualization.

**Visual system:**
- Points appear at random (x, brightness, size) within a bounded field — see `temp-ref/norns/constellations/constellations.lua`.
- X-position = step index in sequence. Y-position = pitch (mapped from note value).
- Point size = velocity/accent. Point brightness = gate probability.
- Crosshair cursor moves across the field to select targets.
- History trail: dim dots showing recently generated points, with draw order → target line.
- Density parameter controls how many points appear per clock cycle.

**When this works:**
- Most musical of all approaches — probability feels like emergent behavior, not rows of text.
- Good for live performance: randomize all, then selectively edit interesting points.
- Natural for Marbles-style distribution shaping (spread = horizontal dispersion, bias = vertical shift).

**When this fails:**
- Hard to edit specific step values — requires cursor targeting.
- No repeatable pattern representation — hard to copy/paste sections.
- Impractical as the primary editing surface; better as a performance overlay.

**Mapping to pages:**
- `StochasticMarblesPage` (Alt) or a dedicated `ConstellationView` as a performance mode overlay on the SequenceEditPage.
- Not suitable as the primary `StochasticSequenceEditPage`.

---

### Alternative F: Skyline Bar Chart (skylines-derived)

Replace step cells with a city-skyline bar chart where height = probability or value.

**Visual system:**
- 16 bars across screen width, height = probability (0-100%) mapped to 0-40px — see `temp-ref/norns/skylines/skylines.lua`.
- Bar width = 6px, spacing = 2px → 16 bars = 128px (full width).
- Two layers: foreground (bright) and background (dim) — inspired by skylines' two-voice display.
- Playhead line sweeps left to right.
- Secondary value shown as horizontal tick on bar (e.g., note number ticked on probability bar).
- Under the bars: 16 small text slots for value readout (note name, length, condition char).

**When this works:**
- Excellent for probability overview — bar height is immediately comparable across all 16 steps.
- Two-layer stacking (foreground/background) shows two probability axes simultaneously (gate prob + note prob).
- Familiar visual metaphor: no learning curve.

**When this fails:**
- 16 bars = 1 section. Need 4 banks for 64 steps.
- Bar chart does not show gate state (on/off) clearly — rest steps look like low bars, ambiguous with low probability.
- Pitch values harder to read than grid cells with note text.

**Mapping to pages:**
- `StochasticSequenceEditPage` (Alt): 16 bars + 16 text slots. F1-F4 switch layer axis. Brightness indicates gate state.
- `StochasticLockPage`: Bars show captured event duration (height = length). Playhead sweeps.
- `StochasticTrackPage` (Alt): 4 larger bars for bias distribution (GPROB, NPROB, LPROB, RBIAS) in the compact console area.

---

### Alternative G: ASCII Character Ribbon (qfwfq-derived)

Replace graphical cells with character-based step display for extreme density.

**Visual system:**
- Two rows of 16 ASCII characters: top row = current step values, bottom row = "target" or "solved" state — see `temp-ref/norns/qfwfq/qfwfq.lua`.
- Gate: `X` = on, `.` = off, `?` = probability dependent, `*` = locked.
- Note: hex digit (`0-F`) for octave + note-class glyph (`C-D-E-F-G-A-B`).
- Length: `1`-`9` for length value.
- Retrigger: `r2`-`r8` or `.` for none.
- Probability animation: characters "solve" into their final value as probability resolves — letters flicker through possibilities before settling.
- Cursor dot beneath active step. Step position indicator as underline stroke.

**When this works:**
- Extremely dense — 4 layers of data in 2 rows of text across full sequence length.
- Cryptic but beautiful — the "solving" animation makes probability visible as process.
- Minimal pixel rendering — pure font rendering, no shape drawing.

**When this fails:**
- Requires learned character mappings — steep for initial use.
- No visual hierarchy — all characters same size/brightness.
- Probability is shown as animation only, not as a static value.
- Hard to edit — keyboard-heavy interaction.

**Mapping to pages:**
- `StochasticLockPage` (Alt): Captured events as ASCII ribbon. Top row = captured gate/note, bottom = source step. Solved/mismatched positions flicker. Locked columns show `=` character.
- Not suitable as the primary `StochasticSequenceEditPage`.

---

### Alternative H: Pitch Class Circle (Automatonnetz-derived)

Replace the pitch field/integer weights with a circular pitch-class visualization for the Pitch Page.

**Visual system:**
- Circle (radius 28) at center of display area — `visualize_pitch_classes()` at `temp-ref/o_c/O_C-Phazerville/software/src/OC_menus.cpp:65-81`.
- 12 pitch class positions plotted on the circle as filled 8x8 bitmap dots — uses `circle_pos_lut` for precomputed coordinates.
- Weight shown by dot radius: small dot = low weight, large dot = high weight, no dot = excluded.
- Connecting lines form a polygon between the 3 highest-weighted pitches (the "chord").
- Bias point shown as a bright ring on the circle perimeter.
- Spread shown as arc width around the bias point.
- Active note as a pulsing filled circle.
- Scale/root markers as small ticks at the appropriate positions.

**When this works:**
- Most musical representation of pitch probability — immediately shows tonal center, chord, and spread.
- Circle naturally shows the octave equivalence and cyclic nature of pitch.
- Works beautifully at 128x64 — 28px radius fits within a 60x60 area, leaving room for parameter slots.

**When this fails:**
- Does not show octave selection — must be a separate display.
- Cannot show per-step pitch assignments.
- Linear (non-octave) parameters like lineartiy have no natural circular mapping.
- Multiple channels require separate circles or overlay.

**Mapping to pages:**
- `StochasticPitchPage` (Alt): Pitch class circle dominates display. PWT editing = tap step key for semitone, encoder for weight. Circle updates in real time. Active note dot + chord polygon. Octave shown as bar below circle. Linearity shown as arc contraction.
- `StochasticMarblesPage` (Alt): Same circle, but bias = arc rotation, spread = arc width, steps = how many dots visible.

---

### Alternative I: Inverted Bar Meter Console (RndWalk + SequenceX hybrid)

Replace the Tuesday-style parameter columns with inverted bar meters for the Track Console.

**Visual system:**
- Parameter value shown as horizontal inverted bar: `gfxInvert(x, y, width, height)` where width is proportional to value — from `temp-ref/o_c/O_C-Phazerville/software/src/applets/RndWalk.h:246-249`.
- Bar centered at midpoint (bipolar) or left-aligned (unipolar).
- Current value number printed to the right of the bar.
- Label above bar.
- F1-F4 select parameter to edit. Selected parameter bar inverts (bright background, dark bar).
- Bar updates in real time as value changes — no value popup needed.

**When this works:**
- Continuous value display fits probabilistic parameters (spread, bias, linearity) better than discrete value text.
- Real-time animation shows envelope/modulation movement.
- Tuesday-compatible layout: 4 columns, each with label + bar + value.

**Mapping to pages:**
- `StochasticTrackPage`: Replace four value-text columns with four bar-meter columns. Same F-key page groups. Same right-side status box.
- `StochasticMarblesPage`: Bigger bars for Spread/Bias/Steps — each as horizontal meter with live distribution preview.

---

### Alternative J: Dense Compact Grid (less-concepts-derived)

Information-dense hybrid of text and graphics for the Sequence Edit Page.

**Visual system:**
- Left side: 8-bit pattern display as 5x4px filled rectangles (current active step bank) — from `temp-ref/norns/less-concepts/less-concepts.lua:1257-1262`.
- Center: Large-font display of current selection value (note name, probability %, length number).
- Right side: 4-5 lines of parameter text showing current layer values and probability.
- Bottom: 2x8 (16) snapshot/preset indicator grid as 4x4px filled rects — lines 1296-1307.
- Probability values shown as percentage text with bar indicator.
- Seed/rule string shown compactly.

**When this works:**
- Maximum information density — shows more dimensions simultaneously than any pure graphic approach.
- Good for expert users who want all values visible at once.
- Binary seed display works well for stochastic state visualization.

**When this fails:**
- Text-heavy — harder to read at performance distance.
- No spatial encoding of probability (no bars, no wedges, just text and small rects).
- Higher cognitive load — requires reading, not seeing.

**Mapping to pages:**
- `StochasticSequenceEditPage` (Alt): Seed/pattern display on left, selected step detail center, layer values right. Preset grid bottom.
- `StochasticTrackPage` (Alt): Full text console, all parameter values + probability % visible at once. Tuesday-like but denser.

---

### Primary / Alternative Mapping Matrix

| Page | Primary | Alt A (Cartesian) | Alt B (Ring) | Alt C (Piano) | Alt D (Register) | Alt E (Star) | Alt F (Skyline) | Alt H (Circle) | Alt I (Bar Meter) | Alt J (Dense Text) | Alt K (Sq/Circle) |
|------|---------|-------------------|--------------|---------------|------------------|--------------|-----------------|----------------|-------------------|---------------------|-------------------|
| SequenceEdit | ✓ | ✓ | ✓ | ✗ | ✓ | overlay | ✓ | ✗ | ✗ | ✓ | ✓ |
| Dice | optional | ✓ | ✓ | ✗ | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ |
| Pitch | ✓ | ✗ | ✓ | ✓ | ✗ | ✗ | ✗ | ✓ | ✗ | ✗ | ✗ |
| Marbles | ✓ | ✗ | ✗ | ✗ | ✗ | ✓ | ✗ | ✓ | ✓ | ✗ | ✓ |
| Lock | ✓ | ✗ | ✗ | ✗ | ✓ | ✗ | ✓ | ✗ | ✗ | ✗ | ✗ |
| Track | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ | ✗ | ✓ | ✓ | ✗ |

---

### Alternative K: Squares-and-Circles Euclidean Ring (squares-and-circles-derived)

Adopt the squares-and-circles visual philosophy as a first-class design principle for the stochastic track. **Squares = locked/deterministic. Circles = probability/fluctuation.** This is not decorative — it encodes the core invariant of the stochastic track (source vs. captured, locked vs. live) into the pixel language itself.

**Visual system:**
- Steps arranged as points on a circular ring (21px radius) — `draw_eclid_cyrcle()` at `temp-ref/o_c/squares-and-circles/src/SEQ/EuclidRythm.cpp:99-175`.
- Active/deterministic steps: filled 3x3 `fillRect` — a small square. The square says "this is solid, this will fire."
- Inactive/probabilistic steps: hollow 3px-radius `drawCircle` — a ring. The circle says "this might fire, this is open."
- Pending/probability-dependent steps: filled 3x3 circle `fillCircle` — a filled dot. The dot says "weighted, not guaranteed."
- Current step playhead: 5x5 hollow rect overlay. A square-within-the-square — deterministic tracking of position.
- Rotation offset (loop window start): 7x7 hollow rect around the "first" step — the window boundary.
- Section indicator (0-3): small filled circle at 12 o'clock position on the ring.
- Center text: step number or probability value for selected step.

**Guard circle rendering:**
- The outer guide circle (`gfx::drawCircle(xx+3, 31+3, 21)`) is only drawn when the step count is large enough for the trigonometrically-placed dots to be meaningfully spaced. The threshold: `fpsin(INT16_MAX / len) / 194 > 6` (about 7+ steps). Below that, only the step dots are shown without the circle.

**Swing visualization:**
- Odd-indexed steps are displaced angularly by a swing amount: `(−128 + swing) × (INT16_MAX / len) / 6`. Low swing = even spacing. High swing = uneven pairing visible in the circle's geometry. The user sees the rhythm deform.

**Two-ring composition:**
- For the Track Page or Dice Page, draw two concentric or side-by-side rings. Left ring = GATE probability layer. Right ring = NOTE probability layer. Or outer ring = current source steps, inner ring = captured lock events. The screen width (128px) fits two 21px-radius circles with 49px center-to-center spacing (`EuclidRythm.cpp:309-310`).

**When this works:**
- The square/circle binary is the most semantically rich visual encoding available for the stochastic track's core concept: locked evaluation vs. live probability.
- Circular layout naturally represents cycling/looping — the 64-step sequence wraps end-to-end.
- Swing is visible as geometric deformation, not a number.
- 21px radius + two circles = full 128x64 screen utilization.
- The ring is compact enough to leave room for parameter text at top and bottom.

**When this fails:**
- Pitch values and note names cannot be shown within the ring dots — requires separate display area.
- 64 steps on a single 21px circle = 2.1 degrees per step = indistinguishable dots. Must split into 4 banks of 16 (22.5 degrees per step) or use two interleaved rings.
- Euclidean rhythms are natural on circles; arbitrary probability patterns less so.
- Circular layout is unfamiliar for step entry — requires learning to associate angular position with step index.
- Hard to show multiple layers (gate + note + len + rtrg) simultaneously — would need nested/overlaid rings.

**Mapping to pages:**
- `StochasticSequenceEditPage` (Alt): 16-step ring, 22.5° per step. F1-F4 layer select changes dot fill semantics (probability-filled for gate prob, note-filled for note value, etc.). Center text = selected step value. Bottom bar = parameter name/value.
- `StochasticDicePage`: Two side-by-side rings of 16 steps each. Left ring = gate prob, right ring = note prob. Brightness of fill encodes probability intensity. F1-F4 switch layer pairs.
- `StochasticMarblesPage`: Single large ring with 12 dots (semitone positions). Spread = arc width of filled dots. Bias = rotation. Steps = how many dots are filled. Active note = bright filled square, others are circles.

**Source file reference:**
- Euclidean circle drawing: `EuclidRythm.cpp:99-175`, `EuclidArp.cpp:118-194`.
- Fixed-point trig for circle positioning: `fpsin()` at `EuclidRythm.cpp:46-74`, `fpcos()` at line 77-80.
- Two-circle layout: `EuclidRythm.cpp:309-310`.
- 303 grid square/circle alternates: `303-Patterns.cpp:185-234`.
- Turing Machine bit register squares: `TuringMachine.cpp:303-319`.
- Graphics primitives: `squares-and-circles-api.h:497-524`.
- Screenshots: `doc/seq_euclidrythm.png`, `doc/seq_euclidarp.png`, `doc/seq_303-patterns.png`, `doc/seq_tm.png`, `doc/mod_seq.png`, `doc/mod_rnd.png`.
- Game bitmap glyphs (note, rest, play, stop, circle, filled_circle, arrows): defined identically in `EuclidRythm.cpp:83-93`, `303-Patterns.cpp:48-54`, `EuclidArp.cpp:95-104`.

---

## Synthesis: Recommended Hybrid Approach

The primary approach (Note/Curve grid + dedicated pages) is the safest MVP — it follows existing Performer idioms and minimizes new visual code. However, the following hybrid incorporates the strongest ideas from alternatives, with the squares-and-circles Euclidean ring as the recommended first deviation from pure Note/Curve grid idiom:

1. **`StochasticSequenceEditPage`** — Keep primary Note/Curve grid. Add **Alternative D register bars** at the bottom of the screen (in the footer/status area) showing all 64 gate states as 1px bars for playback overview. Add **Alternative A's crosshair** as a playhead indicator on the grid.

2. **`StochasticDicePage` (post-MVP)** — Use **Alternative K (Squares-and-Circles Euclidean Ring)** as the primary Dice visualization. A single 16-step ring per probability axis. Square dots = locked/active steps. Circle dots = probabilistic/pending. The ring makes the dice-throw metaphor literal: you see which steps are "loaded" (square) and which are "still rolling" (circle).

3. **`StochasticPitchPage`** — Adopt **Alternative C (Piano + Weight Bars)** fully. Replace the abstract vertical bars with a ProbMeloD-derived keyboard layout. Add **Alternative H pitch class circle** as a secondary view switchable via context menu ("VIEW CIRCLE").

4. **`StochasticMarblesPage`** — Adopt **Alternative I (Bar Meter)** for Spread/Bias/Steps controls (horizontal inverted bars with live animation). Add **Alternative K's Euclidean ring** for the 12-semitone distribution preview: spread = arc width, bias = ring rotation, steps = dot count.

5. **`StochasticLockPage`** — Keep primary 4x16 event strip. Use **Alternative K squares-and-circles semantics**: locked event squares are solid-filled, pending/unlocked events are circles. This makes the locked/unlocked invariant visible at a glance without color.

6. **`StochasticTrackPage`** — Keep Tuesday-style console but with **Alternative I bar meters** instead of plain value text.

1. **`StochasticSequenceEditPage`** — Keep primary Note/Curve grid. Add **Alternative D register bars** at the bottom of the screen (in the footer/status area) showing all 64 gate states as 1px bars for playback overview. Add **Alternative A's crosshair** as a playhead indicator on the grid.

2. **`StochasticPitchPage`** — Adopt **Alternative C (Piano + Weight Bars)** fully. Replace the abstract vertical bars with a ProbMeloD-derived keyboard layout. Add **Alternative H pitch class circle** as a secondary view switchable via context menu ("VIEW CIRCLE").

3. **`StochasticMarblesPage`** — Adopt **Alternative I (Bar Meter)** for Spread/Bias/Steps controls (horizontal inverted bars with live animation). Add **Alternative H pitch class circle** for distribution preview showing which pitch classes are active and how spread/bias shape the field.

4. **`StochasticLockPage`** — Keep primary 4x16 event strip. Add **Alternative G ASCII ribbon** as a compact overlayed mode showing the same data as characters for dense inspection.

5. **`StochasticDicePage`** (post-MVP) — Use **Alternative B (Concentric Ring)** as a 4-layer single-ring probability wheel. This gives at-a-glance layer comparison for a 16-step section.

6. **`StochasticTrackPage`** — Keep Tuesday-style console but with **Alternative I bar meters** instead of plain value text.

---

## Implementation Notes

- Prefer custom pages over list models for PWT, Marbles, lock buffer, and probability overview.
- Reuse `WindowPainter`, `SequencePainter`, and existing bar/marker drawing idioms.
- Reuse Tuesday's four-column parameter console for compact track settings.
- Reuse DiscreteMap's range/fence visual language for pitch range and distribution markers.
- Reuse Indexed's Page+Step macro shortcut pattern for jumping between edit surfaces.
- Reuse Note/Curve step selection and layer bank behavior for the source step page.
- Keep all new page structs in `Pages.h` only after size probes show the page set is acceptable in `.bss`.
- For specific pixel drawing patterns, reference implementations in `temp-ref/o_c/O_C-Phazerville/software/src/applets/` (especially `ProbabilityMelody.h:342-468`, `Shredder.h:296-315`, `ProbabilityDivider.h:244-273`, `RndWalk.h:179-252`, `SequenceX.h` drawing section).
- For Norns-inspired spatial layouts, the file `temp-ref/norns/kreislauf/lib/core/ui.lua` provides a reusable UI toolkit architecture.

## Open Decisions

- Whether `StochasticDicePage` is MVP or post-MVP.
- Whether `Pitch` and `Marbles` should be separate pages or merged if `.bss` pressure is too high.
- Whether accent/legato deserve track-console slots only, or visual markers in `StochasticSequenceEditPage`.
- Whether lock clear should be immediate F5 action or confirmation modal.
- **NEW:** Which alternative visual approach to adopt for the Pitch Page — Piano keyboard (Alt C) or Pitch Class Circle (Alt H) or both with context-menu toggle.
- **NEW:** Whether the Register Bar overview (Alt D) should be a permanent fixture in the SequenceEditPage footer or a toggleable layer.
- **NEW:** Whether the Marbles Page distribution preview should use a histogram bar chart (primary), a pitch class circle (Alt H), or a squares-and-circles Euclidean ring (Alt K).
- **NEW:** Whether the squares-and-circles square/circle binary (Alt K) should be adopted as the universal visual language for the entire track (locked = rect, probabilistic = circle) or used only on specific pages.
- **NEW:** Whether the `StochasticDicePage` should use the Euclidean ring format (Alt K) or the compressed 4x16 grid (primary). The ring is more performative but the grid is more editable.
- **NEW:** If the Euclidean ring is adopted, how many steps per ring: 16 (readable, 4 banks) or 32 (dense, 2 banks) or full 64 (too dense to edit)? Approach B from kreislauf suggest 16 wedges is the practical max for readability on 128x64.
