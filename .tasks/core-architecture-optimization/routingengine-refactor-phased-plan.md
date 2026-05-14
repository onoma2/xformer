# RoutingEngine Conditional State / Lifecycle Refactor — Phased Plan

The ONLY architecture change currently justified by `.tasks/core-architecture-optimization/architecture-change-decision-gates.md`.

**Do not implement code.** This is a plan, not a patch.

---

## 1. Decision-Gate Summary: Why This Is the Only Justified Refactor

From `architecture-change-decision-gates.md`:

| Candidate | Decision | Gate Failures |
|---|---|---|
| **RoutingEngine conditional state** | **Yes — narrow architectural change** | None for union-based compaction (Phase 1). Persistence-across-save/load (old Phase 2) **fails Gate 5** (requires save-format change). |
| Teletype file backup consolidation | No | Gate 4 (not a clear new contract) — local optimization only |
| Teletype PatternSlot redesign | Not yet | Gate 4 (unclear replacement contract), Gate 5 (big-bang migration), Gate 6 (no hardware verification path yet) |
| Engine container / Teletype engine pool | Maybe | Gate 7 (product must cap simultaneous Teletype tracks below 8; if not, savings cancel) |
| Model container / lazy sequence storage | No for now | Gate 5 (hard migration, breaks copy/clear/snapshot), Gate 6 (no verification path) |
| Output ownership unification | No | Gate 1 (no repeated pain — dual-path may be musically intentional), Gate 4 (no simpler replacement contract) |
| UI capability matrix | No for now | Gate 7 (negligible RAM payoff, 7 types manageable) |
| Pattern sparse defaults / diffs | Not yet | Gate 5 (breaks value-copy), Gate 6 (no verification), Gate 7 (speculative) |

RoutingEngine conditional state **passes gates 1-4 and 6-7** for union-based compaction:

1. **Repeated pain:** ~7.4 KB CCMRAM baseline, mostly zero state. Every new shaper inflates all 128 TrackState entries unconditionally.
2. **User-visible semantics:** Shaper state resets are coarse and audible (envelope follower restarting from zero mid-operation).
3. **Local fixes fail:** Skipping `TrackState` access in `updateSinks()` at runtime saves zero RAM because the array is statically allocated.
4. **Clear new contract:** Routing shaper runtime state is stored in the smallest static representation needed for the selected shaper, is reset when that route/track shaper configuration changes, and is not serialized unless a separate project-format decision is made.
5. **Migration path:** Storage layout change, no save-format change. Project compatibility preserved.
6. **Verification path:** Simulator builds for logic; hardware builds for continuous CV/Gate output fidelity.
7. **Net payoff:** Up to ~3 KB CCMRAM recovered from Phase 1 union compaction (56 B → ~28-32 B per entry × 128 entries, **UNVERIFIED — must be confirmed by Phase 0 ARM build measurement**). Additional ~3-4 KB only if Phase 3 bounded pool is justified. Also clarifies the routing-as-modulation architecture finding.

---

## 2. Current-State Evidence

### 2.1 `RouteState` struct layout (RoutingEngine.h:40-62)

```cpp
struct RouteState {
    Routing::Target target = Routing::Target::None;
    uint8_t tracks = 0;
    std::array<Routing::Shaper, CONFIG_TRACK_COUNT> shaper{};
    struct TrackState {
        float location = 0.5f;          // 4 B
        float envelope = 0.f;           // 4 B
        float freqAcc = 0.f;            // 4 B
        bool freqSign = false;          // 4 B (with padding)
        float activityPrev = 0.5f;      // 4 B
        float activityLevel = 0.f;      // 4 B
        bool activitySign = false;      // 4 B (with padding)
        uint16_t ffHold = 0;            // 4 B (with padding)
        uint16_t actHold = 0;           // (shared 4 B block)
        float progCount = 0.f;          // 4 B
        float progThreshold = 1.f;      // 4 B
        bool progSign = false;          // 4 B (with padding)
        float progOut = 0.f;            // 4 B
        float progOutSlewed = 0.f;      // 4 B
        uint16_t progHold = 0;          // 4 B (with padding)
        // === 56 B per TrackState ===
    };
    std::array<TrackState, CONFIG_TRACK_COUNT> shaperState;
    // === 56 B × 8 = 448 B per RouteState ===
};

std::array<RouteState, CONFIG_ROUTE_COUNT> _routeStates;
// === 448 B × 16 = 7,168 B minimum TrackState baseline ===
```

### 2.2 `resetShaperState()` unconditionally zeroes all entries (RoutingEngine.cpp:153-160)

```cpp
void RoutingEngine::resetShaperState() {
    for (auto &routeState : _routeStates) {
        for (auto &st : routeState.shaperState) {
            st = RouteState::TrackState();
```

This is called from `Engine::reset()` (project load, route target change, route shaper change). It **resets accumulated state for all routes and all tracks**, even for routes whose configuration did not change.

### 2.3 `isPerTrackTarget()` classification (Routing.h:352-356)

```cpp
static bool isPerTrackTarget(Target target) {
    return isPlayStateTarget(target) || isTrackTarget(target) || isSequenceTarget(target) ||
           isTuesdayTarget(target) || isChaosTarget(target) || isWavefolderTarget(target) ||
           isDiscreteMapTarget(target) || isIndexedTarget(target);
}
```

**Non-per-track targets (no TrackState needed):** None, Play, PlayToggle, Record, RecordToggle, TapTempo, Tempo, Swing, CvRouteScan, CvRouteRoute, BusCv1-4.
**Per-track targets with stateful shapers:** Only those routes that actually assign a stateful shaper (Location, Envelope, FrequencyFollower, Activity, ProgressiveDivider) to one or more tracks.

### 2.4 Five stateful shapers confirmed (Routing.h:358-369)

| Shaper | Uses TrackState Fields |
|---|---|
| `Location` | `location` |
| `Envelope` | `envelope` |
| `FrequencyFollower` | `freqAcc`, `freqSign`, `ffHold` |
| `Activity` | `activityPrev`, `activityLevel`, `activitySign`, `actHold` |
| `ProgressiveDivider` | `progCount`, `progThreshold`, `progSign`, `progOut`, `progOutSlewed`, `progHold` |

Stateless shapers: None, Crease, TriangleFold, VcaNext.

### 2.5 Runtime branching is insufficient

`RoutingEngine::update()` iterates over all 16 routes and all 8 tracks, applying shaper transforms. Skipping unused TrackState entries at runtime through `if (shaper != None)` **does not reduce static allocation**. The `RouteState[16]` array is a fixed-size struct. The storage layout itself must shrink.

---

## 3. New Contract in One Sentence

> Routing shaper runtime state is stored in the smallest static representation needed for the selected shaper, is reset when that route/track shaper configuration changes, and is not serialized unless a separate project-format decision is made.

### 3.1 Union Discriminant Source Contract

The tagged union's discriminant is **derived from `route.shaper(trackIndex)` at access time** — the same value already used in the `switch (shaper)` in `updateSinks()`. The discriminant is **not** stored as an independent field in the union itself. This avoids two independent sources of truth for shaper type on a real-time 1 kHz path, where disagreement would cause silent state corruption with audible artifacts.

Concretely: `routeState.shaperState[trackIndex]` becomes a union, and the access pattern changes from `st.location` to a branch on `route.shaper(trackIndex)` that reads the correct union member. The union does not carry its own tag field. This preserves the existing access pattern where `route.shaper(trackIndex)` is the single arbiter of which shaper runs.

### 3.2 Union Zero-Initialization Semantics

When a `TrackStateUnion` is zero-initialized (via `= {}` or value-initialization), all bytes are set to zero. This matches the current behavior of `st = RouteState::TrackState()` which zero-initializes all fields. For each shaper, zero-initialization produces:

| Shaper | Zero-initialized values | Correct initial values | Mismatch? |
|---|---|---|---|
| **Location** | `location = 0.f` | `0.5f` | **Yes** — must explicitly initialize |
| **Envelope** | `envelope = 0.f` | `0.f` | No |
| **FrequencyFollower** | `freqAcc = 0.f, freqSign = false, ffHold = 0` | Same | No |
| **Activity** | `all zero` | `activityPrev = 0.5f, activityLevel = 0.f, ...` | **Yes** — must explicitly initialize |
| **ProgressiveDivider** | `progCount = 0.f, progThreshold = 0.f, ...` | `progThreshold = 1.f` | **Yes** — must explicitly initialize |

Zero-initializing a union does **not** produce correct initial values for Location, Activity, or ProgressiveDivider. The implementation must provide a `resetState(Shaper)` helper that sets correct per-shaper initial values, called when:
1. `resetShaperState()` is called (Phase 1 blanket reset, matching current behavior).
2. A shaper config change is detected on a route/track (Phase 2 lifecycle).
3. An inactive route zeroes its state every tick via `st = {}` in `updateSinks()` — this line becomes `st = {}; resetState(route.shaper(trackIndex));` or preferably `resetState(route.shaper(trackIndex))` which sets correct initial values directly.

### 3.3 Bus Target Track-State Sharing Invariant

`RoutingEngine::updateSinks()` uses `shaperState[kBusShaperTrack]` (where `kBusShaperTrack = 0`) for bus targets, and `shaperState[trackIndex]` for per-track targets. A single route cannot be both a bus target and a per-track target — `isBusTarget()` and `isPerTrackTarget()` are mutually exclusive. This invariant means bus targets and per-track targets never compete for the same union slot within a single route. The union redesign must preserve this mutual exclusion. Track 0's union slot serves double duty (bus shaper state OR per-track track-0 state) depending on the route's target type, and this is safe because the target type is immutable for a given route within a tick.

---

## 4. Non-Goals

| Non-Goal | Why Excluded |
|---|---|
| Remove any of the 5 stateful shapers | They produce time-dependent output; removing them would change musical behavior, not just RAM layout. |
| Change the `Route` model serialization format | `Routing::Route` stores per-track `biasPct`, `depthPct`, `creaseEnabled`, `shaper` arrays. These are user-facing persistent parameters. Only runtime state layout changes. |
| Heap allocation for TrackState | Bare-metal STM32 with no MMU. Heap fragmentation would hard-fault. Static bounded layout is preferred. |
| Unified output ownership or Teletype bypass changes | Out of scope for this refactor. The dual-path output question is Direction 4, not Direction 3. |
| Model/Engine container split | Not justified by decision gates. See "No for now" in decision table. |
| Teletype PatternSlot redesign | Not justified. See "Not yet" in decision table. |
| UI capability matrix | Not justified. See "No for now" in decision table. |
| `CONFIG_ROUTE_COUNT` or `CONFIG_TRACK_COUNT` changes | Constants are fixed by UI and project format. Changing them is a product decision, not an architecture refactor. |
| Move TrackState out of CCMRAM | Not a goal. The refactor keeps state in CCMRAM where it belongs (1 kHz engine access). Only reducing the footprint is the goal. |
| Persist shaper state across project save/load | This would require serialization (Gate 5 fail). The contract resets state on project load, matching current behavior. |

---

## 5. Phases

### Phase 0: Measurement-Only Baseline

**Goal:** Record exact current footprint before touching storage layout.

| Detail | Value |
|---|---|
| **Files touched** | None (measurement only) |
| **Changes** | Add temporary `static_assert(sizeof(RoutingEngine::RouteState) <= 468, "RouteState size regressed")` to `RoutingEngine.cpp`. Add temporary `static_assert(sizeof(RoutingEngine::RouteState::TrackState) <= 56, "TrackState size regressed")`. Use `<=` not `==` to avoid build breaks from benign layout changes while still catching regressions. |
| **Exact behavior preserved** | 100% — no functional changes. |
| **Expected RAM delta** | 0 |
| **Verification command** | `arm-none-eabi-size build/stm32/release/src/apps/sequencer/sequencer.elf` and `arm-none-eabi-nm --print-size --size-sort build/stm32/release/src/apps/sequencer/sequencer.elf \| grep -E "routing\|Rout" \| head -20` |
| **Hardware check** | Boot existing firmware. Verify routing with Location, Envelope, FrequencyFollower shapers produces expected ramps with known CV input. |
| **Rollback risk** | None |
| **Exit criteria** | Baseline CCMRAM and `.bss` sizes documented. `RouteState` (≤ 468 B) and `TrackState` (≤ 56 B) sizes confirmed on target. **If actual sizes differ from the 468/56 B estimates, all savings projections in this plan must be recalculated before proceeding to Phase 1.** |

---

### Phase 1: Split TrackState into Per-Shaper Structs + Union/Discriminant

**Goal:** Replace the monolithic 56 B `TrackState` with a fixed-size tagged union. Expected entry size is max(shaper-state variants) + tag/padding; measure exact ARM size. Savings come from removing fields unused by all variants, not from runtime shaper distribution.

**Why this works:**
- `RouteState[16]` is an array of identical structs. You cannot make route 0 smaller than route 1 at compile time because routes change target at runtime.
- But you **can** make every `TrackState` smaller by splitting it into per-shaper state structs.
- A `union` of `{ LocationState, EnvelopeState, FrequencyFollowerState, ActivityState, ProgressiveDividerState }` plus a discriminant is smaller than the monolithic struct because the union shares storage — the 56 B monolith shrinks to the size of the largest variant (~24-28 B **UNVERIFIED — ProgressiveDivider has 6 fields, exact ARM packing depends on alignment**) plus tag/padding.
- Stateless shapers (None, Crease, TriangleFold, VcaNext) conceptually need no state, but in a fixed-array union design they still occupy the union's max size. Actual per-entry RAM does not shrink for stateless entries until a sparse/pool layout (Phase 3) is introduced.

| Detail | Value |
|---|---|
| **Files touched** | `src/apps/sequencer/engine/RoutingEngine.h`, `src/apps/sequencer/engine/RoutingEngine.cpp` |
| **Changes** | Replace `TrackState` with a `union` of per-shaper structs (no independent tag — discriminant comes from `route.shaper(trackIndex)` at access time, per Section 3.1). Each per-shaper struct contains only its required fields. Access in `updateSinks()` branches on `route.shaper(trackIndex)` to read correct union member. `resetShaperState()` stays unconditional for now. The `!route.active()` zeroing path (`st = RouteState::TrackState()` at line ~492) becomes `st = {}; resetState(route.shaper(trackIndex));` or a direct `resetState(Shaper::None)` for inactive routes, since inactive routes still need correct initialization semantics (Section 3.2). Per-shaper initial values for Location (`0.5f`), Activity (`activityPrev = 0.5f`), and ProgressiveDivider (`progThreshold = 1.f`) must be set by `resetState(Shaper)`, not by zero-initialization. |
| **Exact behavior preserved** | All 5 stateful shapers produce identical accumulated-state output. All 4 stateless shapers pass through unchanged. Reset behavior is unchanged (still resets on `Engine::reset()`). |
| **Expected RAM delta** | Estimate (**UNVERIFIED — must be confirmed by Phase 0 ARM build measurement**): 56 B → ~28-32 B per TrackState based on removing fields unused by any variant, plus union sharing. With 128 entries: ~3,072 B saved. Exact size must be measured on ARM build because union alignment/padding is architecture-dependent. Phase 0 measurement is prerequisite — do not start Phase 1 without confirmed baseline sizes. |
| **Verification command** | `arm-none-eabi-size build/stm32/release/src/apps/sequencer/sequencer.elf` — confirm `.ccmram_bss` decreased. Build `build/sim/debug` and run. |
| **Hardware check** | Boot and verify all 9 shaper types produce correct output. Test with continuous CV input for 5 minutes. Check for snaps or DC drift. |
| **Rollback risk** | Low — revert `RoutingEngine.h` and `RoutingEngine.cpp`. |
| **Exit criteria** | `.ccmram_bss` shows decrease. All 9 shapers produce output identical to Phase 0 baseline. Build passes `stm32/release` and `sim/debug`. |

**Critical note:** A static pool sized to worst case (128 entries) with a separate index does **not** save RAM over the current array unless the per-entry size shrinks. The union approach shrinks every entry. A bounded active-state pool (Phase 3) is only viable if you can set a static cap below 128.

**Static bounded layout preferred over heap:** The STM32F4 has no MMU. A union within a fixed array is deterministic, linker-analyzable, and fragmentation-free. No `new`/`delete`.

---

### Phase 2: Narrower Reset Lifecycle — Preserve Full Reset on Engine Reset, Add Per-Track Config-Change Reset

**Goal:** Stop unconditionally resetting all TrackState on every `Engine::reset()` call. Reset only when a route's target or shaper actually changes during normal operation. Preserve full reset on `Engine::reset()` (project load) — matching current save/load semantics.

**Actual scope is smaller than it appears.** The existing `updateSinks()` already detects per-route config changes and resets that route's shaper state (lines 340-357 in RoutingEngine.cpp). It also tracks `routeState.target` and `routeState.shaper[]` for comparison. Phase 2's actual delta is:
1. Removing the blanket `resetShaperState()` call from `Engine::reset()` and replacing it with per-route/per-track lifecycle methods that still reset all state on project load (preserving current save/load semantics).
2. Adding **per-track** granularity to the existing per-route reset — currently, when any shaper changes on a route, the entire route's TrackState array is reset. Phase 2 narrows this to only reset tracks where the shaper actually changed.
3. Using the Phase 1 `resetState(Shaper)` helper to set correct per-shaper initial values instead of zero-initializing the monolithic struct.

The key behavioral change: a shaper change on track 3 of route 5 no longer resets shaper state on tracks 0, 1, 2, 4, 5, 6, 7 of that same route. This is a net improvement (less state destruction), but must be verified to not produce audible artifacts from "stale" state on unchanged tracks.

| Detail | Value |
|---|---|
| **Files touched** | `src/apps/sequencer/engine/RoutingEngine.h`, `src/apps/sequencer/engine/RoutingEngine.cpp` |
| **Changes** | Replace blanket `resetShaperState()` with per-route lifecycle: `resetState(int route, int track, Shaper shaper)` which sets correct per-shaper initial values. On `Engine::reset()` (project load), call this for all routes/tracks to preserve current reset-all behavior. In `updateSinks()`, change the `routeChanged` block from resetting all 8 tracks to only resetting tracks where `route.shaper(trackIndex) != routeState.shaper[trackIndex]` for per-track targets. Preserve the existing full-reset for `!route.active()` and target-type changes. |
| **Exact behavior preserved** | The 5 stateful shapers accumulate across ticks for unchanged routes/tracks. They reset only on active config change or project load. Tracks on the same route whose shaper did not change continue accumulating without interruption. |
| **Expected RAM delta** | 0 (lifecycle change, not footprint change — state was already union-allocated in Phase 1). |
| **Verification command** | `build/stm32/release` boots. Build `build/sim/debug`, run routing tests. |
| **Hardware check** | Run a project with Envelope shaper on CV input. Let envelope accumulate for 10 seconds. Change shaper to None, then back to Envelope — verify envelope restarts from zero (not from stale value). Change a different route's target — verify first route's envelope continues accumulating. Change track 3's shaper on route 5 — verify tracks 0-2, 4-7 on route 5 continue without audible interruption. Save and reload project — verify all shaper state resets (current behavior preserved). **Shaper-switching contract test:** On a single route/track, cycle through Activity → Envelope → ProgressiveDivider → Location, verifying each transition starts from that shaper's correct initial state (not leaked state from the previous shaper). |
| **Rollback risk** | Low — revert Phase 2 changes only. Phase 1 union remains. |
| **Exit criteria** | Stateful shapers accumulate for unchanged tracks. Reset only on active config change (per-track granularity) or project load. No audible snaps during normal operation. |

---

### Phase 3: CONDITIONAL — Bounded Active-State Pool (Only If Product Justifies a Cap)

> **This phase is conditionally entered.** Phase 1-2 is the complete deliverable. Phase 3 is extra compaction that only makes sense if the product team sets a hard cap on simultaneous stateful route-track pairs. Without such a cap, a pool sized to worst case (128 entries) saves nothing over Phase 1's union array — it adds indirection for zero benefit.

**Goal:** If the product can set a static cap on simultaneous stateful route-track pairs, replace the full 128-entry array with a smaller bounded pool + index.

**Why this is optional:** Without a cap below 128, a static pool sized to worst case is the current array with more indirection. It saves nothing. This phase only makes sense if analysis shows users rarely use >N stateful shapers simultaneously.

| Detail | Value |
|---|---|
| **Files touched** | `src/apps/sequencer/engine/RoutingEngine.h`, `src/apps/sequencer/engine/RoutingEngine.cpp`, possibly `Config.h` |
| **Changes** | Define `CONFIG_MAX_STATEFUL_ROUTE_TRACK_PAIRS` (e.g., 64). Replace `RouteState.shaperState[8]` array with a static pool of `TrackStateUnion[CONFIG_MAX_STATEFUL_ROUTE_TRACK_PAIRS]` plus an index map `(route, track) → poolIndex`. Routes/tracks without stateful shapers get `poolIndex = -1`. |
| **Exact behavior preserved** | All shapers behave identically to Phase 1-2 if within cap. **The UI/model must prevent configuring more stateful route-track pairs than the cap allows.** No silent fallback — if the cap is exceeded, the system asserts in debug or shows a user-visible error. In release, the cap is a hard compile-time bound that the UI enforces at configuration time. |
| **Expected RAM delta** | If cap = 64: ~1,800 B saved vs Phase 1 (64 fewer entries × ~28 B average). If cap = 32: ~2,700 B saved. Exact savings depend on union size from Phase 1. |
| **Verification command** | Same as Phase 1. |
| **Hardware check** | Test worst-case project with all 16 routes using stateful shapers on all 8 tracks. If cap = 64, 128 requested entries — verify the UI prevents configuration beyond the cap, or the system asserts/notify clearly. |
| **Rollback risk** | Low — revert to Phase 1 full array. |
| **Exit criteria** | Within cap: identical behavior to Phase 1-2. Cap enforced by UI or model validation — no silent degradation, no hidden performance mode. `.ccmram_bss` shows additional decrease. |

**This phase requires an explicit product decision before implementation begins.** Phase 1-2 already deliver the primary architectural benefit. Phase 3 is extra compaction for constrained deployments. Do not start Phase 3 without documented product confirmation of a realistic cap value.

---

### Phase 4: Hardware Verification

**Goal:** Prove no audible regressions across all 5 stateful shapers under continuous CV input. Manual checks, not oscilloscope-grade metrics.

| Detail | Value |
|---|---|
| **Files touched** | None — verification only. |
| **Changes** | None — verification only. |
| **Exact behavior preserved** | All behavior from Phase 1-2 (or 1-3) must remain identical to baseline. |
| **Expected RAM delta** | 0 |
| **Verification command** | Build `stm32/release`, flash to hardware. |
| **Hardware check** | Manual checks for each of the 5 stateful shapers with continuous CV input (e.g., 0-5V sine wave, 1 Hz): 1) **Location**: smooth ramp, no stair-stepping. 2) **Envelope**: attack/release curves smooth, no snaps on release. 3) **FrequencyFollower**: accumulation over 30s, no sudden drops. 4) **Activity**: responds to CV movement, decays smoothly. 5) **ProgressiveDivider**: threshold gates cleanly, no jitter. Additional: switch shaper None→Envelope→None→ProgressiveDivider on same route/track — check for state leakage. Run full project for 30+ minutes with mixed routing configs. Verify no hard faults. **Quantitative check:** Route a known CV input through each stateful shaper and compare output after N ticks against Phase 0 baseline to within 1 LSB of DAC resolution (~1.2 mV for 12-bit DAC over 5V range). |
| **Rollback risk** | N/A — verification only. If failed, roll back to Phase 1 or 0. |
| **Exit criteria** | All 5 stateful shapers produce smooth, audible output with no snaps, clicks, or DC drift. No state leakage across shaper switches. No hard faults in 30+ minute run. `.ccmram_bss` stable (no creep). |

---

## 6. State Preservation Requirements

The five stateful shapers must preserve accumulated-state behavior exactly. For each:

| Shaper | State Fields | Reset Semantics | Preserved By |
|---|---|---|---|
| **Location** | `location` | Created at `0.5f`. Accumulates via `location += (src - 0.5f) * kRate`. | Phase 1: union stores `location`. Phase 2: persists across ticks; resets on config change or project load. |
| **Envelope** | `envelope` | Created at `0.f`. Follows rectified input with attack/release coefficients. | Phase 1: union stores `envelope`. Phase 2: persists across ticks; resets on config change or project load. |
| **FrequencyFollower** | `freqAcc`, `freqSign`, `ffHold` | Created at `freqAcc=0`, `freqSign=false`, `ffHold=0`. Accumulates on zero-crossings, decays on idle. | Phase 1: union stores all three. Phase 2: persists across ticks; resets on config change or project load. |
| **Activity** | `activityPrev`, `activityLevel`, `activitySign`, `actHold` | Created at `activityPrev=0.5f`, `activityLevel=0.f`, `activitySign=false`, `actHold=0`. Decays and spikes on movement. | Phase 1: union stores all four. Phase 2: persists across ticks; resets on config change or project load. |
| **ProgressiveDivider** | `progCount`, `progThreshold`, `progSign`, `progOut`, `progOutSlewed`, `progHold` | Created at `progCount=0`, `progThreshold=1.f`, `progSign=false`, `progOut=0.f`, `progOutSlewed=0.f`, `progHold=0`. Threshold grows on each division. | Phase 1: union stores all six. Phase 2: persists across ticks; resets on config change or project load. |

**These five shapers are the load-bearing accumulated state.** Any refactor that loses, leaks, or corrupts their state produces audible artifacts.

### 6.1 Zero-Initialization Hazard

Three of the five stateful shapers have non-zero initial values that are **not** produced by zero-initialization:

| Shaper | Zero-init value | Correct initial value | Reset helper must set |
|---|---|---|---|
| Location | `location = 0.f` | `location = 0.5f` | `location = 0.5f` |
| Activity | `activityPrev = 0.f` | `activityPrev = 0.5f` | `activityPrev = 0.5f` |
| ProgressiveDivider | `progThreshold = 0.f` | `progThreshold = 1.f` | `progThreshold = 1.f` |

The `resetState(Shaper)` helper (Section 3.2) must set these values explicitly. Zero-initialization alone is insufficient.

---

## 7. Heap Allocation: Explicitly Rejected

For bare-metal STM32F4 (no MMU, no virtual memory), heap allocation of TrackState arrays is not proposed because:

1. **Fragmentation risk:** `new TrackState[8]` called on route configuration change would fragment CCMRAM over time. A hard fault on allocation failure is unrecoverable in a musical device.
2. **Static bounded layout is sufficient:** Phase 1 uses a union within a fixed array — deterministic, linker-analyzable, and fragmentation-free. Phase 3 adds a smaller bounded pool only if a realistic cap is justified.
3. **Determinism:** Static allocation guarantees deterministic memory layout. Heap makes linker analysis impossible.
4. **No benefit over union array:** An `etl::optional<TrackState>` or pointer-based approach adds indirection without reducing footprint compared to a well-designed per-entry union.

**Preferred approach:** Phase 1: union within the existing `RouteState[16]` array. Phase 3 (optional): smaller static pool + index map, only if product justifies a cap.

---

## 8. Runtime Branching Alone Does Not Save RAM

This document repeats what `ram-recovery-experiments.md` already states:

> "Runtime branching alone (skipping TrackState access in `updateSinks()`) saves zero RAM because the RouteState[16] array is statically allocated with full TrackState[8] in every entry. Savings require changing the storage layout — replacing the unconditional std::array<TrackState, 8> with a conditional or union-based structure that has smaller actual allocation. This is a struct layout change, not just a logic change."

The Phased Plan reflects this: Phase 1 is a per-entry union layout change that shrinks every slot. Phase 2 is a lifecycle change. Neither relies on runtime `if` statements for RAM savings. Phase 3 (optional) is the only phase that reduces entry count, and it requires a product-justified static cap.

---

## 9. Where Findings Go

- Phase 0 measurements → `ram-recovery-experiments.md` Experiment A baseline
- Phase 1 union compaction → `ram-recovery-experiments.md` Experiment A2 (primary RAM recovery)
- Phase 2 lifecycle → `ram-recovery-experiments.md` Experiment A1 (behavior refinement, no RAM change)
- Phase 3 bounded pool (if entered) → `ram-recovery-experiments.md` new experiment (only if product cap justified)
- Phase 4 hardware verification → closes architecture Direction 3 (routing as modulation)
- All verification failures → architecture research (Direction 3) for contract revision
