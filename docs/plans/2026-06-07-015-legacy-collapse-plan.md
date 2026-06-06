# Legacy collapse — remove dead writeRouted + Routable routed-slot

> **For Claude:** Phase-9 cleanup, pulled forward now that phase 6 is complete (all 8 engines on the
> override path). Goal: collapse the three coexisting layers (legacy / transitional / specced) into
> one model for per-track params. Execute slice-by-slice, each gated by STM32 release build + sim +
> Codex ALLOW. No flash until the owner says.

## Live vs dead boundary (verified 2026-06-07)

**DEAD — remove:**
- The 8 engines' `writeRouted` methods (Note/Curve/Tuesday/Stochastic/DiscreteMap/Indexed/PhaseFlux/
  MidiCv, both Track + Sequence) — never called: `migrated()` is true for every per-track param, so
  `updateSinks` writes the override and skips `writeTarget`.
- The `switch (track.trackMode())` per-track-param dispatch inside `Routing::writeTarget`
  (`Routing.cpp` ~236-330) that calls those `writeRouted`.
- `Project::writeRouted` Tempo/Swing cases (read-migrated via `routedValueGlobal`).
- The `routed` slot of `Routable<T>` for every migrated engine field → collapse `Routable<T>` to
  plain `T`. Not serialized (only `base` is), so **pure-RAM win, no file-format change**.
- `setX(value, bool routed)` second param on migrated fields (only the dead `writeRouted` passed
  `true`); collapses to `setX(value)`.

**STAYS LIVE — do NOT touch (not legacy, by design):**
- `PlayState::writeRouted` (Mute/Fill/FillAmount/Pattern — never migrated, not base+delta).
- Shell handlers in `writeTarget`: `Run`, `CvOutputRotate`, `GateOutputRotate` (the per-track
  generic continue-handlers before the trackMode switch). Their Track fields stay `Routable<>`
  (`Track.h:408-410 _runGate/_cvOutputRotate/_gateOutputRotate`), read via `.get(isRouted(...))`.
- `CvRoute` (`CvRoute.h` 2 Routable fields).
- `Routing::isRouted/setRouted` bitmask — still drives UI route-arrow markers (`printRouted`) and the
  live shell-field reads. Keep.
- The `Routable<T>` template itself — still used by the shell fields + CvRoute.
- **`DiscreteMapSync`** — a still-live, non-migrated `Routing::Target` (retired from the override
  *model*, but the target + its `_routedSync` sink on DiscreteMapTrack/IndexedTrack persist for
  external reset/sync; engine readers at DiscreteMapTrackEngine.cpp:140 / IndexedTrackEngine.cpp:202).
  It falls through to `writeTarget`, so it MUST keep a handler there (a per-track `setRoutedSync`
  alongside the Run/CvOutRot/GateOutRot shell handlers). Do NOT delete it with the trackMode switch.
  `_routedInput/_routedScanner/_routedIndexedA/B` ARE dead (getters read the override); `_routedSync`
  is NOT — it's the live Sync sink.

**FLAGGED, separate (do NOT fix here):** `cvRouteScan()/cvRouteRoute()` read `_cvRoute.scan()/.route()`
not the override, but CVR is in the global table → CVR modulation writes an override nothing reads
(latent no-op). Its own fix; leave `Project::writeRouted` CVR cases intact in this collapse.

## Slices

### Slice 1 — delete dead engine writeRouted + the writeTarget dispatch
**Files:** `Routing.cpp` (writeTarget), the 8 engines' `*Track.{h,cpp}` + `*Sequence.{h,cpp}`
(`writeRouted` decl + body + the wrapper track→sequence delegation), tests
`TestCurveWavefolderRouting.cpp` + `TestDiscreteMapSequencePage.cpp` (drop/port the writeRouted refs).
- In `writeTarget`, delete only the `switch (track.trackMode())` block; KEEP the Run/CvOutputRotate/
  GateOutputRotate continue-handlers and the Project/PlayState/Bus branches.
- Delete each engine's `writeRouted` (Track + Sequence). Delete `Project::writeRouted` Tempo/Swing
  cases **only if** that leaves CVR handled — otherwise keep `Project::writeRouted` with just the CVR
  cases (Tempo/Swing dead but harmless if left; prefer removing Tempo/Swing, keeping CVR).
- Build STM32 + sim, fix tests, Codex gate. No behavior change (the deleted paths were unreachable).

### Slices 2-9 — collapse `Routable<T>` → plain `T`, one engine per slice
Order: Note, Curve, Tuesday, Stochastic, DiscreteMap, Indexed, PhaseFlux, MidiCv. Per engine:
- Each migrated `Routable<U> _x` → `U _x`.
- Getter already reads `_x.base` via `routedValueInt`/`routedValue` → change `_x.base` → `_x`.
- `setX(v, bool routed=false)` → `setX(v)` (`_x = clamp(...)`); drop the `routed` param + update the
  few internal callers (edit methods already edit from base).
- Serialization: `_x.write(writer)` → `writer.write(_x)`; `_x.read(reader)` → `reader.read(_x)`
  (format identical — Routable already serialized only base).
- `_x.clear()`/`.setBase()` → plain assignment.
- Build + sim + Codex gate per engine.
Keep `Track` shell fields, `CvRoute`, and (if not collapsed) `Project` tempo/swing as `Routable<>`.

### Slice 10 — final sweep
- Confirm `Routable<T>` is still referenced (shell + CvRoute) — keep the template.
- Update `.tasks` + spec: legacy param path removed; phase 9 (param half) done.

## Invariants
- No serialized-format change (Routable wrote base only). Old projects load unchanged.
- No behavior change: deleted writeRouted/dispatch was unreachable; the override path already drives
  every migrated param.
- Per-slice gate: STM32 release + sim clean (no new warnings) + Codex ALLOW. Owner flashes once at end.
