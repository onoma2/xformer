# PEW|FORMER Algorithm Tester

A visualization tool for analyzing and designing algorithms in the PEW|FORMER Eurorack sequencer firmware, similar to the existing Curve Analysis tool.

## Purpose

This tool provides real-time visualization of how algorithm parameters affect musical output in the TuesdayTrack engine, allowing developers and musicians to:

- Visually analyze how Flow, Ornament, Power, Glide, and Trill parameters affect algorithmic sequences
- See the generated note, gate, velocity, and timing patterns
- View real-time spectrum analysis of algorithmic outputs
- Test all 21 TuesdayTrack algorithms plus a custom algorithm designer
- Hear the algorithmic patterns in real-time through audio sonification
- Design and fine-tune new algorithmic approaches

## Features

### Visualization (Multiple Views)
- **Note Sequence View**: Shows the generated note values (0-1 normalized)
- **Gate Sequence View**: Shows gate timing and length for each step
- **Velocity Sequence View**: Shows velocity variations for each step  
- **Spectrum Analysis**: Real-time FFT analysis of the algorithmic output
- **Step Probability View**: Shows probability values for each step
- **Gate Offset View**: Shows timing offset for each step
- **Trill Indicators**: Shows which steps have trill markers

### Algorithm Support
- All 21 TuesdayTrack algorithms:
  - Test, Tritrance, Stomper, Markov, Chiparp, Goaacid
  - SNH, Wobble, Techno, Funk, Drone, Phase, Raga
  - Ambient, Acid, Drill, Minimal, Kraft, Aphex, Autechre, Stepwave
- Custom algorithm designer with 4 adjustable parameters

### Parameter Controls
- Flow: 1-16 (affects algorithm flow and behavior)
- Ornament: 1-16 (adds variations and complexity)
- Power: 0-16 (controls playback speed)
- Glide: 0-16 (probability of slides between notes)
- Trill: 0-8 (probability of trills)

### Key Controls
- **F1-F12**: Quick select algorithms 1-12
- **1-7**: Switch visualization types
- **P**: Toggle algorithm playback
- **R**: Reset all parameters to defaults
- **[+]/[-]**: Expand/collapse parameter sections
- **Shift + Click**: Reset individual controls to default
- **Click + Drag**: Adjust slider values

### Audio Engine
- Real-time sonification of algorithmic patterns
- Phase-accurate playback of algorithm-generated sequences
- Modulation of audio parameters based on algorithm output

## Usage

1. Build the application using CMake:
   ```
   mkdir build
   cd build
   cmake ..
   make
   ```

2. Run the algorithm tester:
   ```
   ./algo_tester
   ```

3. Select an algorithm using the dropdown or F1-F12 keys
4. Adjust Flow and Ornament parameters to change algorithm behavior
5. Switch visualization types to see different aspects of the algorithm
6. Toggle play to hear the algorithm in real-time
7. Use the Custom algorithm to design new approaches with the 4 parameters

## Hardware Constraints Analysis

Like the Curve Analysis tool, this tool helps visualize how algorithmic parameters will affect the actual hardware:

- Algorithm complexity assessment
- Pattern density visualization
- Timing and probability analysis
- Gate length and offset visualization

## Development

The Algorithm Tester follows the same architectural principles as the Curve Analysis tool:

- Real-time parameter adjustment
- Visual feedback for all changes
- Comprehensive algorithm coverage
- Audio sonification capabilities
- Extensible design for future algorithms