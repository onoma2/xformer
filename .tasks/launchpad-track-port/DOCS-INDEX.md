# Launchpad-track-port — docs index

Recency + current status of the docs in this folder. Updated 2026-07-05.
"Last touched" = last meaningful content edit (this session's edits are uncommitted; git commit dates lag).

## Live — read these first

| Doc | Status | Last touched | What it is |
|-----|--------|--------------|------------|
| `PHASEDPLAN.md` | **Authoritative plan** | 2026-07-04 | The implementation plan. Revised this session for the full 11-mode enum + folded the adversarial/feasibility review (dropped buffer-then-blast, cut DiscreteMap layers, scoped Performer, re-did arithmetic, cleared stale deps). |
| `STATUS.md` | **Current status** | 2026-07-05 | Task board entry + what's done + design-exploration note. |
| `GRID-IDIOMS-REFERENCE.md` | **Design input — NOT in the plan yet** | 2026-07-05 | The surface-model thinking: native-scheme mirror per engine, multi-page structure, lossless overlays, and per-engine extensions + verified feasibility. Material to decide from, not committed scope. |
| `native-scheme-mirror.html` | **Design render (companion)** | 2026-07-05 | Interactive layout render for the above — click page tabs per engine. Open in a browser. |

## Reference — still useful, mind the caveats

| Doc | Status | Last touched | Note |
|-----|--------|--------------|------|
| `GRID-APPROACHES-REVIEW.md` | **Reference, §1.1 stale** | 2026-05-17 (§1.1 flagged 2026-07-04) | Grid patterns from the monome-infra `temp-ref/grid` set. §1.1 buffer-then-blast is marked NOT ADOPTED (UsbMidi already batches). Rest still informs E0. |
| `UI-OPTIMIZATION-REVIEW.md` | **Reference (folded into E0)** | 2026-05-17 | Optimization findings; the live ones became E0 items. |
| `mebitek-research.md` | **Reference** | ~2026-05 | mebitek fork feature extraction (Performer mode, circuit keyboard, LayerMapItem). |

## Historical — background/inputs, some scope superseded

| Doc | Status | Note |
|-----|--------|------|
| `README.md`, `TASK.md` | Framing | Original task scope/framing (~May). |
| `01-indexed` / `02-discrete-map` / `03-tuesday` / `06-notetrack` / `07-curvetrack` -research | Historical research | Per-track grid-suitability studies (~May), pre-date the current model families. |
| `04-midi-cv-track-research.md` | Historical — **scope dropped** | MidiCv removed from the port (no sequence data). |
| `05-teletype-track-research.md` | Historical — **superseded** | Pre-TT2; Teletype is now TT2/Mini, script editing excluded, pattern grid = owner-decision D9. |
| `08-phase2-summary.md`, `09-other-projects-research.md` | Historical | Phase-2 summary + other-fork survey (~May). |

## One-line map
Plan → `PHASEDPLAN.md`. Status → `STATUS.md`. New surface thinking (undecided) → `GRID-IDIOMS-REFERENCE.md` + `native-scheme-mirror.html`. Everything else is reference/historical.
