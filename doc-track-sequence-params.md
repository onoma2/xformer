# PEW|FORMER Track and Sequence Governed Parameters

This document provides a comprehensive list of track-governed and sequence-governed parameters for each track type, including when they're available and when they're routable.

## Note Track

### Track-Governed Parameters
- **playMode** - Play mode (Aligned/Free)
  - Available: Track Page (Page + S2)
  - Routable: No
  - Type: `Types::PlayMode`

- **fillMode** - Fill mode (None/Gates/NextPattern/Condition)
  - Available: Track Page (Page + S2)
  - Routable: No
  - Type: `FillMode`

- **fillMuted** - Whether fill is muted
  - Available: Track Page (Page + S2)
  - Routable: No
  - Type: `bool`

- **cvUpdateMode** - CV update mode (Gate/Always)
  - Available: Track Page (Page + S2)
  - Routable: No
  - Type: `CvUpdateMode`

- **slideTime** - Slide time percentage
  - Available: Track Page (Page + S2)
  - Routable: Yes (Routing::Target::SlideTime)
  - Type: `Routable<uint8_t>` (0-100%)

- **octave** - Octave offset
  - Available: Track Page (Page + S2)
  - Routable: Yes (Routing::Target::Octave)
  - Type: `Routable<int8_t>` (-10 to 10)

- **transpose** - Semitone transpose
  - Available: Track Page (Page + S2)
  - Routable: Yes (Routing::Target::Transpose)
  - Type: `Routable<int8_t>` (-100 to 100)

- **rotate** - Sequence rotation
  - Available: Track Page (Page + S2)
  - Routable: Yes (Routing::Target::Rotate)
  - Type: `Routable<int8_t>` (-64 to 64)

- **gateProbabilityBias** - Gate probability bias
  - Available: Track Page (Page + S2)
  - Routable: Yes (Routing::Target::GateProbabilityBias)
  - Type: `Routable<int8_t>` (-Range to +Range)

- **retriggerProbabilityBias** - Retrigger probability bias
  - Available: Track Page (Page + S2)
  - Routable: Yes (Routing::Target::RetriggerProbabilityBias)
  - Type: `Routable<int8_t>` (-Range to +Range)

- **lengthBias** - Length bias
  - Available: Track Page (Page + S2)
  - Routable: Yes (Routing::Target::LengthBias)
  - Type: `Routable<int8_t>` (-Range to +Range)

- **noteProbabilityBias** - Note probability bias
  - Available: Track Page (Page + S2)
  - Routable: Yes (Routing::Target::NoteProbabilityBias)
  - Type: `Routable<int8_t>` (-Range to +Range)

### Sequence-Governed Parameters
- **runMode** - Run mode (Forward/Backward/Pendulum/PingPong/Random/RandomWalk)
  - Available: Sequence Edit Page (Page + S0)
  - Routable: Yes (Routing::Target::RunMode)
  - Type: `RunMode`

- **divisor** - Timing divisor
  - Available: Sequence Edit Page (Page + S0)
  - Routable: Yes (Routing::Target::Divisor)
  - Type: `uint16_t`

- **scale** - Scale selection
  - Available: Sequence Edit Page (Page + S0)
  - Routable: Yes (Routing::Target::Scale)
  - Type: `uint8_t`

- **rootNote** - Root note
  - Available: Sequence Edit Page (Page + S0)
  - Routable: Yes (Routing::Target::RootNote)
  - Type: `int8_t`

- **firstStep/lastStep** - Active step range
  - Available: Sequence Edit Page (Page + S0)
  - Routable: Yes (Routing::Target::FirstStep/LastStep)
  - Type: `int8_t`

- **Step-specific parameters** (per step in sequence):
  - Gate, GateProbability, GateOffset, Slide, GateMode
  - Retrigger, RetriggerProbability, PulseCount
  - Length, LengthVariationRange, LengthVariationProbability
  - Note, NoteVariationRange, NoteVariationProbability
  - Condition, AccumulatorTrigger
  - Available: Sequence Edit Page (Page + S0)
  - Routable: No (step-level parameters)

---

## Curve Track

### Track-Governed Parameters
- **playMode** - Play mode (Aligned/Free)
  - Available: Track Page (Page + S2)
  - Routable: No
  - Type: `Types::PlayMode`

- **fillMode** - Fill mode (None/Variation/NextPattern/Invert)
  - Available: Track Page (Page + S2)
  - Routable: No
  - Type: `FillMode`

- **muteMode** - Mute mode (LastValue/Zero/Min/Max)
  - Available: Track Page (Page + S2)
  - Routable: No
  - Type: `MuteMode`

- **slideTime** - Slide time percentage
  - Available: Track Page (Page + S2)
  - Routable: Yes (Routing::Target::SlideTime)
  - Type: `Routable<uint8_t>` (0-100%)

- **offset** - CV offset
  - Available: Track Page (Page + S2)
  - Routable: Yes (Routing::Target::Offset)
  - Type: `Routable<int16_t>` (-500 to 500)

- **rotate** - Sequence rotation
  - Available: Track Page (Page + S2)
  - Routable: Yes (Routing::Target::Rotate)
  - Type: `Routable<int8_t>` (-64 to 64)

- **shapeProbabilityBias** - Shape probability bias
  - Available: Track Page (Page + S2)
  - Routable: Yes (Routing::Target::ShapeProbabilityBias)
  - Type: `Routable<int8_t>` (-8 to 8)

- **gateProbabilityBias** - Gate probability bias
  - Available: Track Page (Page + S2)
  - Routable: Yes (Routing::Target::GateProbabilityBias)
  - Type: `Routable<int8_t>` (-Range to +Range)

- **globalPhase** - Global phase offset
  - Available: Track Page (Page + S2)
  - Routable: No
  - Type: `float` (0.0 to 1.0)

### Sequence-Governed Parameters
- **runMode** - Run mode (Forward/Backward/Pendulum/PingPong/Random/RandomWalk)
  - Available: Sequence Edit Page (Page + S0)
  - Routable: Yes (Routing::Target::RunMode)
  - Type: `RunMode`

- **divisor** - Timing divisor
  - Available: Sequence Edit Page (Page + S0)
  - Routable: Yes (Routing::Target::Divisor)
  - Type: `uint16_t`

- **firstStep/lastStep** - Active step range
  - Available: Sequence Edit Page (Page + S0)
  - Routable: Yes (Routing::Target::FirstStep/LastStep)
  - Type: `int8_t`

- **Step-specific parameters** (per step in sequence):
  - Curve, CurveProbability, CurveOffset
  - Gate, GateProbability, GateOffset, GateMode
  - Length, LengthVariationRange, LengthVariationProbability
  - Level, LevelVariationRange, LevelVariationProbability
  - Condition
  - Available: Sequence Edit Page (Page + S0)
  - Routable: No (step-level parameters)

---

## Tuesday Track

### Track-Governed Parameters
- **playMode** - Play mode (Aligned/Free)
  - Available: Track Page (Page + S2)
  - Routable: No
  - Type: `Types::PlayMode`

### Sequence-Governed Parameters
- **divisor** - Timing divisor
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Divisor)
  - Type: `uint16_t`

- **algorithm** - Algorithm selection (0-14)
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Algorithm)
  - Type: `int` (0-14)

- **flow** - Flow parameter
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Flow)
  - Type: `int` (1-128)

- **ornament** - Ornament parameter
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Ornament)
  - Type: `int` (1-128)

- **power** - Power/density parameter
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Power)
  - Type: `int` (0-16)

- **glide** - Glide probability
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Glide)
  - Type: `int` (0-100)

- **trill** - Trill/ratchet probability
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Trill)
  - Type: `int` (0-100)

- **stepTrill** - Step trill probability
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::StepTrill)
  - Type: `int` (0-100)

- **gateOffset** - Gate timing offset
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::GateOffset)
  - Type: `int` (0-100)

- **gateLength** - Gate length percentage
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::GateLength)
  - Type: `int` (0-100)

- **loopLength** - Loop length
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: No
  - Type: `int` (0-32)

- **rotate** - Rotation offset
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Rotate)
  - Type: `int` (-32 to 32)

- **skew** - Timing skew
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: No
  - Type: `int` (-8 to 8)

- **maskParameter** - Mask parameter
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: No
  - Type: `int` (0-15)

- **timeMode** - Time mode
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: No
  - Type: `int` (0-3)

- **maskProgression** - Mask progression
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: No
  - Type: `int` (0-3)

- **start** - Start delay
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: No
  - Type: `int` (0-15)

- **octave** - Octave offset
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Octave)
  - Type: `int` (-5 to 5)

- **transpose** - Semitone transpose
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Transpose)
  - Type: `int` (-60 to 60)

- **rootNote** - Root note
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::RootNote)
  - Type: `int` (-60 to 60)

---

## DiscreteMap Track

### Track-Governed Parameters
- **playMode** - Play mode (Aligned/Free)
  - Available: Track Page (Page + S2)
  - Routable: No
  - Type: `Types::PlayMode`

- **cvUpdateMode** - CV update mode (Gate/Always)
  - Available: Track Page (Page + S2)
  - Routable: No
  - Type: `CvUpdateMode`

### Sequence-Governed Parameters
- **divisor** - Timing divisor
  - Available: Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Divisor)
  - Type: `uint16_t`

- **clockSource** - Clock source (Internal/External/InternalTriangle)
  - Available: Sequence Page (Page + S1)
  - Routable: No
  - Type: `ClockSource`

- **thresholdMode** - Threshold mode (Position/Length)
  - Available: Sequence Page (Page + S1)
  - Routable: No
  - Type: `ThresholdMode`

- **rangeHigh** - High voltage range
  - Available: Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::DiscreteMapRangeHigh)
  - Type: `float` (-5.0 to 5.0)

- **rangeLow** - Low voltage range
  - Available: Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::DiscreteMapRangeLow)
  - Type: `float` (-5.0 to 5.0)

- **gateLength** - Gate length percentage
  - Available: Sequence Page (Page + S1)
  - Routable: No
  - Type: `int` (0-100)

- **slewEnabled** - Whether slew is enabled
  - Available: Sequence Page (Page + S1)
  - Routable: No
  - Type: `bool`

- **offset** - CV offset
  - Available: Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Offset)
  - Type: `float` (-5.0 to 5.0)

- **syncMode** - Sync mode (Off/ResetMeasure/External)
  - Available: Sequence Page (Page + S1)
  - Routable: No
  - Type: `SyncMode`

- **resetMeasure** - Reset measure count
  - Available: Sequence Page (Page + S1)
  - Routable: No
  - Type: `int` (0-64)

- **loop** - Whether sequence loops
  - Available: Sequence Page (Page + S1)
  - Routable: No
  - Type: `bool`

- **octave** - Octave offset
  - Available: Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Octave)
  - Type: `int` (-5 to 5)

- **transpose** - Semitone transpose
  - Available: Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Transpose)
  - Type: `int` (-60 to 60)

- **rootNote** - Root note
  - Available: Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::RootNote)
  - Type: `int` (-60 to 60)

### Stage-Specific Parameters (per stage in sequence)
- **direction** - Direction (Rise/Fall/Both/Off)
  - Available: Stage List Page (Page + S1, second press)
  - Routable: No
  - Type: `TriggerDir`

- **threshold** - Threshold value (-100 to +100)
  - Available: Stage List Page (Page + S1, second press)
  - Routable: No
  - Type: `int16_t`

- **noteIndex** - Note index
  - Available: Stage List Page (Page + S1, second press)
  - Routable: No
  - Type: `int8_t`

---

## Indexed Track

### Track-Governed Parameters
- **playMode** - Play mode (Aligned/Free)
  - Available: Track Page (Page + S2)
  - Routable: No
  - Type: `Types::PlayMode`

- **cvUpdateMode** - CV update mode (Gate/Always)
  - Available: Track Page (Page + S2)
  - Routable: No
  - Type: `CvUpdateMode`

- **slideTime** - Slide time percentage
  - Available: Track Page (Page + S2)
  - Routable: Yes (Routing::Target::SlideTime)
  - Type: `Routable<uint8_t>` (0-100%)

- **octave** - Octave offset
  - Available: Track Page (Page + S2)
  - Routable: Yes (Routing::Target::Octave)
  - Type: `Routable<int8_t>` (-10 to 10)

- **transpose** - Semitone transpose
  - Available: Track Page (Page + S2)
  - Routable: Yes (Routing::Target::Transpose)
  - Type: `Routable<int8_t>` (-60 to 60)

### Sequence-Governed Parameters
- **divisor** - Timing divisor
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Divisor)
  - Type: `uint16_t`

- **runMode** - Run mode (Forward/Backward/Pendulum/PingPong/Random/RandomWalk)
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::RunMode)
  - Type: `RunMode`

- **syncMode** - Sync mode (Off/ResetMeasure/External)
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: No
  - Type: `SyncMode`

- **resetMeasure** - Reset measure count
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: No
  - Type: `int` (0-64)

- **firstStep/lastStep** - Active step range
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::FirstStep/LastStep)
  - Type: `int8_t`

- **loop** - Whether sequence loops
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: No
  - Type: `bool`

- **octave** - Octave offset (sequence-level override)
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Octave)
  - Type: `int` (-5 to 5)

- **transpose** - Semitone transpose (sequence-level override)
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Transpose)
  - Type: `int` (-60 to 60)

- **rootNote** - Root note
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::RootNote)
  - Type: `int` (-60 to 60)

- **selectedScale** - Selected scale
  - Available: Sequence Edit Page (Page + S0) and Sequence Page (Page + S1)
  - Routable: Yes (Routing::Target::Scale)
  - Type: `uint8_t`

### Step-Specific Parameters (per step in sequence)
- **duration** - Step duration
  - Available: Sequence Edit Page (Page + S0)
  - Routable: No
  - Type: `uint16_t`

- **gateLength** - Gate length percentage
  - Available: Sequence Edit Page (Page + S0)
  - Routable: No
  - Type: `uint16_t`

- **noteIndex** - Note index
  - Available: Sequence Edit Page (Page + S0)
  - Routable: No
  - Type: `int8_t`

- **slide** - Whether slide is enabled
  - Available: Sequence Edit Page (Page + S0)
  - Routable: No
  - Type: `bool`

- **groupMask** - Group mask
  - Available: Sequence Edit Page (Page + S0)
  - Routable: No
  - Type: `uint8_t`

### Route-Specific Parameters (for modulation)
- **routeA** and **routeB** - Modulation routes
  - Available: Sequence Edit Page (Page + S0)
  - Routable: Yes (Routing::Target::IndexedA/IndexedB)
  - Type: `RouteConfig`

---

## Summary of Routable Parameters

### Track-Level Routable Parameters:
- **Note Track**: slideTime, octave, transpose, rotate, gateProbabilityBias, retriggerProbabilityBias, lengthBias, noteProbabilityBias
- **Curve Track**: slideTime, offset, rotate, shapeProbabilityBias, gateProbabilityBias
- **Tuesday Track**: No track-level routable parameters (all parameters are sequence-level)
- **DiscreteMap Track**: No track-level routable parameters (all parameters are sequence-level)
- **Indexed Track**: slideTime, octave, transpose

### Sequence-Level Routable Parameters (Common to All):
- **divisor** - Timing divisor (Routing::Target::Divisor)
- **runMode** - Run mode (Routing::Target::RunMode)
- **firstStep/lastStep** - Active step range (Routing::Target::FirstStep/LastStep)
- **scale** - Scale selection (Routing::Target::Scale)
- **rootNote** - Root note (Routing::Target::RootNote)

### Track-Type Specific Routable Parameters:
- **Tuesday Track**: algorithm, flow, ornament, power, glide, trill, stepTrill, gateOffset, gateLength
- **DiscreteMap Track**: rangeHigh, rangeLow, offset
- **Indexed Track**: routeA/B (IndexedA/IndexedB targets)

### Availability by Navigation:
- **Page + S0 (SequenceEdit)**: Sequence-specific parameters and step-level parameters
- **Page + S1 (Sequence)**: Sequence parameters (for tracks that have dedicated sequence views)
- **Page + S2 (Track)**: Track-specific parameters