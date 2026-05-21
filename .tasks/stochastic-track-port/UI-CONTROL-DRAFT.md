# Stochastic Hero-Page UI — Control Mapping Draft

Preliminary mapping of buttons/combos for the four Stochastic hero pages. All names are real fields from `src/apps/sequencer/model/StochasticSequence.h` and `StochasticTrack.h`. Patterns from `docs/performer-architecture.md` §12.

Drawn UI elements **respond to param movements**: encoder turn updates the hero image live. The bell silhouette shifts peak position when bias is edited, widens/narrows when spread is edited, gains/loses quantization ticks when steps changes. Not engine-exact — visibly readable. Same principle for tide, rain, tape: drawing reflects current values so the page is its own legend.

Python preview renderers in `ui-preview/pages_levels.py`, driver `ui-preview/render_levels.py`, outputs in `ui-preview/level-previews/`.

---

## Level → page → hero render

| Level (`StochasticLevel`) | Page | Hero | Editable |
|---|---|---|---|
| Core | CORE-RAIN | falling-dot field | L1 macros: density, complexity |
| Direct | DIRECT-POND | live ripples + raw param strip | L2 raw: variation, tilt, contour, rate, linearity, rest, slide, jump |
| Weights / Tickets | (existing) | scale + duration ticket grids | already shipped |
| (sub of Core) | MARBLES-BELL | distribution silhouette | marblesBias, marblesSpread, marblesSteps |
| (universal) | LOOP-TAPE | tape strip with window bracket | mutate, patience, sleep, first, last, rotate |

Direct page is **not pure monitor** — earlier draft was wrong. `StochasticLevel::Direct` means direct/raw L2 params (the ones the engine derives from L1 macros by default and the user can override). Pond visualization stays as the hero, step buttons add the underlying-param controls.

---

## Per-page mapping

### CORE-RAIN (Pattern A variant — params console + hero image)

**Steps (one continuous param each, encoder writes when step held):**
| Step | Param | Range | Visual feedback |
|---|---|---|---|
| 1 | `density` | 0..100 | rain drop count |
| 2 | `complexity` | 0..100 | drop jitter / streak length |

**F-keys:**
- F0 SHAPE — toggles `marblesMode` Off/On. When On, MARBLES-BELL becomes reachable.
- F1 MODE — cycles coupled rhythm/melody mode (Loop / Live).
- F2 RENEW — calls `refreshLoopSources()` (momentary action).
- F3 LOCK — toggles `_lock` on/off (per-track loop lock).
- F4 NEXT — cycle to next hero page (CORE → MARBLES if enabled → DIRECT → LOOP → tickets → back to CORE).

**Shift modifiers:**
- Shift + F4 — open list-model view (full param list, current behavior preserved as escape hatch).
- Shift + Step1/Step2 — coarse encoder step (×10).

**Page modifiers:**
- Page + Shift — context menu (INIT, EVEN, RAND, COPY, PASTE per `TuesdayEditPage` Pattern A).
- Page + Step8..15 — QuickEdit slots: COPY1/COPY2/COPY3, PASTE1/PASTE2/PASTE3, RAND, reserved.

**Encoder:** edits whichever step is held; default = density if no step held.
**Left / Right:** previous/next pattern (Performer-global convention).

**Hero image responds to:** density (drop count), complexity (jitter), shape (wind angle — even though Shape is a toggle here, marblesMode On adds a subtle directional bias to the rain).

---

### MARBLES-BELL (only when `marblesMode == On`)

**Steps:**
| Step | Param | Range |
|---|---|---|
| 1 | `marblesBias` | 0..100 |
| 2 | `marblesSpread` | 0..100 |
| 3 | `marblesSteps` | 1..100 |

**F-keys:**
- F0 SHAPE — toggle Off → return to CORE-RAIN.
- F4 NEXT — DIRECT-POND.

**Hero image responds to:** bias shifts peak horizontally, spread widens/narrows the curve, steps quantizes into N visible buckets. Live redraw on encoder turn.

**Context menu:** INIT (defaults), RAND, COPY, PASTE.

---

### DIRECT-POND (raw L2 params + live monitor)

**Steps (raw L2 — overrides engine-derived defaults from L1):**
| Step | Param | Range |
|---|---|---|
| 1 | `variation` | -100..+100 |
| 2 | `tilt` | -100..+100 |
| 3 | `contour` | -100..+100 |
| 4 | `rate` | 1..400 |
| 5 | `linearity` | 0..100 |
| 6 | `rest` | 0..100 |
| 7 | `slide` | 0..100 |
| 8 | `jump` | 0..100 |

**F-keys:**
- F0 RESET — reset all L2 raw values to "default" (engine reverts to L1-derived).
- F4 NEXT — LOOP-TAPE.

**Live status box (Pattern F):** note name, gate square, CV voltage, step counter.

**Hero image responds to:** current CV → ripple center y-position; recent gate events → faded echo ripples to the left; pond floor density changes with rate.

---

### LOOP-TAPE

**Steps:**
| Step | Param | Range |
|---|---|---|
| 1 | `mutate` | 0..100 |
| 2 | `patience` | 0..100 |
| 3 | `sleep` | 0..100 |
| 4 | `first` | 0..size-1 |
| 5 | `last` | first..size-1 |
| 6 | `rotate` | -64..+64 |

**F-keys:**
- F0 MODE — toggle rhythm/melody source mode.
- F1 RENEW — `refreshLoopSources()`.
- F2 LOCK — toggle `_lock`.
- F4 NEXT — back to CORE-RAIN (cycle wraps).

**Shift modifiers:**
- Shift + Step1 — manual mutate trigger (one-shot, ignores probability).
- Shift + Step4/Step5 — set `first`/`last` to current playhead position.

**Page modifiers:**
- Page + Shift — context menu (INIT WINDOW, CLEAR LOCK, RESEED).

**Read-only visualization (not user-controlled):**
- Boredom counter (`_loopCycleCount` progress toward patience threshold) — drawn as a fill bar under the tape.
- Sleep countdown (`_sleepRemaining` when > 0) — drawn as a dimming pass over the tape.
- Jump register (`_jumpRegister` ∈ {-1, 0, +1}) — a small ±1 octave marker at the right edge.

**Hero image responds to:** mutation rate → scratch density on the tape; window first/last → bracket positions; rotate → tape offset.

---

## LED color domains (bi-color per `Key.h` LED conventions)

| Domain | Color | Pages using |
|---|---|---|
| Core macros | orange (red+green) | CORE-RAIN steps 1–2 |
| Marbles family | green only | MARBLES-BELL steps 1–3 |
| Direct / raw L2 | green only (dimmer) | DIRECT-POND steps 1–8 |
| Looper / mutation | red only | LOOP-TAPE steps 1–6 |
| Window / loop bracket | red blinking | LOOP-TAPE steps 4–5 when held |
| Held / encoder target | both at full brightness | any step currently being edited |
| QuickEdit available | green dim, only while Page held | steps 8–15 across pages |

OLED is monochrome — the hero image carries the domain via shape (rain / bell / pond / tape), the OLED doesn't repeat the color legend.

---

## Page nav indicator (header)

Tiny dot strip in the header (left of mode label), three dots per page-index, four positions:

```
●○○○  CORE
○●○○  MARBLES
○○●○  DIRECT
○○○●  LOOP
```

Ticket pages (Pitch / Duration) keep their existing F5 NEXT cycle but show the dot as filled.

---

## Open questions

- [ ] How does the user enter Stochastic level switching from elsewhere — TopPage routes to which hero by default? Likely CORE on track-mode swap.
- [ ] When `marblesMode == Off`, does F4 NEXT skip MARBLES or land there with steps showing "OFF"? Recommendation: skip — page is reachable only via SHAPE toggle.
- [ ] Page + Step4/Step5/Step6/Step14 sub-menus (per `IndexedSequenceEditPage`) — should we use any of these for Stochastic-specific helpers (RAND TICKETS, NORMALIZE WEIGHTS, etc.)? Not in MVP.
- [ ] StepSelection multi-step holding on hero pages — useful if user wants to nudge `mutate + sleep` together? Probably overkill at L1; defer.
- [ ] List-model entry/exit — current Stochastic edit page uses a list model fallback. Shift+F4 from any hero should drop into it for advanced params (octave / transpose / cvUpdateMode / etc.).
- [ ] Burst — deferred per Cluster F, no hero page. Goes into list-model fallback.

---

## Next steps

1. Iterate `pages_levels.py` to make hero images respond to param values (live redraw on encoder simulation). Currently they use a fixed `LevelDemo` — extend to take a `(density, complexity, bias, spread, …)` tuple and re-render at multiple values to visualize the response.
2. Add boredom / sleep / jump readouts to `render_loop_tape()`.
3. Add page-nav dot strip helper to `painters.py` for the header.
4. Once visuals lock, draft C++ implementation order: new `StochasticHeroPage.{h,cpp}` (one class, switch on `StochasticLevel` for which hero render to call); wire into `TopPage` dispatch when track is in Stochastic mode.
