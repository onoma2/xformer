Teletype ↔ Performer: Exposure & Emergent Behavior Proposal

Summary
- Goal: expose minimal shared signals so Teletype can modulate and react to other tracks, not just run as a VM.

Current Exposure (Reality Check)
- TIME/M in Clock mode already uses Performer clock ticks.
- TIME/M in Ms mode uses wallclock (os::ticks via bridge).
- TI-TR inputs can read physical gate outputs (GateOut1-8).
- CV inputs only read physical CV In 1-4; CV-out-as-input is not enabled.

Phase 1: Low-Risk, High-Impact
- Add shared bus:
  - BUS.CV n / BUS.TR n read+write shared slots (4–8 slots).
  - Purpose: clean, safe cross-track modulation without touching track internals.
- Add track output readback:
  - TK.CV t / TK.TR t read current outputs for feedback loops.
  - Optional write aliases if not already routed.
- Add transport/clock read ops:
  - TP.RUN / TP.STOP / TP.RESET / TP.TICK (read only).
  - Lets scripts react to transport changes.

Phase 2: Medium Risk
- Pattern read/write:
  - PAT.G t p i / PAT.S t p i x for track pattern data.
  - Guard by track type (Note/Curve/Teletype only).
  - Enables algorithmic pattern generation.

Phase 3: Higher Risk
- Track parameter access:
  - TK.P t p / TK.P t p x for limited param set.
  - Keep scope tight (length, transpose, probability, etc.).

Concrete Emergent Behaviors
- Teletype as meta-modulator:
  - Drives multiple track CV/Gate outputs from algorithmic scripts.
- Polymetric modulation:
  - Teletype runs in Clock mode at its own divisor/multiplier and modulates other tracks.
- Cross-track feedback:
  - Read track outputs, compute logic, write to other tracks (reactive sequencing).
- Algorithmic pattern writing:
  - Generate/transform Performer patterns on the fly.

Design Notes
- Prefer small shared bus + explicit readback over deep track poking.
- Keep UI minimal: bus size + optional track mapping in list model.
- Document differences from Teletype hardware behavior (accumulation vs per-message, etc.).

Phase 4: Direct Engine Fusion (Emergent Hooks)

- **Modulation In (Performer → Teletype)**
  - Add `TeletypeX`, `TeletypeY`, `TeletypeZ`, `TeletypeT` to `Routing::Target`.
  - Allows Performer LFOs/Envelopes to modulate Teletype variables directly.
  - Emergent Result: A Note Track LFO can drive a Teletype algorithmic sequence.

- **Phase Variable (Clock → Teletype)**
  - Expose `Engine::syncFraction()` as Teletype variable `PHS` (0-100% of the bar).
  - Emergent Result: Teletype becomes a tempo-locked function generator/wavetable oscillator.

- **Direct Track Remote (Teletype → Performer)**
  - Add Opcodes: `TRK.MUTE i v` (Mute/Unmute) and `TRK.P i v` (Switch Pattern).
  - Emergent Result: Teletype acts as the "Conductor," switching patterns on other tracks based on algorithmic logic.

- **Variable Export (Teletype → Performer Source)**
  - Add `TeletypeX`... to `Routing::Source`.
  - Emergent Result: Teletype scripts become custom, complex modulators for any other track parameter.
