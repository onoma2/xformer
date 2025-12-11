# Performer Phazer Project Status

## Current Session: Gate Offset Logic Restoration & Scaler Implementation (Dec 5, 2025)

### Completed Features

**✅ Gate Offset (Micro-Timing) Implementation**
- **Algorithmic Generation**: Restored unique timing "groove" logic for 7 algorithms based on original backup code.
- **Positive Shift Strategy**: Normalized all algorithmic offsets to a safe, positive `0-100%` range to ensure compatibility with the engine's delay-based timing system.
- **Scaler Model**: Implemented a user-controllable scaling logic (`scaled = (algo * user * 2) / 100`), allowing the `Gate Offset` knob to fade from Quantized (0%) to Unity (50%) to Exaggerated (100%).
- **UI Integration**: Added `Gate Offset` (GOFS) parameter to Page 3 of the Tuesday Edit menu, replacing the unused `CvUpdateMode`.
- **Default Behavior**: Set default `gateOffset` to **0%**, ensuring new patterns start "On Beat" until the user explicitly dials in the groove.

### Algorithm-Specific Timing Logic (Restored)

| Algorithm | ID | Logic Restored | Range (Approx) | Character |
|-----------|----|----------------|----------------|-----------|
| **TRITRANCE** | 1 | ✅ Yes | 0-10% (Low), 25-35% (Mid), 40-50% (High) | Heavy rolling swing |
| **APHEX** | 3 | ✅ Yes | 0-88% | Complex polyrhythmic drift |
| **AUTECHRE** | 4 | ✅ Yes | 0-10% | Subtle random jitter |
| **STEPWAVE** | 5 | ✅ Yes | 0-6% | Micro-jitter |
| **MARKOV** | 6 | ✅ Yes | 0-25% | Random push/pull |
| **CHIPARP 1** | 7 | ✅ Yes | 0-6% | Fixed pattern offset |
| **WOBBLE** | 9 | ✅ Yes | 0-30% | Phase-based drift |
| **STOMPER** | 2 | ❌ No | 0% | Tight (by design) |
| **CHIPARP 2** | 8 | ❌ No | 0% | Tight (no backup logic found) |
| **SCALEWALKER** | 10 | ❌ No | 0% | Tight (by design) |

### Technical Implementation Details

**Scaler Logic (`TuesdayTrackEngine::tick`)**
```cpp
// Scaler Model: 
// - User Knob 0%   -> Force 0 (Strict/Quantized)
// - User Knob 50%  -> 1x (Original Algo Timing)
// - User Knob 100% -> 2x (Exaggerated Swing)
int userScaler = sequence.gateOffset();
_gateOffset = clamp((int)((result.gateOffset * userScaler * 2) / 100), 0, 100);
```

**Safety Measures:**
- **Clamping**: All offsets strictly clamped to `0-100%` to prevent engine overflows or queue errors.
- **Normalization**: Negative offsets from original algorithms were shifted positively to fit the engine's constraints (since we can't delay "backwards" in time without latency).

### Testing Verification

**Manual Testing Strategy:**
1.  **Init Pattern:** Verify `GOFS` is 0 and playback is tight.
2.  **Algo 1 (TRITRANCE):** Turn `GOFS` to 50%. Verify "High Note" lags significantly.
3.  **Algo 3 (APHEX):** Verify complex timing variations.
4.  **Extreme Settings:** Turn `GOFS` to 100%. Verify no crashes or dropped notes (though timing will be extreme).

**Build Status:**
- ✅ Simulator builds successfully.
- ✅ Zero errors.

---

## Previous Session: SCALEWALKER Implementation (Dec 4, 2025)

*   **New Algorithm:** SCALEWALKER (ID 10) implemented with polyrhythmic subdivisions.
*   **Refactor:** Unified subdivision calculation across algorithms.
*   **Fixes:** Fixed walker persistence, beat detection, and gate leakage bugs.

---