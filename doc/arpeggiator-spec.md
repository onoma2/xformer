# Arpeggiator Track spec Version 0.1

## 1. Class shape
**New TrackMode trio:** `ArpeggiatorTrack` (Model), `ArpeggiatorSequence` (Model), `ArpeggiatorTrackEngine` (Engine).
**Precedent:** Follows the standard `NoteTrack` and the newly drafted `HarmonyTrack` trio pattern.
**Why:** Currently, the `ArpeggiatorEngine` is locked inside pitched tracks (like `NoteTrack`). Extracting it to a dedicated track transforms it into a global CV/Gate processor. It can read polyphonic CV from a `HarmonyTrack` or a multi-jack ADC input and output a monophonic arpeggiated CV/Gate stream, allowing any signal to be arpeggiated.

## 2. Grid / traversal
**Standard 16-Step Grid:** Identical to `NoteTrack`.
**Cursor Model:** The track has two distinct cursors:
1.  **Sequence Cursor:** Advances at the track's master clock divisor to read step data (changes the arpeggiator's behavior per step).
2.  **Arpeggiator Cursor (Engine Transient):** Advances at the specific rate dictated by the active step's `Arp Rate` parameter, iterating through the incoming polyphonic chord/CV array.

## 3. Timing model
**Per-Tick Derivation:** The core sequence advances based on the clock divisor. The actual gate/CV emissions are derived from the `Arp Rate` and the number of active input voices.
**Hold/Idle State:** If no CV source is active or gate is low, it behaves according to the `Hold` mode (either going silent, or sustaining the last known chord memory to continue arpeggiating).

## 4. Identity / storage model
**Stored:** Step-level parameters (Arp Mode, Arp Rate, Octave Range, Gate Length), CV Source selection (Track/Output mapping).
**Derived:** Monophonic CV and Gate output.
**Engine Transient:** `ArpeggiatorEngine` instance, a buffer of the currently latched input CVs (the "Chord Memory"), and the high-speed Arp Cursor.

## 5. Per-cell / per-step record
**Bit-packed per-cell `ArpeggiatorSequence::Step`:**
*   `Mode` (4 bits, 0-15: Up, Down, Up/Down, Random, As-Played, etc.)
*   `Rate` (4 bits, 0-15: LUT index for clock divisors e.g. 1/8, 1/16, 1/32, triplets)
*   `Octave Range` (2 bits, 0-3: 1 to 4 octaves)
*   `Gate Length` (3 bits: Tie, 75%, 50%, 25%, Trigger)
*   `Hold/Defeat` (1 bit: toggle to ignore input changes or bypass arp for this step)
*   **Total:** 14 bits (fits cleanly into a 16-bit word).

## 6. Curve math
*N/A - Primarily deals with discrete CV array indexing and timing.*

## 7. Accumulator
*N/A - Though a global accumulator could be routed to modulate the Arp Rate or Octave Range.*

## 8. Transforms
*N/A - Standard track shift/rotate transforms apply to the 16-step grid.*

## 9. Outputs
**Jack count:** 1 CV and 1 Gate pair (Monophonic output).
**Mapping Rule:** User assigns the track to physical CV/Gate jacks.
**Quantization:** Engine outputs exact 1V/Oct floats based on the input CVs. (Assumes input is pre-quantized, or applies a global project scale post-arp).

## 10. Modulation inputs
**Global Routing:** Step parameters (`Mode`, `Rate`, `Octave Range`) must be added to `Routing::Target` so they can be modulated by LFOs, Accumulators, or incoming CV.

## 11. Performer integration
**Mute:** Forces 0V/Gate Low or holds last note depending on setting.
**Snapshot/Pattern:** Participates normally. Switching patterns switches the active `ArpeggiatorSequence`, instantly changing the step data (e.g., jumping from an Up arp at 1/16 to a Random arp at 1/32).

## 12. Performer alignment
**Borrow Pattern:** Follows `Stochastic` and `Harmony` borrow pattern. Copies `_playMode`, `_fillMode` verbatim from `NoteTrack`.

## 13. Retro lesson gates
*   **Identity Model First:** Output is purely derived from source CV array + step recipe + fast clock pulse.
*   **Container Ceilings:** Must verify `sizeof(ArpeggiatorSequence)` is less than `sizeof(NoteSequence)` (fits easily).

## 14. Spec open questions
*   **Q1: Input Resolution.** How does the engine grab the "chord"? (Decision: The track setup page allows routing up to 4 virtual CV sources or taking a direct link from a `HarmonyTrack`).
*   **Q2: Sorting.** How does "Up" mode know which voltage is lowest? (Decision: The engine must read the raw float CV inputs, sort them numerically in transient memory, and then apply the arp cursor).

## 15. Deferred to v2
*   Programmable rhythm patterns for the arpeggiator (instead of straight 1/16ths).
*   Ratchet integration within the arp cursor.

## 16. Engineering punch list
*   Rip `ArpeggiatorEngine` out of `NoteTrackEngine.cpp` and `MidiCvTrackEngine.cpp`.
*   Create `model/ArpeggiatorTrack.h`, `model/ArpeggiatorSequence.h`, `engine/ArpeggiatorTrackEngine.h`.
*   Refactor the `Arpeggiator` math class to accept a generic array of floats instead of relying on `NoteSequence` state.
*   Add new track type to `Config.h` and `ProjectVersion.h`.

## 17. UI surfaces
*   **EditPage:** Standard 16-step grid. Top row selects step, encoder changes Mode/Rate/Range/Gate.
*   **SequencePage:** List view of the sequence parameters.
*   **TrackPage:** Setup view. Needs new fields: `CV Source 1-4` (Track routing), `Output Map`.
*   Wire `setSequenceEditPage` and `setSequenceView` in `TopPage.cpp`.

## 18. Testing & MVP acceptance
*   **Phase A:** Compiles. `ArpeggiatorTrack` fits in memory.
*   **Phase B:** Engine reads a static array of 3 voltages and outputs an Up sequence at 1/16th.
*   **Phase C:** Track receives 4 CVs from a `HarmonyTrack`, outputs monophonic arpeggio following pattern step locks.
*   **Done Gate:** Verify no regressions in original projects that used the old NoteTrack arp (migrated or deprecated cleanly).