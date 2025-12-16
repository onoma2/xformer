# Resource Analysis: Per-Track Min/Max Implementation

## Objective
Implement per-track control over the Minimum and Maximum range values in the Routing system. This allows a single modulation source (e.g., CV In 1) to control the same parameter (e.g., Filter Cutoff) on multiple tracks with different scaling and offset settings.

## Constraints
*   **Memory:** `Route` object size increase must be acceptable.
*   **Performance:** `RoutingEngine` overhead must be minimal.
*   **UI:** Minimize changes to existing UI classes (`RouteListModel`, `RoutePage`).
*   **Compatibility:** Backward compatibility is NOT required.

## Implementation Options

### Option 1: Array-Based Replacement (Recommended)
Replace the global scalar `_min` and `_max` fields with fixed-size arrays for each track.

**Data Structure Changes:**
```cpp
// Old
float _min;
float _max;

// New
std::array<float, CONFIG_TRACK_COUNT> _min;
std::array<float, CONFIG_TRACK_COUNT> _max;
```
*   **Memory Impact:** Adds ~56 bytes per Route. For 20 routes, ~1.1KB total. Negligible.

**Logic:**
*   **Engine:** Iterates through enabled tracks. Calculates `value = min[i] + src * (max[i] - min[i])` for each track `i`.
*   **UI Workflow:**
    *   **Display:** Shows the value of the *lowest indexed enabled track*. If multiple enabled tracks have different values, can optionally display a "Mixed" indicator (e.g., `~`).
    *   **Editing:** When `Min` or `Max` is edited, the new value is written to **ALL currently enabled tracks** in the `_tracks` bitmask.
    *   **Achieving Divergence:** To set different values:
        1.  Set `Tracks` to only Track 1. Adjust Min/Max.
        2.  Set `Tracks` to only Track 2. Adjust Min/Max.
        3.  Set `Tracks` to Track 1 & 2. (Now both are active with independent settings).

**Pros:**
*   Cleanest data model. "What you see is what you get" (per track).
*   Engine logic remains simple linear interpolation.
*   Direct mapping to "Per Track Range".

**Cons:**
*   Breaking change for serialization.
*   UI "Edit All Enabled" workflow is slightly modal but functional without new UI elements.

### Option 2: Global + Offsets (Hybrid)
Keep the Global Min/Max as a "Master Range" and add per-track offsets.

**Data Structure Changes:**
```cpp
float _min; // Master Min
float _max; // Master Max
std::array<float, CONFIG_TRACK_COUNT> _minOffset;
std::array<float, CONFIG_TRACK_COUNT> _maxOffset;
```

**Logic:**
*   `EffectiveMin[i] = _min + _minOffset[i]`
*   `EffectiveMax[i] = _max + _maxOffset[i]`

**Pros:**
*   Backward compatible (offsets default to 0).
*   Allows "Macro" control (moving Master Min shifts everything).

**Cons:**
*   **Cognitive Load:** Hard to calculate final values mentally ($0.2 + -0.1 = 0.1$).
*   **UI Complexity:** Needs new UI rows for "Offsets" or a deep menu to access them. Without new UI, the feature is inaccessible.

### Option 3: Global + Attenuator (VCA Style)
Keep Global Min/Max, add a per-track "Scale" factor.

**Data Structure Changes:**
```cpp
float _min;
float _max;
std::array<float, CONFIG_TRACK_COUNT> _depth; // 0.0 to 1.0
```

**Logic:**
*   `Value[i] = _min + (Source * _depth[i]) * (_max - _min)`

**Pros:**
*   Simple concept: "How much modulation for Track X?"

**Cons:**
*   **Limited:** Cannot change the *Offset* (start point). Only the *Amount*.
*   If you need Track 1 to sweep 0-50% and Track 2 to sweep 50-100%, this option fails. Option 1 succeeds.

### Other Patterns (if memory/UX tradeoffs change)
- **Per-track clip zones:** Keep global min/max but add per-track clamp windows (low/high inside the global range). Very small per-track storage; simple UI.
- **Per-track attenuvert/polarity:** Scalar per track in [-1, 1] to flip/invert and set depth. Even cheaper than offsets; covers “amount + invert.”
- **Per-track curve/LUT:** Small fixed curve (e.g., 4–8 control points) that remaps 0–1 before applying global min/max. More expressive response shaping for modest RAM.
- **Per-track quantizer:** Snap the remapped value to a small table (steps/scale). Good for musical modulation; per-track table of a few entries.
- **Per-track time smear:** Per-track slew/lag/delay on the routed value. Almost no storage; adds a one-pole filter per track for differentiated feel without range changes.
- **Preset buckets:** A small bank of (min, max, response) presets; each track stores only an index. Lowest per-track cost at the expense of flexibility.
- **Per-track bias/depth (implemented):** Integer bias and depth per track (-100..100) applied after master min/max to shift and scale the mapped value per track.
  - **Math:** `base = min + src * span; value = clamp(mid + (base - mid) * depthPct/100 + span * biasPct/100, min, max)`.
  - **Model:** Route stores per-track bias/depth arrays with serialization version 57.
  - **Engine:** RoutingEngine applies bias/depth per track for per-track targets.
  - **UI:** Page+S5 overlay on Routes shows two-line blocks (`B %+d Tn` / `D %+d`), fixed columns, F0–F3 select track pairs/layers, encoder edits, F4 commit, Page+S5 cancel, context menu (Init/Random/Copy/Paste), marker on track bitmask for non-default shaping.

## Technical Recommendation
**Proceed with Option 1.** It offers the most flexibility (full range control per track) with the cleanest architecture. The UI workflow (Edit applies to enabled tracks) provides a workable solution without requiring a "Selected Track" UI widget.
