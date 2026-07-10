# Grid Interaction Idioms — Beyond "Buttons Are For Layers"

> Date: 2026-07-04
> Sources: `temp-ref/grids/` — full norns/grid instruments: `mlre`, `metrix`, `dreamsequence`, `cheat_codes_2`, `initenere`, `cc-canvas`, `just-play`.
> Scope: **interaction paradigms only** (not DSP, not the optimization idioms already in `GRID-APPROACHES-REVIEW.md`).

## The paradigm to escape

Today's Launchpad model: **8 columns = steps, 8 rows = the value of ONE selected parameter, a Layer button switches which parameter the rows edit.** One parameter visible at a time; the grid is a bar-chart of a single layer. Every one of the seven reference instruments does something richer. The recurring theme: **a held key locally and momentarily changes what the grid means**, so 64 pads carry 3–4× their face-value functions without more physical buttons.

Line refs use each script's own file (e.g. `mlre lib/grid_mlre.lua:140`).

---

## Convergent idioms (ranked by paradigm-break value)

### 1. Multi-parameter-per-column: a tall bar band + a short enum band
The single biggest escape from "one layer at a time." metrix splits the 8 rows of each step-column into two stacked zones edited **simultaneously**: a tall magnitude bar (pulse count / pitch height) and a short enum selector (gate type / octave). A held Shift swaps both bands to a secondary layer (ratchet / probability / transpose). So 2 params are always visible, 4 reachable per page — without a Layer button switching context.
- **Where:** `metrix.lua:356-373` (band split), `:447-480` (`drawMatrix filled` = bar vs point).
- **Escapes layers by:** showing two parameters of every step at once instead of paging one.
- **MK1 fit:** bar band = one color's brightness ramp; enum band = single lit cell in the other color. Red/green/amber is enough for a 2-band split.
- **Plan touch (lossless form):** apply as a **momentary peek**, not a persistent split. Hold a modifier → the bottom rows overlay the secondary parameter's enum band; release → the full-height primary returns. A *persistent* split permanently halves value resolution (8→5 rows) and caps layer access to ~4/page — that's the regression we reject. See "Lossless application" below.

### 2. Two-finger span gesture: hold one step + tap another → set range
Replaces the separate FirstStep/LastStep picker buttons entirely. Press-and-hold a step, tap a second step, release → loop = min..max of the two. Single tap resolves on release to a one-step loop.
- **Where:** `mlre lib/grid_mlre.lua:140-196` (`track_cut`, `held/heldmax/first/second`); `metrix.lua:763-774` (loopy row, direction-agnostic, tap-vs-range disambiguated by `loopWasSelected` on release).
- **Escapes layers by:** turning a two-button modal picker into one continuous gesture on the step row itself.
- **MK1 fit:** loop span = dim fill between endpoints, bright endpoints.
- **Plan touch (lossless form):** **additive** — keep the existing First/Last modal pickers; add the span gesture as a held overlay (hold Loop, tap two steps). Nothing removed, a faster path added. Promote from D7 once E0.9 momentary tracking exists.

### 3. Gesture pattern-recorder: one pad cycles rec → play → overdub, meta+pad = clear
The deepest paradigm break. One pad per track is a state machine: empty→arm+record, recording→stop+loop-play, playing→stop, stopped→play. Meta-key + same pad = erase. It records the **timestamped event stream** (parameter-rich snapshots with real timing), not step values — so any live grid performance becomes an automatable loop, with quantized launch and overdub+undo.
- **Where:** `cheat_codes_2 lib/grid_actions.lua:1531-1617` (`grid_pat_handler` one-button state machine), engine `lib/cc_pattern_time.lua`; `mlre lib/grid_mlre.lua:30-73` (`pattern_actions`), engine `lib/pattern_time_mlre.lua:90-232`. Punch-in variant: `mlre …:99-138`.
- **Escapes layers by:** the grid stops being a value editor and becomes a performance surface whose gestures are captured.
- **MK1 fit:** rec button tiers — recording = pulsing red, playing = green, overdub = amber, has-data = dim, empty = off.
- **Plan touch:** new. Nearest existing item is D1 generators (also A/B capture). Own owner-decision phase — this is a feature, not a port.

### 4. Held modifier keys that multiply meaning (momentary vs latched)
Universal across all seven. One or two keys held as global modifiers reinterpret every other press. cheat_codes distinguishes a **momentary** meta (grid_alt, hold) from a **latched** per-bank alt (alt_lock, toggle). The combination "meta + a control = apply to ALL steps" gives instant fills/bulk-edits.
- **Where:** `metrix.lua:964-970` (shift = per-page layer swap, mod = apply-to-all/randomize); `cheat_codes_2 …:195-206` (momentary grid_alt) + `:364-372` (latched alt_lock); `mlre …:309/326` (MOD/ALT globals); `initenere.lua:677-699` (hold MOD col → row becomes probability slider); `cc-canvas` ALT/ALL columns.
- **Escapes layers by:** making "which parameter" a momentary held state, not a persistent mode you switch and forget.
- **MK1 fit:** modifier keys need no color; held-state feedback = mid-brightness wash.
- **Plan touch:** this **is** E0.9 (Modifier bitmask) — but the reference says add the momentary/latched distinction and the "modifier + control = all steps" bulk-edit, and reuse it for D6 (prob overlay) and #2 above.

### 5. Permanent edge control strip that survives every mode
Reserve one column (or the nav row) for page / record / mute / mode-select that **never changes meaning**, while the rest of the grid is repurposed freely. Stops the whole-grid-per-layer churn.
- **Where:** `cheat_codes_2 …:797` (col 16 persistent); `dreamsequence.lua` col 16 across all views; `mlre.lua:3540-3547` nav row; `cc-canvas` col 16.
- **Escapes layers by:** giving navigation a fixed home so the working area can be anything.
- **MK1 fit:** the scene column and top CC row already exist as controls — but they are **already committed** (scene = track select / mute / fill / pattern; top row = Shift / Layer / First / Last / RunMode / Follow / Navigate). A *new permanent* strip would repossess them and collide with Pattern mode.
- **Plan touch (lossless form):** do **not** claim a new permanent strip. The "fixed home" already exists (scene column + top row). New capability rides **momentary held-modifier overlays** (idiom 4) instead — the grid returns to the base editor on release, and no committed surface is repossessed.

### 6. Rows as independent lanes, each with its own playhead/loop/transport
The inverse emphasis of today's design: a row is not a value scale but a whole track's timeline, several shown at once each with its own moving playhead and loop.
- **Where:** `mlre lib/grid_mlre.lua:472-512` (CUT view, rows 2–7 = 6 tracks' loops); `initenere.lua` (2D field — horizontal playheads per row + vertical sequencers per column, each with own direction/speed).
- **Escapes layers by:** using rows for parallel time, not parallel values.
- **MK1 fit:** 8 rows = 8 tracks, one playhead cell each — natural fit for a Performer overview.
- **Plan touch:** **E5 Performer Overview** (E5.3) is exactly this; adopt mlre's per-row playhead + loop rendering.

### 7. Additive / tiered brightness for state (playhead drawn *into* the value)
Nobody uses a separate row for the playhead. State is layered onto one LED by **adding** brightness: playhead = value cell `+ low`, cued = `− pulse`, dirty = blink. metrix draws the playhead as a brightness bump inside the pulse bar; loop membership is a brightness gate over the whole page. cheat_codes and mlre centralize state→brightness in one table.
- **Where:** `metrix.lua:687-694` (playhead in the bar), `:461-477` (loop = brightness gate); `dreamsequence.lua:4714` (`+led_low` additive), `:4501-4503` (blink=dirty, pulse=clean-selected); `cheat_codes_2.lua:4583` (`led_maps` semantic table, 3 LED-profile columns); `mlre …:448-463` (play/mute/select folded into levels 3/5/10/15).
- **Escapes layers by:** freeing the rows a dedicated playhead/selection layer would cost.
- **MK1 fit:** direct — this **is** E0.5 (LedSemantic). Map brightness tier → color tier: playhead = amber-bright, loop-fill = green-dim, rec = red. Also initenere's **bipolar see-saw pair** (`initenere.lua:789-793`) — encode a signed value in the ratio of two adjacent pads, for bipolar params (transpose, pitch offset) where MK1 has no bipolar color.
- **Plan touch:** E0.5 — extend the enum to additive composition (base + playhead + selection), not just five flat levels.

### 8. Hold+tap = copy/paste, no clipboard mode
Hold a source (pattern slot / step / segment), tap a destination → copy, without leaving the current view. Empty-paste = delete.
- **Where:** `dreamsequence.lua:5326-5347` (hold pattern A, tap B = copy); `cheat_codes_2 …:62-70` (grid_alt + pad = copy/paste pad struct).
- **MK1 fit:** no color need; source held = bright, valid targets = dim.
- **Plan touch:** folds into the D-tier (macros/clipboard) or Baseline B-tier gestures; needs E0.9 held-key tracking.

### 9. Snapshot row: hold to save, tap to recall
A dedicated row/column of scene slots; long-press captures current state, short-press recalls, modifier+press deletes.
- **Where:** `cc-canvas` col 16 rows 1–14 (README); `mlre …:75-97` (`snapshot_actions`, whole-state or focused-track only); `cheat_codes_2 …:434-458` (hold-to-save / release-to-load).
- **Plan touch:** new; overlaps the existing pattern/snapshot system — evaluate against Performer scene mutes (E5.4) before adding.

### 10. Scale-quantized value axis (columns = degrees, not raw pitches)
dreamsequence makes the pitch axis scale degrees I–VII, so every press is in-key and follows key/scale changes; a note-map transform decides whether columns mean chord tones / scale / chromatic / drums.
- **Where:** `dreamsequence.lua:4713-4715`, `:5248-5277`.
- **Plan touch:** relevant to Note/Indexed pitch entry and the B1 circuit keyboard; the sequencer already has scale infrastructure to reuse.

### 11. Tap / hold / double-tap discrimination + momentary audition
A cancellable timer on keydown splits tap from hold; double-tap = "do it now" vs single = cue-quantized; pressing a note pad auditions it live and selects the step in one motion.
- **Where:** `initenere.lua:614-656` (`press_counter` timer); `dreamsequence.lua:5586-5588` (double-tap = jump now); `initenere.lua:702-707` (press = audition + select).
- **Plan touch:** this is **B4** (double-tap/long-press) — the reference confirms the timer approach and adds momentary-audition-on-press.

---

## Lossless application — additive overlays, base editor untouched

The earlier framing (persistent two-band split + a new permanent control strip) **regresses working Note/Curve editing**: layer access drops from 13-in-one-tap to ~4/page, value resolution drops 8→5 rows and loses multi-page range, horizontal step-paging vanishes, and the "fixed strip" repossesses the scene column + top row that already carry track/mute/fill/pattern/Shift. Rejected.

The reconciliation is idiom 4 taken literally: **a held key momentarily borrows the grid and hands it back on release.** So every new capability is an *overlay on the existing editor*, never a replacement of it. vinx already works this way (Navigate-held and Follow-held overlays, `LaunchpadController.cpp:502-512,1839`). Nothing below removes a current capability.

### The base editor stays exactly as today (zero loss)

Kept verbatim — the proven westlicht/xformer grid:
- **Full-height single-layer value editing** — all 8 rows for the selected parameter (`drawBar` / `sequenceEditStep`), with **vertical range paging** for params exceeding 8 (`sequenceUpdateNavigation`, Up/Down) — full range at 1-unit resolution.
- **One-tap Layer picker** — hold Layer → spatial map of all layers, tap one (13 Note / 7 Curve; the 6 unmapped Note layers get *added*, not removed, via E1.1).
- **Horizontal step paging** — `navigation.col` + Left/Right for >8-step sequences (up to 64).
- **First/Last modal pickers, RunMode, double-press gate toggle, Page+step quick-edit** — all retained.
- **Scene column** keeps track select / mute / fill / pattern-select / activity; **top row** keeps Shift / Layer / First / Last / RunMode / Follow / Navigate. **Pattern mode untouched.**

### Structural families — the core is shared, the grid is not

"Base editor" above describes the **value-grid family only**. The eleven modes fall into structurally different grids; most cannot inherit the Note/Curve full-height value layout — they inherit only the **shared core** (held-modifier overlay framework, LED brightness→red/green/amber semantics, VirtualGrid paging, scene-column=tracks + top-row=modifiers, copy/bulk/snapshot, playhead-in-value display). Each family keeps its own native grid:

| Family | Modes | Native grid | Inherits |
|--------|-------|-------------|----------|
| **Value grid** | Note, Curve, Indexed, PhaseFlux | full-height single-layer value + gate row | the full base editor + every value overlay (peek, prob, span, bulk, copy) |
| **Stage grid** | DiscreteMap | stage-select row + threshold/note bars + direction enum | core + copy/bulk/snapshot/span/brightness; NOT the value-layer editor |
| **±Keypad** | Tuesday, Fractal | increment/decrement param keypad — no per-step values | core framework only (copy/randomize/bulk, paging for Fractal brackets); value overlays are N/A — the pads stay a ±keypad |
| **Selection-mask** | Stochastic | per-page cell mask → edit masked set | core + mask select/bulk; peek/gate-row don't apply |
| **Text / pattern** | TT2, TT2Mini | text keypad + 4×64 pattern page | core navigation + paging only; value overlays don't apply |
| **None** | MidiCv | no grid | nothing — OLED list only |

So the overlays below are the **value-grid family's** vocabulary. For the other families, only the ones marked "core" transfer; the rest are structurally N/A. The lossless guarantee is per-family: no family loses what its own OLED/hardware editor already does — it just gains the core overlays that fit its structure.

### Mirror each engine's native scheme — don't borrow foreign interfaces

An earlier draft assigned each family a "signature interface" borrowed from a norns script (fader bank, seed field, playable bank). That overreached: **six of seven new engines already have a bespoke physical control language** on the panel, and the pads should carry *that*, not replace it.

These engines drive the panel **physically, not semantically** — the step buttons aren't steps, and First/Last/RunMode/Fill are remapped to engine actions. An engine's control is a four-part combination: **encoder (values) + prev/next (nav) + step-buttons-as-shortcuts + OLED param fields (the extensive params)**. The Launchpad carries only two of the four — pads-as-shortcuts and scene/top-as-nav/F-keys. It has **no encoder and no param-field display**, so it mirrors the shortcut/nav/select layer and reads values back, but does not set them precisely. That stays on the panel.

**Every track is multi-page — the pads always show one page.** Note/Curve/Indexed page through *layers* (Note ≈ 19, Curve 7, Indexed 3+ — Gate/Retrig/Length/Note/Cond and sub-layers, via F-keys + hold-Layer picker); the engine tracks page through param slots / sub-pages / topics (via F-keys + NEXT). Don't draw or reason about any of them as a single grid — one page is on the pads at a time, and the same paging control switches which.

| Engine | Pads mirror | Panel keeps |
|--------|-------------|-------------|
| Note / Curve / Indexed | the layer-page stack — one full-height layer page at a time, F-keys page through all (~19 / 7 / 3+). Circuit keyboard (vinx B1) is one added page, for live note entry | encoder fine-edit |
| Tuesday | y1 = param UP, y2 = param DOWN (mirrors S1–8 / S9–16); F1–4 slots + NEXT pages; First/Last/RunMode/Fill → Reseed/Reset/cycle-algo/Jam | encoder edits the slot values |
| Fractal | y1 add / y2 subtract on the current sub-page (Trunk/Branch/Ornament/Source via NEXT); queue-jumps, pool bits, ornament punch per sub-page | encoder edits the focused edge |
| DiscreteMap | y1 = select stage, y2 = cycle direction (direct mirror); section paging; F-key mode toggles | encoder edits threshold / note |
| Stochastic | y1–2 = selection mask; NEXT = Live/Loop/Pitch/Duration pages; F-keys per page | encoder edits the masked set |
| PhaseFlux | 16 stage cells + topic/slot system (◄► cycle topics, F1–4 slots) | encoder edits the slot |
| TT2 / Mini | 64-cell pattern page; F1–4 = pattern 1–4; Page+step = length/start/end | exact int16 typed on the panel keypad |

Value readback (dim bars) is fine on the pads; value *entry* precision is not the pad's job. For the engine tracks the Launchpad is a **complementary shortcut/nav surface faithful to each engine's own ergonomics**, not a self-contained editor. The genuinely additive, pad-appropriate extras (copy/bulk/snapshot overlays, and a Performer live-lanes mode from mlre for cross-track) still apply — but they sit on top of each engine's native mirror, not in place of it.

### New value is momentary overlays only (each hold-to-reveal, restore on release)

| Overlay | Hold | Grid becomes | Idiom | Loss? |
|---------|------|--------------|-------|-------|
| **Second-param peek** | a peek modifier | bottom rows overlay the step's gate/retrig enum while held; release → full-height primary back | 1 | none — primary keeps 8 rows |
| **Loop span** | Loop | tap two steps = loop min..max; First/Last pickers still there | 2 | none — added path |
| **Prob dots** | Prob | per-step probability column; tap height to set | 4/D6 | none |
| **Bulk / all-steps** | Mod + edit | the edit applies to every step at once (fills) | 4 | none |
| **Copy / paste** | Copy | tap source pad, tap dest; empty paste = clear | 8 | none |
| **Generator palette + A/B** | Gen | grid = generator shortcuts; preview/apply on the top row while held | D1 | none — additive to OLED GeneratorPage |
| **Snapshot recall** | Snap | scene slots: tap recall, hold save | 9 | none |

### Display, already lossless

- **Additive brightness** (idiom 7) already exists (`stepColor` blends the playhead into the value cell). Keep it; E0.5 just maps brightness tier → MK1 red/green/amber. No rows spent on a separate playhead/selection layer.
- **Bipolar see-saw pair** (idiom 7) — optional read-out for signed params (transpose, pitch offset) where MK1 has no bipolar color; additive.
- **Scale-quantized note entry** (idiom 11) — an option on the Note/Indexed pitch layer; the base chromatic entry stays.

### The one real cost — parked, not forced

- **Gesture pattern-recorder** (idiom 3) needs one *dedicated* pad per track (rec/play/overdub state-cycler) — the only idiom that can't be purely momentary. It stays an **owner-decision feature** (its own phase), not folded into the lossless base.

### Net

Per family, nothing its native editor already does is removed. The value-grid family (Note/Curve/Indexed/PhaseFlux) keeps full-height single-layer editing at full resolution and access; the other families keep their own native grids and inherit only the core overlays that structurally fit. Escaping "buttons are for layers" happens through *momentary* repurposing on top of each family's own structure — not by imposing one universal value grid, and not by permanently taking any grid away.

## Per-engine extensions — the circuit-keyboard analog (+ feasibility)

The circuit keyboard sets the pattern: an extension of a track's control surface that (a) fits the engine's data model, (b) adds a *live/gestural* affordance the encoder+OLED editor can't, and (c) isn't available elsewhere in Performer. One candidate per engine, ranked by **verified** feasibility (two code scouts, 2026-07-05). Interactive layout render: `native-scheme-mirror.html` (this folder).

| Engine | Extension | Verdict | Gating fact (file:line) |
|--------|-----------|---------|-------------------------|
| **DiscreteMap** | transfer-function map — whole input→output on one grid (input X, note Y), draw the map, see all 32 stages at once | **Feasible + new** | map runs continuously on a live input CV every tick (`DiscreteMapTrackEngine.cpp:95,232`); setters public (`DiscreteMapSequence.h:42,48,78`). Caveat: true live-CV needs External clock mode |
| **Stochastic** | density/chaos pad — sculpt probability/density live, hear it evolve; rest-prob lane | **Feasible + new** | shaping knobs re-map on next trigger, no reseed (`StochasticTrackEngine.cpp:270-289,854-868`). Caveat: deterministic re-map vs fixed seed (`:436,187`) — evolves audibly but not fresh entropy without reseed |
| **PhaseFlux** | phase/accumulator pad — live-nudge per-stage phase offset + accumulator, or XY phase×warp | **Feasible + new** | real fields+setters read live (`PhaseFluxSequence.h:181-198`; engine `:316,449,588`). Caveat: phase lands on next per-cycle schedule rebuild, not sub-cell |
| **Indexed** | group launcher — pads mute/solo/trigger groups A–D + fire model macros live | **Feasible + new** | `toggleGroup`/`setGroupMask` per step (`IndexedSequence.h:142-150`) + `populateWithMacro*` (`:545-552`) |
| **Fractal** | branch/ornament launcher — live launch grid for transforms | **Trivial — but low novelty** | `queueSegment`/`queueOrnament` exist and are *already* panel-button-triggered (`FractalSequenceEditPage.cpp:654,686`; engine `FractalTrackEngine.h:75-76`). Re-surfacing = same two setters; but it already exists in Performer — ergonomic win, not new |
| **Curve** | contour painter — tap column heights to set the CV shape | **Feasible — thin novelty** | setters live (`CurveSequence.h:89,110,122,167`). But pads are on/off, so it's discrete tap-height = the OLED bar editor on a new surface; no analog finger-draw. Weakest on the "not elsewhere" test |
| **Tuesday** | variation deck — capture algorithm states, launch/morph | **Partial** | clipboard snapshot exists but UI-scoped, N=3, hard-overwrite paste (`TuesdayEditPage.cpp:842-897`). Deck = hoist to model + widen N + add **morph (nonexistent)**; morph is the new/hard part |
| **TT2** | programmable grid — expose pads to the VM (Teletype-native grid ops) | **Hard — project-scale** | no grid device in the VM at all — not stubbed, absent. `G.*` are Geode ops; `LIVE.GRID` has no handler (`TeletypeNativeOps.cpp:4211-4216`; `TT2OpNames.cpp:214`). Needs framebuffer + LED ops + key inlet + bridge, from scratch |
| **MidiCv** | chord-into-arp keyboard | **Parked** | out of the port; live-note→arp input path unverified |

### Two caveats on every extension

1. **Baseline mode-port cost comes first.** The controller dispatches grid draw/edit only for Note+Curve — ~12 `switch(trackMode())` points, all `default: break` (`LaunchpadController.cpp:303-620`). Each engine's pad surface is ~150–250 mechanical lines (mirror of the Curve case) before any extension. Extensions sit *on top* of the E1–E6 port; "cheap extension" means cheap once the mode is ported.
2. **Pads are on/off.** Velocity is thresholded to a bool (`LaunchpadDevice.cpp:40`) — no pressure/aftertouch. Every "draw / XY / sculpt" is discrete per-column tap-height, matching the existing bar idiom. Anything implying smooth analog gesture is unavailable on MK1.

Live edits need no commit step — engines read the mutable model each tick (`NoteTrackEngine.cpp:162,195`; `CurveTrackEngine.cpp:151,316`), so a pad write lands on the next tick.

### Sweet spot

**DiscreteMap, Stochastic, PhaseFlux, Indexed** — feasible *and* genuinely new; each re-surfaces a runtime mechanism the engine already honors live. **Fractal, Curve** — feasible but don't pass the novelty bar (already in Performer / same op on a new surface). **Tuesday** — real but needs new morph code. **TT2** — the one that's a subsystem, not a feature; scope separately if wanted.

## Note on a stale sibling doc
`GRID-APPROACHES-REVIEW.md §1.1` still recommends the buffer-then-blast MIDI optimization, which the 2026-07-04 feasibility review found unbuildable (UsbMidi already batches; no bulk-transfer API) and which was removed from PHASEDPLAN E0. That section is now stale against the plan.
