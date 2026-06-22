---
id: feat-tt2-mod-geode-ops
schema: plan
title: "feat: TT2 MO.* / G.* modulator & Geode control ops"
type: feat
status: active
date: 2026-06-14
branch: feat/tt2-v2-native
origin: docs/brainstorms/2026-06-14-tt2-mod-geode-ops-requirements.md
depth: standard
---

# feat: TT2 MO.* / G.* modulator & Geode control ops

Origin requirements: `docs/brainstorms/2026-06-14-tt2-mod-geode-ops-requirements.md`.

---

## Problem

`TrackMode::TeletypeV2` scripts cannot touch the modulator or Geode engines. The
legacy `TeletypeTrackEngine` carries duplicate envelope/LFO/Geode generators driven
by `E.*`/`LFO.*`/`G.*` ops reading `scene_state_t`; that duplication dies with the
bridge. Performer already has the real engines: 8 global `Modulator` slots
(`Project::modulator(n)`, driven by `ModulatorEngine`) and a standalone
`GeodeConfig`/`GeodeEngine`. Native TT2 should expose those existing engines to
scripts as thin BUS-style accessors — not redefine modulator control.

Separately, modulator params are addressed inconsistently: the model stores named
fields (`shape`/`rate`/`depth`/…, some reused per shape — Spring's `rate`=STRIKE,
`attack`=TENSION), while `ModulatorPage` has its own partial `Function` enum
(`Shape`/`Rate`/`Depth`/`Offset`/`Phase`). There is no single address table a script,
the engine, and the UI all agree on.

---

## Scope

In scope:
- A canonical **modulator param dictionary** (`Modulator::Param`, shape = address 0)
  shared by engine, UI, and ops — the "same address" for every field across all
  shapes.
- Native `MO.*` ops: bare `MO n` (read output), generic `MO.P slot addr [v]`
  (any field by address), named verbs `MO.SHAPE/RATE/DEPTH/MODE/OFF` + `MO.TRIG` as
  readable sugar over the dictionary, with single-letter aliases.
- Native `G.*` handlers over the existing (already-parsing) `G.*` tokens.
- `TT2Host` modulator + Geode access methods.
- Unit tests; the lexicon work `MO.*` requires.

### Deferred to Follow-Up Work
- **`ModulatorPage` realignment to the dictionary (U2)** — deferred to its own focused
  unit; needs a physical-slot → `Param` adapter + `ui-preview` (see U2). The shared
  address goal is already met in the model (the UI edits the same fields the dictionary
  addresses); this is a cosmetic-internal refactor, not an op prerequisite.
- Named verbs for the long-tail fields (attack/decay/sustain/release/smooth/phase/
  invert/rateDomain). They are reachable immediately via `MO.P` by address; only the
  readable verb sugar is deferred.

### Outside scope (origin non-goals)
- The modulator engine rework (`.tasks/modulator-enhancements.md`) — independent
  UI-driven track; these ops adapt to whatever it lands.
- A summing knob+script arbitration model (would need modulator routing targets).
- Legacy `E.*`/`LFO.*`/`G.*` bridge ops + `TeletypeTrackEngine` duplicate
  generators — untouched until bridge deletion.
- MIDI-out / non-modulator concerns.

---

## Key Technical Decisions

**D1 — Canonical modulator param dictionary (`Modulator::Param`).** One address table
in the model, shape first, authoritative for engine + UI + ops. The address is the
storage field; the shape reinterprets the label. The dictionary covers **all 15
persisted, page-editable `Modulator` fields** (the full serialized set — verified
against `Modulator.h`). Order (the value `v` in `MO.P slot addr v`):

| addr | field | Spring label | core verb? | value range |
|------|-------|--------------|------------|-------------|
| 0 | shape | shape | `MO.SHAPE` | Shape enum 0..9 |
| 1 | rate | STRIKE | `MO.RATE` | Free 1..1600 / Tempo 6..6144 |
| 2 | depth | depth | `MO.DEPTH` | 0..127 |
| 3 | mode | mode | `MO.MODE` | 0..2 |
| 4 | offset | offset | `MO.OFF` | offset range |
| 5 | attack | TENSION | — | attack range |
| 6 | decay | RING | — | decay range |
| 7 | sustain | sustain | — | sustain range |
| 8 | release | release | — | release range |
| 9 | amplitude | amplitude | — | 0..127 |
| 10 | smooth | CLANG | — | smooth range |
| 11 | phase | PICKUP | — | phase range |
| 12 | invert | invert | — | 0/1 |
| 13 | rateDomain | rateDomain | — | 0/1 (Free/Tempo) |
| 14 | gateSource | gateSource | — | `Routing::Source` ordinal |

`Modulator` gains `paramCount()`, `paramGet(addr) -> int`, `paramSet(addr, v)`,
`paramName(addr, shape) -> const char*` (per-shape label). All addresses are
reachable via `MO.P` from day one; named verbs cover the core (0..4). Existing named
setters/getters (`setShape`/`rate()`/`setGateSource`/…) stay; the dictionary
dispatches to them (so no storage change). `gateSource` is a `Routing::Source` enum —
`paramGet/Set` cast its ordinal (advanced; via `MO.P` only, no core verb). **Rule:
`MO.P` writes to `gateSource` are raw/unvalidated** — they bypass the UI's curated
list and self-modulator filter (`gateSourceAllowed`). This is an accepted advanced-use
escape hatch (BUS-spirit: expose the internal, trust the script author); an
out-of-range or self-referential ordinal is stored as-is and the engine tolerates it
(no crash; a self-source gate simply reads the slot's own output). No clamping is
added.

**D2 — Host-mediated engine access, op layer stays off `Engine.h`.** `TT2Host` gains
modulator + Geode methods, mirroring `hostBusCv`/`hostTrackPattern`. For config it
exposes the model objects by reference — `hostModulator(idx) -> Modulator&` (whose
`paramGet/paramSet` is the dictionary) and `hostGeodeConfig() -> GeodeConfig&`. These
are **model** headers, not `Engine.h`. Engine-resident values/actions get explicit
methods: `hostModulatorOutput` (→ `ModulatorEngine::currentValue`, public via
`Engine::modulatorEngine()`), `hostModulatorTrigger` (→ `ModulatorEngine::triggerManual`,
which already exists), `hostGeodeMix` (→ `GeodeEngine::mixLevel`,
for `G.VAL`), `hostGeodeTriggerVoice`, `hostGeodeTriggerAll`, and `hostGeodeRun()` /
`hostGeodeSetRun(int16_t macro)` — the latter encapsulate the M2 run mapping
(0..16383 ↔ `modulator(1).offset`), keeping the M2 mechanics out of the op handler.
`hostGeodeTriggerVoice`/`hostGeodeTriggerAll` **mirror the live gate-fire sequence**
(`Engine.cpp:236`), not just `triggerVoice`: `triggerVoice(v, divs, reps)` +
`setVoicePhase(v, 0)` + `markVoiceTriggered(v)` + `triggerImmediate(v, timeNorm,
intone, run, mode)` (run from M2, time/intone/mode from `GeodeConfig`). Without the
full sequence a script-triggered voice won't fire immediately (`update()` alone resets
phase). All four read the live state `TT2TrackEngine` already holds.
**`Engine::_geodeEngine` is private** with no accessor (only `modulatorEngine()` is
public), so this work adds a public `Engine::geodeEngine()` accessor (mirroring
`modulatorEngine()`); the Geode host methods route through it.

**D3 — `MO.*` is a new op namespace; `G.*` reuses existing tokens.** The `G.*` op
structs already exist in `teletype/src/ops/hardware.c` and parse today; native TT2
only registers `tt2NativeOpTable` handlers for the existing `E_OP_G_*` indices. The
modulator namespace is `MO` (the only existing `MO…` token is `MOD` = modulo `a % b`,
`params=2`; one name binds one op with one fixed arity, so modulator ops cannot live
under `MOD`). `MO` and `MO.*` are free and need new op structs (D5).

**D4 — Op surface (native units, last-writer-wins, verbs + generic + aliases).**
Writes set the stored field directly via the dictionary; the knob can re-set it;
whoever moved last holds (origin D3 — modulator fields are not routing targets, so no
base+delta machinery). Values are native units, matching the `MI.*` precedent (origin
D4). Each named field op has a single-letter alias (via `MAKE_ALIAS_OP`); in the
native table both op-enum indices point at the same handler.

| canonical | alias | arity | behavior |
|-----------|-------|-------|----------|
| `MO n`            | —      | 1 (get) | read slot output, native 0..127 |
| `MO.P n addr [v]` | —      | 2, get/set | get/set field `addr` (dictionary, D1) |
| `MO.SHAPE n [v]`  | `MO.S` | 1, get/set | addr 0 |
| `MO.RATE n [v]`   | `MO.R` | 1, get/set | addr 1 |
| `MO.DEPTH n [v]`  | `MO.D` | 1, get/set | addr 2 |
| `MO.MODE n [v]`   | `MO.M` | 1, get/set | addr 3 |
| `MO.OFF n [v]`    | `MO.O` | 1, get/set | addr 4 |
| `MO.TRIG n`       | `MO.T` | 1 (no return) | trigger the slot (action, not an address) |

`G.*` (Geode; existing tokens that already parse). Geode claims all 8 modulator slots
with fixed roles: **M1 = clock, M2 = run, M3–M8 = the 6 voices**. The documented G.*
domains are in `teletype/tl-manual.md`; native mapping resolves each against the live
engine (`GeodeConfig` fields + `GeodeEngine` + the M1/M2/voice modulator wiring):

| op (alias) | manual domain | native mapping |
|------------|---------------|----------------|
| `G.TIME` (`G.T`) | 0..16383 | `GeodeConfig::time` |
| `G.TONE` (`G.I`) | 0..16383, 8192=noon | `GeodeConfig::intone` |
| `G.RAMP` (`G.RA`) | 0..16383 | `GeodeConfig::ramp` |
| `G.CURV` (`G.C`) | 0..16383, 8192=linear | `GeodeConfig::curve` |
| `G.MODE` (`G.M`) | 0/1/2 | `GeodeConfig::mode` |
| `G.RUN` (`G.N`) | 0..16383, 8192=noon | **M2 run center** — set `modulator(1).offset` (0..16383 → offset −64..+63, 8192→0); read back from M2. **Quantized to offset's 128 steps** — readback is the nearest representable macro, not bit-exact (run is a coarse macro; no separate stored state, since the live engine reads run from M2) |
| `G.TUNE` | v=0 all, 1–6 | `GeodeConfig::setTune` (v=0 → all) |
| `G.S` | t i n | compound: set `time`, `intone`, run macro in one line (no trigger) |
| `G.V` | v=0 all, divs 1–64, reps −1..255 | full host gate-fire sequence (`hostGeodeTriggerVoice`/`All`, D2) — `triggerVoice`+`setVoicePhase(0)`+`markVoiceTriggered`+`triggerImmediate`; v=0 → all. divs/reps are **trigger args only** (not persisted to `GeodeConfig`) |

There is **no `G.*` op to read/write per-voice `GeodeConfig::divs`/`repeats`** — the
manual exposes none, and `G.V` passes divs/reps as trigger parameters only. Per-voice
rhythm config stays UI-edited. (`G.TUNE` is the one per-voice config op.)
| `G.VAL` (`G.L`) | read raw 0–16383 | `GeodeEngine::mixLevel()` |
| `G.O` (offset) | raw 0–16383 | **unsupported** — no native field (legacy `_geodeParams.offset`); use `MO.OFF` on voice slots |
| `G.BAR` (`G.B`) | 1–128 | **unsupported** — no native field; live engine derives timing from transport (M1 phase) |
| `G.R` (route to CV) | cv 1–4, v 0–6 | **unsupported** — voices are fixed to M3–M8; route via the modulator CV-out assignment |

Pinned semantics:
- **`G.RUN` is a fixed run macro on M2**, not unsupported (corrects the prior draft).
  `GeodeConfig` has no run field; live run = M2 output (`Engine.cpp`:
  `run = (currentValue(1)−64)/64`). `G.RUN x` sets M2's run center via its `offset`
  (Geode-enable already parks M2 at depth 0, so run is fixed by default; M2 depth, if
  added, sweeps run around that center). `G.N` is the documented alias.
- **Voice index 0 = all voices** (manual + legacy `setGeodeTune`/trigger): per-voice
  ops and `G.V` treat `n=0` → all, `n=1..6` → voice `n-1`, `n>6` → no-op. Index 0 is
  **not** out-of-range.
- `G.O`/`G.BAR`/`G.R` stay nullptr → the evaluator returns `TT2EvalError::UnsupportedOp`
  (an explicit error, **not** a silent no-op; `TT2Evaluator.h:112`). This is intended:
  the op has no live-engine home, so the script author gets a clear "unsupported" rather
  than a silently-ignored line, and uses the `MO.*`/routing equivalent. Each is
  documented with a one-line comment pointing at the live-engine reason.

**D5 — `MO.*` lexicon follows the teletype op-add procedure.** New `tele_op_t`
structs (canonical + `MAKE_ALIAS_OP` aliases) in a new `teletype/src/ops/modulator.c`;
register in `tele_ops[]` (`teletype/src/ops/op.c`); regenerate
`teletype/src/ops/op_enum.h` via `python3 utils/op_enums.py`; add tokens to
`teletype/src/match_token.rl` and **regenerate `teletype/src/match_token.c` with
ragel**; wire the new `.c` into `teletype/module/config.mk`,
`teletype/tests/Makefile`, `teletype/simulator/Makefile`, and
`src/apps/sequencer/CMakeLists.txt`. Ragel is a documented dev dependency
(`teletype/README.md`); CMake compiles the committed `match_token.c`, so the
regenerated `.c` must be committed.

---

## High-Level Technical Design

Directional guidance for review, not implementation specification.

```
Param dictionary (D1) is the spine — one table, three consumers:

  Modulator::Param   ──used by──▶  MO.P / MO.SHAPE / ... (ops)
        │                          ModulatorPage (UI: order + labels)
        └─ paramGet(addr) / paramSet(addr, v) / paramName(addr, shape)

script: "MO.P 3 2 100"   (addr 2 = depth)   ≡  "MO.DEPTH 3 100"  ≡  "MO.D 3 100"
   parse() ──▶ lowerCommand() ──▶ tt2NativeOpTable[...]
   handler: h->hostModulator(2).paramSet(2, 100);   // last-writer-wins
   ModulatorPage shows depth=100 live; knob can re-set it.

script: "MO 3"        ──▶ push h->hostModulatorOutput(2)   // 0..127
script: "G.TIME 4000" ──▶ h->hostGeodeConfig().setTime(4000);
```

---

## Implementation Units

### U1. Modulator param dictionary (model)

**Goal:** Add the canonical `Modulator::Param` address table + dictionary accessors,
the shared spine for ops and UI.

**Requirements:** D1.

**Dependencies:** none.

**Files:**
- `src/apps/sequencer/model/Modulator.h` (modify — add `Param` enum, `paramCount()`,
  `paramGet`, `paramSet`, `paramName(addr, shape)`)
- `src/tests/unit/sequencer/TestModulatorParam.cpp` (create)
- `src/tests/unit/sequencer/CMakeLists.txt` (modify — register)

**Approach:** `enum class Param : uint8_t { Shape=0, Rate, Depth, Mode, Offset,
Attack, Decay, Sustain, Release, Amplitude, Smooth, Phase, Invert, RateDomain,
GateSource, Last }` — all 15 persisted fields. `paramGet`/`paramSet` switch over
`Param` and call the existing named getters/setters (no new storage — the dictionary
is a façade over current fields, preserving each field's own clamp).
`GateSource` casts to/from the `Routing::Source` ordinal; `Invert`/`RateDomain` are
0/1. `paramName(addr, shape)` returns the per-shape label (default label, Spring
overrides for Rate/Attack/Decay/Smooth/Phase per the existing Spring reuse comment).
Out-of-range addr is a no-op / returns 0.

**Patterns to follow:** `Modulator.h` existing getters/setters + the Spring-reuse
comment block; `ModulatorPage`'s shape-specific labelling for the per-shape names.

**Execution note:** test-first — the dictionary is pure model logic, ideal for
red/green.

**Test scenarios:**
- `paramSet(Depth, 100)` then `paramGet(Depth)` == 100; matches `depth()`
- each core addr (Shape/Rate/Depth/Mode/Offset) set/get roundtrips and agrees with the
  named getter
- long-tail addrs (Attack, Amplitude, GateSource) set/get roundtrip via the dictionary;
  `paramGet(Amplitude)` matches `amplitude()`, `paramGet(GateSource)` matches the
  `gateSource()` ordinal
- `paramSet` past a field's range clamps to that field's own clamp (e.g. Depth>127)
- `paramName(Rate, Sine)` vs `paramName(Rate, Spring)` differ ("RATE" vs "STRIKE")
- out-of-range addr (`Last`, 99) get returns 0, set is a no-op (no OOB)

**Verification:** dictionary roundtrips for every address; values agree with the named
accessors; builds clean.

---

### U2. ModulatorPage references the shared dictionary (UI) — DEFERRED

**Status: deferred to its own focused unit** (needs a design pass + `ui-preview`,
not implementation-ready here).

**Why deferred:** `ModulatorPage::Function` is a **5-value** enum of physical control
slots (`Shape`/`Rate`/`Depth`/`Offset`/`Phase`), and the Geode/JustF special-case
rendering depends on those exact controls (`ModulatorPage.cpp` ~621). "Replace
`Function` with `Modulator::Param` (15 entries) + dictionary order = on-screen order"
is too loose: the dictionary has 15 addresses but the page surfaces 5 encoder targets
per page, and reordering them is a visual/UX change. A correct realignment needs an
**adapter** mapping the page's physical control slots onto dictionary addresses
(preserving the current 5-slot layout), designed and validated with `ui-preview`.

**Important — the shared-address goal is already met without this unit.** The
canonical address table lives in the model (`Modulator::Param`, U1) and the UI already
edits the **same underlying `Modulator` fields** the dictionary addresses. So engine,
ops, and UI already resolve the same address to the same storage field. U2 only makes
the page *reference the dictionary internally* (single source for order/labels) — a
cosmetic-internal refactor, not a prerequisite for the ops.

**Next step when picked up:** specify the physical-slot → `Param` adapter, render with
`ui-preview`, confirm order/labels with the user, then wire `ModulatorPage` through
`paramGet`/`paramSet`/`paramName`. No op-side dependency.

---

### U3. TT2Host modulator + Geode access methods

**Goal:** Extend `TT2Host` with modulator/Geode accessors implemented on
`TT2TrackEngine`.

**Requirements:** D2.

**Dependencies:** U1 (host returns `Modulator&` whose `paramGet/Set` is the dictionary).

**Files:**
- `src/apps/sequencer/engine/Engine.h` (modify — add public `geodeEngine()` accessor,
  mirroring `modulatorEngine()`; `_geodeEngine` is currently private with no accessor)
- `src/apps/sequencer/engine/TT2Host.h` (modify)
- `src/apps/sequencer/engine/TT2TrackEngine.h` (modify)
- `src/apps/sequencer/engine/TT2TrackEngine.cpp` (modify)

**Approach:** Add a public `Engine::geodeEngine()` (const + non-const, like
`modulatorEngine()` at `Engine.h:207`). Then add to `TT2Host`:
`hostModulator(uint8_t idx) -> Modulator&`, `hostModulatorOutput(uint8_t idx) -> int16_t`
(0..127), `hostModulatorTrigger(uint8_t idx)`, `hostGeodeConfig() -> GeodeConfig&`,
`hostGeodeMix() -> int16_t` (raw 0..16383, for `G.VAL`),
`hostGeodeTriggerVoice(uint8_t v, int16_t divs, int16_t repeats)`,
`hostGeodeTriggerAll(int16_t divs, int16_t repeats)`,
`hostGeodeRun() -> int16_t` / `hostGeodeSetRun(int16_t macro)` (the M2 run macro).
Implement against
`Project::modulator(n)`, `Project::geode()`, `_engine.modulatorEngine()` (public) and
`_engine.geodeEngine()` (new). `hostModulatorOutput` → `ModulatorEngine::currentValue(idx)`;
`hostModulatorTrigger` → `ModulatorEngine::triggerManual(idx)`. `TT2Host.h` includes
`model/Modulator.h` + `model/GeodeConfig.h`. Reuse `ScopedHost`. The op layer (and
`TT2Host.h`) still does not include `Engine.h` — only `TT2TrackEngine.cpp` does.

**Patterns to follow:** `hostBusCv`/`hostSetBusCv`/`hostTrackPattern`;
`Engine::modulatorEngine()` (`Engine.h:207`) for the accessor shape; `Engine.cpp:272`
Geode→modulator wiring.

**Execution note:** `MO.TRIG` is resolved — `ModulatorEngine::triggerManual(int)`
already exists (`ModulatorEngine.h:121`); `hostModulatorTrigger` calls it. No latch
needed.

**Test scenarios:** `Test expectation: none -- host plumbing, covered by U4/U6.`

**Verification:** compiles with overrides + the new `Engine::geodeEngine()`; STM32
release builds; `TT2Host.h` / op layer still have no `Engine.h` include.

---

### U4. Native G.* handlers

**Goal:** Make the existing `G.*` tokens functional in native TT2.

**Requirements:** D3; success criterion "configure Geode globals + per-voice tune +
trigger voices (passing divs/reps), voices appear in M3–M8".

**Dependencies:** U3.

**Files:**
- `src/apps/sequencer/engine/TeletypeNativeOps.cpp` (modify — `opG*` handlers + table
  registration)
- `src/tests/unit/sequencer/TestTeletypeV2Geode.cpp` (create)
- `src/tests/unit/sequencer/CMakeLists.txt` (modify)

**Approach:** Register handlers per the D4 G.* mapping table. `G.TIME`/`G.TONE`/
`G.RAMP`/`G.CURV`/`G.MODE` → `GeodeConfig` setters/getters; `G.TUNE` → `GeodeConfig`
per-voice tune (voice 0 = all); `G.S` → compound time/intone/run; `G.V` →
`hostGeodeTriggerVoice`/`hostGeodeTriggerAll` (voice 0 = all, divs/reps are trigger
args only); `G.VAL` → `hostGeodeMix`. **`G.RUN` (alias
`G.N`) → `hostGeodeSetRun`/`hostGeodeRun`** (fixed run macro on M2, 0..16383/8192-noon,
D4) — supported, not nullptr. **`G.O`/`G.BAR`/`G.R` stay nullptr (unsupported)** — no
native field; each gets a one-line comment with the live-engine reason. Voice index
semantics (D4): `0` = all, `1..6` = voice `n-1`, `>6` = no-op (0 is not OOB).
Leftmost-first pop order. Other unmapped `E_OP_G_*` tokens stay nullptr, listed in a
comment.

**Patterns to follow:** `opBus`/`opRt`; the leftmost-first pop convention;
`TestTeletypeV2Input`/`TestTeletypeV2Midi` (stub `TT2Host`).

**Execution note:** test-first against a stub host.

**Test scenarios:**
- `G.TIME 4000` then `G.TIME` reads back 4000
- `G.TONE`, `G.RAMP`, `G.CURV`, `G.MODE` set/get roundtrip
- `G.RUN`/`G.N` (alias) set/get the run macro via the stub host (handler dispatch);
  the real M2-offset mapping is quantized to 128 steps (D4) and not unit-tested here
- `G.S t i n` sets time/intone/run in one line
- `G.VAL`/`G.L` returns `hostGeodeMix()` (raw 0–16383)
- `G.O`, `G.BAR`, `G.R` each evaluate to `TT2EvalError::UnsupportedOp` (explicit error,
  asserted — not a silent no-op)
- `G.TUNE 1 3 2` sets voice-1 tune num/den; `G.TUNE 0 3 2` sets it on **all** voices
  (no divs/repeats op exists — those are `G.V` trigger args only)
- `G.V 1 4 2` fires voice 0 **immediately** via the full sequence (stub asserts
  `triggerVoice` + `setVoicePhase(0)` + `markVoiceTriggered` + `triggerImmediate` all
  called, not just `triggerVoice`); `G.V 0 4 2` fires all voices
- voice index > 6 is a no-op (no OOB); index 0 is **all**, not rejected
- value past native range clamps (e.g. time > 16383)

**Verification:** supported `G.*` handlers non-nullptr; `G.O`/`G.BAR`/`G.R` remain
nullptr and assert `UnsupportedOp`; tests pass; on-device drives Geode voices into
M3–M8 (hardware audition).

---

### U5. MO.* op lexicon

**Goal:** Add the `MO.*` tokens (bare `MO`, `MO.P`, named verbs + aliases).

**Requirements:** D3, D4.

**Dependencies:** none (parallel; U6 depends on this + U3).

**Files:**
- `teletype/src/ops/modulator.c` (create — `MO`, `MO.P`, `MO.SHAPE/RATE/DEPTH/MODE/OFF/
  TRIG` canonical + `MO.S/R/D/M/O/T` aliases)
- `teletype/src/ops/op.c` (modify — `tele_ops[]`)
- `teletype/src/ops/op_enum.h` (regenerate via `python3 utils/op_enums.py`)
- `teletype/src/match_token.rl` (modify) + `teletype/src/match_token.c` (regenerate, ragel)
- `teletype/module/config.mk`, `teletype/tests/Makefile`,
  `teletype/simulator/Makefile`, `src/apps/sequencer/CMakeLists.txt` (modify)

**Approach:** Per D5. Arity: `MO` = `MAKE_GET_OP(MO, …, 1, true)`; `MO.P` =
`MAKE_GET_SET_OP(MO.P, …, 2, true)` (slot + addr; set on extra value); named field ops
= `MAKE_GET_SET_OP(…, 1, true)`; aliases = `MAKE_ALIAS_OP` pointing at the canonical
get/set. C op bodies are inert stubs — parser metadata only; native dispatches via
`tt2NativeOpTable`. **Dependency direction:** add the `&op_MO…` entries to `tele_ops[]`
in `op.c` **first**, then run `op_enums.py` (it regexes `&op_…` out of `tele_ops[]`,
strips `op_`, and writes `op_enum.h` in array order). Then add the `match_token.rl`
tokens and run ragel. `match_token.c` is gitignored (regenerated per-checkout, like the
existing `G.*` tokens); commit `op_enum.h` + `.rl`, not `.c`.

**Patterns to follow:** `op_G_*` defs in `hardware.c`; `MAKE_ALIAS_OP` in
`teletype/src/ops/op.h`; `teletype/README.md` "Adding a new OP".

**Test scenarios:**
- `MO.P 3 2 100` parses (`E_OK`) and lowers to a 3-arg op
- `MO.DEPTH 3 100` and alias `MO.D 3 100` parse to their own enum indices
- `MO 3` parses as a 1-arg op
- longest-match: `MO.RATE` vs `MO.R`, `MO.MODE` vs `MO` resolve correctly
- teletype op-table self-consistency test passes with all new ops + aliases

**Verification:** all `MO.*` lines parse + lower; consistency test passes; STM32 links
`modulator.c`.

---

### U6. Native MO.* handlers

**Goal:** Implement `MO.*` behavior against the modulator slots via the dictionary +
host.

**Requirements:** D1, D2, D4; success criterion "read a slot's output and get/set its
fields in native units, reflected live on `ModulatorPage`".

**Dependencies:** U3 (host), U5 (op enum entries).

**Files:**
- `src/apps/sequencer/engine/TeletypeNativeOps.cpp` (modify — `opMo*` handlers + table
  registration for canonical and alias `E_OP_MO_*` indices, both → same handler)
- `src/tests/unit/sequencer/TestTeletypeV2Modulator.cpp` (create)
- `src/tests/unit/sequencer/CMakeLists.txt` (modify)

**Approach:** `MO.P` pops slot + addr (+ value on set) → `hostModulator(idx).paramGet/
paramSet(addr, v)`. Named verbs are the same with a fixed addr (e.g. `opMoDepth` calls
`paramSet(Param::Depth, …)`). `MO` → `hostModulatorOutput`. `MO.TRIG` →
`hostModulatorTrigger`. Slot 1-based→0-based with the `opBus`-style guard (1..8).
Leftmost-first pop; get/set by `isSetPosition && remaining args`. Register canonical +
alias indices to one handler.

**Patterns to follow:** `opBus` (index guard + get/set-by-remaining-args); `MI.*` for
native 0..127 returns; leftmost-first pop.

**Execution note:** test-first per op.

**Test scenarios:**
- `MO.P 3 2 100` then `MO.P 3 2` reads 100; equals `MO.DEPTH 3` read
- `MO.DEPTH 3 100` and alias `MO.D 3 100` behave identically and match `MO.P 3 2 100`
- `MO.SHAPE 1 5` sets shape ordinal 5; `MO.SHAPE 1` reads 5
- `MO.RATE`/`MO.MODE`/`MO.OFF` set/get roundtrip (one via alias)
- `MO.P 3 5 …` (Attack, a long-tail addr) set/get roundtrips — full coverage via MO.P
- `MO 3` returns stub `hostModulatorOutput(2)` (0..127, get-only)
- depth>127 clamps; shape/mode clamp to enum range; bad addr is a no-op
- slot index 0 and 9 rejected/clamped without OOB
- `MO.TRIG 2` (and `MO.T 2`) invoke `hostModulatorTrigger(1)`

**Verification:** `MO.*` dispatch (canonical + alias) non-nullptr; tests pass;
on-device `MO.P`/`MO.DEPTH`/`MO.SHAPE` changes show live on `ModulatorPage`.

---

### U7. Op-coverage bookkeeping + docs

**Goal:** Reconcile the coverage test + roadmap docs with the new ops.

**Requirements:** success criterion "the **supported subset** of `MO.*`/`G.*` no longer
UnsupportedOp; legacy bridge ops untouched". `G.O`/`G.BAR`/`G.R` intentionally remain
`UnsupportedOp` (D4) and are not claimed as supported.

**Dependencies:** U4, U6.

**Files:**
- `src/tests/unit/sequencer/TestTeletypeV2NativeOps.cpp` (modify if it lists `MO.*`/
  `G.*` as unsupported, or carries drifting size asserts)
- `src/tests/unit/sequencer/TestTeletypeV2Parity.cpp` (verify `MO.*`/`G.*` stay
  excluded as engine ops — no parity addition expected)
- `docs/teletype_v2.md` (modify — mark modulator/Geode native ops + the param
  dictionary done; legacy E/LFO/G still bridge-only)
- `.tasks/STATUS.md` + `.tasks/teletype-v2-dialect.md` (modify)

**Approach:** Move the **supported** `MO.*`/`G.*` ops (canonical + aliases) to supported
if the coverage test enumerates them; keep `G.O`/`G.BAR`/`G.R` listed as
intentionally-unsupported (they return `UnsupportedOp`). Confirm the parity harness's
deterministic set excludes them.
Bump TT2 size-drift asserts only if a struct actually grew (these ops add no
`TT2Runtime` state — none expected).

**Test scenarios:** `Test expectation: none -- bookkeeping + docs; existing suites
stay green.`

**Verification:** full `TestTeletypeV2*` suite + STM32 release green; docs updated.

---

## Risks

- **UI realignment scope (U2).** Promoting the dictionary to the on-screen param order
  is an OLED change — `ui-preview` gated and user-confirmed before shipping. It can
  land independently of the ops, so the op units are not blocked on UI sign-off.
- **Ragel regeneration (U5).** `MO.*` needs ragel installed + `match_token.c`
  regenerated and committed. Mitigation: documented; the legacy `G.*`/`E.*`/`LFO.*`
  additions already exercised this path.
- **Engine accessor (U3).** Adds a public `Engine::geodeEngine()` (mirrors
  `modulatorEngine()`); `_geodeEngine` was private with no accessor. `MO.TRIG` is
  resolved via the existing `ModulatorEngine::triggerManual`.
- **Generated-file drift.** `op_enum.h` is generated by `utils/op_enums.py`; U5
  mandates the script, not manual edits.

---

## Verification (overall)

- The param dictionary roundtrips for every address and agrees with the named
  accessors; `MO.P` and the named verbs are equivalent.
- Supported `MO.*` (incl. aliases) and supported `G.*` parse, lower, and dispatch to
  native handlers; the unsupported Geode ops (`G.O`/`G.BAR`/`G.R`) parse and lower,
  then return `UnsupportedOp` at eval.
- `ModulatorPage` addresses params through the same dictionary (ui-preview validated).
- Full `TestTeletypeV2*` + `TestModulatorParam` suites + STM32 release green.
- On-device: `MO.*` edits show live on `ModulatorPage`; `G.*` drives Geode voices into
  M3–M8 (hardware audition — the user's check).
