# XFORMER User Manual

## 0. WARININGS

The update is based on Phazerville fork of the original firmware, so no
Mebitek or other features. No new modulation ROUTES are implemented (yet).
MIDI and Launchpad functionalities WERE NOT tested at all.

It is important to note that 99,5% of the code was written by musician littel
helpers, that you can by for couple of dollars a month.

I have tested it on my unit and it seems that everything that I care about works. But no backwards compartibility, no guaranties, no sincere or not condolecenses are planned to happen.

Use at your own risk. Make a backup of your projects on a separate SD.




## 1. Introduction

This manual covers the XFORMER-specific features:

   - Algo Track (Based on TINRS Tuesday idea of algorithmic sequencing and
its open sourced code).
   - Note Track:
   	- Harmony Engine (Inspired by Instruo Harmonaig and based on its manual),
	- Accumulator (Inspired by Intellijel Metropolix and based on its manual).
	- Basic Pulse Count and Gate Modes (same as previous)

It assumes you are already familiar with the standard functionality of the Westlicht PER|FORMER..

This manual will focus exclusively on these new features, providing detailed descriptions, practical examples, and essential shortcuts to help you integrate them into your musical workflow.

## 2. Algo Track

### 2.1 What is Algo Track?

Algo Track is a special track type that generates musical content algorithmically. Most of the algos are not even pseudorandom so same parameters will produce same results. There are practically infinite amount of this results though, since you have quite a handful of values to choose form.

To enable Algo Track:
1. Navigate to the track settings page for a selected track
2. Change the track type to "Algo"
3. Once enabled, the track will no longer respond to manually programmed notes but will instead be driven by the selected algorithm.

### 2.2 UI Overview

When a track is set to Algo type, its UI page changes to display controls specific to the algorithmic engine. The UI provides access to 3 pages of parameters:

- **Shift-S1**: Main UI

    	**Page 1:** Algorithm, Flow, Ornament, Power

    	**Page 2:** Loop, Scan, Rotate, CV Mode

    	**Page 3:** Skew, Gate Offset, Glide, Trill

Key elements include:

- **Algorithm Selector:** Choose from 21 built-in algorithms (0-20)
- **Flow (0-16):** Controls pattern generation, often seeding main RNG for algorithm behavior
- **Ornament (0-16):** Controls variations and secondary parameters, often seeding extra RNG
- **Power (0-16):** Controls density and trigger probability. Power=0 equals
no sound.
- **Loop Length:** Sets pattern length (0=Infinite, 1-128 for finite loops)
- **Output Display:** Visual feedback of the generated notes and gates

- **Shift-S2**: opens divisor, root, octave and menu

- **Shift-S3**: opens menu with the same contols as 3 pages of main UI

### 2.3 The Algorithms

The XFORMER includes 21 diverse Tuesday algorithms, each offering a unique approach to generative sequencing. The original Tuesday algos were adapted from using 2 Gate-CV pairs (gate+cv, accent+velocity) to 1 pair of Performer's track, so accents and velocities were translated to gate lengths and slides where possible. Algos are unaware of project tempo and clock division of the track, so if something doesn't sound right adjust tracks rate and change POWER.

**0. SIMPLE** - Basic test pattern with octave sweeps or chromatic walks
- Flow: Determines mode (0-8 = octave sweep, 9-16 = chromatic walk) and sweep speed
- Ornament: Controls accent probability and velocity

**1. TRITRANCE** - German minimal style arpeggios based on 3-phase cycling
- Flow: Seeds main RNG for b1 (high note) and b2 (phase offset)
- Ornament: Seeds extra RNG for b3 (note offset for octave changes)
- Creates evolving 3-phase patterns with chance operations

**2. STOMPER** - Acid bass patterns with slides and state machine transitions
- Flow: Seeds pattern modes (extra RNG)
- Ornament: Seeds note choices (main RNG)
- Uses complex 14-state pattern machine with different behaviors

**3. MARKOV** - Markov chain melody generation using 8x8x2 transition matrix
- Flow: Contributes to matrix generation (main RNG)
- Ornament: Contributes to matrix generation (extra RNG)
- Each note choice depends on previous two notes in sequence

**4. CHIPARP** - Chiptune arpeggio patterns with chord progressions
- Flow: Seeds main RNG for chord generation
- Ornament: Seeds chord RNG and determines playback direction
- Generates 4-step chord patterns typically playing notes (0, 2, 4, 6) + base offset

**5. GOACID** - Goa/psytrance acid patterns with systematic transposition
- Flow: Seeds note selection RNG
- Ornament: Seeds accent RNG and initializes pattern flags
- Applies systematic transposition (+3 semitones, -5 semitones based on conditions)

**6. SNH** - Sample & Hold random walk algorithm
- Flow: Seeds main RNG
- Ornament: Seeds extra RNG
- Creates evolving CV values using filtered random walk

**7. WOBBLE** - Dual-phase LFO bass with alternating patterns
- Flow: Seeds main RNG
- Ornament: Seeds extra RNG
- Uses two different phase oscillators to generate notes

**8. TECHNO** - Four-on-floor club patterns with kick and hi-hat patterns,
that are mixed at Gate output and the adapted to generate Pitch. It is not
intended to create rythms, more techno melodic grooves.
- Flow: Determines kick pattern type (4 variations),
- Ornament: Determines hi-hat pattern type (4 variations)
- Creates kick drum patterns and hi-hat patterns on off-beats

**9. FUNK** - Syncopated funk grooves with ghost notes
- Flow: Determines pattern selection (8 patterns)
- Ornament: Determines syncopation and ghost note probability
- Includes ghost notes with reduced velocity

**10. DRONE** - Sustained drone textures with slow movement
- Flow: Determines base note
- Ornament: Determines interval and variation
- Creates sustained notes with very long gate lengths

**11. PHASE** - Minimalist phasing patterns with gradual shifts
- Flow: Determines pattern content
- Ornament: Determines phase drift speed
- Creates pattern that shifts position gradually over time

**12. RAGA** - Indian classical melody patterns with traditional scales
- Flow: Determines scale type (Bhairav, Yaman, Todi, Kafi-like)
- Ornament: Determines ornament type
- Uses traditional Indian classical scales and melodic movements

**13. AMBIENT** - Harmonic drone and event scheduler
- Flow: Sets up drone chord root note
- Ornament: Controls event timing randomness
- Creates harmonic drones with scheduled events

**14. ACID** - 303-style patterns with slides and octave accents
- Flow: Seeds main RNG for 8-step sequence
- Ornament: Seeds accent pattern and octave mask
- Classic Roland TB-303 style patterns with slides and accents

**15. DRILL** - UK Drill hi-hat rolls and bass slides
- Flow: Seeds main RNG
- Ornament: Controls triplet mode (high ornament = triplets)
- Characteristic UK Drill patterns with hi-hat rolls and bass slides

**16. MINIMAL** - Staccato bursts and silence gaps
- Flow: Controls burst length (2-8 steps)
- Ornament: Controls click density and silence length
- Creates staccato bursts separated by silence gaps

**17. KRAFT** - Precise mechanical sequences
- Flow: Seeds main RNG for base note
- Ornament: Seeds ghost note mask
- Kraftwerk-style precise mechanical repetition patterns

**18. APHEX** - Polyrhythmic event sequencer
- Flow: Seeds patterns for 3 different tracks
- Ornament: Sets initial phase positions
- Creates polyrhythmic patterns across multiple time signatures

**19. AUTECH** - Algorithmic transformation engine
- Flow: Controls rule timing
- Ornament: Creates sequence of transformation rules
- Creates evolving patterns with algorithmic transformations

**20. STEPWAVE** - Scale stepping with chromatic trill
- Flow: Controls scale step direction
- Ornament: Controls trill direction and step size
- Creates scale-based stepping with chromatic trill variations

### 2.4 Parameters Common for Every Algo

#### Scan
- **Range:** 0 to (128 - actualLoopLength)
- **Function:** Determines the offset point on the 128 step long tape where the algorithm reads its data from, so you can choose what shorter loop to play expexting that is repeatable.
- **Effect:** For finite loops, scan offsets the window into the pre-generated pattern buffer
- **Usage:** Allows you to access different sections of the algorithm's infinite output without changing other parameters

#### Rotate
- **Range:** Bipolar (depends on loop length: from -maxRot to +maxRot where maxRot = loopLength - 1)
- **Function:** Rotates the start point within finite loops
- **Effect:** Shifts the entire pattern forward or backward within the loop boundaries
- **Usage:** Only applicable to finite loops (loopLength > 0), has no effect on infinite loops
- **Note:** Works in conjunction with scan - rotate shifts within the loop first, then scan offsets the entire loop into the infinite tape

#### Glide
- **Range:** 0 to 100%
- **Function:** Probability for pitch slides between notes
- **Effect:** When a note transition occurs, there's a percentage chance (determined by this parameter) that a glide will occur
- **Usage:** Each algorithm may implement glide differently, but generally affects portamento/slide behavior between notes
- **Implementation:** The algorithm consumes the random value (for consistent pattern generation) even if glide doesn't occur

#### GOffs
- **Range:** 0 to 100%
- **Function:** Probability for gate offset if it is hanled by algo, or plain
  gate offset probability. Resuls may vary.


#### CV Update Mode
- **Options:** Free or Gated
- **Free Mode:** CV output updates every step regardless of gate state (continuous evolution of the CV voltage), good for slide heavy algos.
- **Gated Mode:** CV output only updates when a gate fires (original Tuesday behavior where CV changes only with note events)
- **Effect:** Changes the character of pitch modulation - Free mode creates continuous pitch evolution, Gated mode creates stepped changes that follow the rhythm

#### Trill
- **Range:** 0 to 100%
- **Function:** Probability for retriggering/trilling effect on certain notes
- **Effect:** Adds rapid alternation between the main note and a variant (often at an interval like a second or third)
- **Usage:** Algorithms that support trilling will use this parameter to determine how often to add trill effects
- **Note:** Not all algorithms support trilling - it's implemented per algorithm based on musical context

#### Skew
- **Range:** -8 to +8
- **Function:** Density skew across the loop based on position, operates from
  base POWER setting, so has effect when POWER<8.
- **Effect:**
  - **Positive values (+1 to +8):** Creates build-up effect where density increases toward the end of the loop (first part at base power, last part at maximum power 16)
  - **Negative values (-1 to -8):** Creates fade-out effect where density decreases toward the end of the loop (first part at maximum power 16, last part at base power)
  - **Zero:** No skew (density is uniform throughout the loop)
- **Usage:** Creates dynamic variation in note density across the length of your loop for more musical and evolving patterns

### 2.6 Shortcuts Specific to TuesdayTrack

- **F1-F4:** Select parameter on current page, rotate the encoder to change
- **F5:** Advance to next parameter page (Page 1 → Page 2 → Page 3 → Page 1)
- **Shift+F5:** Reseed algorithm (generates new random pattern while keeping parameters)
- **Context Menu (Long Press):**
  - **INIT:** Reset all TuesdayTrack parameters to default values (sets algorithm to 0, flow to 0, ornament to 0, etc.)
  - **RESEED:** Regenerate algorithm pattern with current parameters

## 3. Harmony (Note Track Enchancement)

### 3.1 How the Harmony System Works

The Harmony Engine creates complex harmonic relationships between tracks by generating chords based on scale degrees and applying various harmonic transformations. It works by taking a root note from a "Master" track and generating harmonized chord tones that can be assigned to "Follower" tracks.

The system uses a combination of:

- **Scale intervals:** Based on the selected mode (Ionian, Dorian, Phrygian, etc.)
- **Chord qualities:** Determined by the mode (Major7, Minor7, Dominant7, HalfDim7)
- **Inversions:** Root, 1st, 2nd, and 3rd inversions
- **Voicings:** Close, Drop2, Drop3, and Spread voicings
- **Transpose:** Global transposition of all chord tones

### 3.2 Different Modes and Settings

**Harmony Modes:**

- **Ionian (0):** Major scale - I∆7, ii-7, iii-7, IV∆7, V7, vi-7, viiø
- **Dorian (1):** Dorian mode - i-7, ii-7, ♭III∆7, IV7, v-7, viø, ♭VII∆7
- **Phrygian (2):** Phrygian mode - i-7, ♭II∆7, ♭III7, iv-7, vø, ♭VI∆7, ♭vii-7
- **Lydian (3):** Lydian mode - I∆7, II7, iii-7, #ivø, V∆7, vi-7, vii-7
- **Mixolydian (4):** Mixolydian mode - I7, ii-7, iiiø, IV∆7, v-7, vi-7, ♭VII∆7
- **Aeolian (5):** Natural minor - i-7, iiø, ♭III∆7, iv-7, v-7, ♭VI∆7, ♭VII7
- **Locrian (6):** Diminished scale - iø, ♭II∆7, ♭iii-7, iv-7, ♭V∆7, ♭VI7, ♭vii-7

**Voicing Options:**

- **Close (0):** All chord tones in closest possible position
- **Drop2 (1):** Second highest note dropped down an octave
- **Drop3 (2):** Third highest note dropped down an octave
- **Spread (3):** Third and fifth above root, seventh an octave higher

### 3.3 How to Use Harmony Roles (Master/Follower)

**Setting up Master/Follower Relationships:**
1. Configure one track as the "Master" track (typically a Note track playing scale degrees)
2. Enable Harmony Engine on the Master track with desired settings
3. Set Following type on ANOTHER note track
4. Each Master track can generate up to 4 harmonized outputs (root, third, fifth, seventh)
5. By default Performer settings tracks designated as Followers will update
   their CV output on Gate events. So enable gates where you want to hear a
chord.

**Master Track Configuration:**
-
- The root note of the chord is determined by the Master track's current note
The Harmony Engine converts these scale degrees to chord tones based on the selected mode
- Per step overrides of Inversion and Voicing are available on the NOTE(F4)
button layer for Master tracks(push F4 several times to get there)

**Follower Track Configuration:**
- Follower tracks receive the harmonized chord tones from the Master
- Each Follower can be assigned to a specific chord tone (root, third, fifth, or seventh)

```
!!! Followers DON'T change their Harmony Scale automatically if you change
master!!!
```

- Multiple Followers can be assigned to the same chord tone for thickening
- Per step overrides of Chord Degree (root,3,5,7) are available on the NOTE(F4)
button layer for Master tracks(push F4 several times to get there)

### 3.4 Examples of Creating Harmonies

**Major Harmony Example:**

- Master Track: Ionian mode, Scale Degree 0 (root)
- Chord Output: Cmaj7 (C-E-G-B)
- Follower 1: Assigned to Root → Plays C
- Follower 2: Assigned to Third → Plays E
- Follower 3: Assigned to Fifth → Plays G
- Follower 4: Assigned to Seventh → Plays B

**Minor Harmony Example:**

- Master Track: Dorian mode, Scale Degree 1 (ii chord)
- Chord Output: Dm7 (D-F-A-C)
- Follower 1: Close voicing → D-F-A-C
- Follower 2: Drop2 voicing → D-A-C-F

**Complex Harmony Example:**

- Master Track: Mixolydian mode, Scale Degree 4 (v chord)
- Chord Output: G7 (G-B-D-F)
- Follower 1: Root position → G-B-D-F
- Follower 2: 1st inversion → B-D-F-G
- Follower 3: 2nd inversion → D-F-G-B
- Follower 4: 3rd inversion → F-G-B-D

### 3.5 Shortcuts Related to Harmony

- **SECOND PRESS of Shift-S3**  Harmony Engine
- **Voicing Quick Switch:** Use encoder to cycle through voicing options (Close, Drop2, Drop3, Spread)
- **Inversion Control:** Use parameter to cycle through inversions (0-3)
- **Transpose Adjust:** Use encoder with shift to adjust transpose in larger steps

## 4. Accumulator

### 4.1 What the Accumulator Does

The Accumulator functions as a counter that can count up, down, or in complex patterns based on trigger inputs. The accumulator maintains a current value that can be used to modulate various parameters in real-time. Currently it is used ONLY to move tracks pitch every playhead pass. Not every update mode is implemented.

Key features:
- Counts trigger events and maintains a running value
- Supports multiple counting modes (Wrap, Pendulum, Random, Hold)
- Provides unipolar or bipolar output ranges
- Can be triggered by step events, gate events, or retrigger events

```
!!! Currently Rettigger events advance Accum counter by the number of
retrigers of the step, so by modulating retrigger value you can get different
amount of scale increments!!!
```

Accumulator is enabled by default. Push NOTE(F4) several times to get to
Accumulator page with. Push step and rotate encoder to change accumulator
increments for the step. Small persistend dot will appear in top right corner of the scuare.

Number in the top line indicates current Accum counter number.

### 4.2 Different Modes and Settings

**Polarity Options:**

- **Unipolar (0):** Output range from 0 to maxValue
- **Bipolar (1):** Output range from minValue to maxValue (centered around 0)

**Activation Options:**

- **TRACK:** All the steps will be transosed in following accumulator counter
increase.
- **STAGE:** Only the steps with accumulator triggers on them will be updated.

**Direction Options:**

- **Up (0):** Count in positive direction
- **Down (1):** Count in negative direction
- **Freeze (2):** Hold current value, no counting

**Order (Counting Pattern):**

- **Wrap (0):** Wraps around when reaching bounds (0→max→0 or max→0→max)
- **Pendulum (1):** Bounces between min and max values (0→max→0→max...)
- **Random (2):** Jumps to random values within range
- **Hold (3):** Moves toward bounds and holds when reached

**Trigger Modes:**

- **Step (0):** Responds to step advancement
- **Gate (1):** Responds to gate events
- **Retrigger (2):** Responds to retrigger events(adds number of retriggers
to)


### 4.5 Shortcuts Related to Accumulator

- **SECOND PRESS of Shift-S3** Navigate to ACCUM page for main controls
- **THIRD PRESS of Shift-S3** Access detailed parameter controls on ACCST page
**Shift+F4:** Resets accum to 0



## Pulse Count and Gate Mode (Note Track Enchancement)

### Pulse Count

  Pulse Count is a feature that allows a single step in a sequence to play for
  multiple clock pulses before advancing to the next step. This enables more
  complex rhythmic patterns where individual steps can be subdivided.

  **Technical Details**:

   - Pulse Count values range from 0 to 7, representing 1 to 8 pulses
   - A value of 0 means the step advances after 1 pulse (normal behavior)
   - A value of 7 means the step holds for 8 total pulses before advancing

### Gate Mode

  Gate Mode determines which of the multiple pulses (when using Pulse Count) will
  actually fire a gate signal. This allows for complex rhythmic patterns where not
  every pulse in a step necessarily triggers an output.

  **Technical Details**:
   - Gate Mode values range from 0 to 3, representing 4 different modes
   - Implemented as a 2-bit field in the step data structure
   - Works in conjunction with Pulse Count to create rhythmic variations

  The four Gate Modes:

   1. All (0): Fires gates on every pulse (default, backward compatible)
      - When pulse count is set to 3 (4 pulses), all 4 pulses will fire gates
      - Creates the most basic retriggering pattern

   2. First (1): Single gate on first pulse only
      - Only the first pulse in the sequence fires a gate
      - The step still holds for the specified number of pulses, but only triggers
        once
      - Useful for creating longer notes with rhythmic timing

   3. Hold (2): One long gate for entire duration
      - Single gate starts on the first pulse and continues to the end
      - The gate length is extended to cover all pulses defined by pulse count
      - Creates sustained notes that follow the pulse count timing

   4. FirstLast (3): Gates on first and last pulse only
      - Gates fire on the first pulse and the last pulse of the sequence
      - Creates an "bookend" pattern with silence in between
      - Useful for creating rhythmic accents at the beginning and end of a held note

## 5. Global Output Rotation (CV & Gate)

This feature allows you to dynamically rotate which tracks are assigned to physical CV and Gate outputs. It acts like a virtual 8-channel sequential switch, letting your modulation patterns "spin" around your connected modules.

### How it Works

The rotation applies to the physical output's lookup of its assigned track. When rotation is applied, the output will instead look for its source from a shifted position within a dynamically defined "pool" of rotating tracks.

- **Selective Rotation:** Only tracks explicitly enabled for rotation (via the Routing page) will participate. Static tracks remain untouched.
- **Dynamic Pool:** The system identifies all tracks currently targeted for rotation. These tracks form a "pool" that rotates amongst themselves.

### Setup Instructions

1.  **Configure Track Layout:**
    *   Navigate to the **Layout Page** (usually by pressing SHIFT + S2, then selecting "LAYOUT").
    *   Go to **CV Output Mode** (F4). For each physical CV output (1-8), select the **Track** that should usually drive it.
    *   Repeat for **Gate Output Mode** (F3).
    *   *Example:* CV Out 1 -> Track 1, CV Out 2 -> Track 2, CV Out 3 -> Track 3.

2.  **Create a Rotation Route:**
    *   Navigate to the **Routing Page** (usually by pressing SHIFT + S2, then selecting "ROUTING").
    *   Select an **empty Route slot**.
    *   Set **Target** to `CV Out Rot` (for CV output rotation) or `Gate Out Rot` (for Gate output rotation).
    *   Set **Source** (e.g., `CV In 1`, a slow LFO, or MIDI CC). This source will control the speed and direction of the rotation.
    *   Adjust **Min** and **Max** to define the range of rotation steps. The default is 0 to 8, allowing up to 8 steps of rotation. You can set it to -8 to 8 for bipolar control.
    *   **Crucially: Select the Tracks to Rotate.** In the "Tracks" section of the Routing page, use the track selection mask to **select only the tracks you want to participate in this rotation pool**. For example, if you want Tracks 1, 2, and 3 to rotate, select Track 1, Track 2, and Track 3.
    *   **COMMIT** the route.

### Example Use Case

*   **Goal:** Rotate 3 LFOs (Track 1, 2, 3) across 3 filter inputs (CV Out 1, 2, 3).
*   **Layout Page:**
    *   CV Out 1 -> Track 1
    *   CV Out 2 -> Track 2
    *   CV Out 3 -> Track 3
    *   CV Out 4 -> Track 4 (Static, e.g., for a bassline)
*   **Routing Page:**
    *   Target: `CV Out Rot`
    *   Source: `CV In 1` (LFO source)
    *   Min: `0`, Max: `3` (to rotate 3 steps)
    *   Tracks: `X X X - - - - -` (Track 1, 2, 3 selected)
*   **Effect:** As `CV In 1` modulates from 0 to 3, the output of Track 1 will move from CV Out 1 -> 2 -> 3 -> 1, Track 2 will move 2 -> 3 -> 1 -> 2, and so on. CV Out 4 will remain static, always receiving from Track 4.

## 6. New Curve Track Features

### Global Phase

The Global Phase feature allows you to offset the playback position of your curve sequence. This shifts the entire sequence playback by a specified phase amount without changing the actual step data.

- **Access:** Press F5 to cycle through edit modes: Step → Phase → Wavefolder → Step
- **Function:** When enabled, the sequence will read values from a position that's offset by the phase value
- **Range:** 0.00 to 1.00 (can be adjusted with encoder, press for 0.1 steps, release for 0.01 steps)
- **Effect:** The phase offset is visualized with a dimmer vertical line showing the phased step position compared to the actual current step

### Shift+Step Shortcut for Multi-Step Shape/Min/Max Editing

When multiple steps are selected in a CurveSequence, you can use shift with encoder to create smooth transitions across the selected steps:

- **Multi-Step Shape Editing:** Select multiple steps, press Shift while turning the encoder - this will set all selected steps to the same shape but create a smooth transition from the minimum to maximum values across the steps
- **Shift+Encoder with Multiple Steps Selected:** When you have multiple steps selected and turn the encoder with shift, it creates a gradient of min/max values across the selected steps

### Wavefolding Features

The CurveTrack includes advanced wavefolding capabilities accessible through the Phase button:

- **Access:** Press F5 repeatedly to cycle to the Wavefolder screen
- **Parameters:**
  - **FOLD:** Controls the folding amount (0.00 to 1.00) - increases harmonic complexity
  - **GAIN:** Controls input gain (0.00 to 2.00) - adjusts signal strength before folding
  - **FILTER:** DJ filter control (-1.00 to 1.00) - applies low-pass (negative) or high-pass (positive) filtering
  - **XFADE:** Crossfade between original and processed signal (0.00 to 1.00) - blend between dry and wet signals

### LFO Context Menu Feature

A quick way to fill sequences with common LFO waveforms:

- **Access:** Press and hold the "STEP 5" button while on a CurveSequence page (not CurveSequenceEdit page)
- **Options:**
  - **TRI:** Triangle wave pattern using Triangle curve shapes
  - **SINE:** Sine wave approximation using SmoothUp/SmoothDown alternating
  - **SAW:** Sawtooth wave pattern using RampUp shapes
  - **SQUA:** Square wave pattern using StepUp/StepDown alternating

These features enhance the CurveTrack's capabilities, allowing for more complex modulation patterns and easier creation of LFO-style CV sequences.

### Phase Visualization

When Global Phase is active, the engine continues to track both the actual step position and the phased step position:

- The actual current step position is displayed with a brighter vertical line
- The phased step position is shown with a dimmer vertical line
- This visual feedback helps you understand how the phase offset is affecting the sequence playback

These new features make the CurveTrack significantly more powerful for creating complex modulation sources, from simple LFOs to intricate evolving modulation patterns with wavefolding and phase shifting capabilities.


