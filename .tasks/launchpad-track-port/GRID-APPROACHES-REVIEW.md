# Grid Approaches from /temp-ref/grid — Review for Launchpad

> Date: 2026-05-17  
> Sources: `midigrid/`, `kria/`, `n.kria/`, `meadowphysics/`, `libavr32/`  
> Focus: What can help the Launchpad Mini MK1 implementation

---

## 1. midigrid — MIDI Grid Abstraction Layer

### 1.1 Buffer-Then-Blast MIDI Optimization — NOT ADOPTED (premise doesn't hold here)

> Superseded by the 2026-07-04 feasibility review. Kept for the record; do not implement. Also removed from PHASEDPLAN E0 (was E0.6 + E0.11).

**What midigrid does:** accumulates all LED changes in a Lua table and sends them as one `midi:send()` on refresh.

**Why it doesn't port:** the xformer TX path already batches. `LaunchpadDevice::sendMidi` enqueues into a 128-entry `MidiMessage` ring buffer (`src/platform/stm32/drivers/UsbMidi.h:18-24,96`) that drains asynchronously — there is no bulk-transfer / `send_bulk` API at the device-class layer, and `syncLeds()` already fills that ring in one pass (`LaunchpadDevice.cpp:52-84`). The proposed `uint8_t buf[256]` scratch buffer only served this idea and would add .bss against the RAM budget the plan polices, for no measurable gain. On MK1 there is no SysEx LED protocol either.

### 1.2 Config-Driven Device Abstraction (Capability Flags)

**What it does:** Each device config declares a `caps` table:
```lua
caps = {
    sysex = true,          -- supports sysex LED updates
    rgb = true,            -- full RGB
    lp_double_buffer = false,
    cc_edge_buttons = true
}
```

**Current approach:** Hard-coded `if (info.productId == 0x0069)` branching in `LaunchpadController.cpp`.

**How we can adopt it:** Add a `DeviceCapabilities` struct to each `LaunchpadDevice` subclass:
```cpp
struct DeviceCapabilities {
    bool supportsRgbSysex;
    bool supportsDoubleBuffer;
    bool ccEdgeButtons;
    uint8_t maxBrightnessLevels;
};
```

This replaces runtime PID checks with feature flags. Cleaner, more maintainable, and easier to add new devices.

**Effort:** ~30 minutes.

### 1.3 Brightness Ladder for Non-RGB Devices

**What it does:** Maps monome 0-15 brightness to a small set of device velocities:
```lua
brightness_handler = function(val)
    if val == 0 then return 0
    elseif val < 8  then return 1   -- green
    elseif val < 12 then return 3   -- yellow
    elseif val < 16 then return 5   -- red
    else return 0 end
end
```

**How we can help MK1:** The Mini MK1 only has red/green with 4 brightness levels each. Currently we use a 4×4 `mapColor(red, green)` table. We could adopt a **brightness ladder** approach for simple indicators:
- Off = 0
- Dim = low red/green
- Medium = medium red/green/amber
- Bright = full red/green/yellow

This is more intuitive than manually picking red/green combinations. Could be added as an optional `BrightnessLadder` color mode for MK1.

**Effort:** ~30 minutes.

### 1.4 Virtual Grid / Paging for Larger Grids

**What it does:** `mg_128.lua` and `mg_256.lua` create virtual grids larger than 8×8 by using "quads" (pages). An internal `grid_buf` holds the full virtual state, while `views[quad]` maps subsets to physical MIDI notes.

**How we can adopt it:** Our IndexedTrack (48 steps) and DiscreteMapTrack (32 stages) already do pagination manually. We could formalize this into a `VirtualGrid` helper:
```cpp
template<int VirtualCols, int VirtualRows>
class VirtualGrid {
    uint8_t gridBuf[VirtualCols][VirtualRows];
    int viewOffset = 0;  // which 8-column page is visible
    void setLed(int virtualCol, int row, uint8_t brightness);
    void refresh(ViewPort viewport);  // maps visible subset to physical LEDs
};
```

This would centralize the pagination logic instead of duplicating it in every track type.

**Effort:** ~2 hours. Reusable across Indexed, DiscreteMap, and any future >8-step track.

### 1.5 LED Double-Buffering for Launchpad

**What it does:** Launchpad configs support a double-buffer sysex toggle that flips display buffers atomically:
```lua
display_double_buffer_sysex = function(self)
    if self.current_display_buffer == 0 then
        return tab.split('0xB0, 0x00, 0x31', ', ')
    else
        return tab.split('0xB0, 0x00, 0x34', ', ')
    end
end
```

**How we can adopt it:** The Launchpad Pro and MK3 support double-buffered LED updates in programmer mode. We could draw to the back buffer, then flip atomically — eliminating visual tearing during mode switches.

**Impact:** Visual polish only. Not critical for MK1 (no double buffer support).

**Effort:** ~1 hour for MK2+/Pro.

---

## 2. Kria / n.kria — Parameter-Lock Sequencer

### 2.1 Bottom Row as Control Row

**What it does:** Kria dedicates the bottom row (y=7 or y=8) to track select, page select, and modifier keys. The upper 7 rows are the parameter editing field.

**Current approach:** Our Launchpad uses the top function row (CC 104-111) and right scene column for controls, leaving the full 8×8 grid for data.

**How we can learn from it:** Kria's control-row pattern is elegant because it's always visible. Our current approach separates controls to the edges (function row + scene column), which is good for maximizing the data grid. However, we could borrow Kria's **modifier key concept** for the function row:
- **Hold Layer** = page/modifier mode (like Kria's page select)
- **Hold Shift + Function** = modifier overlay (like Kria's Loop/Time/Prob mods)

This is already partially implemented. The insight from Kria is that **modifiers should be hold-based, not toggle-based**, for faster access.

### 2.2 Loop Mod: Hold + Two-Tap Loop Set

**What it does:** Hold the **Loop** modifier, press first step, press second step → sets loop start/end. Releasing Loop restores original loop if temporary.

**Current approach:** We have separate FirstStep and LastStep buttons that show a picker grid.

**How we can adopt it:** Add a **Loop modifier** (e.g., hold Shift + FirstStep) that enables two-tap loop setting directly on the grid. This is faster than the current picker approach:
```
Hold Loop → tap step 3 → tap step 12 → loop = 3..12
```

This maps perfectly to Note/Indexed/DiscreteMap tracks. Could be added as an **alternative** to the existing FirstStep/LastStep buttons, not a replacement.

**Effort:** ~1 hour.

### 2.3 Probability as a First-Class Modifier

**What it does:** Kria has a dedicated **Prob** modifier key. Holding it overlays the current page with probability dots (4 levels: 0%, 25%, 50%, 100%).

**Current approach:** Probability is a layer in the layer map (GateProbability, RetriggerProbability, NoteVariationProbability).

**How we can adopt it:** Add a **Probability overlay mode** (hold Shift + Layer, or dedicate a new modifier). While held, the grid shows probability for the currently visible steps as dot height. Tap a step at the desired height to set probability.

This is especially useful for Note track where probability is spread across 3 different layers. A unified probability overlay would consolidate them.

**Effort:** ~1.5 hours.

### 2.4 Double-Tap for Alt Pages

**What it does:** Double-tapping a page button toggles an **alt page** (e.g., Note → Retrig, Octave → Transpose).

**Current approach:** We use the Layer button to select layers. No alt-page concept.

**How we can adopt it:** Double-tap the **Layer** button to enter an **alt layer map** with secondary parameters:
- First tap of Layer = show primary layers (Gate, Note, Slide, etc.)
- Double-tap of Layer = show secondary layers (Condition, Retrigger, LengthVariation, etc.)

This doubles layer density without adding more grid cells.

**Effort:** ~1 hour.

### 2.5 Temporary Looping

**What it does:** While holding a modifier, loop changes are temporary. Releasing the modifier restores the original loop.

**How we can adopt it:** While holding **Shift + FirstStep**, any loop changes are temporary. Release Shift → loop snaps back. This is a **performance feature** for live jamming.

**Effort:** ~30 minutes.

### 2.6 Brightness Semantics for Visualization

**What it does:** Kria uses 4 brightness levels with clear semantics:
- `OFF` (0) = no data
- `LOW` (2) = out-of-loop / inactive
- `MED` (5) = in-loop / data present
- `HIGH` (12) = active / selected / playhead

**Current approach:** We use boolean `active` and `current` in `stepColor()`:
```cpp
bool red = (!active || current);
bool green = !current;
return color(red, green);
```

**How we can improve:** Adopt Kria's semantic brightness levels for richer visual feedback:
| Semantic | Kria (16 levels) | Launchpad MK1 Equivalent |
|----------|-----------------|--------------------------|
| No data | OFF | Off |
| Out of loop | LOW | Dim Red |
| In loop, inactive | MED | Dim Green / Amber |
| Active / selected | HIGH | Bright Green |
| Playhead | highlight(l) | Bright Yellow |

This would require expanding `stepColor()` to accept a `Brightness` enum instead of boolean flags. More expressive and matches the mental model of other grid sequencers.

**Effort:** ~1 hour. Refactor `stepColor()` + update all call sites.

### 2.7 Cued Pattern Switching with Pulsing LED

**What it does:** Pattern changes can be cued to take effect on the next loop boundary. The grid shows a **pulsing LED** for cued states.

**Current approach:** Pattern mode shows "requested pattern" with dimmed colors.

**How we can improve:** Add a **pulsing animation** for cued patterns. Since MK1 has 4 brightness levels, we can alternate between two brightness levels every few frames to create a pulse effect. This requires tracking "animation phase" in the controller state.

**Effort:** ~1.5 hours.

### 2.8 Independent Divisors Per Parameter

**What it does:** In Kria, every parameter page (note, gate, octave, etc.) has its own clock divisor and loop length. The note sequence can be length 5 while the gate sequence is length 12.

**Not applicable to our model:** PER|FORMER uses a single step array per sequence where all parameters share the same step clock. However, this pattern could inspire **per-layer loop lengths** if the model ever supports it.

---

## 3. Meadowphysics — Generative Gate Sequencer

### 3.1 Row-as-Track Metaphor

**What it does:** Each of the 8 rows is one independent voice. No spatial collision — voices are vertically segregated.

**Current approach:** Our Launchpad shows one track at a time (the selected track).

**How we can adopt it:** In **Performer Mode Overview**, we already show 8 tracks × 8 steps. Meadowphysics suggests making this more powerful:
- Each row = one track (already true)
- Show **mini-parameter bars** per track in overview (e.g., gate on/off + current step position)
- Use the full row height for parameter visualization

This is more of an inspiration than a direct port.

### 3.2 Three-Level Brightness Semantics

**What it does:** Meadowphysics uses exactly 3 levels:
- `L0` (dim) = valid range / background
- `L1` (medium) = set point / current value
- `L2` (bright) = playhead / selected

**How we can adopt:** Simpler than Kria's 4 levels. For MK1, map directly:
- Dim = background / range
- Medium = data value
- Bright = playhead / selection

This is already close to what we do. The insight is to use **dim for ranges** — e.g., in DiscreteMapTrack, show the valid threshold range as a dim bar, the current threshold as medium, and the active stage as bright.

### 3.3 Mode-Gated Columns

**What it does:** The same physical grid cells change meaning based on mode (position range vs speed vs rules).

**Current approach:** Our modes (Sequence, Pattern, Performer) are global. Within Sequence mode, the grid always shows step data.

**How we can adopt it:** Add **sub-modes within Sequence mode** that reuse the grid for different parameters:
- **Default:** Step data grid
- **Hold Navigate:** Navigation / zoom grid (like Kria's page toggle)
- **Hold Layer:** Layer selector grid (already implemented)
- **Hold RunMode:** Run mode + note mode picker grid (already planned)

This is essentially what we already do with function buttons. The meadowphysics insight is that **mode-gating should feel spatial** — the grid itself becomes the control surface, not just a data display.

### 3.4 Min/Max Range Fills

**What it does:** A continuous dim bar from `min` to `max` with a bright endpoint at the current value.

**How we can adopt it:** For DiscreteMapTrack thresholds and IndexedTrack durations:
```
Row 7 (bottom): dim red fill from min to max
Row 7: bright green dot at current value
```

This makes the valid range immediately visible. Currently we only show the current value, not the range context.

**Effort:** ~1 hour.

### 3.5 Fast Press vs Long Press Detection

**What it does:** Meadowphysics uses a timer threshold (`key_times`, 10 ticks) to distinguish fast press from long press:
```c
if(key_times[n] > 10) { /* long press */ }
```

**Current approach:** We have Down/Up/Press/DoublePress but no LongPress event type.

**How we can adopt it:** Add long-press detection using the existing `_buttonTracker`:
```cpp
void update() {
    // Check for long-press expiry
    for each held button:
        if (ticksHeld > LONG_PRESS_THRESHOLD) {
            dispatchButtonEvent(button, ButtonAction::LongPress);
            markAsLongPressed(button);
        }
}
```

Use cases:
- Long-press pattern slot = save pattern
- Long-press step = copy step
- Long-press track button = mute/solo

**Effort:** ~1.5 hours.

### 3.6 Generative Feedback Loops (Rules)

**What it does:** When voice A hits zero, it applies a rule to modify voice B (increment, decrement, random, stop, etc.).

**Not directly applicable:** This is a sequencer algorithm, not a UI pattern. However, the **visualization of cross-voice relationships** is relevant:
- Show trigger/toggle/sync targets as small dots in a side column
- Use color to indicate which voice affects which

This could inspire a **routing visualization** for DiscreteMapTrack or IndexedTrack route outputs.

---

## 4. libavr32 — Monome Grid Infrastructure

### 4.1 Quadrant-Level Dirty Tracking

**What it does:** A single `u8` bitfield `monomeFrameDirty` with one bit per 8×8 quadrant:
```c
extern u8 monomeFrameDirty;   // bit 0 = quad 0 (x:0-7,y:0-7)
                              // bit 1 = quad 1 (x:8-15,y:0-7)
                              // etc.
```

When an app sets an LED, the library computes the quadrant and sets the dirty bit. `monome_grid_refresh()` only transmits dirty quadrants.

**Current approach:** Our `syncLeds()` diffs individual LEDs (`_deviceLedState[i] != _ledState[i]`), not regions.

**How we can adopt it:** For an 8×8 grid, we can use a **single dirty bit** or a **row-dirty mask** (8 bits, one per row). This is coarser than per-cell diffing but much faster:
```cpp
uint8_t _dirtyRows = 0;  // bit i = row i changed

void setGridLed(int row, int col, Color color) {
    int idx = row * 8 + col;
    if (_ledState[idx] != color.data) {
        _ledState[idx] = color.data;
        _dirtyRows |= (1 << row);
    }
}

void syncLeds() {
    for (int row = 0; row < 8; ++row) {
        if (_dirtyRows & (1 << row)) {
            // Send entire row at once (Novation supports row sysex)
            sendRowSysex(row, &_ledState[row * 8]);
        }
    }
    _dirtyRows = 0;
}
```

**Impact:** Eliminates the 80-iteration loop in `syncLeds()` when only a few rows changed. Row-level sysex is also more efficient than per-cell NoteOn.

**Effort:** ~1.5 hours.

### 4.2 Event Queue Decoupling

**What it does:** ISR-level USB RX callbacks only post events to a circular queue. All app logic runs in the main loop.

```c
void ftdi_rx_done() {
    // ISR context
    event_post(kEventMonomeGridKey, packed_data);
}

// Main loop
while (event = event_next()) {
    app_event_handlers[event.type](event.data);
}
```

**Current approach:** Our MIDI is already received into a ring buffer in ISR, then drained in `Ui::update()`. This pattern is already implemented.

**No change needed.** Our architecture matches this already.

### 4.3 Single Shared TX Scratch Buffer

**What it does:** A static 72-byte local buffer (`txBuf[72]`) is reused for every outbound packet. No per-quadrant allocation.

**Current approach:** Each `syncLeds()` call builds MIDI messages on the stack or inline.

**How we can adopt it:** Pre-allocate a single `uint8_t _txScratch[256]` in `LaunchpadDevice`. All `syncLeds()` implementations build into this buffer. Reduces stack pressure and avoids repeated allocation.

**Effort:** ~15 minutes.

### 4.4 Protocol Function-Pointer Tables

**What it does:** Clean separation between 40h/series/mext protocols without runtime branches:
```c
void (*monome_grid_map)(u8 x, u8 y, u8* data);
void (*monome_arc_map)(u8 enc, u8* data);
```

**Current approach:** Virtual functions in C++ (`setLed()`, `syncLeds()`, `recvMidi()`).

**How we can improve:** De-virtualize into function-pointer tables as recommended in UI-OPTIMIZATION-REVIEW.md. Same pattern, different language.

---

## 5. Cross-Cutting Patterns Worth Adopting

### 5.1 Unified Dirty Tracking Strategy

Combine insights from **midigrid** (buffer-then-blast), **libavr32** (quadrant/row dirty bits), and our current diff sync:

```cpp
class LaunchpadDevice {
protected:
    std::array<uint8_t, 80> _ledState;
    std::array<uint8_t, 80> _deviceLedState;
    uint8_t _dirtyRows = 0;        // libavr32-style row dirty mask
    uint8_t _txScratch[256];       // libavr32-style shared TX buffer
    int _txLen = 0;

public:
    void setLed(int row, int col, Color color) {
        int idx = row * 8 + col;
        if (_ledState[idx] != color.data) {
            _ledState[idx] = color.data;
            _dirtyRows |= (1 << row);
        }
    }

    void syncLeds() {
        buildTxBuffer();   // midigrid-style batching into _txScratch
        if (_txLen > 0) {
            sendBulk(_txScratch, _txLen);
            _deviceLedState = _ledState;
            _dirtyRows = 0;
            _txLen = 0;
        }
    }
};
```

**Benefits:**
- No per-cell iteration when no LEDs changed (`_dirtyRows == 0` → skip)
- Single USB transfer for all changes
- Clean separation between "mark dirty" and "transmit"

### 5.2 Semantic Color Levels

Replace boolean `active/current` in `stepColor()` with a semantic enum:

```cpp
enum class LedSemantic : uint8_t {
    Off,           // no data
    Background,    // out of loop / inactive
    DataPresent,   // in loop, has value
    Active,        // selected / current edit target
    Playhead,      // current step
    Cued,          // pending change (pulsing)
};

Color semanticColor(LedSemantic s, int brightness = 3);
```

**Mapping for MK1:**
| Semantic | Color |
|----------|-------|
| Off | Off |
| Background | Dim Red |
| DataPresent | Dim Green / Amber |
| Active | Bright Green |
| Playhead | Bright Yellow |
| Cued | Pulsing Amber |

This unifies the color logic across all track types and makes the UI more visually consistent.

### 5.3 VirtualGrid Helper for Pagination

Extract the pagination logic from IndexedTrack (48 steps) and DiscreteMapTrack (32 stages) into a reusable helper:

```cpp
class VirtualGrid {
    int _virtualWidth;   // 48 for Indexed, 32 for DiscreteMap
    int _viewOffset = 0; // which 8-column page is visible
public:
    int virtualToPhysicalCol(int virtualCol) const;
    int physicalToVirtualCol(int physicalCol) const;
    bool isVisible(int virtualCol) const;
    void pageLeft()  { _viewOffset = std::max(0, _viewOffset - 8); }
    void pageRight() { _viewOffset = std::min(_virtualWidth - 8, _viewOffset + 8); }
    void setViewOffset(int step) { _viewOffset = (step / 8) * 8; }
};
```

### 5.4 Modifier Overlay System

Borrow Kria's modifier concept for the function row:

```cpp
enum class Modifier : uint8_t {
    None    = 0,
    Shift   = 1 << 0,
    Loop    = 1 << 1,   // new: hold Shift+FirstStep
    Prob    = 1 << 2,   // new: hold Shift+Layer
    Time    = 1 << 3,   // new: hold Shift+RunMode
};
```

While a modifier is held, the grid shows an overlay:
- **Loop modifier:** grid becomes loop start/end selector (two-tap)
- **Prob modifier:** grid shows probability dots per step
- **Time modifier:** grid shows clock divisor per step

---

## 6. Prioritized Adoption List

### Quick Wins (≤ 1 hour each)

| # | Pattern | Source | Effort | Impact |
|---|---------|--------|--------|--------|
| 1 | Buffer-then-blast MIDI in `syncLeds()` | midigrid | ~1h | High — reduces USB overhead |
| 2 | Semantic color levels enum | Kria | ~1h | Medium — visual consistency |
| 3 | Single shared TX scratch buffer | libavr32 | ~15min | Low — stack hygiene |
| 4 | Device capability flags struct | midigrid | ~30min | Low — maintainability |
| 5 | Brightness ladder for MK1 | midigrid | ~30min | Low — simpler color API |

### Medium Features (1–2 hours each)

| # | Pattern | Source | Effort | Impact |
|---|---------|--------|--------|--------|
| 6 | Row-dirty mask + row-level sysex | libavr32 | ~1.5h | High — faster sync |
| 7 | VirtualGrid pagination helper | midigrid | ~2h | Medium — reusable pagination |
| 8 | Loop modifier (hold + two-tap) | Kria | ~1h | Medium — faster loop editing |
| 9 | Probability overlay mode | Kria | ~1.5h | Medium — consolidated prob editing |
| 10 | Long-press detection | meadowphysics | ~1.5h | Medium — new interaction gesture |
| 11 | Min/max range fills | meadowphysics | ~1h | Low — visual context |

### Architectural (2+ hours)

| # | Pattern | Source | Effort | Impact |
|---|---------|--------|--------|--------|
| 12 | De-virtualize device layer | libavr32 | ~2h | Medium — removes vtable |
| 13 | LED double-buffering for MK2+/Pro | midigrid | ~1h | Low — visual polish |
| 14 | Cued pattern pulsing animation | Kria | ~1.5h | Low — visual feedback |
| 15 | Double-tap Layer for alt layers | Kria | ~1h | Low — layer density |

---

## 7. What NOT to Adopt

| Pattern | Source | Why Not |
|---------|--------|---------|
| Parameter-lock per-parameter divisors | Kria | Our model uses single step array; incompatible without model changes |
| Generative rule system (voice A modifies B) | meadowphysics | Algorithm feature, not UI pattern; would be a new track type |
| i2c inter-module communication | libavr32 | Hardware-specific, not relevant to Launchpad USB MIDI |
| FTDI serial grid protocol | libavr32 | Monome-specific, not MIDI |
| 16×8 grid assumption | Kria | We have 8×8; pagination is the adaptation, not the assumption |
| Hot-plug device detection | midigrid | Already handled by `ControllerManager` |

---

## 8. Synthesis: Recommended Pre-Flight Additions

Based on this review, add these to the pre-flight checklist before feature work:

```markdown
- [ ] Add `LedSemantic` enum + `semanticColor()` helper (Kria brightness semantics)
- [ ] Refactor `syncLeds()` to buffer-then-blast MIDI (midigrid optimization)
- [ ] Add `_dirtyRows` row-dirty mask to `LaunchpadDevice` (libavr32 quadrant tracking)
- [ ] Extract `VirtualGrid` pagination helper for >8-step tracks (midigrid virtual grid)
- [ ] Add `Modifier` bitmask enum for overlay system (Kria modifier pattern)
- [ ] Add `DeviceCapabilities` struct to device classes (midigrid capability flags)
```

These are all **infrastructure** changes that make the subsequent track-porting work easier and more consistent. Total effort: ~6 hours.
