# teletype-v2-dialect

## Goal

Define Teletype v2 as a Performer-native Teletype++ dialect: preserve hardware-independent pasted Teletype scripts, keep Performer-added ops, extend CV/TR to 8 outputs, and drop Monome hardware-only legacy as a design driver.

## Key files

- `docs/teletype_v2.md` — dialect contract, keep/drop/redesign inventory, compatibility rule, verification gates.
- `docs/teletype_v2_phase1_plan.md` — Phase 1 execution plan for native program/runtime/output state.
- `teletype/src/match_token.rl` — current Ragel token source and source-compatibility frontend.
- `teletype/src/ops/op_enum.h` — current runtime op enum.
- `teletype/src/ops/op.c` — current compiled runtime op table.
- `src/apps/sequencer/CMakeLists.txt:156` — current compiled Teletype source list.

## Decisions log

- 2026-05-26: Phase 0 is considered complete after source sanity checks: every parser-visible op/mod spelling is classified, `W.ACT`/`WR.ACT` aliasing is documented, and external hardware families are confirmed not to drive the native dialect.
- 2026-05-26: V2 keeps Ragel/`match_token.rl` as the tokenizer/parser frontend for pasted-script compatibility; the replacement boundary is after parsing, where parsed tokens lower into native Performer command/runtime state.
- 2026-05-26: Compatibility rule is script-level, not project-file-level: hardware-independent pasted Teletype scripts should run as Teletype++, but old Performer project migration and Monome hardware emulation are out of scope.
- 2026-05-26: Phase 1 starts with state ownership only: native program storage, native runtime state, and native output state; no interpreter execution before RAM/ownership are measurable.

## Open questions

- [ ] Exact packed command record width for native program storage.
- [ ] Exact pattern storage shape: reuse current pattern constants or define narrower Teletype++ storage.
- [ ] Whether `P.CYC` / `PN.CYC` dead implementations should be deleted or deliberately resurrected later.

## Completed steps

- [x] Reframed v2 as a breaking Performer-native dialect, not a compatibility wrapper.
- [x] Added hard compatibility rule for hardware-independent pasted Teletype scripts.
- [x] Classified current op surface into Core Keep, Performer-Added Keep, Redesign/Extend, and Drop/Unsupported.
- [x] Verified all 435 parser op/mod spellings from `match_token.rl` are covered by the spec categories.
- [x] Verified all 434 op enum constants have parser spellings.
- [x] Added `docs/teletype_v2_phase1_plan.md`.

## Next action

Start Phase 1 Step 1: define fixed-size native program storage and size probes without adding interpreter behavior.

## Notes

- Do not reintroduce slots, `scene_state_t`, `tele_*` callback chains, `TeletypeBridge`, or `g_activeEngine` as v2 architecture.
- `CV` and `TR` remain canonical names; extend them to 1-8 and add `.ALL` forms rather than creating an `OUT.*` namespace.
- STM32 release RAM numbers are the acceptance gate once structs exist.
