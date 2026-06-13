# UI / Edit-Page Flash Hyperoptimization — Research Findings

Snapshot: 2026-06-13. Branch `feat/tt2-v2-native`.

Measured with `arm-none-eabi-size` / `nm --print-size --size-sort` / `c++filt`
(ArmGNUToolchain 15.2) against fresh `build/stm32/release` objects.

## Situation

The firmware is at the 1 MB flash ceiling **on this branch**. At HEAD (without the
pending Q op batch) it links; adding Q's ~4.5 KB overflows ROM by 4,460 bytes
(`region ROM overflowed`). `.text` alone is ~975 KB, but total ROM (`.text` +
`.rodata` + `.data` + `.ARM.extab` + `.ARM.exidx`) is what overflows — effective
headroom is ~0. The resource-optimization task's "~199 KB flash headroom" figure
predates TT2 and is feat/stochastic; the TT2 dialect (Ragel parser, full op_enum,
upstream op metadata, engine) consumed it.

This doc supersedes the per-page snapshots in `editpages-flash.md` / `pf-flash.md`
(those symbol sizes are stale; the sources grew).

---

## Headline: the compiler-flag lever dwarfs every page refactor

Release build flags (`src/CMakeLists.txt:26-27`, `src/platform/stm32/CMakeLists.txt:9`):
`-O2 -funroll-loops -fshort-enums`, `-ffunction-sections -fdata-sections` +
linker `--gc-sections`, **no `-flto`**.

| Lever | Est. flash win | Risk | Notes |
|---|---|---|---|
| **Drop `-funroll-loops`** | several KB–tens of KB | low | pure code-growth flag; no benefit on UI/sequencer workload |
| **`-O2` → `-Os`** (whole or selective) | ~50–110 KB (5–12% of .text) | med | must verify engine/DSP/tick timing; selective `-O2` on `engine/` hot files + `-Os` elsewhere is the safe middle |
| **Enable `-flto`** | ~30–75 KB (3–8%) | med-high | dedups inlined helpers across TUs; watch ISR/timing inlining + ODR with the C Teletype sources |

Dead-code stripping (`--gc-sections`) is already on. The per-page refactors below
total ~26–43 KB at medium risk and high effort. **The flags are the first move** —
they likely reclaim more than the entire UI refactor program, at a fraction of the
work. Do them first, behind a timing-verification pass on the audio/engine paths.

---

## Per-page findings

Bloat in this layer is overwhelmingly **intra-function duplication** (parallel
`cell`/`edit` switches, copy-pasted draw scaffolding, repeated enum→label switches),
plus PhaseFlux's `std::function` tax. WindowPainter/BasePage scaffolding is already
shared — don't chase frame/header/footer.

### PhaseFluxEditPage — 41,749 B
- `editSlot` 9,770 + **~2 KB of out-of-line lambda thunks** the old doc didn't count → true `std::function` cost ~11.6 KB.
- 23 `forEachCell([&]{…})` sites each build a `std::function` from a fresh capturing lambda.
- **Fix:** the documented `PhaseFluxStageEdit` enum + `editStage(stage, edit, value)` + a non-template `applyToCells(edit, value)` iterator. Kills all 23 thunks; `editSlot` becomes pure slot→enum mapping. `togglePressSlot`'s 4 lambdas are already non-capturing → free function pointers.
- **Win ~3–5 KB. Risk low-med, effort M.** Highest single-page win.

### TrackPage — 24,797 B
- ~13.9 KB (56%) is inline `*ListModel::cell`/`edit`/`formatValue` instantiated only here. Each model = parallel per-Item `cell` + `edit` switches. Heaviest: TeletypeTrackListModel ~3.3 KB, StochasticPerformanceListModel ~2.6 KB, TuesdaySequenceListModel ~1.9 KB.
- **Fix:** per-model row-descriptor table (label + formatter tag + edit-routine tag) collapsing the two parallel switches. Relocating to .cpp does **not** save flash — they're single-instance already.
- **Win ~2–4 KB (3–4 models). Risk med, effort M-L.**

### RoutingPage — 23,011 B
- `matrixEditSource` (1,524) and `matrixEditScale` (1,820) are line-for-line parallel — same group-jump loop copy-pasted. The three draw fns (`drawTabEditor`/`drawMidi`/`drawBus` = 7.1 KB) each rebuild the same footer 5-slot array + scrollbar + header. Four `handle*Key` tab-cycle blocks identical.
- **Fix:** merge into `stepDraftSource(delta, group, scale)` + `adjacentGroupIndex()`; extract `drawRoutingFooter()` and `cycleTab(dir)`.
- **Win ~2.5–3.5 KB. Risk low-med, effort M.**

### CurveSequenceEditPage — 29,173 B
- `draw` (4,088) inlines three full screens; Wavefolder and Chaos column views are near-verbatim. `encoder` (3,140) inlines two `showMessage` builders incl. the 8-case `AdvancedGateMode` long-name switch (also duplicated short-name in `drawDetail`). Four context actions repeat the first/last-step range ladder.
- **Fix:** `drawParamColumns(descriptor)` + shared `drawBar`; move gate/gate-prob message builders out of `encoder`; `advancedGateModeShortName/LongName`; `resolveContextRange()`.
- **Win ~3.5–4.5 KB. Risk med (draw — ui-preview first), effort M.**

### DiscreteMapSequencePage — 30,986 B
- `clusterContextAction` 5,280 inlines five threshold algorithms; the "collect active+selected targets" loop is copy-pasted 4×; Swell/InverseSwell/AccelRit4 share an identical cumulative-weight writer (3 copies); a `std::sort` on ≤8 ints drags in introsort+heap (~890 B).
- **Fix:** `collectActiveTargets()`, `writeWeightedSpan()`, replace `std::sort` with insertion sort.
- **Win ~1.3–1.8 KB (the std::sort swap alone ~0.9 KB). Risk med, effort M.**

### StochasticSequenceEditPage — 30,124 B
- Doc proposal correct in direction, stale in detail: real triplicated 16-slot map is in `contextAction` (3,448), not `editLiveStep` (748). `stoEng()` lambda redefined 5×; NewR/NewM renew block duplicated ~6×; sequence-fetch boilerplate ~15×.
- **Fix (staged):** A) `selectedStochasticEngine()` + `currentSequence()` accessors; B) `handleRenew()` + `toggleSourceMode()`; C) `kLiveSlots[16]` descriptor table for Init/Random/readout.
- **Win ~2–3 KB. Risk A=low/B=low-med/C=med, effort S/M/M.** Caveat: per-member-pointer table must preserve shift asymmetry; leave relative `editLiveStep` writes hand-coded.

### NoteSequenceEditPage — 20,894 B
- `draw` (2,900) + `drawDetail` (1,588) duplicate four enum→abbrev switches (GateMode/Harmony/Inversion/Voicing), short vs long. Seven layer cases repeat the centered-step-text idiom; ~10 drawDetail cases are percent-print clones.
- **Fix:** short/long lookup tables (dense 0..N enums), `drawCenteredStepText()`, factor the drawDetail percent tail.
- **Win ~1.5–2.5 KB. Risk low-med (ui-preview glyph rows), effort M.**

### TeletypeScriptViewPage — 17,052 B
- Four save/load slot paths near-identical (suspend → busy.show → FileManager::task lambda → completion lambda), each its own `std::function`. `kShiftCount[16]`/`kBaseCount[16]` are arrays of identical values.
- **Fix:** one parameterized `slotIo(verb, FileType, task)`; replace constant-count arrays with a scalar.
- **Win ~1.5–2.5 KB. Risk med (overwrite/confirm flow), effort M.**

### ModulatorPage — 16,241 B
- `draw` 5,252 monolith: per-shape footer-label tree + mirrored value-build tree; scope-box draw byte-identical to `drawGeodeVoice`'s; param panel duplicated too.
- **Fix:** `drawScopeBox()` + `drawParamPanel()` (low-risk ~1 KB); shape×page label/value table (higher risk — JustF/Geode override specific cells post-fill).
- **Win ~2–3 KB. Risk med, effort M-L.**

### GeneratorPage — 15,107 B
- `drawValueGenerator` 3,680 (+504 clone) has 9 `[&]` lambdas sharing the same per-step scaffold; reroll shape (randomize→update→arm-preview) duplicated 3× across `keyPress`/`contextAction`.
- **Fix:** `rerollGenerator(seedOnly)`; merge Random/Algo context arms; optional shared per-step renderer.
- **Win ~1.5–2.5 KB. Risk low (merges)/med (renderer), effort S/M-L.**

### IndexedSequenceEditPage — 32,734 B
- Not the duplicated-map shape; mostly genuine timeline algorithm. One real dup: the `EditMode` switch + gate-decrement clamp ladder appears twice in `encoder`.
- **Fix:** `editStepValue(step, mode, delta, …)` from both branches.
- **Win ~0.6–1 KB. Risk low, effort S-M.** Don't touch `draw`/quick-edit.

### TuesdayEditPage — 10,942 B
- Six parallel param switch chains; `paramShortName`/`paramMax`/`paramIsBipolar` → one `ParamDesc[]` table. Copy/paste = mirror 17-field lists.
- **Win ~0.6–1 KB. Risk low, effort S-M.** (`format`/`value`/`edit` resist tabling — distinct calls.)

### TeletypePatternViewPage — 5,627 B
- `setStart/setEnd/setLength` + keyboard inc/dec each repeat a lock→set→syncPattern→unlock envelope.
- **Fix:** `withLockedState(fn)` wrapper. **Win ~0.4–0.7 KB. Risk low, effort S.**

### ListPage — 11,163 B
- ~6.1 KB is the inline routing-modulation editor (SPREAD sub-view) — real single-instance feature code, not duplication. **No per-object win; leave it.**

---

## Cross-cutting refactors (touch many pages at once)

- **Shared `keyboard()` nav/clipboard dispatch in BasePage** — 31 pages reimplement the same flat keycode if-chain (~5.8 KB total). Hoist `handleNavigationKeys()`/`handleClipboardKeys()`. **~2–4 KB, risk low-med.**
- **Centralized enum→name helpers** (`ui/model/EnumNames.h`) — 8 pages define local name switches + `CSWTCH` rodata. **~1–2 KB, risk low.**

---

## Recommended execution order

1. **`-funroll-loops` removal** — measure; likely several KB–tens of KB, near-zero risk.
2. **`-Os` evaluation** (whole-image, then selective `-O2` on engine hot paths) behind an engine-timing verification pass — the dominant lever (~50–110 KB).
3. **`-flto` evaluation** — stack on top (~30–75 KB) if timing/ODR hold.
4. **PhaseFlux `editSlot` std::function removal** (~3–5 KB) — biggest, well-specced page win.
5. **RoutingPage source/scale merge + footer/tab helpers** (~2.5–3.5 KB) — high confidence.
6. **Curve column-merge + gate-mode names** (~3.5–4.5 KB, ui-preview).
7. **Stochastic A→B→C**, **Note tables**, **TrackPage ListModel tables**, **Modulator/Generator extractions** — as needed.
8. Cross-cutting keyboard() + enum-name centralization for broad sweep.

Realistic page+cross-cutting total: ~26–43 KB (medium risk, high effort).
The flags (steps 1–3) likely make the flash problem disappear on their own and
should be proven first; the page refactors are the durable structural cleanup and
the fallback if `-Os`/LTO can't be fully adopted for timing reasons.

## Measurement commands

```bash
SIZE=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi/bin/arm-none-eabi-size
NM=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi/bin/arm-none-eabi-nm
FILT=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi/bin/arm-none-eabi-c++filt
O=build/stm32/release/src/apps/sequencer/CMakeFiles/sequencer_shared.dir/ui/pages
$SIZE $O/<Page>.cpp.obj
$NM --print-size --size-sort --radix=d $O/<Page>.cpp.obj | $FILT | tail -30
```
