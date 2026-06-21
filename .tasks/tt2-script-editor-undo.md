# tt2-script-editor-undo

## Goal
Edit-safety in the TT2 script editor: stop losing typed lines, and give single-level undo of a
committed line change.

## Key files
- `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.{cpp,h}` — `commitLine`/`deleteLine` capture, `undo()`, Ctrl+Z binding, the `_history` ring

## Decisions log
- 2026-06-21: Keep failed-parse lines in history. `commitLine` returned on parse error *before* `pushHistory`, so a typo'd long line was lost. Now it pushes the raw buffer on the error path too — up-arrow recall pulls it back for repair (`5ffd6c82`).
- 2026-06-21: Single-level line undo (`Ctrl+Z`, `54639118`). Capture the pre-image (raw `TT2Command` + line + pre-length) before `commitLine` overwrites or `deleteLine` removes a committed line; `undo()` restores it exactly under `_engine.lock()`. ~60 B resident. Covers edit, paste-then-commit, and line removal. Invalidated on script switch / load / page enter. `Ctrl+X`/`Ctrl+V` are buffer-only (no committed change), so no undo needed.
- 2026-06-21: Rejected the bigger options after a RAM check. Pages are resident in CCMRAM and the script page is already the fattest (~864 B); CCMRAM is down to ~3.4 KB free this session. So script-snapshot undo ring (~1-2.4 KB) and multi-line select/move (~2.7 KB + new bindings) are **deferred** — too much resident RAM for the benefit, and there's a pending 4bpp-framebuffer reclaim that should land first. The 64 B inverse-op undo was the right size.

## Open questions
- [ ] Runtime play-test: edit-then-Ctrl+Z and delete-then-Ctrl+Z on sim/hardware (build-verified only; not unit-testable without page+engine).

## Completed steps
- [x] Failed-parse lines kept in `_history`. `5ffd6c82`
- [x] Ctrl+Z single-level undo (overwrite + delete), ~60 B. `54639118`
- [x] Sim + STM32 release clean.

## Notes
RAM context: `.ccmram_bss` ~53.8 KB of 56 KB (~3.4 KB free); undo added ~60 B. Main `.bss+.data`
~115 KB, ~7.7 KB under the 120 KB warning (improved by the vendored-C-tree purge). Multi-line
select/move + multi-level undo are the deferred next tier, gated on the framebuffer RAM reclaim.
