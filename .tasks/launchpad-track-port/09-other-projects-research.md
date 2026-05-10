# Launchpad Implementation Ideas from Other Projects

## Research Summary

I've researched three innovative Launchpad sequencer projects that provide excellent ideas for enhancing our PER|FORMER/XFORMER implementation:

1. **Cavian** - DIY Eurorack sequencer with Launchpad Mini support
2. **Beats** - Go-based drum machine/sample sequencer
3. **Extracadian** - Polyphonic step sequencer with advanced features

All projects demonstrate unique approaches to Launchpad integration that can inform our implementation.

## Reference Files

**Cavian (temp-ref/cavian/):**
- `/temp-ref/cavian/Hardware/cavian_launchpad.sch` - Hardware schematics
- `/temp-ref/cavian/Hardware/cavian_launchpad.brd` - PCB layout
- `/temp-ref/cavian/Software/cavian_launchpad.ino` - Arduino source code
- `/temp-ref/cavian/Software/cavian_launchpad/launchpad_svg.svg` - Launchpad button mapping diagram

**Beats (temp-ref/beats/):**
- `/temp-ref/beats/main.go` - Main sequencer implementation
- `/temp-ref/beats/sequencer.go` - Sequencer core logic
- `/temp-ref/beats/launchpad.go` - Launchpad device interface
- `/temp-ref/beats/samples.go` - Sample management
- `/temp-ref/beats/README.md` - Project documentation

**Extracadian (temp-ref/extracadian/):**
- `/temp-ref/extracadian/lib/` - Core sequencer logic (zero-heap in hot path)
- `/temp-ref/extracadian/src/` - Hardware glue layer (~350 lines)
- `/temp-ref/extracadian/lib/grid/` - Grid controller and renderer
- `/temp-ref/extracadian/lib/hal/` - Hardware abstraction layer
- `/temp-ref/extracadian/README.md` - Project documentation

## Key Ideas from Cavian

### 1. Dual Operation Modes
- **Vertical Mode (8x8):** 8 tracks × 8 steps (current PER|FORMER/XFORMER view)
- **Horizontal Mode (1x64):** Single track × 64 steps (scrollable with zoom)
- **Zoom Function:** Focus on 8-step sections of longer sequences

### 2. Enhanced Probability System
- **Visual Probability Feedback:** LED brightness/color intensity based on probability
  - Red (0-49%), Yellow (50-99%), Green (100%), Blinking (variable)
- **Probability Assignment Mode:** Press PROB button + select tracks/groups to apply
- **SWING Control:** Dedicated swing probability encoder (0-100%)

### 3. Group and Preset Management
- **Track Grouping:** 8 groups with visual feedback (color-coded)
- **504 Save Points:** 8 banks × 63 presets with grid-based selection
- **Loop Types:** Group loop, preset loop, custom loop boundaries
- **Visual Save System:** Use grid to visualize save banks and selected points

### 4. Performance Features
- **FILL Function:** Hold scene button + FILL for track-specific fill
- **RANDOM Function:** Randomization with configurable parameters
- **MUTE System:** Per-track mute with visual feedback (red LEDs)

## Key Ideas from Beats

### 1. Mode-Based Operation
- **Pattern Mode:** Default mode for step editing
- **Mutes Mode:** Track muting with visual feedback
- **Visual Feedback:** Red = active steps, Green = current step, Red (mutes mode) = muted tracks

### 2. Track Selection
- **Coordinate System:** Combination of top row and right column buttons for track addressing
- **Visual Indicators:** Lights up both indicators when track is selected
- **Step Visualization:** Clear current step indicator that moves through grid

### 3. Performance Enhancements
- **Mute Mode:** Live performance mute control
- **Step Flash:** Green flash on step activation during playback
- **External Sync:** Clock synchronization via MIDI or OSC

### 4. Grid Layout Ideas
- **Tracks/Steps:** 8x8 grid for tracks/steps
- **Top Row:** Scene selection
- **Right Column:** Track parameters
- **Bottom Row:** Transport controls

## Key Ideas from Extracadian

### 1. Advanced View System
- **Vertical View:** 8 functional columns (Group, Preset, Step, Channel, Mute, 3×Control)
- **NxN Views:** 4 layouts (8×8, 4×16, 2×32, 1×64) trading channel count for step visibility
- **Zoom View:** Sub-step editing at 24 PPQN (6 sub-steps per 16th note)
- **Project Mode:** Project save/load interface
- **SR Settings:** Shift register configuration views

### 2. Navigation System
- **View Switching:** Control column buttons switch views with chain cycling
- **Scrolling:** Auto-follow during playback, scroll indicators (Green = can scroll, Red = at limit)
- **Double-Tap:** Channel double-tap jumps to 1×64 view focused on that channel

### 3. Polyphonic Step Sequencer Features
- **Per-Channel, Per-Preset Step Lengths:** 0-8 steps with visual feedback
- **Trig Modes:** Normal, SR1 (shift register 1), SR2 (shift register 2) with color coding
- **Length-Set Gesture:** Hold ≥1s to set loop-aware step lengths

### 4. Input Priority System
- **Fill UI → Copy/Paste/Clear → Random/Chaos → Normal** priority levels
- **Multi-Preset Editing:** Hold channel button ≥500ms + press steps for batch editing

## Implementation Plan for PER|FORMER/XFORMER

### Phase 1: Foundation (Next 2-3 Weeks)
1. **Add Vertical/Horizontal Mode Toggle:** Implement dual operation modes
2. **Enhance Probability Visualization:** Add LED brightness/color intensity based on probability
3. **Implement Performer Mode:** Add performer mode with sequence length control and follow mode

### Phase 2: Advanced Features (Next 3-4 Weeks)
1. **Add Track Grouping:** Implement 8 track groups with color-coded feedback
2. **Enhance Loop Management:** Add group loop, preset loop, custom loop boundaries
3. **Implement Zoom Function:** Focus on 8-step sections of longer sequences

### Phase 3: Polish and Optimization (Next 2-3 Weeks)
1. **Add Scene Selection:** Use top row for scene/preset management
2. **Enhance Mute System:** Add per-track mute with visual feedback (red LEDs)
3. **Implement Double-Tap Functionality:** Quick navigation and editing

### Phase 4: Advanced Features (Future)
1. **Project Management:** Implement project save/load with grid visualization
2. **Sub-Step Editing:** 24 PPQN resolution with 6 sub-steps per 16th note
3. **Shift Register Controls:** Advanced shift register configuration

## Files to Modify
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp`
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h`
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadDevice.h`
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadMk3Device.cpp` (and other device implementations)

## Code Snippets for Key Features

### Dual Operation Modes
```cpp
enum class Mode : uint8_t {
    Sequence,
    Pattern,
    Performer,
    Vertical,    // New!
    Horizontal,  // New!
};

void sequenceDrawVertical() {
    for (int track = 0; track < 8; ++track) {
        for (int step = 0; step < 8; ++step) {
            Color color = stepColor(sequence.isStepActive(step), currentStep == step);
            setGridLed(track, step, color);
        }
    }
}

void sequenceDrawHorizontal() {
    int offset = _horizontalOffset;
    for (int step = 0; step < 8; ++step) {
        for (int row = 0; row < 8; ++row) {
            int absoluteStep = offset + row * 8 + step;
            if (absoluteStep < 64) {
                Color color = stepColor(sequence.isStepActive(absoluteStep), 
                                       currentStep == absoluteStep);
                setGridLed(row, step, color);
            } else {
                setGridLed(row, step, colorOff());
            }
        }
    }
}
```

### Probability Visualization
```cpp
inline Color colorProbability(int percent) const {
    if (percent == 0) return colorOff();
    if (percent < 50) return Color(percent / 20, 0);      // Red (0-2.5 brightness)
    if (percent < 100) return Color(3, (percent-50)/10);  // Yellow
    return colorGreen();                                   // 100% = green
}

inline Color colorGroup(int groupId) const {
    static const Color colors[] = {
        colorGreen(), colorRed(), colorYellow(), color(3, 1),
        color(1, 3), color(2, 2), color(3, 0), color(0, 3)
    };
    return colors[groupId % 8];
}
```

### Performer Mode
```cpp
void performerEnter() {
    // Initialize performer mode
    _performer.selectedChannel = 0;
    _performer.sequenceLength = 16;
    _performer.followMode = true;
}

void performerDraw() {
    // Draw sequence length indicator
    for (int i = 0; i < 8; ++i) {
        bool active = i < (_performer.sequenceLength / 8);
        setFunctionLed(i, active ? colorGreen() : colorOff());
    }
    
    // Draw channel select
    for (int i = 0; i < 8; ++i) {
        bool selected = i == _performer.selectedChannel;
        setSceneLed(i, selected ? colorYellow() : colorOff());
    }
}
```

## Conclusion

The research reveals numerous innovative ideas for enhancing our Launchpad implementation. The most impactful improvements include:

1. **Dual operation modes** for different sequencing tasks
2. **Visual probability feedback** using LED brightness/color intensity
3. **Comprehensive group/preset management** with visual feedback
4. **Advanced loop controls** (group loop, preset loop, custom loop)
5. **Grid-based save system** with visual feedback
6. **Performer mode** for live performance
7. **Vertical/horizontal view toggle** for different sequence lengths

These ideas can be implemented incrementally to enhance the PER|FORMER/XFORMER's usability while maintaining compatibility with the existing hardware and codebase.