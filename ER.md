# ER-101/ER-102 Comprehensive Manual

## Introduction

The ER-101 Indexed Quad Sequencer and ER-102 Quad Modulator represent a unique approach to voltage-based sequencing in the eurorack modular synthesizer environment. Unlike traditional step sequencers that advance one step per clock division, the ER-101 uses an "indexed" approach where each step has its own duration and gate length, allowing for complex timing relationships that would be difficult to achieve with conventional sequencers.

The ER-102 complements the ER-101 by providing group-based modulation, where external CV signals can affect only steps belonging to specific groups, enabling dynamic and responsive modulation.

## ER-101 Indexed Quad Sequencer

### Core Concept

The ER-101 is fundamentally a voltage table sequencer. Instead of storing musical notes, each step contains an index pointer that references a position in a voltage table. This allows for complex, non-musical voltage relationships that are not constrained to traditional scales or semitones.

### Duration-Based Sequencing

Unlike traditional sequencers that advance one step per clock division, the ER-101 has a unique timing system:

- **Step Duration**: Each step has its own duration (measured in clocks or sub-clocks)
- **Gate Length**: Each step has its own gate length (measured independently of duration)
- This creates a "duration-based" sequencing model rather than a "division-based" one

When a step is active, it remains so for the duration specified in its duration parameter, then advances to the next step regardless of external clock signals. This allows for rhythmically complex sequences that don't follow simple divisions of the master clock.

### Voltage Table System

The ER-101 uses a voltage table (100 positions) to map step indices to output voltages:

- Each step contains an index value (0-99) that points to a position in the voltage table
- The voltage table can be edited to contain any voltage values
- This allows for non-musical voltage relationships and custom tuning systems
- The table can be used to create curves, ramps, or any custom voltage shapes

### List Editing

The ER-101 supports variable-length sequences with insert/delete capabilities:

- Steps can be inserted or deleted at any position
- The sequence length is not fixed to 16 or 32 steps
- When inserting a step, it initially mirrors the preceding step's parameters
- This allows for dynamic sequence construction and modification

### Mathematical Operations

The ER-101 includes a mathematical operations system for batch processing parameters:

- Apply mathematical operations (add, subtract, multiply, divide) to ranges of steps
- Operations can be applied to duration, gate length, or voltage index parameters
- This enables complex parameter transformations across multiple steps at once

### User Interface Concepts

The ER-101 uses a grid controller interface (typically an LPD8 or similar) where:

- Grid buttons represent steps in the sequence
- Pressing a button selects that step for editing
- Encoder knobs adjust various parameters of the selected step
- Function buttons provide access to different parameter types and modes

## ER-102 Quad Modulator

### Group System

The ER-102 introduces a group system that allows for selective modulation:

- Steps on the ER-101 can be assigned to one or more groups (A, B, C, D)
- External CV inputs can be configured to affect only steps belonging to specific groups
- This creates conditional modulation based on the step's group membership

A step can belong to multiple groups simultaneously, allowing for complex modulation routing possibilities.

As described in the manual: "This displays the focused group. The dot means this group has members." "The displayed step is a member of the displayed group." "Add (remove) steps to (from) the displayed group." "This button inverts groups" and "This button shifts groups and patterns by one step."

### Modulation Matrix

The ER-102 provides a modulation matrix where:

- CV inputs (typically from LFOs, envelopes, or other modulation sources) can be routed to affect ER-101 parameters
- The routing is conditional on group membership
- When a CV source is routed to a group, only steps belonging to that group will be affected by that modulation source
- Multiple CV sources can affect the same group, or different sources can affect different groups

According to the manual: "There are 3 channels of CV/gate that can be routed to any set of groups." "Route these signals to any parameter of any step via the group mods." The CV inputs are 14-bit sampled at 7kHz and accept a +/-10V range.

### Types of Modulation

The ER-102 can modulate several parameters on the ER-101:

- **Gate Length**: Modulate the gate length of active steps
- **Duration**: Modulate the duration of active steps (affects timing)
- **Voltage Index**: Modulate the voltage index of active steps (affects output voltage)

The manual elaborates: "High and low are transforms." "Slope contains a gain factor for each step parameter." "The slope modifier determines the gain of each channel as it affects the gain of each channel as it" "on the group modifiers."

### Group Modifier Controls

According to the manual: "Group Modifiers:" with controls for:
- CV and GATE modulators
- High/Low transforms
- Slope with gain factors
- Individual controls for parameters like gate, duration, etc.
- Reset to transition controls
- Part/Select Activate controls

The group modifiers provide "CV/gate that can be routed to any set of groups" with channels labeled as (cv-a), (cv-b), (gate).

### Group Display Features

The manual describes: "Focus the user interface and select which transform (high vs low) is active on its respective channel." "This LED is lit when the displayed step is a member of the displayed group."

As noted: "When the GROUP display is focused, the number of steps in the group (for the current track) is shown on the top row of the display."

## Technical Implementation

### Timing Resolution

The ER-101 uses a high-resolution timing system where:
- Step durations can be specified with sub-clock precision
- This allows for complex rhythmic relationships that wouldn't be possible with simple clock divisions
- The timing resolution is dependent on the internal clock rate and can be quite fine

### Voltage Output

The ER-101 outputs voltages based on:
- The index value of the currently active step
- The voltage value at the corresponding position in the voltage table
- Any modulation from the ER-102 system applied to that step if it belongs to a routed group

### Clock Sources

The ER-101 can operate with:
- Internal clock (synchronized to the master clock)
- External clock sources
- The timing is based on the duration values rather than divisions of the clock

## Creative Applications

### Complex Rhythms

The duration-based approach allows for:
- Polyrhythmic patterns that don't resolve for many cycles
- Rhythms with non-standard divisions
- Grooves that shift over time as step durations change

### Dynamic Voltage Curves

By programming the voltage table and step indices carefully:
- Complex voltage curves can be created and looped
- Non-musical voltage relationships can be explored
- Custom waveforms can be sequenced

### Conditional Modulation

The ER-102 group system enables:
- Modulation that only affects certain parts of a sequence
- Interactive patching where modulation is conditional on musical context
- Dynamic response to external control signals

## Synchronization and Integration

The ER-101 and ER-102 pair can be synchronized with:
- External clock sources for tempo synchronization
- Other modular equipment via gate and CV signals
- MIDI via conversion interfaces

The system can function as:
- A complex LFO with programmable waveforms
- A dynamic arpeggiator with complex timing
- A polyrhythmic control voltage source
- A modulation sequencer for timbral evolution

## Practical Setup

### Basic Configuration

1. Program the voltage table with desired voltage values
2. Create a sequence by setting index and duration values for each step
3. Assign steps to groups if ER-102 modulation is desired
4. Route external CV sources to groups on the ER-102 to enable conditional modulation
5. Connect clock and reset signals as needed
6. Route the output CV and gate signals to destination modules

### Advanced Techniques

- Use the mathematical operations to create complex parameter relationships
- Program the voltage table to create specific waveforms or voltage curves
- Use group assignment to create polyrhythmic modulation patterns
- Combine with external modulation sources for dynamic responses
- Use insert/delete functionality to dynamically modify sequences during performance

## Conclusion

The ER-101/ER-102 system represents a sophisticated approach to voltage sequencing with powerful tools for creating complex rhythmic and melodic patterns. The indexed approach combined with group-based conditional modulation provides capabilities that go well beyond traditional step sequencers, enabling a new paradigm of voltage-based musical expression.