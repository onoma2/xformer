# Generator Preview/Apply + Step Selection + 64-Step Context

## Goal

Implement A/B preview workflow for the Generator page: enter in ORIGINAL state, encoder re-roll creates preview, F0 toggles between original and preview, APPLY commits, CANCEL reverts. Add step selection and section (bank) navigation for 64-step context. Port as much code from the Vinx fork as possible.

## Key Files

- `src/apps/sequencer/engine/generators/SequenceBuilder.h` — add 3-copy state machine
- `src/apps/sequencer/engine/generators/Generator.h` — add delegate methods
- `src/apps/sequencer/engine/generators/RandomGenerator.h` — add Variation, randomizeParams, displayValue
- `src/apps/sequencer/ui/pages/GeneratorPage.h` — add state machine + step selection
- `src/apps/sequencer/ui/pages/GeneratorPage.cpp` — rewrite for A/B workflow

## Reference Files (Vinx)

- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/SequenceBuilder.h` — 3-copy impl with AcidSequenceBuilder, ChaosSequenceBuilder, applyEntropy
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/Generator.h` — mode enum with Acid/Chaos/ChaosEntropy, delegate methods
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/RandomGenerator.h` — Variation param, randomizeParams, randomizeSeed, randomizeContextParams, displayValue
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/EntropyTargets.h` — 14-target enum + blendValue
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/ChaosGenerator.h` — 14-target chaos (Phase 3 consumer)
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/ChaosEntropyGenerator.h` — thin entropy wrapper (Phase 3 consumer)
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/GeneratorPage.h` — state machine, step selection, section
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/GeneratorPage.cpp` — full 2093-line workflow

## Decisions Log

- 2026-05-16: Squashed "Random generator preview/apply", "Generator preview/apply workflow", and "64-step context visualization" into one task. The A/B state machine is the core infrastructure that all three depend on; step selection and bank visualization are the UI layer on top of it.

## Open Questions

- [ ] Should `EntropyTargets.h` be ported in Phase A or deferred to Phase E (entropy consumer)? Decision: defer to Phase 3 chaos/entropy task.
- [ ] XFORMER-specific entropy layers (AccumulatorTrigger, PulseCount, GateMode, HarmonyRoleOverride, etc.) — how to map EntropyTarget to these? Needs custom `applyTarget<NoteSequence>` specialization.

## Completed Steps

- [x] Research: Vinx fork generator architecture fully analyzed
- [x] Research: XFORMER current state fully analyzed
- [x] Research: Delta between Vinx and XFORMER documented
- [x] Research: RAM footprint recorded (see RESEARCH.md)

## Notes

RAM budget is tight. Current `.data + .bss = 118,648 (90.5%)`. Phase A adds 1 copy of the active sequence type per builder instance (~9.5 KB for NoteSequence, worst case). Builders are created on-demand and live on the stack or in Container<NoteSequenceBuilder> — check sizeof impact before implementing.
