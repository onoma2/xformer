# Feature: Single-Geode CV Engine (Simplified Spec)

Goal: a compact, CV-rate “Just Friends Geode” style generator that is easy to script from Teletype, uses Performer’s transport for timing, and avoids extra layers/duplicate state.

---

## Summary

- 6 internal voices summed to one CV output.
- Each voice has Div + Repeats; global controls shape time/curve/spread and a single “physics” macro (RUN+MODE).
- Runs at control rate (1 kHz). Not an audio oscillator.

---

## Parameters

### Global (performance)
- TIME (0..16383): base envelope time. Lower = faster, higher = slower.
- INTONE (-16383..16383): spreads TIME across voices (voice 1 slow ↔ voice 6 fast). 0 = equal.

With INTONE at noon, all speeds are identical.

As INTONE turns CW, the speed of each slope besides IDENTITY increases, with the size of the change proportional to the index of the output. This means that 6N will increases its speed the most, while 2N will speed up the least. Once INTONE is fully CW, the 2N slope speed will be double IDENTITY's speed, 3N will be three times IDENTITY's speed, and so on.
As INTONE turns CCW from noon, the speed of each slope will instead decrease proportionally to its index, meaning 6N will decrease its speed the most and 2N decreases its speed the least. IDENTITY remains unaffected. Once INTONE is fully CCW, 2N will be precisely 1/2 the speed of IDENITY, 3N will be 1/3 the speed of IDENITY, and so on


- RAMP (0..16383): attack/decay skew. 0 = fast attack/slow decay, 8192 = triangle, max = slow attack/fast decay.
- CURV (-16383..16383): shape bend.


With CURVE at noon, the slopes are perfectly linear. This position is ideal for creating ramps, sawtooths, and triangles according to the position of the RAMP knob.
Turning CURVE CW from noon deforms the slopes into exponential curves. Exponential curves change slowly at the start of each phase and increase their rate of change as the phase progresses.
When CURVE reaches fully CW, the slopes become sinusoidal (i.e. sine waves).
Turning CURVE CCW from noon deforms the slopes the other way into logarithmic curves, which change quickly at the start of the rise or fall stage, and then decrease their rate of change as the phase progresses.
Eventually, once CURVE reaches fully CCW, it makes rectangular shapes. The slope goes to its maximum immediately at the start of its rise phase and stays there until the end of its rise phase. Once the fall phase begins, the slope falls to its minimum immediately and stays there for the duration of its fall phase. The pulse-width is determined by the RAMP control. As a result, in cycle mode, with CURVE fully CCW, RAMP becomes an audio-rate pulse-width modulation input


- RUN (0..16383): macro for dynamics (per MODE).
- MODE (0,1,2): 0=Transient, 1=Sustain, 2=Cycle.

### Per-voice (configuration)
For voice i = 1..6
- DIV i (1..64): subdivisions per measure (polyrhythm). Higher = more hits.
- REP i (-1..255): repeats in a burst. -1 = infinite.

---

## Timing Model

- Use engine’s measure fraction (0..1) from transport or clock timebase.
- Each voice keeps a phase accumulator:
  - phase += deltaMeasure * DIV
  - trigger on wrap (phase >= 1)
- Repeats decrement on trigger (unless -1).

---

## Voice Envelope

Each voice is a simple AR slope:
- On trigger: set target level from MODE/RUN, start attack.
- Attack/decay times derived from TIME and INTONE:
  - baseMs = map(TIME)
  - voiceScale = pow(2, INTONE * (i - 3.5)/5)  // spread
  - voiceMs = baseMs * voiceScale
- RAMP sets attack/decay split, CURV shapes slope.

---

## Physics (MODE + RUN)

n shape, Just Type inherits it's functionality from the standard mode. However, atop it sits a rhythmic engine for polymetric & -phasic patterns. Fundamentally this is a 'clocked' mode, whether internally so or via a continuous tick. The TIME & INTONE controls maintain their standard free-running influence, speeding up and slowing down envelopes, while the rhythms are controlled remotely.

Notes in Geode are a combination of a standard trigger along with a number of repeats & a rhymthic division. The former sets the number of envelope events to create, while the latter chooses the rhythmic relation of those repeats to the core timebase. The MODE switch selects how the amplitudes of repeated elements change over time. These changes are further modified by the RUN jack for fluid rhythmic variation under voltage control. These undulations are highly interactive with the TIME & INTONE controls, where the different MODE settings will handle overlapping repeats in drastically different ways. Start with TIME set very fast, then dial it back to hear how the repeats entangle.

Once these rhythmic streams are moving, their pattern can be corralled into a set of quantized steps. Using odd-subdivisions for notes, with even quantize, will enable patterns to break out of the evenly-spaced-repeats model. Try prime numbers (5/7/11) for divisions, but 4/8/16 for quantize to create traditional syncopated rhythms.

Geode: Transient
When set to transient, each repeat will have the full velocity (or a reduced one set by VTR / vtrigger()).

RUN voltages will introduce a rhythmic variation every n repeats. At 0V, every note is emphasized (hence sounding static). Increasing a little and every 2nd note is emphasized. Further and a cycle of 3 velocities is introduced, and so on up to a cycle of 10 notes. The velocities decrease in a 'sawtooth' pattern.

With negative voltages, the same cycles are introduced, however the pattern is reversed, dropping the volume at first, then rising up over n repeats.

Regular syncopated rhythms are great here. Try 8 repeats, triggered every 8 clocks and choose a rhythm with RUN.

Geode: Sustain
In sustain repeats decay to silence over the duration of repeats.

By adding a RUN voltage the rate of decay can be modified. At 0V it takes exactly repeats to fade away. As RUN increases the repeats fade more quickly, however they will reflect back up when hitting the minimum. With around 1V the repeats will decay to near zero, then back to full volume by the last repeat, creating a triangle shape. Further increasing this level 'folds' into multiple waves per set of repeats.

Negative values slow the decay rate, making the fade out effect more and more subtle. At -5V the amplitudes are almost uniform.

Creative use of this behaviour can introduce a third temporal element to the Geode equation: TIME & INTONE set a base envelope rate, divisions sets the rhythm of notes, and RUN sets the amplitude cycle relative to repeats.

This mode is useful for creating pseudo-delay envelopes.

Geode: Cycle
cycle introduces a complex, repeats sensitive amplitude cycling. The rhythm is generated similarly to transient, however the variation is applied continuously rather than in single steps of beats.

Applying RUN voltage emphasizes every 2nd then 3rd then 4th event, however all the in-between beats are available too. Subtle CV shifts allow for a variation of groove with nothing but volume manipulation.

Negative RUN levels emphasize every fraction of a beat, which is a hard thing to think about, let alone control. As the voltage becomes lower the rhythmic cycles change more rapidly, starting to feel random. This zone is great to explore if you want to introduce some unpredictability into a rhythmic pattern.

This mode is a source of endless subtle movement and works extra well with QT / quantize() active.

RUN returns a per-trigger gain in [0..1].

- MODE 0 (Transient): accent mask.
  - mask = (stepIndex % map(RUN, 1..8) == 0)
  - gain = mask ? 1.0 : 0.3

- MODE 1 (Sustain): decay per repeat.
  - damp = map(RUN, 0.05..0.25)
  - gain = pow(1.0 - damp, repeatIndex)

- MODE 2 (Cycle): amplitude LFO across burst.
  - rate = map(RUN, 1..4)  // cycles per burst
  - gain = 0.5 + 0.5 * sin(phaseInBurst * rate * 2pi)

---
Percussive Timebase
Geode needs a timebase from which to calculate the rates for the envelope sequences. This base can be set with a continuous stream of events (ie a clock) useful for when you need to synchronize the events to other elements. Alternatively a simple beats-per-minute value can be used if Just Friends is free running and doesn't need to play in time with others.

JF.TICK divisions / ii.jf.tick( divisions )
Clock Geode with a stream of divisions ticks per measure.

divisions

Tells Just Type how many tick messages will be received per measure, where a measure is 4 beats.
1 to 48 ticks per measure are allowed
4 means 1 tick per beat
0 acts a reset to synchronize to the start of the measure
Typically JF.TICK / tick() will be called in a METRO. For 60 bpm, you can send JF.TICK 4 once per second, or JF.TICK 8 twice per second etc. Once you are comfortable using it in a standard way, the divisions value can be modulated to create rhythmically related clock multiplications and divisions of the repeats.

JF.TICK bpm / ii.jf.tick( bpm )
Set timebase for Geode with a static bpm.

bpm

Number of beats per minute where a measure is 4 beats.
Must be between 49 and 255 bpm.
0 acts a reset to synchronize to the start of the measure
JF.TICK (proposed) / ii.jf.get( 'tick' )
Returns the current Geode tempo in beats per minute.

Individual Rhythms
JF.VOX channel divs repeats / ii.jf.play_voice( channel, divs, repeats )
Create a stream of rhythmic envelopes on the named channel. The stream will continue for the count of repeats at a rhythm defined by divs.

channel

select the channel to assign this rhythmic stream
0 sets all channels
divs

Divides the measure into this many segments
4 creates quarter notes
15 creates 15 equally spaced notes per bar (weird!)
repeats

Number of times to retrigger the envelope
-1 repeats indefinitely
0 will still create the initial trigger but no repeats
1 will make 2 events total (the initial trigger, and 1 repeat)
Round-Robin Rhythms
JF.NOTE divs repeats / ii.jf.play_note( divs, repeats )
Works as JF.VOX / play_voice() but with dynamic allocation of channel. Assigns the rhythmic stream to the next channel.

divs

Divides the measure into this many segments
4 creates quarter notes
15 creates 15 equally spaced notes per bar (weird!)
repeats

Number of times to retrigger the envelope
-1 repeats indefinitely
0 will still create the initial trigger but no repeats
1 will make 2 events total (the initial trigger, and 1 repeat)
Time Quantization
JF.QT divisions / ii.jf.quantize( divisions )
Quantize Geode events to divisions of a measure.

When non-zero, all events are queued & delayed until the next quantize event occurs. Using values that don't align with the division of rhythmic streams will cause irregular patterns to unfold.

division

delay all events until this division of the timebase
0 deactivates quantization
1 to 32 sets the subdivision & activates quantization
If you need your rhythms to stay on a regular grid, activate that grid with Quantization. By setting a regular quantization (try 8 or 16) you can experiment with irregular divisions when triggering VOX or NOTE (try 7, 11, 13, 15) and those repeats will be locked into the quantized grid. Couple this with dynamic control over RUN and you have a very powerful groove generator with a few high level controls. Instant percussion inspiration!

While it uses a different implementation, this functionality can create Euclidean rhythms, though 'rotating' the rhythms requires delaying the VOX or NOTE calls.

JF.QT (proposed) / ii.jf.get( 'quantize' )
Returns the number of divisions quantize is currently set to.

Teletype Reference
Set values or call actions:
JF.TR channel state Set trigger channel to state
JF.RMODE mode Set RUN state to mode
JF.RUN volts Set RUN voltage to volts
JF.SHIFT volts Transpose frequency / speed by volts (v/8)
JF.VTR channel level Trigger channel with velocity set by level
JF.TUNE channel numerator denominator Alter the INTONE relationship to IDENTITY
JF.MODE state Activates Synthesis or Geode

Synthesis

JF.NOTE pitch level Play a note, dynamically allocated to a voice
JF.VOX channel pitch level Play a note on a specific voice
JF.GOD state If state, retune to A=432Hz (default A=440Hz)

Geode

JF.NOTE divs repeats Play a sequence, dynamically allocated to a channel
JF.VOX channel divs repeats Play a sequence on a specific channel
JF.TICK divs Clock Geode with a stream of ticks at divs per measure
JF.TICK bpm Set timebase for Geode with a static bpm
JF.QT divs Quantize Geode events to divs of the timebase

Proposed commands, yet to be implemented as of Teletype 3.2

JF.ADDR index Set all connected Just Friends to ii address index
JF.PITCH channel pitch Same as JF.VOX but doesn't trigger the envelope
## Output

- Sum all 6 voice outputs.
- MIX jack behavior: Each slope's output value is divided by its index. The MIX jack then outputs the largest of the resulting values (analog max or "OR"). IDENTITY is divided by 1 (unaffected), 2N is divided by 2, 3N divided by 3, and so on. This provides a unique modulation source otherwise requiring a large patch.
- Expose mixed output as GEO.VAL (0..16383 raw or -5..+5V via CV op).

---

## Teletype Ops (minimal set)

- GEO.TIME x
- GEO.TONE x
- GEO.RAMP x
- GEO.CURV x
- GEO.RUN x
- GEO.MODE x
- GEO.DIV i x
- GEO.REP i x
- GEO.VAL

Optional:
- GEO.READ i (voice value) if needed for debug

---

## Teletype Routing (per-voice outs)

If Geode lives inside Teletype, individual voices can be routed to TT CV outputs via scripts:

- Example (voice 1 to CV1, voice 2 to CV2):
  - CV 1 GEO.READ 1
  - CV 2 GEO.READ 2

Notes:
- TT has only 4 CV outputs, so at most 4 voices can be routed directly at once.
- Remaining voices can be mixed via GEO.VAL or sent to BUS.

Optional op for convenience:
- GEO.OUT i n  // assign voice i to CV out n (1..4)

---

## Implementation Outline

- Add GeodeEngine (pure C++) with:
  - voice phase, repeat counters, AR state
  - update(dt, measureFrac)
  - output()
- TeletypeTrackEngine holds one GeodeEngine.
- TeletypeBridge exposes setters/getters for ops.

---

## JF Operations Reuse Strategy

The following existing JF operations can be extended to support Geode functionality:

- `JF.VOX channel divs repeats`: In Geode mode, interprets parameters as (channel, divs, repeats) instead of (channel, pitch, velocity)
- `JF.NOTE divs repeats`: In Geode mode, interprets parameters as (divs, repeats) for dynamic voice allocation
- `JF.MODE`: Already exists to switch between Synthesis and Geode modes
- `JF.RMODE` and `JF.RUN`: Control the RUN voltage behavior in Geode modes
- `JF.TICK`: Sets the underlying timebase of Geode
- `JF.QT`: Applies quantization to Geode events

This eliminates the need for separate `GEO.DIV` and `GEO.REP` operations, maximizing reuse of existing infrastructure.

---

## Notes / Constraints

- Control-rate only (1 kHz). Not audio-rate.
- No new clock: lock to engine measure/transport for consistency.
- Keep memory small (static arrays, no heap).
