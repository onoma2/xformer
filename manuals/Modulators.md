# Modulators

XFORMER includes **8 global modulators** (Mod 1–8) for project-wide modulation. They are CV/Gate signal generators whose output can offset physical CV/MIDI outputs and serve as Routing/CvRoute sources.

## Access

- **Navigate**: `PERFORMER` page → **MOD** (double-press ROUTING key), or press F1-F5 on USB keyboard
- Select modulator: Track buttons 1–8 (left column of grid)

## Shapes

| Shape | Description |
|-------|-------------|
| **SINE** | Standard cosine LFO |
| **TRIANGLE** | Linear triangle wave |
| **SAW UP** | Rising sawtooth |
| **SAW DOWN** | Falling sawtooth |
| **SQUARE** | 50% pulse |
| **RANDOM** | Stepped random, with slew ("clocked" or "triggered") |
| **ADSR** | Attack-Decay-Sustain-Release envelope |
| **LORENZ** | Chaotic attractor (3D sub-tick iterations) |
| **LATOOCARFIAN** | Chaotic attractor (phase-stepping) |

## Modes

| Mode | Behavior |
|------|----------|
| **Free** | Phase advances continuously |
| **Sync** | Phase resets on gate rising edge |
| **Retrigger** | Phase resets on gate, amplitude scales with gate length |
| **Hold** | Only advances while gate is high; freezes when gate drops |

### Gate source
When a mode uses a gate, the **GATE TRACK** parameter selects which track's gate triggers the modulator.

## Parameters

### Page 1: RATE / P1 / P2 / DEPTH (F1–F4, F5)

| Parameter | LFO/Random | Chaos | ADSR |
|-----------|-----------|-------|------|
| **F1** SHAPE | Shape name | Shape name | Shape name |
| **F2** RATE | Musical divisions (1/64..16 bars) | Hz 0.1..100.0 | Attack (ms) |
| **F3** DEPTH / P1 | Depth % (0–127) | P1 → chaos param | Decay (ms) |
| **F4** OFFSET / P2 | Offset (−64..+63) | P2 → chaos param | Sustain level |
| **F5** PHASE / SLEW | Phase 0°–359° | Slew/Smooth | Release (ms) |

- **Depth**: controls output amplitude. Range is ±5V at `depth=127`; default = 25 (~±1V).
- **Chaos P1/P2**: parabolic curve `t*(2−t)`. Attack/decay fields map to full Lorenz rho=10..50 and beta=0.5..4.0.
- **LFO rate encoder**: right = **faster** (smaller musical divisor).
- **Chaos rate encoder**: right = **faster** (higher Hz).
- **Rate display**: decimal shown for `< 1000 Hz` (e.g. `66.0Hz`).

### Page 2: (ADSR only — press Left/Right to switch)

- **F1** AMPLITUDE: overall ADSR amplitude (0–127)
- No other parameters on page 2.

### Routing Overlay (Shift+Page to toggle)

| Function | Description |
|----------|-------------|
| **F1** MODE | Free/Sync/Retrigger/Hold |
| **F2** GATE | Select gate track (1–8) |
| **F3** TARGET | None → MIDI 1–16 → CV 1–8 |
| **F4** EVENT | CC / Note (MIDI only; "N/A" for CV) |
| **F5** CCNUM | CC number 0–127 (MIDI CC only) |

- **Default target**: **None** (unassigned). No output produced until selected.
- Switching target **clears old assignments** (prevents Modulove leak of simultaneous MIDI+CV).
- Exit overlay = apply routing. Flash message confirms.

## Output paths

### 1. Direct CV offset
When a modulator is routed to a **CV output** (TARGET = CV 1..8):
- Modulator value is added as a **post-track, post-routing** offset before DAC write
- Applied after: track CV, rotation, CvRoute lane mixing
- **Not** applied during `cvOutputOverride` (Monitor scope mode)

### 2. MIDI CC
When routed to **MIDI output** (TARGET = MIDI 1..16, EVENT = CC):
- Throttled CC send via `OutputState`
- CC value derived from `currentValue()` (0–127)

### 3. RoutingEngine source (Routing page)
On any **Routing** route, set SOURCE to `Mod 1`..`Mod 8`:
- Value read as `currentValue / 127` → 0..1 range
- Feeds into standard routing chain: normalization → bias → depth → shaper → target

### 4. CV Router input (CvRoute page)
Each CvRoute lane input cycles: **CvIn → Bus → (2 modulators) → 0V**

| Lane | Available modulators |
|------|---------------------|
| Lane 1 | Mod 1, Mod 2 |
| Lane 2 | Mod 3, Mod 4 |
| Lane 3 | Mod 5, Mod 6 |
| Lane 4 | Mod 7, Mod 8 |

Modulators are normalized to **±5V** before mixing into the lane.

## Shortcuts

| Input | Action |
|-------|--------|
| Track buttons 1–8 | Select modulator |
| Left / Right | Change modulator (normal) / switch ADSR/Chaos page |
| Shift+Page | Toggle routing overlay |
| Shift+encoder | Fine control (rate: 0.1 Hz / 1 ms steps; depth: 1 step) |
| Encoder press | Chaos/LFO/Random: no effect; ADSR: gate trigger in "triggered" mode |
| F1–F5 (USB keyboard) | Select SHAPE/RATE/DEPTH/OFFSET/PHASE (or overlay functions) |
| Tab (USB keyboard) | Context menu (same as Shift+Page) |
| Up/Down arrows (USB) | Encoder rotation (+1 / −1) |
| Left/Right arrows (USB) | Same as hardware Left/Right keys |

## Context menu

Available context menu item: **Route** — toggles the modulator's first free CV output assignment (or clears existing).

Note: Modulove original includes a "quick-map popup" (Output/Mode/CC# fields), but the popup is unreachable without a dedicated physical ContextMenu key on standard hardware. The context menu Route action is the primary assignment mechanism.

## Architecture notes

- `_currentValue[8]`: shared across Random and Chaos shapes (mutually exclusive — no buffer collision)
- Chaos slew: uses `_lastRandomValue` as smoothed buffer; cleared on retrigger
- Lorenz: 1ms sub-tick loop, `dt = 0.001 * hz` iterations per engine tick
- Latoocarfian: phase-stepped at Hz frequency
- `applyModulatorOffset()` runs after `updateCvRouteOutputs()`, after track CV but before DAC
