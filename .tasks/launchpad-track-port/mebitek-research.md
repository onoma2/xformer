# Launchpad Implementation Research - temp-ref/mebitek/

## Research Findings Summary

The temp-ref/mebitek/ repository contains an excellent, well-architected Launchpad implementation that provides strong insights and reusable patterns for our PER|FORMER/XFORMER project.

## Reference Files

**Mebitek Performer (temp-ref/mebitek-performer/):**
- `/temp-ref/mebitek-performer/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h` - Main controller interface
- `/temp-ref/mebitek-performer/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` - Main controller implementation
- `/temp-ref/mebitek-performer/src/apps/sequencer/ui/controllers/launchpad/LaunchpadDevice.h` - Base device class
- `/temp-ref/mebitek-performer/src/apps/sequencer/ui/controllers/launchpad/LaunchpadMk3Device.h/cpp` - Mk3/Mini Mk3/X specific implementation
- `/temp-ref/mebitek-performer/src/apps/sequencer/ui/controllers/launchpad/LaunchpadProDevice.h/cpp` - Pro/Pro Mk3 specific implementation
- `/temp-ref/mebitek-performer/src/apps/sequencer/ui/controllers/ControllerManager.h` - Controller discovery and management
- `/temp-ref/mebitek-performer/src/apps/sequencer/model/UserSettings.h` - Launchpad configuration settings

## Architecture Overview

### Device Support
- Supports multiple Launchpad models: Mk1, Mk2, Mk3, Mini, Pro, Pro Mk3, X
- Clean device abstraction hierarchy with base `LaunchpadDevice` and model-specific implementations
- Uses product ID detection for automatic device type selection

### Mode-Based Design
The implementation follows a **three-mode architecture**:

1. **Sequence Mode** - Detailed step-by-step editing with layer-based approach
2. **Pattern Mode** - Pattern management and selection across all tracks
3. **Performer Mode** - Live performance view with real-time sequence manipulation

## Key Innovations & Reusable Patterns

### 1. Layer Mapping System

**Implementation**:
```cpp
static const LayerMapItem noteSequenceLayerMap[] = {
    [int(NoteSequence::Layer::Gate)]                        =  { 0, 0 },
    [int(NoteSequence::Layer::GateProbability)]             =  { 1, 0 },
    [int(NoteSequence::Layer::GateOffset)]                  =  { 2, 0 },
    [int(NoteSequence::Layer::Note)]                        =  { 0, 3 },
    // ... 15+ more layers
};
```

**Benefits for PER|FORMER/XFORMER**:
- Separate layer maps per track type allow custom layouts
- Dynamic layer-to-grid mapping adapts to each track's specific parameters
- Easy to extend with new track types by adding new layer maps

### 2. Navigation System

**Features**:
- Handles sequences longer than 8 steps using virtual grid sections
- Layer-specific navigation with range detection
- Visual feedback for current position
- Support for large sequences (up to 64 steps)

**Code Pattern**:
```cpp
void sequenceUpdateNavigation() {
    switch (_project.selectedTrack().trackMode()) {
        case Track::TrackMode::Note: {
            auto layer = _project.selectedNoteSequenceLayer();
            auto range = NoteSequence::layerRange(layer);
            _sequence.navigation.top = range.max / 8;
            _sequence.navigation.bottom = (range.min - 7) / 8;
            break;
        }
        // Track-specific navigation...
    }
}
```

### 3. Circuit Keyboard Layout

**Innovative Note Entry System**:
- Rows 3-4: Note selection (semitones and tones)
- Row 6: Octave selection (-4 to +3 octaves)
- Row 7: Page navigation
- Currently playing notes highlighted in red

**Useful for PER|FORMER/XFORMER**:
- Provides a musical interface for note entry
- Could be adapted for our TuesdayTrack or IndexedTrack

### 4. Track-Specific Handling

**Current Track Support**:
```cpp
switch (_project.selectedTrack().trackMode()) { 
    case Track::TrackMode::Note:
    case Track::TrackMode::Curve:
    case Track::TrackMode::Stochastic:
    case Track::TrackMode::Logic:
    case Track::TrackMode::Arp:
    // Track-specific logic...
}
```

**Handling Approach**:
- Each track type has dedicated layer maps
- Track-specific visualization methods (bits, bars, dots, notes)
- Separate draw/edit methods for each track type

### 5. Performer Mode

**Two Sub-Layers**:
- **Sequence Length**: Adjust sequence lengths on the fly
- **Overview**: Visual feedback of all tracks' current state
- Follow mode for tracking current step during playback

**Use Case**:
- Perfect for live performance and quick sequence manipulation

### 6. Configuration System

**Customization Options**:
```cpp
class LaunchpadStyleSetting : public Setting<int> {
    // "classic" (0) or "blue" (1) color scheme
};

class LaunchpadNoteStyle : public Setting<int> {
    // "classic" (0) or "circuit" (1) note entry
};
```

**Benefits**:
- Easy to add new color schemes
- Configurable note entry styles
- User preferences stored in settings

### 7. LED Color Management

**Color Scheme System**:
- Classic style: Yellow/green/red
- Blue style: Blue color variations
- 4-level brightness control
- Device-specific color mapping

**Implementation Pattern**:
```cpp
Color color(bool red, bool green, int brightness = 3) const {
    return Color(red ? brightness : 0, green ? brightness : 0);
}
```

## Detailed Track Type Layouts

### Note Track (15+ Layers)
- **Gate/Note/Slide**: Primary musical parameters
- **Probability**: Gate, retrigger, note variation probabilities
- **Timing**: Gate offset, length, stage repeats
- **Conditions**: Step conditions for advanced logic

### Curve Track (7 Layers)
- **Shape Control**: Shape, shape variation, probability
- **Min/Max Values**: Curve amplitude parameters
- **Gate Control**: Gate and gate probability

### Stochastic Track
- **Probability Focused**: Note variation, octave variation probabilities
- **Loop Control**: Special loop and rest probability settings

### Logic Track
- **Logic Operations**: Gate logic, note logic parameters
- **Conditional**: Step conditions for logical operations

### Arp Track
- **Arpeggiator Mode**: Octave control, hold function
- **MIDI Integration**: MIDI keyboard mode for live performance

## Reusable Code Patterns

### 1. Device Abstraction
```cpp
class LaunchpadDevice {
    virtual void recvMidi(uint8_t cable, const MidiMessage &message);
    virtual void setLed(int row, int col, Color color);
    virtual void syncLeds();
};
```

### 2. Layer Map Definition
```cpp
struct LayerMapItem {
    uint8_t row;
    uint8_t col;
};

static const LayerMapItem noteSequenceLayerMap[] = {
    [int(NoteSequence::Layer::Gate)] = { 0, 0 },
    // ...
};
```

### 3. Range Mapping
```cpp
struct RangeMap {
    int16_t min[2];
    int16_t max[2];
    int map(int value) const;
    int unmap(int value) const;
};

static const RangeMap curveMinMaxRangeMap = { { 0, 0 }, { 255, 7 } };
```

### 4. Mode Dispatch
```cpp
switch (_mode) {
    case Mode::Sequence:
        sequenceDraw();
        break;
    case Mode::Pattern:
        patternDraw();
        break;
    case Mode::Performer:
        performerDraw();
        break;
}
```

## Portability Assessment

### High Portability (Can be directly adopted):
1. Device abstraction architecture
2. Mode-based design pattern
3. Layer mapping system
4. Range mapping utility
5. Color management system

### Medium Portability (Needs adaptation):
1. Track-specific layer maps (need to match our track types)
2. Navigation system (our track lengths may differ)
3. Visualization methods (our track data structures vary)

### Low Portability (Need to re-implement):
1. Track type switch cases (our track types are different)
2. Specific layer implementations (our layers vary)

## Implementation Recommendations for PER|FORMER/XFORMER

### Phase 1: Foundation
1. Adopt device abstraction architecture
2. Implement mode-based design (Sequence/Pattern/Performer)
3. Create base layer mapping system
4. Implement range mapping utility
5. Add color management system

### Phase 2: Track-Specific Implementation
1. Create layer maps for our track types:
   - TuesdayTrack: Algorithm, Flow, Ornament, Power
   - DiscreteMapTrack: Threshold, Note, Slide, Direction
   - IndexedTrack: Note, Duration, Gate, Slide
2. Implement track-specific navigation
3. Create track-specific visualization methods

### Phase 3: Advanced Features
1. Implement performer mode
2. Add circuit keyboard layout
3. Implement configuration system
4. Add track mute/solo functionality

## Conclusion

The temp-ref/mebitek/ Launchpad implementation is an excellent reference with:

1. **Clean architecture** with clear separation of concerns
2. **Innovative UI patterns** like circuit keyboard and layer mapping
3. **Comprehensive device support** across multiple Launchpad models
4. **Well-documented code** with detailed comments
5. **Scalable design** that can be extended to new track types

The implementation provides a solid foundation that we can build upon, with the layer mapping system and mode-based architecture being particularly valuable for our PER|FORMER/XFORMER integration.