# O_C Applets & Apps: Portability Analysis for XFormer

## Sources
- **Phazerville Suite**: https://firmware.phazerville.com/App-and-Applet-Index
- **Benisphere Suite**: https://github.com/benirose/O_C-BenisphereSuite
- **Hemisphere Suite**: https://github.com/Chysn/O_C-HemisphereSuite

## Already Integrated (XFormer equivalents)

| O_C Feature | XFormer Equivalent |
|------------|-------------------|
| Euclidean patterns | Tuesday Algo track |
| CV/Gate routing | CVRoute matrix |
| Quantizer | Harmony Engine |
| LFO | CurveTrackEngine |
| Shift register | Enigma Jr |
| Envelope | Piqued-style EG |

---

## High Priority Ports (Direct Functionality)

### 1. **EuclidX** (Euclidean Rhythm Generator)
- **Source**: Phazerville/Benisphere
- **What it does**: Classic Euclidean algorithm (Bjorklund) for rhythm generation
- **XFormer match**: Already has Tuesday Algo, but EuclidX has **probability per step** and **rotation CV**
- **Port scope**: Add probability + rotation to existing Algo track
- **Status**: PARTIAL - Tuesday has euclidean, needs probability layer

### 2. **Brancher** (Bernoulli Gate)
- **Source**: Phazerville
- **What it does**: Probabilistic routing - trigger goes A or B based on probability
- **XFormer match**: CVRoute has gate switching, but no probabilistic routing
- **Port scope**: New routing modifier with probability
- **Status**: NEW FEATURE NEEDED

### 3. **CV-Recorder** (CV Recorder/Smoother)
- **Source**: Phazerville/Benisphere
- **What it does**: Record CV up to 384 steps, smooth/playback
- **XFormer match**: No direct equivalent
- **Port scope**: Could extend Discrete track to record mode
- **Status**: NEW FEATURE - useful for capture/playback

### 4. **Seq32** (32-step sequencer with 8 patterns)
- **Source**: Phazerville/Benisphere
- **What it does**: Record/play 32 steps, CV switch between 8 patterns
- **XFormer match**: Indexed track type (CV pattern selection)
- **Port scope**: Enhance Indexed track with better recording
- **Status**: PARTIAL - Indexed exists, needs recording UI

### 5. **DrumMap** (MI Grids clone)
- **Source**: Phazerville
- **What it does**: Euclidean drum patterns with X/Y position control
- **XFormer match**: Tuesday algo for rhythm, but no X/Y interpolation
- **Port scope**: Add Grids-style X/Y control to Algo track
- **Status**: PARTIAL - needs X/Y pattern morphing

### 6. **TwoRings** (Dual Turing Machine shift registers)
- **Source**: Phazerville/Benisphere
- **What it does**: Shift register with probability, CV-controlled length
- **XFormer match**: Enigma Jr for single register
- **Port scope**: Add second register + better CV control
- **Status**: PARTIAL - Enigma Jr exists

### 7. **Strum** (Arpeggiator)
- **Source**: Phazerville
- **What it does**: Advanced arpeggiator with direction modes, octave spread
- **XFormer match**: Tuesday has some arp features
- **Port scope**: Enhance arp with spread, direction, probability
- **Status**: PARTIAL - basic arp exists

### 8. **Shuffle** (Swing generator)
- **Source**: Phazerville
- **What it does**: Clock shuffle with triplets on second output
- **XFormer match**: Clock system has swing
- **Port scope**: Add triplet-based swing mode
- **Status**: PARTIAL - basic swing exists

### 9. **Palimpsest** (Accent Sequencer)
- **Source**: Phazerville
- **What it does**: Layered accents with probability
- **XFormer match**: Gate modes + probability
- **Port scope**: Enhance gate system with accent layering
- **Status**: PARTIAL - has probability gates

### 10. **Carpeggio** (X-Y pitch table)
- **Source**: Phazerville
- **What it does**: 2D grid of pitches from scale, XY input moves through
- **XFormer match**: Harmony Engine scale system
- **Port scope**: Add XY pad input for scale navigation
- **Status**: NEW FEATURE - interesting UI concept

---

## Medium Priority (CV Utilities)

### 11. **AttenOff** (Attenuverter + Offset + Mix)
- **Source**: Phazerville/Benisphere
- **What it does**: CV mixing with +/-200% range
- **XFormer match**: Routing has depth/bias controls
- **Port scope**: Could enhance routing UI
- **Status**: PARTIAL - routing has these

### 12. **Calculate** (Dual Min/Max/Sum/Diff/Mean/Random/S&H)
- **Source**: Phazerville/Benisphere
- **What it does**: CV math operations
- **XFormer match**: Accumulator does some math
- **Port scope**: Add CV math ops to routing
- **Status**: PARTIAL - basic ops exist

### 13. **Logic** (AND/OR/XOR/NAND/NOR/XNOR)
- **Source**: Phazerville/Benisphere
- **What it does**: Gate logic operations
- **XFormer match**: Gate modes
- **Port scope**: Add more gate logic options
- **Status**: PARTIAL - basic gate logic exists

### 14. **Slew** (Dual channel slew)
- **Source**: Phazerville/Benisphere
- **What it does**: Linear and exponential slew limiter
- **XFormer match**: Curve Studio has shape control
- **Port scope**: Enhance curve shapes
- **Status**: PARTIAL - shapes exist

### 15. **ClockDivider** (Complex clock divider/multiplier)
- **Source**: Phazerville
- **What it does**: Clock division with CV, resets, direction
- **XFormer match**: Clock system
- **Port scope**: Add more clock division options
- **Status**: PARTIAL - clock exists

### 16. **ProbDiv** (Probabilistic clock divider)
- **Source**: Phazerville
- **What it does**: Clock with random skip probability
- **XFormer match**: Clock Skipper
- **Port scope**: Enhance clock system
- **Status**: PARTIAL - skip exists

---

## Low Priority (Audio/Effects - Harder to port)

### 17. **Dr. LoFi** (Bitcrushing delay)
- **Source**: Phazerville
- **What it does**: PCM delay with bitcrush, rate reduction
- **XFormer match**: None
- **Port scope**: Audio processing - NOT suitable for XFormer
- **Status**: SKIP - audio not core focus

### 18. **BitBeat** (Bytebeat generator)
- **Source**: Phazerville
- **What it does**: Algorithmic audio from bit math
- **XFormer match**: None
- **Port scope**: Audio generation - NOT suitable
- **Status**: SKIP

### 19. **BootsNCat** (Noise percussion)
- **Source**: Phazerville
- **What it does**: Noise-based drums
- **XFormer match**: None
- **Port scope**: Audio - NOT suitable
- **Status**: SKIP

---

## Full-Screen Apps (Harder to port - need dedicated UI)

### 20. **Piqued** (Quad envelope generator)
- **Source**: Phazerville
- **What it does**: 4 envelope generators with triggers
- **XFormer match**: Could extend CurveTrackEngine
- **Port scope**: Multi-channel envelopes - complex
- **Status**: CONSIDER - useful but big implementation

### 21. **Low-rents** (Lorenz attractor)
- **Source**: Phazerville
- **What it does**: Chaotic CV from Lorenz equations
- **XFormer match**: Chaos Engine already exists!
- **Port scope**: Check if Lorenz is already in Chaos Engine
- **Status**: CHECK - might be in Chaos Engine

### 22. **Enigma** (Turing Machine sequencer)
- **Source**: Phazerville
- **What it does**: Shift register with probability, longer patterns
- **XFormer match**: Enigma Jr exists
- **Port scope**: Extend Enigma Jr with longer patterns
- **Status**: PARTIAL - Jr version exists

### 23. **Quadraturia** (Quadrature wavetable LFO)
- **Source**: Phazerville
- **What it does**: 4-phase wavetable LFO
- **XFormer match**: CurveTrackEngine has shapes
- **Port scope**: Add quadrature mode to curves
- **Status**: CONSIDER

### 24. **Meta-Q** (Dual sequenced quantizer)
- **Source**: Phazerville
- **What it does**: Quantizer with internal sequence
- **XFormer match**: Harmony Engine
- **Port scope**: Check if meta-q features are in Harmony
- **Status**: PARTIAL

### 25. **Automatonnetz** (Neo-Riemannian transformations)
- **Source**: Phazerville
- **What it does**: Chord progression from 5x5 matrix
- **XFormer match**: Harmony Engine has modes
- **Port scope**: Add chord progression matrix to Harmony
- **Status**: INTERESTING - chord progression matrix

### 26. **Scale-Editor** (Microtonal scale editor)
- **Source**: Phazerville
- **What it does**: Edit and save microtonal scales
- **XFormer match**: Harmony Engine has scale support
- **Port scope**: Add microtonal scale editing
- **Status**: CONSIDER

### 27. **Scenery** (Macro CV switch/crossfader)
- **Source**: Phazerville
- **What it does**: CV switching and crossfading
- **XFormer match**: CVRoute
- **Port scope**: Enhance routing UI
- **Status**: PARTIAL - routing exists

### 28. **Harrington 1200** (Neo-Riemannian triad progressions)
- **Source**: Phazerville
- **What it does**: Chord progressions for triads
- **XFormer match**: Harmony Engine
- **Port scope**: Chord progression system
- **Status**: INTERESTING - progression engine

### 29. **CopierMaschine** (Quantizing ASR)
- **Source**: Phazerville
- **What it does**: Analog shift register with quantization
- **XFormer match**: Discrete track
- **Port scope**: Add quantization to Discrete
- **Status**: CONSIDER

### 30. **Neural Net** (6 Neuron logic processor)
- **Source**: Phazerville
- **What it does**: Threshold logic neurons
- **XFormer match**: None
- **Port scope**: CV processing - interesting concept
- **Status**: INTERESTING

---

## Unique/Interesting Features to Consider

### 31. **TB-3PO** (303-style sequencer)
- **Source**: Phazerville
- **What it does**: Acid sequencer with slides, accents, predictable randomization
- **Why interesting**: Great UX for 303-style programming
- **Port scope**: Enhance existing Algo track with 303-style controls
- **Status**: INTERESTING UX PATTERNS

### 32. **Pigeons** (Fibonacci melody generator)
- **Source**: Phazerville
- **What it does**: Fibonacci-ratio based melodies
- **Why interesting**: Unique melodic algorithm
- **Port scope**: New Algo mode based on Fibonacci ratios
- **Status**: NEW ALGO MODE

### 33. **Relabi** (Random walk modulation)
- **Source**: Phazerville
- **What it does**: Brownian motion, 1/f noise
- **XFormer match**: Chaos Engine
- **Port scope**: Check if Relabi algorithms are in Chaos
- **Status**: CHECK

### 34. **RunglBook** (Chaotic shift-register)
- **Source**: Phazerville
- **What it does**: Shift register with chaotic modulation
- **Why interesting**: Combines shift register + chaos
- **Port scope**: Combine Enigma + Chaos Engine
- **Status**: INTERESTING COMBINATION

### 35. **LowerRenz** (Orbiting particles/chaos)
- **Source**: Phazerville
- **What it does**: Orbiting particles, chaotic modulation
- **XFormer match**: Chaos Engine
- **Port scope**: Check if in Chaos Engine
- **Status**: CHECK

### 36. **Trending** (Rising/falling/moving detector)
- **Source**: Phazerville
- **What it does**: CV trend detection and state output
- **Why interesting**: Useful for modulation
- **Port scope**: Add to CV processing
- **Status**: INTERESTING UTILITY

### 37. **Envelope-Follower**
- **Source**: Phazerville
- **What it does**: Audio envelope following
- **XFormer match**: None
- **Port scope**: Audio processing - NOT core
- **Status**: SKIP

---

## Implementation Priorities

### Tier 1: Quick Wins (Existing code to enhance)
1. **EuclidX probability + rotation** → Enhance Tuesday Algo
2. **Brancher probability gate** → New routing modifier
3. **DrumMap X/Y** → Add to Algo track
4. **Seq32 recording UI** → Enhance Indexed track

### Tier 2: Medium Effort (New track types or major features)
5. **CV-Recorder** → Record mode for Discrete
6. **Carpeggio XY** → Harmony Engine enhancement
7. **TB-3PO UX** → 303-style Algo mode
8. **Pigeons** → Fibonacci Algo mode

### Tier 3: Large Effort (Full-screen apps)
9. **Piqued EG** → Multi-channel envelope system
10. **Neural Net** → CV processing system
11. **Harrington 1200** → Chord progression engine
12. **Automatonnetz** → Chord matrix

### Tier 4: Nice to Have (Low priority)
- Scale-Editor microtonal support
- Quadraturia quadrature LFO
- CopierMaschine quantized ASR

---

## References

- Phazerville: https://firmware.phazerville.com/
- Benisphere: https://github.com/benirose/O_C-BenisphereSuite
- Hemisphere Suite: https://github.com/Chysn/O_C-HemisphereSuite
- O_C Phazerville: https://github.com/djphazer/O_C-Phazerville
- O_C T41: https://github.com/PaulStoffregen/O_C_T41
