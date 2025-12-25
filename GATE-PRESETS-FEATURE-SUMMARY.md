# Gate Presets Feature - Summary

## What Was Added

A quick-edit menu for applying musically useful gate configurations to selected steps with a single button press.

**Access:** Press **Page + Step 7** on Curve Sequence Edit page

---

## The 8 Presets

| # | Name | Description | Settings |
|---|------|-------------|----------|
| 1 | **CLK x2** | Clock doubler from LFO | Events: Zero Up + Down, 4 ticks |
| 2 | **EOC/EOR** | End of Cycle/Rise triggers | Events: Peak + Trough, 8 ticks |
| 3 | **RISING** | Gate during rising slope | Advanced: Rising Slope |
| 4 | **FALLING** | Gate during falling slope | Advanced: Falling Slope |
| 5 | **PEAKS** | Trigger at peaks only | Events: Peak, 8 ticks |
| 6 | **TROUGHS** | Trigger at troughs only | Events: Trough, 8 ticks |
| 7 | **WINDOW** | Gate in middle 50% of range | Advanced: Window 25-75% |
| 8 | **CLEAR** | Remove all gate settings | Events: 0, Parameter: 0 |

---

## How It Works

### Selection-Aware Application

**With Steps Selected:**
```
1. Hold steps 1-4
2. Page + Step 7
3. Select "CLK x2"
→ Preset applied only to steps 1-4
```

**Without Selection:**
```
1. Don't select any steps
2. Page + Step 7
3. Select "EOC/EOR"
→ Preset applied to entire loop range (First Step → Last Step)
```

---

## Code Changes

### Files Modified

**1. CurveSequenceEditPage.h** (lines 54-55)
```cpp
void gatePresetsContextShow();
void gatePresetsContextAction(int index);
```

**2. CurveSequenceEditPage.cpp**

**Added enum and menu items** (lines 84-105):
```cpp
enum class GatePresetsContextAction {
    ClockDoubler,
    EocEor,
    RisingGate,
    FallingGate,
    PeaksOnly,
    TroughsOnly,
    WindowGate,
    ClearGates,
    Last
};

static const ContextMenuModel::Item gatePresetsContextMenuItems[] = {
    { "CLK x2" },
    { "EOC/EOR" },
    { "RISING" },
    { "FALLING" },
    { "PEAKS" },
    { "TROUGHS" },
    { "WINDOW" },
    { "CLEAR" },
};
```

**Added keyboard shortcut** (lines 588-592):
```cpp
if (key.pageModifier() && key.is(Key::Step 7)) {
    gatePresetsContextShow();
    event.consume();
    return;
}
```

**Added implementation** (lines 1405-1492):
- `gatePresetsContextShow()` - Shows the context menu
- `gatePresetsContextAction()` - Applies selected preset to target steps

---

## Technical Details

### Preset Configurations

**CLK x2:**
```cpp
eventMask = ZeroRise | ZeroFall;  // bits 2+3 = value 12
parameter = 2;                     // 4 ticks
```

**EOC/EOR:**
```cpp
eventMask = Peak | Trough;         // bits 0+1 = value 3
parameter = 3;                     // 8 ticks
```

**RISING:**
```cpp
eventMask = 0;                     // Advanced mode
parameter = AdvancedGateMode::RisingSlope;  // value 1
```

**FALLING:**
```cpp
eventMask = 0;                     // Advanced mode
parameter = AdvancedGateMode::FallingSlope; // value 2
```

**PEAKS:**
```cpp
eventMask = Peak;                  // bit 0 = value 1
parameter = 3;                     // 8 ticks
```

**TROUGHS:**
```cpp
eventMask = Trough;                // bit 1 = value 2
parameter = 3;                     // 8 ticks
```

**WINDOW:**
```cpp
eventMask = 0;                     // Advanced mode
parameter = AdvancedGateMode::Window;       // value 7
```

**CLEAR:**
```cpp
eventMask = 0;
parameter = 0;
```

### Application Logic

```cpp
// Determine target steps
std::bitset<CONFIG_STEP_COUNT> targetSteps;
if (_stepSelection.any()) {
    targetSteps = _stepSelection.selected();
} else {
    // Apply to loop range
    for (int i = sequence.firstStep(); i <= sequence.lastStep(); ++i) {
        targetSteps.set(i);
    }
}

// Apply preset to all target steps
for (size_t stepIndex = 0; stepIndex < sequence.steps().size(); ++stepIndex) {
    if (targetSteps[stepIndex]) {
        auto &step = sequence.step(stepIndex);
        step.setGateEventMask(eventMask);
        step.setGateParameter(parameter);
    }
}
```

---

## User Messages

Each preset shows a clear confirmation message:

- `GATE PRESET: CLOCK DOUBLER`
- `GATE PRESET: EOC/EOR`
- `GATE PRESET: RISING SLOPE`
- `GATE PRESET: FALLING SLOPE`
- `GATE PRESET: PEAKS ONLY`
- `GATE PRESET: TROUGHS ONLY`
- `GATE PRESET: WINDOW 25-75%`
- `GATE PRESET: CLEARED`

---

## Integration with Existing Features

### Works With Other Page Shortcuts

- **Page + Step 4** → Macro shapes (M-DAMP, M-FM, etc.)
- **Page + Step 5** → LFO patterns (Triangle, Sine, etc.)
- **Page + Step 6** → Transform operations (Invert, Reverse, etc.)
- **Page + Step 7** → **Gate presets (NEW!)**

### Complements Manual Editing

Presets are **starting points**, not limitations:

1. Apply preset (e.g., "CLK x2")
2. Select specific steps
3. Manually adjust GATE MODE layer for custom trigger lengths
4. Result: Quick setup + fine-tuned control

---

## Example Workflows

### Workflow 1: Quick Clock Setup
```
Goal: Get 2× clock from bipolar LFO

1. Create Bell shape on step 1
2. Set Range to Bipolar 5V
3. Page + Step 7 → "CLK x2"
4. Done! Zero crossing triggers active
```

### Workflow 2: Accent Pattern
```
Goal: Triggers only on loud parts

1. Create varying MIN/MAX across steps (some loud, some quiet)
2. Select all steps
3. Page + Step 7 → "WINDOW"
4. Result: Only peaks produce gates
```

### Workflow 3: Progressive Build
```
Goal: Increasing rhythmic complexity

1. Select steps 1-4 → Page + Step 7 → "PEAKS"
2. Select steps 5-8 → Page + Step 7 → "EOC/EOR"
3. Select steps 9-12 → Page + Step 7 → "CLK x2"
4. Select steps 13-16 → Page + Step 7 → "CLEAR"

Result: Simple → Complex → Very Complex → Breakdown
```

---

## Musical Benefits

1. **Speed:** Apply complex gate configurations in 2 button presses
2. **Consistency:** Same preset across multiple tracks = cohesive patterns
3. **Experimentation:** Try different presets rapidly to find what works
4. **Learning:** Presets teach good combinations (inspect with encoder messages)
5. **Live Performance:** Quick pattern changes during performance
6. **Workflow:** Combine with macro shapes for instant musical results

---

## Documentation Created

1. **CURVE-GATE-PRESETS-GUIDE.md** - Complete user guide with:
   - Detailed preset descriptions
   - Musical use cases for each preset
   - Step-by-step workflows
   - Preset combinations
   - Troubleshooting
   - Reference table

2. **GATE-PRESETS-FEATURE-SUMMARY.md** (this file) - Technical overview

3. Updated **CURVE-GATE-IMPROVEMENTS-SUMMARY.md** - Will add preset info

---

## Build Status

✅ **Build successful** - No errors or warnings
✅ **Integration complete** - Works with existing Page shortcuts
✅ **Messages working** - Clear user feedback
✅ **Selection-aware** - Respects step selection

---

## Future Enhancements (Optional)

- [ ] User-definable preset slots (save custom combinations)
- [ ] Preset preview (show what will be applied before confirming)
- [ ] Preset library expansion (more than 8 options)
- [ ] Per-track preset memory (recall last used preset per track)
- [ ] Preset morphing (blend between two presets)

---

**Quick Access: Page + Step 7 on Curve Sequence Edit page**
**8 Presets: CLK x2, EOC/EOR, RISING, FALLING, PEAKS, TROUGHS, WINDOW, CLEAR**
