# CurveTrack Launchpad Support Improvements

## Current Status
- **Current Layers**: Shape, ShapeVariation, ShapeVariationProbability, Min, Max, Gate, GateProbability
- **Missing Features**: Chaos, FilterFolder, AdvancedGateMode, EventGateBits, various curve features
- **Current Visualization**: Limited to bars and dots
- **Current Navigation**: Basic layer navigation

## Subtasks

### 1. Add Missing CurveSequence Layers
- **Chaos Layer**: Add layer for chaos parameter
- **FilterFolder Layer**: Add layer for filter/folder parameters
- **AdvancedGateMode Layer**: Add layer for advanced gate modes
- **EventGateBits Layer**: Add layer for event gate bits (Peak, Trough, ZeroRise, ZeroFall)
- **Layer Map Positioning**: Place new layers in logical positions (rows 3-6)

### 2. Improve Curve Visualization
- **Chaos Visualization**: Special visualization for chaos parameter
- **FilterFolder Visualization**: Visual indicators for filter/folder states
- **Advanced Gate Visualization**: Visual feedback for advanced gate modes
- **Event Gate Visualization**: LED patterns for event gate bits

### 3. Enhance Navigation System
- **Layer Navigation Buttons**: Better visual feedback for layer selection
- **Range Detection**: Improve navigation for different value ranges
- **Quick Jump Buttons**: Add buttons for quick access to common layers

### 4. Implement Performance Features
- **Performer Mode**: Add performer mode for CurveTrack with sequence length control
- **Live Curve Editing**: Support for real-time curve parameter adjustment
- **Quick Shape Toggle**: Double-press functionality for quick shape selection

### 5. Add Advanced Gate Features
- **Advanced Gate Mode Selection**: Layer for selecting advanced gate modes (RisingSlope, FallingSlope, AnySlope, Compare25, Compare50, Compare75, Window)
- **Event Gate Control**: Layer for event gate bits control
- **Gate Logic Visualization**: Visual indicators for gate logic operations

### 6. Improve Curve Shape Control
- **Shape Variation Visualization**: Better visualization for shape variation
- **Min/Max Range Visualization**: Enhanced bar graphs for min/max values
- **Shape Probability Visualization**: Visual feedback for shape variation probability

### 7. Add FilterFolder Support
- **Filter Parameter Editing**: Layer for filter parameters
- **Folder Parameter Editing**: Layer for folder parameters
- **FilterFolder Visualization**: Visual representation of filter/folder behavior

### 8. Add Chaos System Support
- **Chaos Parameter Editing**: Layer for chaos parameter
- **Chaos Visualization**: Dynamic LED patterns for chaos behavior
- **Chaos Range Mapping**: Range maps for chaos parameter (0-100 to 0-7)

### Files to Modify
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` - Add layer maps, visualization methods, navigation logic
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h` - Add new methods and layer definitions
- `/src/apps/sequencer/model/CurveSequence.h` - Ensure layer definitions are complete

### Status
Pending

### Priority
High