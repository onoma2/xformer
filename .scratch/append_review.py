#!/usr/bin/env python3
import sys

appendix = r'''

---

## GLM5.1 Review and Plan

### Adversarial Review of Part 1 (The Status Quo)

The diagnosis of the four friction domains is directionally correct but contains one fatal factual error and several important omissions.

**"Real-time string parsing" is false.** This claim underpins the entire Part 2 proposal and it is wrong. The C VM does not parse strings at runtime. The Ragel scanner (`scanner.rl`, `match_token.rl`) compiles text into `tele_command_t` structs — arrays of `(tag, value)` pairs — at edit time. These are stored in `scene_script_t.c[]` and executed via indexed function-pointer dispatch from `tele_ops[]`. The execution path (`process_command` in `teletype.c:257`) walks pre-parsed tag/value arrays right-to-left with a stack machine. Zero string operations, zero heap allocation, zero parsing on the hot path. The `tele_command_t` format IS a compiled intermediate representation. It is functionally identical to the "bytecode" Part 2 proposes inventing.

The actual performance concerns are narrower than claimed:
- `tele_tick()` runs a delay-queue scan (8 entries at DELAY_SIZE=8) every `update()` — O(8), not alarming.
- `runMetro()` can fire multiple script executions in a single `update()` via catch-up loop — bounded by metro period vs. frame time.
- `std::sqrt()` per active slew channel per frame — real but minor.
- `updateAdc()` every frame — needs measurement, not assumption.

The global-pointer hack (`g_activeEngine`, `ScopedEngine` RAII) is correctly identified and is a real problem for multi-track safety. But the claim that "parsing and executing string-based scripts inside `update()`" causes "CPU starvation" is fabricated. The string-to-command parse happens exactly once: when the user finishes editing a script in the UI.

The bridge bloat is correctly measured (~40 `tele_*` callbacks, 678 lines) but the count is inflated from "100+" in the original claim. The stub count is also overstated — II, GRID, and ARC are stubs, but that is because those hardware devices do not exist on Performer, not because of architectural failure.

### Adversarial Review of Part 2 (The Native Refactor)

**The bytecode compiler proposal solves a problem that does not exist.** The system already has a two-stage pipeline: text -> `tele_command_t` (at edit time) -> function-pointer dispatch (at run time). A new bytecode format would be a second intermediate representation doing the same job. The proposed `[OP_PUSH_LITERAL, 500, OP_SET_CV, 1]` is semantically identical to the existing `{tag: OP, value: E_OP_CV}, {tag: NUMBER, value: 1}, {tag: NUMBER, value: 500}` stored in `tele_command_t` — just with different enum names.

The actual execution overhead is: one array-indexed lookup into `tele_ops[]` plus one indirect function call per op. This is already a jump table. A custom bytecode ISA would replace it with... a switch statement indexing into the same C++ methods. No measurable speedup.

**"Discard `scene_state_t`" is the right direction with the wrong scope estimate.** Kimi caught this (missing delay queues, turtle, grid, MIDI buffers, RNG states, calibration). But the deeper problem is that the existing task work already has a better answer: the `teletype-runtime-architecture` task (Phases 0-4, all hardware-verified) already established that `scene_state_t` should remain runtime truth, with clips as explicit load/capture payloads. The clip architecture (`PatternSlot` -> `TeletypeClip`, two persistence contracts) already solved the "Pattern Problem" without discarding the struct. The v2 document proposes demolishing a building whose load-bearing walls have already been reinforced.

**`std::array<std::string, 8>` is indeed worse** — Kimi caught this and is correct. On an STM32 with ~12 KB headroom, heap-allocated strings for script storage would be a regression from the fixed-size `tele_command_t` arrays already in use.

**The DSP remapping is under-specified but directionally sound.** Envelopes and LFOs are already computed natively in `TeletypeTrackEngine::updateEnvelopes()` and `updateLfos()` — they do not run through the C VM at all. The C VM only sets parameters (rate, depth, attack, decay) via `tele_env_*` / `tele_lfo_*` callbacks, which then configure the native C++ implementations. Geode is the exception — it has substantial state in the engine (`TeletypeTrackEngine.h` lines 244-268) and its own update loop. Extracting it into a standalone class is reasonable but is a local refactor, not an architectural revolution.

**No mention of the `teletype-cv-source-combiner` task** — already designed (V0 priority law, 7-phase plan, paused). This task directly addresses the I/O ownership problem (Section 1.1 of the v2 doc) and has a concrete implementation plan. The v2 document reinvents this from scratch.

### Adversarial Review of the Kimi Plan

**Phase 1 ("Compiler Shadow") is unnecessary.** The existing `tele_command_t` IS the compiled form. Shadow-testing bytecode against the C VM means building a second interpreter that produces the same outputs as the first interpreter running on the same pre-parsed commands. The 20 "hottest ops" (`CV`, `TR`, `DEL`, `M`, `IF`, `ADD`, `P`) already execute as direct function-pointer calls. The proposed bytecode would call the same C++ methods through a different dispatch mechanism.

**"Profile current CPU usage with real scripts on STM32 before designing the bytecode ISA"** is good advice. But if the profiling shows the function-pointer dispatch is fast enough — which the zero-allocation, no-parsing execution model suggests it will — the entire bytecode effort collapses.

**Phase 2 ("State Surgical Extraction") is the right idea applied to the wrong target.** The `teletype-runtime-architecture` task already extracted clip ownership (Phases 0-4, hardware-verified, 1,136 B saved). The next surgical extraction should target the bridge callbacks, not `scene_state_t` internals.

**The acceptance criteria are sound** — bytecode <= 50% of C VM time, container gate, zero regression, golden tests. But they are criteria for a project that should not exist.

### What Actually Needs Doing

The four real problems, ranked by impact and feasibility:

1. **Bridge deletion.** Replace the 40 `tele_*` C callbacks + `g_activeEngine` global with direct C++ method calls. The C VM ops already call function pointers; those function pointers currently go through `TeletypeBridge.cpp` -> `g_activeEngine->method()`. They could instead receive a `TeletypeTrackEngine*` context parameter through `exec_state_t` or `scene_state_t` user-data. This is a mechanical refactor, not a rewrite. It eliminates multi-track safety concerns and the RAII scope overhead.

2. **CV source combiner.** Resume the paused `teletype-cv-source-combiner` task. The V0 priority law (ENV > GEODE > LFO > CV held value) and 7-phase plan already exist. This fixes the I/O ownership problem without touching the VM.

3. **Geode extraction.** Extract `updateGeode()` and its ~30 lines of state into a standalone `GeodeEngine` class. Local refactor, no architectural change. Reduces `TeletypeTrackEngine` coupling and makes Geode testable in isolation.

4. **`scene_state_t` surgical reduction** only if profiling proves the RAM matters. Current measurements: `TeletypeTrack` = 7,104 B, below the `NoteTrack` gate (9,544 B). The container already pays for the max arm regardless. Any savings inside `TeletypeTrack` that do not reduce it below the next-smallest track type are free RAM but do not change the top-level container. The `resource-optimization` task already recovered 8,472 B through safe wins and correctly concluded "safe wins exhausted."

### Plan

**Phase 0: Measure (1-2 sessions)**
Profile `TeletypeTrackEngine::update()` on STM32 with representative scripts (metro script firing every 100ms, delay-heavy script, Geode-active script). Measure: total CPU% per frame, worst-case frame time, `tele_tick()` cost, `process_command()` cost. If the VM execution itself is under 5% of frame budget, the bytecode project is definitively dead and we move to the real problems.

**Phase 1: Bridge Context Pass-through (2-3 weeks)**
- Add a `void *user_data` field to `scene_state_t` (or use an existing unused field).
- On `TeletypeTrackEngine::reset()`, set `user_data = this`.
- Replace `g_activeEngine` lookups in bridge callbacks with a `TeletypeTrackEngine *engine = static_cast<TeletypeTrackEngine *>(ss->user_data)` pattern.
- Remove `ScopedEngine` RAII, remove `g_activeEngine` global.
- `TeletypeBridge.cpp` shrinks from 678 lines to thin forwarding functions that extract the engine from the passed `scene_state_t`.
- Multi-track safety achieved without a rewrite.

**Phase 2: Bridge Deletion (3-4 weeks)**
- After context pass-through, the bridge functions are trivial wrappers. Replace them by having the C ops call engine methods directly through the context pointer, or by making the ops themselves thin inline C++ wrappers.
- Alternatively: keep the `tele_*` C linkage functions but make them one-line extraction + call, eliminating the bridge file as a concept.
- Test: run all existing scripts, verify identical CV/gate output on STM32.

**Phase 3: CV Source Combiner (3-4 weeks)**
- Resume the paused `teletype-cv-source-combiner` task from Phase 1 of its existing 7-phase plan.
- Implement source cache -> split raw render -> ENV ownership -> LFO ownership -> Geode ownership -> remove old paths.
- Fixes the double-indirection I/O problem. User sees `CV 1` output on the track's configured physical CV output.

**Phase 4: Geode Extraction (1-2 weeks)**
- Extract `updateGeode()`, voice state, and Geode-specific parameters into a standalone `GeodeEngine` class.
- `TeletypeTrackEngine` holds `GeodeEngine _geode` and delegates.
- `tele_g_*` bridge callbacks become `_geode.setParameter()` calls.

**Phase 5: Optional State Reduction (ongoing, low priority)**
- Continue the `resource-optimization` deferred items (P4b slot backup consolidation, P7 engine extraction) only if RAM pressure returns.
- No `scene_state_t` demolition. The struct stays as runtime truth, as established by the clip architecture work.

**What we do NOT do:**
- No bytecode compiler. The system already compiles at edit time.
- No `scene_state_t` replacement. The clip architecture already solved state ownership.
- No new intermediate representation. `tele_command_t` is the IR.
- No "compile on edit in a UI task." Compilation (Ragel scan) already happens at edit time, synchronously, and is fast enough for the 6-line scripts LITE mode supports.
- No Geode synthesis rewrite. The DSP code works; extraction is about ownership clarity, not correctness.
'''

filepath = '/Users/foronoma/Work/Code/Eurorack/performer-phazer/docs/teletype_v2.md'
with open(filepath, 'a') as f:
    f.write(appendix)
print(f'Written {len(appendix)} bytes')
