# ByteLab Modulator Shape Design

Created: 2026-06-24
Status: design record / pre-plan

## Summary

Performer should land ByteLab as a per-slot modulator shape, not as a literal
Leibniz hardware clone, not as a Geode-style bank mode, and not as a new
TrackMode first.

The useful product is a deterministic byte-logic shaper that turns existing
Performer sources into bit-sliced gates, lookup-table transforms, and stepped
modulation values. Its output should remain the normal value of that modulator
slot, so the existing routing matrix, MIDI CC output, and CV assignment paths can
continue to consume `Mod1..Mod8`.

This is control-rate DSP. Performer runs the sequencing engine at 1 kHz, while
Leibniz hardware is a discrete high-speed parallel logic pipeline. The design
should preserve the musical behavior that matters here: sharp clocked state
changes, repeatable bit logic, byte-addressed transforms, and routeable
modulation. It should not promise 2 MHz audio-rate bus processing.

## Why Modulator Shape

The existing modulator subsystem is the right landing zone, but ByteLab should
live inside one modulator slot:

- `Mod1..Mod8` are already routing sources.
- Modulator values already drive MIDI output.
- CV output assignment can already consume modulator sources.
- A per-slot shape avoids adding track model, track engine, output routing, and
  route-source surface before the concept is proven.
- A per-slot shape avoids hidden bank-wide behavior: `MOD 3 - BYTELAB` means the
  user is editing the output of `Mod3`.

Rejected for MVP:

- New TrackMode first. Too much model/engine/UI/output surface for a byte logic
  experiment.
- New physical output bus. Existing routing and modulator sources are enough.
- Audio-rate Leibniz emulation. Performer is not scheduled for that.
- Full virtual patchbay first. Start with one fixed shaper pipeline.
- Geode-style bank mode. Geode is a specific multi-voice generator; ByteLab is a
  shaper. It should not consume the whole `M1..M8` bank.

## Product Identity

The feature name is `ByteLab`. Leibniz remains the external hardware reference,
not the product name.

The feature is a `Modulator::Shape` option. Each modulator slot can independently
be `ByteLab`:

```
MOD 3 shape = ByteLab
```

The output remains the normal modulator value in `0..127`, so existing route and
MIDI output paths keep working.

## MVP Pipeline

The first implementation should be a fixed direct shaper pipeline:

```
source level
  -> input gain / bias
  -> byte
  -> control-rate divisor hold
  -> bit op / mask
  -> LUT or bypass
  -> dry/wet mix
  -> output depth / offset
  -> output
```

### 1. Drezno-Style Input Quantizer

Read one existing Performer source and quantize it to an 8-bit byte:

```
adc = clamp((source01 - 0.5) * inputGain + 0.5 + inputBias, 0, 1)
byte = clamp(round(adc * 255), 0, 255)
```

Source selection should reuse existing `Routing::Source` concepts where possible.
For the first cut, one input source is enough.

Phase 1 input source rules:

- `IN` may select another modulator slot `M1..M8`, excluding the current slot.
- `IN` may not select another ByteLab modulator in Phase 1.
- `IN = self` is invalid.
- If the selected input becomes invalid, fall back to `None` or the first valid
  non-ByteLab modulator source.

Do not force a next-index or lower-index rule. All 8 modulator slots should be
ByteLab-capable; the only Phase 1 restriction is avoiding self-reference and
ByteLab-to-ByteLab chains.

### 2. Control-Rate Divisor

ByteLab should optionally hold its byte conversion for a small integer divisor:

- `/1`
- `/2`
- `/3`
- `/4`
- `/6`
- `/8`

This is a sample-and-hold / control-rate divider, not an accumulator. The source
continues moving; ByteLab only refreshes its byte/mask/LUT result every N control
samples or clock events.

### 3. Lipsk-Style Bit Operation

Apply an 8-bit mask through a selected operation:

```
masked = bitOp(byte, mask)
```

MVP operations:

- XOR mask
- AND mask
- OR mask
- MSB fold

The mask is the main live-performance control. Flipping high bits creates the
large voltage discontinuities that make the Leibniz technique musically
distinct.

### 4. Jena-Style Lookup Table

Use a fixed 256-entry table:

```
lutOut = lut[masked]
```

MVP LUTs should be immutable `const uint8_t[256]` tables in flash:

- bypass / linear
- linear
- triangle
- sine-ish
- folded / Chebyshev-ish

No editable LUT storage in MVP.

### 5. Dry/Wet Mix And Output Depth/Offset

Blend the unshaped source byte with the shaped byte:

```
mixed = mix(adc, lutOut / 255, amount)
output = clamp((mixed - 0.5) * depth + 0.5 + offset, 0, 1)
```

This keeps the engine legible. The same input always creates the same output for
the same bit-op, mask, LUT, and mix settings. No hidden state should affect the
MVP output.

### 6. Output

Each ByteLab modulator slot outputs the shaped value:

- final shaped byte scaled to `0..127`
- optional bit gate output can be deferred unless it is needed for the first
  musical demo

This gives stepped shaper curves without inventing new routing outputs.

## Timing Contract

All state changes are clocked unless the user explicitly selects a free-running
mode.

Clock source options:

- transport tick/division
- routed gate edge
- manual trigger from the page

Between clock edges, outputs hold. This is important. The feature should feel
like arithmetic logic stepping through a bus, not like a smoothed LFO.

The engine must remain compatible with the 1 kHz update loop. Multiple pending
transport ticks can be processed in one engine update, so any edge-clocked state
must be advanced per tick/event, not by sampling only the final state.

## JustF Interaction

JustF is a transient rate-link tool. ByteLab is a per-slot shaper and has no
normal `RATE` parameter. Its `DIV` control is a sample-and-hold divisor, not an
LFO rate.

Rules:

- ByteLab slots are ignored by JustF spread.
- ByteLab cannot be the JustF M1 master.
- JustF bake skips ByteLab slots.
- Shift+RATE should not enter JustF from a ByteLab page.
- ByteLab may shape JustF indirectly: if `M1` is a normal Sine/ADSR/etc
  modulator affected by JustF, `M3 = ByteLab IN M1` is valid and shapes that
  JustF-derived motion.
- ByteLab-to-ByteLab input remains invalid in Phase 1.

Geode remains different: it is bank-wide and mutually exclusive with JustF.
ByteLab should not be globally mutually exclusive with JustF; it only opts out
as a JustF follower or master.

## Model Shape

Add persisted config inside each `Modulator` slot:

```
Modulator::_byteLab
class ByteLabConfig
```

Candidate persisted fields:

- input source
- clock source
- clock division / mode
- input gain
- input bias
- control-rate divisor
- bit operation
- bit mask
- LUT index
- mix amount
- depth
- offset

Keep the model small. Use byte-sized fields wherever possible.

No `ProjectVersion` bump during feature development. Dev project breakage is
acceptable under current policy.

## Engine Shape

Add per-slot runtime state inside `ModulatorEngine`:

```
ModulatorEngine::_byteLabState[CONFIG_MODULATOR_COUNT]
struct ByteLabState
```

Runtime state:

- current ADC value
- current input byte
- masked byte
- LUT output byte
- held output value
- divisor counter
- previous clock/gate state

The normal `ModulatorEngine` update writes the final value for the slot.

Update order should avoid recursion:

1. Update normal non-ByteLab modulators.
2. Update ByteLab modulators from allowed non-ByteLab input values.

Phase 1 should reject ByteLab-to-ByteLab inputs rather than building a dependency
solver. A later version may allow acyclic chains if there is a clear musical
reason.

## UI Shape

Use `ModulatorPage`.

Display should show:

- active byte value
- ADC value after gain/bias
- 8-bit strip
- active mask bits
- selected bit operation
- selected LUT
- mix amount

Any OLED layout work must be rendered through `ui-preview/` before judging text
fit, bit-strip spacing, or footer labels.

MVP controls should be sparse:

- select `ByteLab` shape
- select input
- input gain
- input bias
- select clock
- control-rate divisor
- select bit operation
- edit mask
- select LUT
- edit mix
- depth
- offset

Page layout:

- Page 1: `IN`, `GAIN`, `BIAS`, `DIV`, `BITS`
- Page 2: `OP`, `FOLD`, `MIX`, `DEPTH`, `OFFSET`

`BITS` is not primarily edited as a 0..255 numeric value. When `BITS` is focused,
the left visual area becomes an 8-bit editor:

```
7 6 5 4 3 2 1 0
1 0 1 1 0 1 0 0
      ^
```

Interaction:

- encoder moves the bit cursor across bit 7..0
- encoder press toggles the selected bit
- Shift+encoder rotates the whole mask left/right
- optional function shortcuts may clear or invert all bits, but are not required
  for MVP

The normal parameter list should show both compact forms:

- bit strip, e.g. `10110100`
- hex, e.g. `B4`

Do not make the primary interaction a 256-step rotary mask list. That is too
opaque for live editing.

Do not build a decorative patchbay page first.

The reference behavior prototype is:

`ui-preview/bytelab-live/index.html`

## Deferred

- ByteLab-to-ByteLab chains with acyclic dependency resolution.
- Rostock-style delay ring.
- Poczdam-style arbitrary bus matrix.
- Erfurt-style accumulator / scan mode.
- Editable/custom LUTs.
- Multiple simultaneous input buses.
- New routing source enum for each bit.
- Per-output tap assignment.
- TrackMode version with indexed gate/CV outputs.
- Audio-rate behavior.
- SD-card preset packs.

## Acceptance Criteria

- A selected non-ByteLab modulator source can be quantized to an 8-bit byte.
- Input gain and bias change the byte activity density before bit operations.
- Control-rate divisor holds the byte/mask/LUT result for `/1`, `/2`, `/3`,
  `/4`, `/6`, and `/8`.
- Bit mask changes visibly and audibly affect generated modulation.
- Bit mask editing uses an 8-bit cursor editor; the user can move across bits and
  toggle one bit without rotating through all 256 mask values.
- Bit operation can be XOR, AND, OR, or MSB fold.
- LUT output uses exactly 256 entries and no runtime math beyond indexing.
- Output is stateless for a fixed source, gain/bias, divisor phase, bit op,
  mask, LUT, mix, depth, and offset setting.
- `DEPTH` and `OFFSET` scale/shift the final shaped result.
- Any `M1..M8` slot can use ByteLab as its shape.
- ByteLab output is the normal output value for that modulator slot.
- ByteLab input rejects self-reference and ByteLab-to-ByteLab sources in Phase 1.
- JustF ignores ByteLab slots, cannot use ByteLab M1 as master, and skips ByteLab
  slots on bake.
- ByteLab can shape a non-ByteLab source whose rate is currently being derived by
  JustF.
- Existing routing and MIDI output can consume the generated values with no new
  route-source model.
- STM32 release build passes.
- RAM sections and `Engine`/`Model` sizes are checked after implementation.

## Open Questions

- Should Phase 1 allow CV inputs as ByteLab sources, or only other non-ByteLab
  modulator slots?
- Should bit-gate outputs exist in Phase 1, or wait for the deferred tap page?
- Should accumulator scan add `step`, `masked`, or a selectable source when the
  deferred mode is designed?

## Next Step

Write a plan for the MVP only:

1. `ByteLabConfig`
2. `ByteLabState`
3. `ModulatorEngine` per-slot update integration
4. `ModulatorPage` minimal UI
5. Unit tests for byte ops, LUT indexing, and stateless output behavior
6. STM32 release RAM/build check
