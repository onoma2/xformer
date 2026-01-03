# Teletype Track Learning Tutorial for Performer

## Performer Teletype Keyboard Shortcuts

The Performer implementation of Teletype includes specific keyboard shortcuts for editing scripts:

### Function Keys (F1-F8)
- **F1-F8**: Select script 0-7 (F1=Script 0, F2=Script 1, etc.)

### Step Keys (Steps 1-16)
- **Steps 1-16**: Insert common teletype commands/variables (cycles through options)
  - **Step 1**: 1 → A → B (cycles through 1→A→B)
  - **Step 2**: 2 → C → D (cycles through 2→C→D)
  - **Step 3**: 3 → E → F (cycles through 3→E→F)
  - **Step 4**: 4 → G → H (cycles through 4→G→H)
  - **Step 5**: 5 → I → J (cycles through 5→I→J)
  - **Step 6**: 6 → K → L (cycles through 6→K→L)
  - **Step 7**: 7 → M → N (cycles through 7→M→N)
  - **Step 8**: 8 → O → P (cycles through 8→O→P)
  - **Step 9**: 9 → Q → R (cycles through 9→Q→R)
  - **Step 10**: 0 → S → T (cycles through 0→S→T)
  - **Step 11**: . → U → V (cycles through .→U→V)
  - **Step 12**: : → W → X (cycles through :→W→X)
  - **Step 13**: ; → Y → Z (cycles through ;→Y→Z)
  - **Step 14**: CV → IN (cycles through CV→IN)
  - **Step 15**: TR → PARAM (cycles through TR→PARAM)
  - **Step 16**: EVERY → IF (cycles through EVERY→IF)

### Shift + Step Keys (Steps 1-16)
- **Shift + Steps 1-16**: Insert special characters
  - **Shift + Step 1**: + (Plus)
  - **Shift + Step 2**: - (Minus)
  - **Shift + Step 3**: * (Multiply)
  - **Shift + Step 4**: / (Divide)
  - **Shift + Step 5**: % (Modulo)
  - **Shift + Step 6**: = (Equals)
  - **Shift + Step 7**: < (Less than)
  - **Shift + Step 8**: > (Greater than)
  - **Shift + Step 9**: ! (Logical NOT)
  - **Shift + Step 10**: & (Bitwise AND)
  - **Shift + Step 11**: | (Bitwise OR)
  - **Shift + Step 12**: ^ (Bitwise XOR)
  - **Shift + Step 13**: $ (Dollar sign)
  - **Shift + Step 14**: @ (At symbol)
  - **Shift + Step 15**: ? (Question mark)
  - **Shift + Step 16**: ; (Semicolon)

### Navigation and Editing
- **Left Arrow**: Backspace (delete character to the left)
- **Shift + Left Arrow**: Move cursor left
- **Right Arrow**: Insert space
- **Shift + Right Arrow**: Move cursor right
- **Encoder Press**: Load selected line into edit buffer
- **Shift + Encoder Press**: Commit line (save changes)
- **Encoder Turn**: Move cursor left/right (normal) or select line (with Shift)

### Other Controls
- **Page + F4**: Exit script view
- **Page + Function Keys**: Not handled (reserved for other functions)

## Teletype Pattern View Keyboard Shortcuts

The Performer implementation also includes a pattern view for editing teletype patterns:

### Function Keys
- **F1-F4**: Select pattern 0-3 (F1=Pattern 0, F2=Pattern 1, etc.)

### Step Keys (Steps 1-16)
- **Steps 1-9**: Insert digits 1-9 into the currently selected pattern position
- **Step 10 (0)**: Insert digit 0 into the currently selected pattern position
- **Step 14**: Set pattern length to current row position
- **Step 15**: Set pattern start position to current row
- **Step 16**: Set pattern end position to current row

### Navigation and Editing
- **Left Arrow**: Backspace digit (while editing) or delete current row (with Shift)
- **Right Arrow**: Insert new row at current position (with Shift toggles turtle display)
- **Encoder Turn**: Move up/down through pattern rows
- **Encoder Press**: Negate current value (make positive/negative)
- **Shift + Encoder Press**: Commit edited value
- **Page + F4**: Exit pattern view

## Introduction

This tutorial is designed for intermediate teletype users who want to learn how to use the Teletype Track functionality within the Performer firmware. The Teletype Track in Performer provides a powerful way to integrate teletype scripts with your sequencer, allowing for complex musical patterns and interactions.

## Prerequisites

Before starting this tutorial, you should have:
- Basic understanding of teletype operations and syntax
- Familiarity with the Performer sequencer interface
- Understanding of CV/Gate concepts in modular synthesis

## Overview of Teletype in Performer

The Teletype Track in Performer provides:
- 4 CV outputs (CV 1-4) mapped to Performer's CV outputs
- 4 Gate outputs (TR A-D) mapped to Performer's gate outputs
- 4 trigger inputs (TI-TR1-4) to trigger scripts
- 2 CV inputs (TI-IN and TI-PARAM) from Performer's CV inputs
- Integration with Performer's clock system
- Pattern and variable storage

## Section 1: Basic Script Setup

### Step 1: Creating Your First Script

Let's start with a simple script that toggles a gate output when triggered:

```
TR.TOG A
```

This script will toggle gate output A each time the trigger input receives a signal.

### Step 2: Adding CV Output

Now let's add a CV output to the script:

```
TR.TOG A
CV 1 V 5
```

This will toggle gate A and set CV 1 to 5 volts when triggered.

### Step 3: Using Variables

Let's make the CV voltage dynamic using variables:

```
X ADD X 1
CV 1 V X
TR.TOG A
```

This script increments variable X each time it's triggered, sets CV 1 to that voltage, and toggles gate A.

## Section 2: Working with Patterns

### Step 4: Basic Pattern Usage

Patterns allow you to create sequences of values. Let's create a simple melody pattern:

```
CV 1 N P.NEXT
```

This script sets CV 1 to the next note in the pattern each time it's triggered.

### Step 5: Loading Pattern Values

To load values into the pattern, you can use the following commands in LIVE mode:

```
P.PUSH 0
P.PUSH 12
P.PUSH 7
P.PUSH 5
P.L 4
```

This creates a 4-step pattern with semitone values (0, 12, 7, 5) representing a simple chord progression.

### Step 6: Multiple Patterns

You can use multiple pattern banks (0-3):

```
P.N 1        // Switch to pattern 1
P.PUSH 0
P.PUSH 2
P.PUSH 4
P.PUSH 5
P.L 4
```

Then in your script:

```
CV 1 N PN.NEXT 1    // Get next value from pattern 1
```

## Section 3: Advanced Control Flow

### Step 7: Using EVERY for Clock Division

```
EVERY 2: TR.P A      // Pulse gate A every 2 triggers
EVERY 4: TR.P B      // Pulse gate B every 4 triggers
CV 1 V RAND 5        // Set CV 1 to random voltage 0-5V
```

### Step 8: Conditional Logic with IF

```
IF GT X 5: CV 1 V 5
IF LTE X 5: CV 1 V 0
X ADD X 1
TR.TOG A
```

This script checks if X is greater than 5, sets CV 1 accordingly, increments X, and toggles gate A.

### Step 9: Loops with L

```
L 1 4: TR.P I        // Pulse gates 1-4 in sequence
CV 1 N RAND 24       // Random note in 2-octave range
```

## Section 4: Integration with Performer Clock

### Step 10: Clock-Based Timing

In Performer, you can set the Teletype track to use clock timing instead of ms timing:

```
M.ACT 1              // Enable metronome
M 500                // Set metronome to 500ms (120 BPM)
```

### Step 11: Using Metro Script

The metro script runs at the metronome interval:

```
// In M script:
CV 2 V DRUNK         // Set CV 2 to drunk-walk voltage
DRUNK.WRAP 1         // Enable wrapping
```

## Section 5: CV Inputs and Parameter Control

### Step 12: Using CV Inputs

You can use the IN and PARAM variables to read from CV inputs:

```
CV 1 V IN            // Set CV 1 to value of IN input
CV 2 V PARAM         // Set CV 2 to value of PARAM input
```

### Step 13: Scaling CV Inputs

```
X SCALE 0 16383 0 10 IN    // Scale IN from 0-16383 to 0-10V
CV 1 V X
```

## Section 6: Advanced Pattern Operations

### Step 14: Pattern Manipulation

```
P.NEXT              // Get next value in pattern
P.I ADD P.I 2       // Skip 2 steps in pattern
CV 1 N P.HERE       // Set CV to current pattern value
```

### Step 15: Pattern Ranges

```
P.START 2           // Set pattern start to index 2
P.END 6             // Set pattern end to index 6
P.WRAP 1            // Enable wrapping within range
```

## Section 7: Randomness and Probability

### Step 16: Random Operations

```
PROB 50: TR.P A      // 50% chance to pulse gate A
CV 1 V RRAND 2 8    // Random voltage between 2 and 8V
```

### Step 17: Drunk Walk

```
CV 1 V DRUNK         // Use drunk walk for CV
DRUNK.MIN 0          // Set minimum value
DRUNK.MAX 16383      // Set maximum value
DRUNK.WRAP 0         // Don't wrap at boundaries
```

## Section 8: MIDI Integration

### Step 18: MIDI Operations (Performer-specific)

```
MI.NV                // Get latest MIDI note as voltage
MI.CCV               // Get latest MIDI CC as voltage
CV 1 V MI.LNV        // Set CV 1 to latest MIDI note voltage
```

## Section 9: Delay and Stack Operations

### Step 19: Using Delays

```
TR.P A
DEL 100: TR.P B     // Trigger B 100ms after A
DEL 200: TR.P C     // Trigger C 200ms after A
```

### Step 20: Using Stacks

```
S: CV 1 V 5         // Stack this command
S: TR.P A           // Stack this command
S.ALL               // Execute all stacked commands
```

## Section 10: Complex Example

### Step 21: Complete Pattern Sequencer

Here's a complete example that combines multiple concepts:

```
// Script 1 (triggered by input):
CV 1 N P.NEXT        // Next note in main pattern
TR.P A               // Pulse gate A
X ADD X 1            // Increment step counter
MOD X 8              // Wrap at 8 steps
EQ X 0               // Check if at step 0
PROB 75: SCRIPT 2    // 75% chance to run script 2 on step 0

// Script 2 (called conditionally):
CV 2 V RAND 10       // Random CV on output 2
TR.P B               // Pulse gate B
```

## Section 11: Troubleshooting and Tips

### Common Issues:

1. **Scripts not running**: Check trigger input mapping in the UI
2. **CV outputs not working**: Verify CV output mapping in the UI
3. **Pattern not advancing**: Make sure you're using P.NEXT or manually incrementing P.I

### Performance Tips:

1. **Keep scripts efficient**: Complex scripts can impact performance
2. **Use appropriate timing**: Use clock timing for synced sequences
3. **Organize patterns**: Use different pattern banks for different purposes

## Section 12: Advanced Techniques

### Step 22: Function Scripts

Use SCRIPT to call other scripts as functions:

```
// Script 1: Main logic
CV 1 V RAND 5
SCRIPT 3             // Call script 3

// Script 3: Function
CV 2 V ADD CV 2 1    // Increment CV 2
```

### Step 23: Queue Operations

Use queues for smoothing or averaging:

```
Q CV 1               // Add current CV 1 value to queue
Q.N 4                // Set queue length to 4
CV 2 V Q.AVG         // Set CV 2 to average of queue
```

## Conclusion

This tutorial has covered the fundamental concepts of using Teletype in the Performer firmware. From basic scripts to complex pattern operations, you now have the tools to create intricate musical sequences and interactions.

Remember to experiment with different combinations of operations, and don't hesitate to refer back to the teletype manual for more detailed information about specific operations.

The key to mastering Teletype in Performer is practice and experimentation. Try modifying the examples in this tutorial to create your own unique patterns and behaviors.