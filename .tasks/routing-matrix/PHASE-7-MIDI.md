# Phase 7 — MIDI as a routing source (LEARN)

> **DONE 2026-06-08** — shipped as commits `41a553f5`→`74b90565`→`d81c8dc9`→`84697813`→`ea183679`.
> Design: `docs/plans/2026-06-08-016-midi-routing-source-design.md`. The flow shifted from the
> original brainstorm: LEARN lives on a dedicated **MIDI tab** (12th, after BUS), not the grid —
> the grid picks MIDI as a plain source, the page hosts capture + field editing. See STATUS.md.
> This file is kept as the original resume prompt / design trail.

Resume prompt for the modulation-matrix overhaul. Branch `refactor/routing-matrix`.

## Where we are

The matrix is functionally complete for **CV-domain** modulation across every engine:
Note/PhaseFlux/Curve/Tuesday/DiscreteMap/Indexed/Stochastic engine pages + Pitch/Clock/Global
bands + Bus, all on the draft→COMMIT model. Phases 1–6 + 9 done; bus unified to base-0 delta +
bus-hub edit flow; static engine-page ring; Stochastic revamped (9 curated targets + BurstHold
>50% randomization) and in the ring. **The remaining capability is MIDI as a source** — the source
picker deliberately excludes it today.

## What already exists (model infra — do NOT rebuild)

- `Routing::Route` already carries a `MidiSource` (`model/Routing.h:864`, `route.midiSource()`),
  with `MidiSource::Event` types (`ControlAbsolute`/`ControlRelative`/…, `Routing.h:621`) +
  `MidiSourceConfig`.
- `Source::Midi` enum sentinel + `Routing::isMidiSource()` (`Routing.h:511`).
- `MidiLearn` engine class (`engine/MidiLearn.h`, `_engine.midiLearn()`); `RoutingPage::exit()`
  already calls `midiLearn().stop()`.
- `RouteBrowse::sourceList` currently **drops** `Source::Midi` (`model/RouteBrowse.h`) — the one
  line gating MIDI out.

## Goal

Expose MIDI as a route's source via a LEARN capture, integrated into the existing draft→COMMIT flow
(live route untouched until COMMIT; one source per row).

## Constraints (unchanged)

- Frozen spec `docs/plans/2026-06-04-013-modulation-ui-spec.md` (+ its §15 amendments — phase 7
  reserved a LEARN F-key, originally F1; reconcile with current footers).
- TDD per slice (`TestRouteBrowse` / `TestRouteGetterMigration` patterns). Build sim + STM32
  release green each slice.
- **No ProjectVersion bump** (MidiSource already serializes).
- **ui-preview render before any OLED change.**

## Settle first (brainstorm, don't code yet)

1. LEARN flow in the draft model — arm → next incoming MIDI event fills the draft's `MidiSource` →
   shows on the draft row → COMMIT. Lives in matrix door, param door, or both?
2. Picker integration — a "MIDI" entry in the source overlay that arms LEARN, or a dedicated
   F-key? Reconcile against the occupied matrix footer (VIEW/EDIT/MOD/CANCEL/COMMIT) + bus footer.
3. Event scope — CC-absolute first, or also relative/note/velocity? (`MidiSource::Event`.)
4. Display — how a MIDI source renders in a grid cell / param row (not a `printSourceAbbrev` CV token).

## Out of scope (stays deferred, spec §13)

Shaper UI (None/TriangleFold only until the shaper port) — keep phase 7 MIDI-only.

## First steps

Brainstorm the 4 questions → render the LEARN flow in `ui-preview/` → per-phase TDD → flip
`sourceList` to include Midi **last** (once capture + display + commit path is live).
