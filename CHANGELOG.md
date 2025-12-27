# Changelog

## [Unreleased]

### Added
- **Algo (Tuesday) Track Enhancements**:
  - **Context Menu**: Added INIT, RESEED, RAND, COPY, PASTE options
  - **3-Slot Clipboard**: Copy/Paste 1-3 on Steps 9-14 (Quick Edit)
  - **Expanded Clipboard**: Now includes Octave, Transpose, Root Note, Divisor, Mask
  - **Reset Shortcut**: Shift+Step 16 (index 15) hard resets the track engine
  - **Status Box**: Visual feedback for Note, Gate, CV, and Step
  - **Quick Edit**: Randomize parameters (Step 15)
  - **Reseed**: Shift+F5 shortcut and Context Menu option
- **Initialization Defaults**:
  - Curve, Indexed, and DiscreteMap tracks now default to **Free** play mode
  - Curve and DiscreteMap sequences now default to **Divisor 192**
- **Routing Enhancements**:
  - **VCA Next Shaper (VC)**: Amplitude modulation using the next route's raw source value
  - **Per-Track Reset Target**: Hard reset of track engine on rising edge of routed signal
- **New Track Type: DiscreteMap**:
  - Threshold-based sequencer mapping input voltages (Internal Ramp or External CV) to 32 discrete output stages
  - Features: Directional triggering (Rise/Fall), Scale quantization, Slew
  - **Scanner Target**: `DMap Scan` routing target that toggles stage directions via CV
  - **Generators**: Random, Toggle, Linear, Logarithmic, Exponential generation of thresholds/notes
  - **Quick Edit**: `Even` distribution and `Flip` directions shortcuts
- **New Track Type: Indexed**:
  - Duration-based sequencer where each step has an independent length in clock ticks
  - Features: Non-grid timing, Polyrhythms, Group-based modulation
  - **Math Page**: Batch operations (Add, Sub, Mul, Div, Ramp, Quantize, Random, Jitter)
  - **Quick Edit**: `Set First Step` (Rotate), `Active Length`, `Run Mode`, `Reset Measure`
- **Documentation**: Comprehensive updates to XFORMER_MANUAL and individual track manuals
- **Accumulator UI Enhancements**: Visual indicators for accumulator state across all Note track pages
  - **Counter display**: Shows current accumulator value (e.g., "+5", "-12") in header at x=176 (aligned with step 12)
    - Uses Font::Tiny to match step number size
    - Uses Color::Medium for subtle display that doesn't compete with primary elements
    - Only visible when accumulator is enabled
    - Displayed on all Note track pages: STEPS, SEQUENCE, ACCUM, ACCST, HARMONY
  - **Corner dot indicator**: Small dot in top-right corner of steps with accumulator trigger enabled
    - Visible across all pages and layers
    - Uses contrasting color: black (Color::None) when gate on, bright when gate off
    - AccumulatorTrigger layer inherits gate squares and shows corner dots for trigger-enabled steps
  - New WindowPainter::drawAccumulatorValue() function for reusable counter display
- **Gate Mode feature**: Per-step gate firing control during pulse count repetitions with 4 distinct modes
  - Gate mode accessible via Gate button cycling (GATE → GATE PROB → GATE OFFSET → SLIDE → GATE MODE)
  - Four gate modes:
    - **A (ALL)**: Fires gates on every pulse (default, backward compatible)
    - **1 (FIRST)**: Single gate on first pulse only, silent for remaining pulses
    - **H (HOLD)**: One long gate held high for entire step duration
    - **1L (FIRSTLAST)**: Gates on first and last pulse only
  - Visual display showing compact mode abbreviations (A/1/H/1L) for each step
  - Detail overlay showing mode abbreviation when adjusting with encoder
  - Seamless integration with pulse count feature for powerful rhythmic control
  - Comprehensive unit tests for gate mode model layer (6 test cases, all passing)
  - Full compatibility with all play modes, retrigger, gate offset, and gate probability
- **Pulse Count feature**: Metropolix-style step repetition where each step can repeat for 1-8 clock pulses before advancing
  - Per-step pulse count parameter (0-7 representing 1-8 pulses)
  - Pulse count accessible via Retrigger button cycling (RETRIG → RETRIG PROB → PULSE COUNT)
  - Visual display showing pulse count value (1-8) for each step
  - Detail overlay showing pulse count value when adjusting with encoder
  - Pulse counter state tracking in NoteTrackEngine for timing control
  - Full integration with both Aligned and Free play modes
  - Comprehensive unit tests for pulse count model layer (7 test cases, all passing)
- **Accumulator functionality**: Stateful counter that increments/decrements based on configurable parameters
- Accumulator parameters: Mode, Direction (Up/Down/Freeze), Order (Wrap/Pendulum/Random/Hold), Polarity (Unipolar/Bipolar), Min/Max values, and Step Size
- Real-time pitch modulation using accumulator value
- ACCUM page for editing accumulator parameters
- ACCST page for per-step accumulator trigger configuration
- Per-step accumulator trigger flags to enable/disable triggering on specific steps
- Unit tests for accumulator functionality and modulation
- **Accumulator Trigger Mode feature**: Control when accumulator increments with three distinct modes
  - TRIG parameter accessible on ACCUM page via encoder cycling (STEP → GATE → RTRIG)
  - Three trigger modes:
    - **STEP**: Increment once per step (default, backward compatible)
    - **GATE**: Increment per gate pulse (respects pulse count and gate mode)
    - **RTRIG**: Increment N times for N retriggers (all ticks fire immediately at step start)
  - Visual display showing mode names (STEP/GATE/RTRIG) in ACCUM page
  - TriggerMode enum with 2-bit bitfield for memory efficiency
  - Packed with _hasStarted in single byte for optimized serialization
  - Full integration with STEP/GATE/RTRIG logic in NoteTrackEngine
  - **Known behavior**: RTRIG mode ticks fire immediately at step start (burst mode) due to gate queue architecture
    - See RTRIG-Timing-Research.md for technical investigation and workaround analysis
    - See Queue-BasedAccumTicks.md for detailed implementation plan if future enhancement needed
    - Recommendation: Accept current behavior (pointer invalidation risks in queue-based approaches)
  - All unit tests updated and passing (15 test cases in TestAccumulator)
- **Harmony Feature**: Harmonàig-style harmonic sequencing with master/follower relationships
  - **Core Parameters**:
    - **HarmonyRole**: Off, Master, FollowerRoot, Follower3rd, Follower5th, Follower7th
    - **MasterTrackIndex**: Range 0-7 (tracks 1-8) specifying which track to follow
    - **HarmonyScale**: 7 Ionian modes (0-6): Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian
  - **Per-Step Overrides for Master Tracks**:
    - **Per-Step Inversion Override**: SEQ, ROOT, 1ST, 2ND, 3RD
    - **Per-Step Voicing Override**: SEQ, CLOSE, DROP2, DROP3, SPREAD
  - **UI Integration**: Harmony page accessible via Sequence button cycling (NoteSequence → Accumulator → Harmony)
  - **HarmonyEngine**: Core harmonization logic with all 7 Ionian modes and diatonic chord qualities
  - **Inversion Algorithm**: All 4 inversions fully implemented (Root, 1st, 2nd, 3rd)
  - **Voicing Algorithm**: All 4 voicings fully implemented (Close, Drop2, Drop3, Spread)
  - **19 Passing Unit Tests** across HarmonyEngine, NoteSequence, and Model integration
  - **Modulation Order**: Base note + transpose → Harmony → Accumulator → Note variation
  - **Full Compatibility**: Works with all existing features (Accumulator, Note variation, Gate modes, etc.)
- **Tuesday Track**: TUE track type providing generative/algorithmic sequencing inspired by Mutable Instruments Marbles, Noise Engineering Mimetic Digitalis, and Tuesday Eurorack module
  - **Core Parameters**:
    - **Algorithm**: 0-12 (TEST, TRITRANCE, STOMPER, MARKOV, CHIPARP, GOACID, SNH, WOBBLE, TECHNO, FUNK, DRONE, PHASE, RAGA)
    - **Flow**: 0-16, sequence movement/variation (seeds RNG)
    - **Ornament**: 0-16, embellishments and fills (seeds extra RNG)
    - **Power**: 0-16, number of notes in loop (linear note count)
    - **LoopLength**: Inf, 1-64, pattern length
    - **Glide**: 0-100%, slide probability
    - **Scale**: Free/Project, note quantization mode
    - **Skew**: -8 to +8, density curve across loop
  - **TuesdayPage UI**: F1-F5 select parameters with visual bar graphs showing values 0-16
  - **Reseed Functionality**: Shift+F5 shortcut and context menu RESEED option
  - **13 Implemented Algorithms** with genre-specific musical characteristics
  - **Dual RNG System**: Flow/Ornament create deterministic patterns (same values = same patterns)
  - **Long Gates**: Support for up to 400% gate length for sustained notes
  - **Scale Quantization**: Can use project scale or chromatic
  - **Advanced Controls**: CV Update Mode (Free/Gated), Rotate (bipolar), Scan, Sequence parameters
- **Tuesday Sequence Page**: New TuesdaySequencePage with sequence-related parameters
  - **Rotate** parameter: Bipolar rotation (±32767) for pattern transformation
  - **Scan** parameter: Scan position (0-100%) within loop
  - **CV Update Mode**: Free (continuous) or Gated (only when note plays)
  - **Playhead progress indicator**: Visual progress through pattern
- **Simplified Musical Algorithms**: 10 genre-specific algorithms optimized for Tuesday track
  - **SIMPLE_AMBIENT**: Long sustained notes with slides for ambient feel
  - **SIMPLE_TECHNO**: Driving 4/4 patterns with occasional syncopation
  - **SIMPLE_JAZZ**: Swing feel with chord tones and scale notes
  - **SIMPLE_CLASSICAL**: Counterpoint patterns with smooth transitions
  - **SIMPLE_MINIMALIST**: Phasing patterns with consistent rhythm
  - **SIMPLE_BREAKBEAT**: Classic breakbeat rhythms with bass/snare
  - **SIMPLE_DRONE**: Sustained drone with occasional harmony
  - **SIMPLE_ARPEGGIO**: Ascending/descending arpeggiated patterns
  - **SIMPLE_FUNK**: Syncopated bass lines with groove
  - **SIMPLE_RAGA**: Traditional ascending/descending melodic patterns

### Changed
- NoteTrackEngine gate firing logic now respects gate mode setting
- Gate generation in triggerStep() uses switch statement to control gate firing based on mode
- HOLD mode extends gate length to cover entire step duration (divisor * (pulseCount + 1))
- NoteSequenceEditPage updated with gate mode layer integration in Gate button cycling
- NoteTrackEngine now tracks pulse counter state to control step advancement timing
- Step advancement logic modified to respect pulse count values (steps hold for N pulses)
- Step advancement reordered: triggerStep() now called BEFORE counter reset and step advance
- Pulse count lookup changed from stale _currentStep to authoritative _sequenceState.step()
- NoteSequenceEditPage updated with pulse count layer integration
- Moved accumulator trigger toggling from Gate button (F1) to Note button (F4) cycling
- Updated NoteSequenceEditPage to include AccumulatorTrigger in Note button cycling sequence
- Modified triggerStep() to include accumulator ticking when step has isAccumulatorTrigger set
- Updated evalStepNote() to incorporate accumulator value into pitch calculation
- Added mutable member pattern to accumulator for thread-safe operation
- Memory-optimized accumulator using bitfields for parameters
- **NoteTrackEngine trigger mode integration**:
  - STEP mode ticks added at line ~353 with triggerMode check
  - GATE mode ticks added at line ~392 inside shouldFireGate block
  - RTRIG mode ticks added at line ~410 with immediate N-tick loop
- **AccumulatorListModel UI integration**:
  - Added TriggerMode to Item enum for ACCUM page display
  - Implemented indexed value support for encoder cycling through modes
  - Added formatValue() cases for displaying STEP/GATE/RTRIG text
- **Accumulator initialization order**:
  - Fixed member initialization to follow declaration order
  - Added constructor body assignment for _currentValue = _minValue
- **TestAccumulator test suite**:
  - Updated all tests to consume delayed first tick before assertions
  - Fixed tick_down_enabled test to allow negative values with Hold mode
  - Added reset() calls where needed to sync currentValue after parameter changes
- **Harmony Engine Integration**:
  - Direct integration in NoteTrackEngine::evalStepNote() following Accumulator modulation pattern
  - Master/follower relationships implemented with synchronized step playback
  - Harmony modulation applied after base note + transpose, before accumulator
  - Per-step inversion/voicing overrides read from master sequence during harmonization
- **TuesdayTrack Implementation**:
  - Dual RNG seeding from Flow/Ornament parameters for deterministic patterns
  - Cooldown-based density control for Power parameter (linear note count)
  - Algorithm-specific gate length variations with support for long gates (up to 400%)
  - Clock sync and deterministic loops for consistent pattern generation
  - Pre-generated 128-step buffer for finite loops and 256-step warmup for mature patterns
  - UI improvements: Paginated TuesdayEditPage with S1 navigation and parameter alignment
- **Tuesday Sequence Parameters**:
  - Added Rotate parameter for pattern transformation with bipolar range
  - Added Scan parameter for position within loop
  - Added CV Update Mode parameter (Free/Gated) for pitch change behavior
  - Added playhead progress indicator for visual feedback
  - Added bounds checks and safety guards to prevent segfaults
- **Per-Step Inversion/Voicing Implementation**:
  - Added per-step override bitfields in NoteSequence::Step (bits 25-27, 28-30)
  - Updated UI layer to show compact abbreviations (S/R/1/2/3 for inversion, S/C/2/3/W for voicing)
  - Modified harmonization to read per-step overrides from master steps when following
- **TUE Track UI Integration**:
  - Updated TrackPage with Shift+F5 reseed shortcut and context menu RESEED option
  - Added visual bar graphs for parameter values in TuesdayPage
  - Added TuesdaySequencePage for sequence-level parameters
  - Implemented algorithm-specific parameter mapping for Flow/Ornament controls

### Fixed
- **Critical gate mode bug**: Fixed pulse counter timing issue where triggerStep() was called after counter reset
  - Root cause: _pulseCounter was reset before triggerStep() could read it
  - Fix: Reordered operations to call triggerStep() before advancing step
  - Impact: Gate modes now see correct pulse counter values (1, 2, 3...)
- **Critical pulse count bug**: Fixed step index lookup using stale _currentStep instead of _sequenceState.step()
  - Root cause: _currentStep is set inside triggerStep() and becomes stale after advancement
  - Fix: Changed to use _sequenceState.step() which is authoritative
  - Impact: Pulse count now reads from correct step index
- **RETRIGGER mode bug**: Fixed retrigger=1 not incrementing accumulator
  - Root cause: Tick code only in `stepRetrigger > 1` branch, else branch had no tick
  - Fix: Added accumulator tick in else branch for retrigger=1 case
  - Impact: retrigger=1 now correctly increments once in RTRIG mode
- **RETRIGGER mode bug**: Fixed accumulator only incrementing once despite multiple retriggers
  - Root cause: Tick code inside `while (retriggerOffset <= stepLength)` loop, which exits early when LENGTH is short
  - Fix: Moved accumulator ticks to separate for loop BEFORE gate queue scheduling
  - Impact: Now ticks exactly N times for N retriggers, regardless of LENGTH parameter
- **Accumulator initialization bug**: Fixed undefined behavior from incorrect member initialization order
  - Root cause: `_currentValue` initialized using `_minValue` before `_minValue` was initialized
  - Fix: Initialize both in declaration order, then set `_currentValue = _minValue` in constructor body
  - Impact: All accumulator tests now pass without garbage values
- **Test failures**: Updated TestAccumulator suite for delayed first tick behavior
  - Fixed 8 test cases to consume delayed first tick before assertions
  - Fixed tick_down_enabled to use Hold mode with negative minValue range
  - All 15 test cases now passing
- Test compilation errors related to dummy implementation classes
- GateOutput constructor dependencies in unit tests
- **Tuesday Track Serialization Bug**: Fixed missing version guards causing "Failed to load (end_of_file)" errors
  - Root cause: TuesdayTrack::read() had no version guards
  - Fix: Added ProjectVersion::Version35 with version guards on all read() calls
  - Impact: Projects with Tuesday tracks load correctly
- **Tuesday Track Division by Zero Bug**: Fixed hardware reboots when loading projects with Tuesday tracks
  - Root cause: PHASE `_phaseLength` and DRONE `_droneSpeed` defaulted to 0, causing modulo by zero
  - Fix: Added safe defaults (4 and 1) plus runtime guards at all modulo operations
  - Impact: Hardware no longer reboots when loading projects with Tuesday tracks
- **Harmony Inversion Bug**: Fixed where harmony inversion/voicing were read from master sequence instead of follower
  - Root cause: Per-step inversion/voicing parameters were being read from wrong sequence during harmonization
  - Fix: Updated to read per-step overrides from master sequence when harmonizing followers
  - Impact: Per-step inversion and voicing overrides now work correctly
- **Skew Calculation Safety**: Added safety guards to prevent segfault in skew calculation
  - Root cause: Possible division by zero or invalid index access in skew calculations
  - Fix: Added bounds checking and safe defaults in skew implementation
  - Impact: Skew parameter now works safely with all values
- **Loop Length Display**: Corrected LOOP parameter max for proper bar display (64 = full bar)
  - Root cause: Incorrect max value in UI display
  - Fix: Updated maximum value to match actual range
  - Impact: Loop length now displays correctly in UI
- **Tuesday Algorithm Fixes**: Fixed CHIPARP/GOACID algorithms and ensured Glide=0 properly disables slides
  - Root cause: Placeholder implementations and Glide parameter not properly respected
  - Fix: Replaced placeholders with proper implementations, added Glide=0 handling
  - Impact: All algorithms now work correctly with proper Glide behavior
- **Per-Step Harmony Role Override**: Added proper handling for per-step harmony role overrides
  - Root cause: Per-step harmony role parameters were not being applied correctly
  - Fix: Added proper UI and engine handling for per-step role overrides
  - Impact: Users can now set per-step harmony roles
- **CV Update Mode Implementation**: Added proper implementation of CV Update Mode feature
  - Root cause: Continuous pitch evolution vs gated behavior was not configurable
  - Fix: Added Free/Gated CV update mode parameter with proper engine support
  - Impact: Users can choose between continuous or gated pitch updates
- **Tuesday Track Buffer Generation**: Fixed buffer regeneration to happen instantly when parameters change
  - Root cause: Delayed buffer updates led to stale patterns
  - Fix: Added immediate buffer regeneration on parameter changes
  - Impact: Parameter changes now immediately affect pattern output
- **Tuesday Track Scan Parameter**: Fixed Scan parameter to work with finite loops, not just infinite
  - Root cause: Scan was only implemented for infinite loops
  - Fix: Extended Scan functionality to finite loops
  - Impact: Scan now works with all loop types
- **Tuesday Track Rotate Parameter**: Limited Rotate to loop length for easier use
  - Root cause: Rotate values beyond loop length were confusing
  - Fix: Constrained Rotate to valid loop range
  - Impact: More intuitive Rotate parameter behavior
- **Tuesday Track Reset Issue**: Disabled reset measure for infinite loops to allow truly infinite patterns
  - Root cause: Reset was interrupting infinite loops
  - Fix: Removed reset measure for infinite loop patterns
  - Impact: Truly infinite patterns now possible without interruption
- **Tuesday Track Power Parameter**: Changed Power parameter to be linear (Power = number of notes)
  - Root cause: Non-linear power was confusing for users
  - Fix: Made Power represent actual note count in loop
  - Impact: More intuitive power parameter behavior
- **Tuesday Track Skew Function**: Changed skew from ramp to step function for clearer density control
  - Root cause: Ramp function was not providing clear density variation
  - Fix: Implemented step function for more distinct density changes
  - Impact: Clearer and more predictable skew behavior
- **Tuesday Track Clock Divisor Drift**: Fixed timing drift when using non-default clock divisors (1/8, 1/4, etc.)
  - Root cause: Step index was incremented instead of calculated from tick count, causing timing misalignment
  - Fix: Calculate stepIndex from relativeTick/divisor and align resetDivisor to loop duration multiples
  - Impact: Tuesday tracks now stay in perfect sync with other tracks at all clock divisions
- **Tuesday Track Display Step Sync**: Fixed playhead display being ahead of actual playback
  - Root cause: _stepIndex was incremented at end of processing, making UI show next step
  - Fix: Added _displayStep variable set before processing for accurate UI sync
  - Impact: OverviewPage playhead now matches actual step being played
- **Tuesday Track Loop Length Options**: Added missing loop length values
  - Added: 95, 96, 127, 128 to loop length selection
  - Impact: More flexibility in loop length choices
- **Tuesday Track Scan Clamping**: Fixed scan parameter to respect loop boundaries
  - Root cause: Scan could exceed buffer bounds when loop length changed
  - Fix: Dynamic scan clamping based on (128 - loopLength), re-clamp when loop length changes
  - Impact: Scan parameter now properly constrained to valid range
- **Tuesday Track OverviewPage Display**: Added TuesdayTrack visualization to OverviewPage
  - Shows blinking gate indicator and step counter (step/loopLength for finite loops)
  - Matches NoteTrack infobox display style
  - Impact: Visual feedback for Tuesday track state on overview

## v0.1.42 (6 June 2022)

#### Fixes

- Support Launchpad Mk3 with latest firmware update (thanks to Novation support)

## v0.1.41 (18 September 2021)

#### Improvements

- Add support for Launchpad Pro Mk3 (thanks to @jmsole)
- Allow MIDI/CV track to only output velocity (#299)

#### Fixes

- Fix quick edit (#296)
- Fix USB MIDI (thanks to @av500)

## v0.1.40 (9 February 2021)

Note: This release changes the behavior of how note slides are generated. You may need to adjust the _Slide Time_ on existing projects to get a similar feel.

#### Improvements

- Improved behavior of slide voltage output (#243)
- Added _Play Toggle_ and _Record Toggle_ routing targets (#273)
- Retain generator parameters between invocations (#255)
- Allow generator parameters to be initialized to defaults
- Select correct slot when saving an autoloaded project (#266)
- React faster to note events when using MIDI learn
- Filter NPRN messages when using MIDI learn

#### Fixes

- Fixed curve playback cursor when track is muted (#261)
- Fix null pointer dereference in simulator (#284)

## v0.1.39 (26 October 2020)

#### Improvements

- Added support for Launchpad Mini MK3 and Launchpad X (#145)
- Improved performance of sending MIDI over USB

## v0.1.38 (23 October 2020)

#### Fixes

- Fixed MIDI output from monitoring during playback (#216)
- Fixed hanging step monitoring when leaving note sequence edit page

#### Improvements

- Added _MIDI Input_ option to select MIDI input for monitoring/recording (#197)
- Added _Monitor Mode_ option to set MIDI monitoring behavior
- Added double tap to toggle gates on Launchpad controller (#232)

## v0.1.37 (15 October 2020)

#### Fixes

- Output curve track CV when recording (#189, #218)
- Fix duplicate function on note/curve sequence page (#238)
- Jump to first row when switching between user scales
- Fixed printing of route min/max values for certain targets

#### Improvements

- Added _Fill Muted_ option to note tracks (#161)
- Added _Offset_ parameter to curve tracks (#221)
- Allow setting swing on tempo page when holding `PERF` button
- Added inverted loop conditions (#162)
- Improved step shifting to only apply in first/last step range (#196)
- Added 5ms delay to CV/Gate conversion to avoid capturing previous note (#194)
- Allow programming slides/ties using pitch/modulation control when step recording (#228)
- Added _Init Layer_ generator that resets the current layer to its default value (#230)
- Allow holding `SHIFT` for fast editing of route min/max values

## v0.1.36 (29 April 2020)

#### Fixes

- Update routings right after updating each track to allow its CV output to accurately modulate the following tracks (#167)

#### Improvements

- Added fill and mute functions to pattern mode on Launchpad (#173)
- Added mutes to song slots (#178)
- Added step monitoring on curve sequences (#186)
- Added a `hwconfig` to support DAC8568A (in addition to the default DAC8568C)

## v0.1.35 (20 Jan 2020)

#### Fixes

- Fix loading projects from before version 0.1.34 (#168)

## v0.1.34 (19 Jan 2020)

**PLEASE DO NOT USE THIS VERSION, IT CONTAINS A BUG PREVENTING IT FROM READING OLD PROJECT FILES!**

#### Fixes

- Fix inactive sequence when switching track mode (#131)

#### Improvements

- Added _Scale_ and _Root Note_ as routing targets (#166)
- Expanded number of MIDI outputs to 16 (#159)
- Expanded routable tempo range to 1..1000 (#158)
- Generate MIDI output from track monitoring (#148)
- Allow MIDI/CV tracks to consume MIDI events (#155)
- Default MIDI output note event settings with velocity 100
- Indicate active gates of a curve sequence on LEDs

## v0.1.33 (12 Nov 2019)

#### Fixes

- Fixed handling of root note and transposing of note sequences (#147)

#### Improvements

- Add mute mode to curve tracks to allow defining the mute voltage state (#151)
- Increased double tap time by 50% (#144)

## v0.1.32 (9 Oct 2019)

#### Fixes

- Fix _Latch_ and _Sync_ modes on permanent _Performer_ page (#139)

## v0.1.31 (15 Aug 2019)

#### Improvements

- Added _Slide Time_ parameter to MIDI/CV track (#121)
- Added _Transpose_ parameter to MIDI/CV track (#128)
- Allow MIDI/CV tracks to be muted

#### Fixes

- Use _Last Note_ note priority as default value on MIDI/CV track
- Added swing to arpeggiator on MIDI/CV track

## v0.1.30 (11 Aug 2019)

This release is mostly dedicated to improving song mode. Many things have changed, please consult the manual for learning how to use it.

#### Improvements

- Added _Time Signature_ on project page for setting the duration of a bar (#118)
- Increased the number of song slots to 64 (#118)
- Added context menu to song page containing the _Init_ function (#118)
- Added ability to duplicate song slots (#118)
- Show song slots in a table (#118)
- Allow song slots to play for up to 128 bars (#118)
- Improved octave playback for arpeggiator (#109)

#### Fixes

- Do not reset MIDI/CV tracks when switching slots during song playback
- Fixed some edge cases during song playback

## v0.1.29 (31 Jul 2019)

#### Improvements

- Added ability to select all steps with an equal value on the selected layer using `SHIFT` + double tap a step (#117)
- Added _Note Priority_ parameter to MIDI/CV track (#115)

## v0.1.28 (28 Jul 2019)

#### Improvements

- Allow editing first/last step parameter together (#114)

#### Fixes

- Fixed rare edge case where first/last step parameter could end up in invalid state and crash the firmware
- Fixed glitchy encoder by using a full state machine decoding the encoders gray code (#112)

## v0.1.27 (24 Jul 2019)

This release primarily focuses on improvements of curve tracks.

#### Improvements

- Added _shape variation_ and _shape variation probability_ layers to curve sequences (#108)
- Added _gate_ and _gate probability_ layer to curve sequences (#108)
- Added _shape probability bias_ and _gate probability bias_ parameters to curve tracks (#108)
- Added better support for track linking on curve tracks including recording (#108)
- Added specific fill modes for curve tracks (variation, next pattern and invert) (#108)
- Added support for mutes on curve tracks (keep CV value at last position and mute gates) (#108)
- Added support for offsetting curves up and down by holding min or max function buttons (#103)

## v0.1.26 (20 Jul 2019)

#### Improvements

- Evaluate transposition (octave/transpose) when monitoring and recording notes (#102)
- Added routing target for tap tempo (#100)

## v0.1.25 (18 Jul 2019)

#### Improvements

- Allow chromatic note values to be changed in octaves by holding `SHIFT` (#101)
- Swapped high and low curve types and default curve sequence to be all low curves (#101)
- Show active step on all layers during step recording

#### Fixes

- Fixed euclidean generator from being destructive (#97)
- Fixed handling of active routing targets (#98)
- Fixed some edge cases in the step selection UI
- Fixed ghost live recording from past events
- Fixed condition layer value editing (clamp to valid values)

## v0.1.24 (9 Jul 2019)

#### Fixes

- Fixed writing incorrect project version

## v0.1.23 (8 Jul 2019)

**PLEASE DO NOT USE THIS VERSION, IT WRITES THE INCORRECT PROJECT FILE VERSION!**

#### Improvements

- Check and disallow to create conflicting routes (#95)
- Added new _Note Range_ MIDI event type for routes (#96)

#### Fixes

- Fixed track selection for new routing targets added in previous release
- Fixed hidden editing of route tracks

## v0.1.22 (6 Jul 2019)

#### Improvements

- Pressing `SHIFT` + `F[1-5]` on the sequence edit pages selects the first layer type of the group (#84)
- Solo a track is now done with `SHIFT` + `T[1-8]` on the performer page (#87)
- Added ability to hold fills by pressing `SHIFT` + `S[9-16]` on the performer page (#65)
- Added new _Fill Amount_ parameter to control the amount of fill in effect (#65)
- Added new routing targets: Mute, Fill, Fill Amount and Pattern (#89)
- Added new _Condition_ layer to note sequences to conditionally trigger steps (#13)
- Do not show divisor denominator if it is 1

#### Fixes

- Fixed unreliable live recording of note sequences (#63)
- Fixed latching mode when selecting a new global pattern on pattern page

## v0.1.21 (26 Jun 2019)

#### Improvements

- Added recording curve tracks from a CV source (#72)
- Added ability to output analog clock with swing (#5)
- Added sequence progress info to performer page (#79)

#### Fixes

- Fixed navigation grid on launchpad when note/slide layer is selected

## v0.1.20 (15 Jun 2019)

This is mostly a bug fix release. From this release onwards, binaries are only available in intel hex format, which is probably what most people used anyway.

#### Improvements

- Allow larger range of divisor values to enable even slower sequences (#61)
- Added hwconfig option for inverting LEDs (#74)

#### Fixes

- Fixed changing multiple track modes at once on layout page
- Fixed MIDI channel info on monitor page (show channel as 1-16 instead of 0-15)
- Fixed "hidden commit button" on layout page (#68)
- Fixed race condition when initializing a project
- Fixed detecting button up events on launchpad devices

## v0.1.19 (18 May 2019)

#### Improvements

- Added arpeggiator to MIDI/CV track (#47)
- Show note names for chromatic scales (#55)
- Added divisor as a routable target (#53)
- Improved free play mode to react smoothly on divisor changes (#53)

#### Fixes

- Handle _alternative_ note off MIDI events (e.g. note on events with velocity 0) (#56)
- Update linked tracks immediately not only when track mode is changed

## v0.1.18 (8 May 2019)

#### Improvements

- Added _Slide Time_ parameter to curve tracks to allow for slew limiting (#40)

#### Fixes

- Fixed delayed CV output signals, now CV signal is guaranteed to be updated **before** gate signal (#36)

## v0.1.17 (7 May 2019)

**Note: The new features added by this release are not yet heavily tested. You can always downgrade should you experience problems, but please report them.**

#### Improvements

- Added gate offset to note sequences, allowing to delay gates (#14)
- Added watchdog supervision (#31)

## v0.1.16 (5 May 2019)

#### Improvements

- Improved UI for editing MIDI port/channel (#39)

#### Fixes

- Fixed tied notes when output via MIDI (#38)
- Fixed regression updating gate outputs in tester application (#37)

## v0.1.15 (20 Apr 2019)

#### Improvements

- Added CV/Gate to MIDI conversion to allow using a CV/Gate source for recording (#32)

#### Fixes

- Fixed setting gate outputs before CV outputs (#36)

## v0.1.14 (17 Apr 2019)

#### Improvements

- Removed switching launchpad brightness in favor of full brightness
- Enhanced launchpad pattern page
    - Show patterns that are not empty with dimmed colors (note patterns in dim yellow, curve patterns in dim red) (#25, #26)
    - Show requested patterns (latched or synched) with dim green

#### Fixes

- Fixed saving/loading user scales as part of the project
- Fixed setting name when loading user scales (#34)

## v0.1.13 (15 Apr 2019)

#### Fixes

- Fixed Launchpad Pro support
- Fixed _Reset Measure_ when _Play Mode_ is set to _Aligned_ (#33)

## v0.1.12 (7 Apr 2019)

#### Improvements

- Refactored routing system to be non-destructive (this means that parameters will jump back to the original value when a route is deleted) (#27)
- _First Step_, _Last Step_ and _Run Mode_ routes affect every pattern on a track (instead of just the first one)
- Indicate routed parameters in the UI with an right facing arrow
- Added `ROUTE` item in context menu of sequence pages to allow creating routes (as in other pages)
- Added support for Launchpad Pro

## v0.1.11 (3 Apr 2019)

This release is mostly adding support for additional launchpad devices. However, while testing I unfortunately found that the hardware might have an issue on the USB power supply (in fact there is quite some ripple on the internal 5V power rail) leading to spurious resets of the launchpad. Connecting the launchpad through an externally powered USB hub seems to fix the issue. Therefore it is beneficial to run the launchpad with dimmed leds (default). For now, the brightness can be adjusted by pressing `8` + `6` on the launchpad. I will investigate the hardware issue as soon as possible.

#### Improvements

- Added preliminary support for Launchpad S and Launchpad Mk2

#### Fixes

- Fixed launchpad display regression (some sequence data was displayed in wrong colors)
- Fixed fill regression (fills did not work if track was muted anymore)

## v0.1.10 (1 Apr 2019)

#### Improvements

- Implemented _Step Record_ mode (#23)

## v0.1.9 (29 Mar 2019)

#### Improvements

- Improved editing curve min/max value on Launchpad (#19)
- Show octave base notes on Launchpad when editing notes (#18)
- Allow selecting sequence run mode on Launchpad

#### Fixes

- Fixed stuck notes on MIDI output

## v0.1.8 (27 Mar 2019)

#### Improvements

- Added `CV Update Mode` to note track settings to select between updating CV only on gates or always
- Added ability to use curve tracks as sources for MIDI output

#### Fixes

- Fixed copying curve tracks to clipboard (resulted in crash)

## v0.1.7 (23 Mar 2019)

#### Fixes

- Fixed glitchy encoder (#4)
- Implemented legato notes and slides via MIDI output (#3)
- Fixed routing and MIDI output page when clearing or loading a project

## v0.1.6 (17 Mar 2019)

#### Fixes

- Fixed MIDI output (note and velocity constant values were off by one before)

## v0.1.5 (1 Feb 2019)

### !!! This version is not backward compatible with previous project files !!!

#### Improvements

- `Run Mode` of sequences is now a routing target
- Added retrigger and note probability bias to track parameters and routing targets
- All probability values are now in the range of 12.5% to 100%
- Nicer visualization of target tracks in routing page
- Allow clock output pulse length of up to 20ms

## v0.1.4 (24 Jan 2019)

#### Improvements

- Rearranged quick edit of sequence parameters due to changes in frontpanel
- Changed the preset scales
- Note values are now displayed as note index and octave instead of note value and octave
- Flipped encoder reverse mode in hardware config to make default encoder in BOM act correctly
- Flipped encoder direction in bootloader

#### Bugfixes

- Fixed clock reset output state when device is powered on
- Fixed tap tempo
- Fixed pattern page when mutes are activated
- Fixed clock input/output divisors

## v0.1.3 (14 Nov 2018)

#### New Features

- Added MIDI output module
    - Generate Note On/Off events from gate, pitch and velocity of different tracks (or constant values)
    - Generate CC events from track CV
- Overview page
    - Press `PAGE` + `LEFT` to open Overview page
- Simple startup screen that automatically loads the last used project on power on
- Added new routing targets for controlling Play and Record

#### Improvements

- Sequence Edit page is now called Steps page
- Show track mode on Sequence page (same as on Track page)
- Improved route ranges
    - Allow +/- 60 semitone transposition
- Restyled pattern page
    - Differentiate from Performer page
    - Show selected pattern per track
- Exit editing mode when committing a Route or MIDI Output

#### Bugfixes

- Fixed exponential curves (do not jump to high value at the end)
- Fixed loading project name
- Fixed showing pages on leds
- Fixed launching patterns in latched mode
- Fixed setting edit pattern when changing patterns on Launchpad

## v0.1.2 (5 Nov 2018)

- No notes

## v0.1.1 (2 Oct 2018)

- Initial release
