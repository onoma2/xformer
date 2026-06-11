# PhaseFlux Timing v0.3 — Top-Down Time (Spec)

**Status:** accepted 2026-06-12 — all product calls resolved; ready for planning.

## 1. Problem

PhaseFlux v0.2 timing is bottom-up: every cell computes its own ticks
(`stageDivisorTicks(slot) × stageLen/64 × divisor/12 × 100/clockMult`) and the
pattern is whatever they sum to. Pattern length is never a first-class
quantity, so:

- Four controls scale length on one axis (divisor, clockMultiplier global;
  stageDivisor slot, stageLen per cell) plus LenN nudging it a fifth way —
  and the per-cell base-duration chooser (`stageDivisor`) turns out to be
  dead code: no UI edit path exists; every cell has sat on its default
  (musically, a quarter note) since the field was added.
- Names lie: clockMultiplier=150 *shortens*, divisor=24 *lengthens*.
- Three silencers with three time semantics: skip (Adaptive drops the
  cell's time, Fixed keeps it), stageLen=0 (drops time in both modes),
  pulseCount=0 (keeps time).
- Skip count leaks into timing through everything measure-anchored.
- No edit gesture conserves pattern length; any Len touch moves the grid.

Family survey (who owns time):

| Track | Pattern length | Step edits move time? |
|---|---|---|
| NoteTrack | steps × divisor (grid) | never — step length is gate only |
| DiscreteMap | **= divisor (declared period)** | never — stages are thresholds on a ramp |
| Indexed | Σ step durations (ER-101) | always — compensated by DUR-TR transfer |
| PhaseFlux v0.2 | Σ slot×len (bottom-up) | always — **no compensating tool** |

v0.3 adopts the in-family precedents: **DiscreteMap's declared period** as the
time owner, **Indexed's transfer** as the boundary edit.

---

## 2. Requirements (owner-stated)

- **R1 — 256 musical events.** 16 cells × 16 pulses must be reachable at
  musical spacing, not granular noise. This is a named capability: the
  largest pattern in the box (NoteTrack 64 steps, Indexed 48; PhaseFlux 256).
- **R2 — clear pulse spacing.** Spacing within a cell must be predictable
  from that cell's own controls; no other cell's existence may change it.
- **R3 — perceivable sync from settings.** Lock to the bar by construction,
  not by leaning on resetMeasure to re-anchor.
- **R4 — actual phasing between two PhaseFlux tracks.** Reich-style: free
  drift (Rate offset) and held/ modulated phase relationship (globalPhase),
  without losing the grid.

---

## 3. Model

```
period (declared, sequence-level)            — THE grid anchor
  └─ cells own SHARES of it (weights)        — boundaries, not durations
       └─ pulses divide their cell           — unchanged (§6.1 pipeline)
```

- **Period** is declared from a slot table of musical values only:
  **1/4, 1/2, 1, 2, 4, 8, 16, 32 bars** (8 slots, 3 bits). Out-of-sync
  periods are unrepresentable (R3). 32 bars hosts 256 events at straight
  eighth-note spacing; 16 bars at sixteenths (R1). Default: 1 bar.
- **Cell span** = `weight_i / Σweights × period`. A cell has no absolute
  duration of its own.
- **No mode. Skip and weight are orthogonal** (decision 2026-06-12):
  - **skip = mute only.** The cell's span plays as silence. Never touches time.
  - **weight = time only.** Weight 0 removes the span. The one deliberate
    way to take a cell's time away (the Even morph skips weight-0 cells, so
    it cannot silently revive a removed span).
  - Fresh default: cells 1–4 weight 64 active, cells 5–16 weight 0 + skip —
    one bar of quarters, same musical default as v0.2.
- **One labeled grid-breaker**: clockMultiplier survives as **Rate**, the
  only control that detunes the pattern against the bar, labeled as such.

### Invariants (the point of the redesign)

1. Pattern length is constant under **all** per-cell edits: weight, skip,
   pulseCount, gate, every curve param.
2. The last cumulative boundary equals `period` exactly (by construction,
   not by floor-stretch).
3. Skip never changes any cell's span (R2): spacing inside a cell depends
   only on its own weight, the period, and its pulseCount.
4. Exactly one control (Rate) can move the pattern off the bar grid.

---

## 4. Parameters

### Kept / re-meaning

- **Period** — new 3-bit slot param per §3 (replaces divisor's role; the
  old raw-tick divisor encoding is gone). Routable.
- **clockMultiplier → Rate** — unchanged range 50..150, re-labeled. The one
  grid-breaker; also the free-drift phasing control between two PhaseFlux
  tracks (R4). Note: 1% steps bound the slowest drift to 1% of period per
  period — acceptable for v0.3; finer resolution via routing if ever needed.
- **stageLen → Weight** — same 7-bit storage, default 64. Relative share,
  no unit. All-equal = even division.
- **globalPhase** (exists, Macro page: Phase/GWarp/Snap/Zero) — kept as-is;
  **delta: make it routable** (new ParamKey) so LFO/CV-driven phasing works
  (R4). cyclePhaseWarp (GWarp) unchanged.
- **resetMeasure** — unchanged; now optional rather than load-bearing (R3).
- **skip** — mute only (§3).
- **pulseCount, gateLength, all curve/pitch params** — unchanged.

### Repurposed

- **LenN → Even morph** (decision 2026-06-12): all-cells weight morph toward
  even division — `weight' = (weight==0) ? 0 : weight + (64−weight)×morph/64`,
  morph 0..64 (0 = as-authored, max = flat grid). Zero length effect
  (grid-safe), performable; **skips weight-0 cells** so §3's "weight 0 removes
  the span" holds. Plain weight-edit also renormalizes all boundaries — Even
  is the all-cells morph variant; both ride the weight axis but both are
  length-safe (so neither breaks the grid, unlike v0.2's length controls).
  Same macro slot, range 0..64.

### Deleted

- **stageDivisor** — outright; its 3 bits return to spare. (Never
  UI-editable; dead since birth.)
- **cycleLength mode (Adaptive/Fixed)** — no replacement; orthogonal
  skip/weight covers both intents (§3).
- **min-cycle proportional stretch** — invariant 2 replaces it.

---

## 5. Engine

`computeCumulativeTicks` v2 collapses to:

```
W = Σ weight[cell]                          // all cells; skip irrelevant to time
cumulativeTicks[i+1] = round(periodTicks × Σ_{k≤i} weight[k] / W)
cycleTicks = periodTicks                    // exact, always
```

- `periodTicks` from the Period slot table × bar length; Rate applies once
  to the period (`× 100 / rate`), not per cell.
- Even morph: `weight'[i] = (weight[i]==0) ? 0 : weight[i] + (64−weight[i])×morph/64`
  before the sums (lerp toward 64; weight-0 cells skipped — §4).
- Skipped cells keep their span; the engine emits silence inside it.
- Rounding lives at interior boundaries only; the endpoint is exact.
- The `fixedCycleLength` parameter and the kMinCycleTicks floor are deleted.
- Everything downstream (stage phase, pulse spread, scope slice widths)
  already consumes `cumulativeTicks` — unchanged.

---

## 6. Editing — transfer as the boundary gesture

Weights are traversal-independent (the cell carries its share wherever the
snake walk places it). Two edit forms on the Weight encoder (P1, the old Len
slot), mirroring Indexed DUR-TR:

- **Plain edit**: weight ±Δ. Grid-safe (invariant 1) but renormalizes — all
  boundaries shift slightly. For sculpting one cell's share.
- **Transfer (W-TR)**: second press of the same F-key toggles transfer; the
  encoder moves Δ from the selected cell to the **next cell in traversal
  order** (loop-aware, skipping zero-weight cells):

  ```
  cur.weight += Δ;  next.weight -= Δ;    // Σ const → only the shared
                                          // boundary moves, nothing else
  ```

  Clamped like Indexed `_durationTransfer` (neither weight leaves 0..127).
  Shift = coarse. Footer shows `W-TR` while active.

---

## 7. Serialization

Write/read the new layout unconditionally. No migration, no sentinels, no
version bump — there is no installed base; dev project files break across
branches by standing policy.

---

## 8. UI

- Sequence list: **Period** row (slot table, shows bars); **Rate** row
  (ex-Clock Mult); the Cycle row is gone.
- P1 encoder slot 0: **Weight** (display format decided at implementation
  via ui-preview renders: raw 0..127 vs share % vs musical fraction).
- Same F-key toggles **W-TR** (footer label swap, DUR-TR pattern).
- Macro page: LenN slot becomes **Even** (the morph).
- Per-cell effective pulse rate display (e.g. "≈1/16") considered at
  implementation — supports R2 readability; ui-preview before shipping.
- All OLED scopes keep working via `cumulativeTicks`.

---

## 9. Out of scope

- Finer Rate resolution (flagged in §4; routing covers it if needed).
- Any new curve/pitch behavior; the §6.1/§6.2 pipelines are untouched.
- Indexed/DiscreteMap/NoteTrack — no changes outside PhaseFlux.

## 10. Resolved decisions (2026-06-12)

1. Mode deleted — skip/weight orthogonal. (was open q1)
2. Weight display — deferred to implementation ui-preview. (open q2)
3. Period slot table to 32 bars — settled by R1. (was open q3)
4. LenN repurposed as Even morph. (was open q4)
