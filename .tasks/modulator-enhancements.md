# modulator-enhancements

## Goal
Two additions to the global modulator engine, captured from the routing/shaper discussion
(2026-06-08). Both are independent; item 1 is the bigger one (wallclock dependency).

## Item 1 — free wallclock Hz mode
Add a **completely free, wall-time** modulator mode whose **rate is defined in Hz**, decoupled
from the sequencer clock.

- Today every mode (incl. `Mode::Free`) sets rate in **ticks** — `Modulator::rate()` is
  `6..6144`, "musical divisions of PPQN" (`model/Modulator.h:104`). So "Free" is still
  clock-relative, not absolute time.
- New mode advances phase from real wall-time at a Hz rate (e.g. 0.01–50 Hz), so an LFO runs at
  an absolute speed regardless of tempo / clock stopped.
- **Depends on the `WallClock` service** (`wallclock-time-architecture` task — 64-bit µs
  free-running reference + `WallTimer`). Phase advance = `dt_seconds * rateHz` per update.
- Touches: `model/Modulator.h` (new Mode + a Hz rate field/interpretation; rate display in Hz),
  `engine/ModulatorEngine.{h,cpp}` (wall-time phase advance for the new mode),
  `ui/pages/ModulatorPage.{h,cpp}` (mode + Hz rate edit/display). ui-preview render first.
- Why it matters here: the legacy stateful shapers were all hardcoded to "~1 Hz LFO"; a
  Hz-defined modulator makes that rate explicit and tunable — the natural home for the
  followers if/when they migrate (see the shaper discussion below).

## Item 2 — shaper selector after modulator depth
Put the routing **shaper selector** (None / Fold / Crease + off-center variants — `RouteShaper`)
into the modulator signal chain, **after depth**: `shape generator → depth → SHAPER → output`.

- Add a shaper field to the modulator model (reuse `Routing::Shaper` or a curated subset),
  apply it in `ModulatorEngine` after the depth scale, expose a selector on `ModulatorPage`
  **after the depth control** (engine + model + ui, all three).
- Lets a modulator fold/crease its own output without a routing hop — a modulator can be a
  folded LFO directly.
- ui-preview render the ModulatorPage layout change first (selector placement after depth).

## Key files
- `model/Modulator.h` — Mode enum (Free/Sync/Retrigger), `rate()` (ticks), `depth()` (0-127).
- `engine/ModulatorEngine.{h,cpp}` — per-modulator update / phase advance.
- `ui/pages/ModulatorPage.{h,cpp}` — modulator edit page; `ui-preview/pages_modulator.py`.
- `model/RouteShaper.h` — the shaper stage to reuse for item 2.

## Context / related
- Came out of the routing-matrix shaper cleanup: the routing lane now keeps only the stateless
  folds (None/Fold/Crease + F30/F70/C10/C90). The 5 **stateful** shapers (Location/Envelope/
  FrequencyFollower/Activity/ProgressiveDivider) are slated to leave routing and become
  **input-reading modulators** — which needs the modulator engine to gain an input-source
  concept (separate prerequisite). VcaNext → `scaleSource` (routing); ProgressiveDivider →
  clock/gate utility or drop.
- Item 1's Hz mode + the input-reading-modulator work together would let those followers run as
  real, rate-defined modulators instead of inline ~1 Hz routing shapers.

## Depends on
- Item 1: `wallclock-time-architecture` (WallClock/WallTimer substrate).
- Item 2: nothing (reuses `RouteShaper`).

## Open
- [ ] Item 1: Hz rate range + resolution; how the mode coexists with the tick-rate field
      (separate field vs reinterpret); behavior when clock stopped.
- [ ] Item 2: full `Routing::Shaper` set vs a curated modulator subset; whether the off-center
      variants belong here too.
