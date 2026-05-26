# Harmony Track spec Version 0.1

## 1. Class shape
**New TrackMode trio:** `HarmonyTrack` (Model), `HarmonySequence` (Model), `HarmonyTrackEngine` (Engine).
**Precedent:** Follows the standard `NoteTrack` / `CurveTrack` trio pattern.
**Why:** Harmony logic is currently tightly coupled to `NoteSequence`. Extracting it to a dedicated track makes it a global CV processor, natively solving per-pattern changes and per-step micro-sequencing (parameter locking) without bloating other sequence models.

## 2. Grid / traversal
**Standard 16-Step Grid:** Identical to `NoteTrack`.
**Cursor Model:** Follows standard clock divider and reset measure logic. Steps advance sequentially or randomly (if play mode allows) to lookup the current step's harmony rules.

## 3. Timing model
**Per-Tick Derivation:** No internal duration logic (unlike `NoteTrack`). Timing strictly follows the track's clock divisor to advance steps.
**Strumming & Queues:** Output is polyphonic. If a `Strum` parameter is active, the engine does not output all 4 chord voices instantly. Instead, it pushes the CV and Gate updates for Voice 1, Voice 2, Voice 3, and Voice 4 into a `SortedQueue` with staggered absolute tick offsets (e.g., Up-Strum: Voice 1 at `tick+0`, Voice 2 at `tick+10`, etc.).
**Idle State:** If no CV source is routed, or the source CV hasn't changed, the engine idle-holds its previous outputs. Output updates only occur when either (A) the CV source changes, or (B) the sequence advances to a new step with different harmony locks.

## 4. Identity / storage model
**Stored:** Step-level parameters (Quality Override, Mode, Inversion, Voicing), Pattern-level overrides, CV Source selection, CV Output target mapping, Strum Amount/Direction.
**Derived:** The actual output voltages. The engine applies the stored harmony rules to the live CV source to derive up to 4 pitch outputs.
**Engine Transient:** `HarmonyEngine` instance, previous source CV value (for edge-detection), and independent `_gateQueue` / `_cvQueue` structures to handle polyphonic strumming offsets.

## 5. Per-cell / per-step record
**Bit-packed per-cell `HarmonySequence::Step`:**
*   `Quality Override` (6 bits, 0-63: `Auto` (Diatonic), or absolute Amalgamated dictionary lookups e.g., `M7add13`, `sus24`).
*   `Mode` (3 bits, 0-7: Ionian to Locrian, + Off)
*   `Inversion` (2 bits, 0-3: Root, 1st, 2nd, 3rd)
*   `Voicing` (2 bits, 0-3: Close, Drop2, Drop3, Spread)
*   `Transpose` (Signed 7-bit, -24 to +24 semitones)
*   `Gate` (1 bit, optional, to pass-through or choke the source gate)
*   **Total:** 21 bits (fits within a 32-bit word, leaving room for future expansion).

## 6. Curve math
*N/A - Harmony relies on interval lookup tables, not continuous curves.*

## 7. Accumulator
*N/A - No accumulators required for MVP pitch processing.*

## 8. Transforms
*N/A - Standard track shift/rotate transforms apply to the 16-step grid.*

## 9. Outputs
**Jack count:** Up to 4 CV/Gate pairs (polyphonic output).
**Mapping Rule:** The user maps the 4 virtual voices (Voice 1, Voice 2, Voice 3, Voice 4) to the track's 4 physical CV output assignments. (Note: These are named generic "Voices" rather than "Root/3rd/5th/7th" to accommodate absolute extended/suspended chords that do not follow standard tertiary structures).
**Quantization:** Engine outputs exact 1V/Oct floats. Assumes source CV is already quantized (or engine snaps it to the nearest scale degree before harmonizing).

## 10. Modulation inputs
**Global Routing:** Step parameters (`Inversion`, `Voicing`, `Quality Override`, `Mode`) must be added to `Routing::Target` so they can be modulated by incoming CV or internal `CurveTracks`.

## 11. Performer integration
**Mute:** Forces 0V or holds last note depending on setting.
**Snapshot/Pattern:** Participates normally. Switching patterns switches the active `HarmonySequence`, instantly changing the step data and chord progressions.

## 12. Performer alignment
**Borrow Pattern:** Follows `Stochastic` borrow pattern. Copies `_playMode`, `_fillMode` verbatim from `NoteTrack`.

## 13. Retro lesson gates
*   **Identity Model First:** Output is purely derived from source CV + step recipe.
*   **Container Ceilings:** Must verify `sizeof(HarmonySequence)` is less than `sizeof(NoteSequence)` (currently the largest, so it should fit cleanly in the `_container` union without bloating RAM).

## 14. Spec open questions
*   **Q1: CV Source Representation.** How does the engine identify its input? (Decision: Use the existing `RoutingEngine` or an internal track-pointer to read another track's logical `cvOutput()`).
*   **Q2: Gate Passthrough.** Does the Harmony track generate its own gates, or just copy the master track's gate to all 4 chord outputs? (Decision: Copy master gate, allowing the step `Gate` bit to choke specific chord steps).

## 15. Deferred to v2
*   Algorithmic chord progressions (generating modes automatically).
*   Arpeggiator integration (arpeggiating the generated chord across the outputs).

## 16. Engineering punch list
*   Rip `HarmonyEngine` instantiation out of `NoteTrackEngine.cpp`.
*   Remove harmony fields/enums from `NoteSequence.h`.
*   Create `model/HarmonyTrack.h`, `model/HarmonySequence.h`, `engine/HarmonyTrackEngine.h`.
*   Add new track type to `Config.h` and `ProjectVersion.h`.

## 17. UI surfaces
*   **EditPage:** Standard 16-step grid. Top row selects step, encoder changes Inversion/Voicing/Quality Override.
*   **SequencePage:** List view of the sequence parameters.
*   **TrackPage:** Setup view. Needs new fields: `CV Source` (Track 1-8), `Output Map` (Voice 1->CV1, Voice 2->CV2, etc.), and `Strum` (bipolar value for Up/Down strum delay).
*   Wire `setSequenceEditPage` and `setSequenceView` in `TopPage.cpp`.

## 18. Testing & MVP acceptance
*   **Phase A:** Compiles. `HarmonyTrack` instances fit in memory unions.
*   **Phase B:** Basic Passthrough. Track receives CV from NoteTrack 1, passes it out to CV 2 unaltered.
*   **Phase C:** Harmonization. Track receives C4, outputs C4, E4, G4, B4 to four CV outputs based on Step 1 locks.
*   **Done Gate:** Existing `NoteTrack` harmony tests are rewritten to pass using the new `HarmonyTrackEngine`.