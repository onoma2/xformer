# Comprehensive Analysis: Original C vs C++ Port

After reviewing the original C source code in `algo-research/tuesday`, this document provides a definitive evaluation of the architectural analysis claims regarding the Tuesday algorithm port. The findings are **significantly more severe** than initially assessed.

---

## 1. **Fatal Model Shift (tick → buffer)** - **CONFIRMED TRUE**

### Original C Architecture (Tuesday.c)

```c
// GENERATION PHASE (happens once when parameters change)
void Tuesday_Generate(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S)
{
    int len = S->tpboptions[P->tpbopt] * S->beatoptions[P->beatopt];
    for (int i = 0; i < len; i++)
    {
        // Fill buffer with ALL steps
        Algo->Gen(T, P, S, &Randoms[0], &FuncSpecific[0], i,
                  &T->CurrentPattern.Ticks[i]);
    }
}

// PLAYBACK PHASE (happens every tick)
void Tuesday_Tick(Tuesday_PatternGen *T, Tuesday_Params *P)
{
    Tuesday_Tick_t *Tick = &T->CurrentPattern.Ticks[T->Tick];  // Read from buffer

    if (Tick->vel >= T->CoolDown)  // Apply density gating during playback
    {
        T->CoolDown = CoolDownMax;
        // Fire gate...
    }
}
```

**Key characteristics:**
- Algorithms generate **entire pattern buffer** upfront (up to 320 ticks)
- Buffer contains pre-computed `{note, vel, accent, slide}` for every step
- Playback reads from buffer and applies **velocity-based density gating**
- RNG state is preserved across loop boundaries (buffer regeneration)

### C++ Port Architecture (TuesdayTrackEngine.cpp)

```cpp
TrackEngine::TickResult TuesdayTrackEngine::tick(uint32_t tick)
{
    if (stepTrigger) {
        // Generate step in real-time
        TuesdayTickResult result = generateStep(tick);

        // Apply density gating immediately
        if (result.velocity >= _coolDown) {
            _microGateQueue.push({tick + offset, true, volts});
        }
    }
}
```

**Key characteristics:**
- Algorithms generate **one step at a time** during playback
- No buffer - immediate processing
- Density gating applied during generation, not playback
- RNG state advances continuously (no loop boundary reset)

### Impact

This is a **fundamental architectural mismatch**. The original algorithms were designed to fill a buffer, allowing:
- **Cross-step analysis** (e.g., pattern transformations in AUTECHRE)
- **RNG state preservation** across loop boundaries
- **Post-generation processing** (Pattern_Rotate, Pattern_Transpose, Pattern_Reverse)

The C++ port **eliminated the buffer model entirely**, breaking algorithms that depend on:
- Seeing the full pattern before playback
- Applying transformations to the buffer
- Deterministic RNG reset at loop boundaries

**Verdict: TRUE and CRITICAL**

---

## 2. **RNG Decorrelation Destroyed** - **CONFIRMED TRUE**

### Original C Code (Tuesday.c:1342-1360)

```c
int X = T->seed1 >> 3;  // Extract high 5 bits
int Y = T->seed2 >> 3;  // Extract high 5 bits

// Create 4 correlated RNG streams
Tuesday_RandomSeed(&Randoms[0], X + Y * 32);
Tuesday_RandomSeed(&Randoms[1], X + Y * 32 + 1);
Tuesday_RandomSeed(&Randoms[2], X + Y * 32 + 32);
Tuesday_RandomSeed(&Randoms[3], X + Y * 32 + 33);

// Dithering system for smooth transitions
unsigned char ditherx = dither[xbase * 3 + XFade - 1];
unsigned char dithery = dither[ybase * 3 + YFade - 1];

for (int i = 0; i < len; i++) {
    // Generate from 4 RNG streams
    for (int j = 0; j < 4; j++) {
        Algo->Gen(T, P, S, &Randoms[j], &FuncSpecific[j], i, &Ticks[j]);
    }

    // Blend using dither table
    ApplyDither(i, ditherx, &Ticks[0], &Ticks[1], &Top);
    ApplyDither(i, ditherx, &Ticks[2], &Ticks[3], &Bot);
    ApplyDither(i, dithery, &Top, &Bot, &T->CurrentPattern.Ticks[i]);
}
```

### C++ Port (TuesdayTrackEngine.cpp:18-37)

```cpp
uint32_t flowSeed = (flow - 1) << 4;
uint32_t ornamentSeed = (ornament - 1) << 4;

_rng = Random(flowSeed);
_extraRng = Random(ornamentSeed + 0x9e3779b9);  // Golden ratio salt
```

### Critical Differences

| Original C | C++ Port | Impact |
|-----------|----------|--------|
| `X + Y * 32` (correlated) | `(flow - 1) << 4` and `(ornament - 1) << 4` (independent) | **Different seed space** |
| 4 RNG streams with +0/+1/+32/+33 offsets | 2 RNG streams with golden ratio salt | **No correlation structure** |
| Dithering system for smooth transitions | No dithering | **No smooth parameter interpolation** |
| Shifts by 3 bits (`>> 3`) | Shifts by 4 bits (`<< 4`) | **Different resolution** |

**Verdict: TRUE - Completely different seeding strategy**

The original created a **correlation structure** between parameter values, while the C++ port uses **independent seeds**. This fundamentally changes the musical behavior when tweaking knobs.

---

## 3. **Scale Quantization Bypassed** - **FALSE**

The C++ port correctly calls `scaleToVolts()` on every gate firing (TuesdayTrackEngine.cpp:926, 1022). This claim was incorrect.

**Verdict: FALSE**

---

## 4. **Timing Model Replaced** - **ACTUALLY WORSE THAN CLAIMED**

### Original C - Beat-Locked Buffer Generation

```c
// Algorithms receive TPB (ticks per beat) in pattern container
T->CurrentPattern.TPB = S->tpboptions[P->tpbopt];

// Algorithms can query beat boundaries
if (T->Tick % T->CurrentPattern.TPB == 0) {
    T->Gates[GATE_BEAT] = GATE_MINGATETIME;
}
```

### C++ Port - Beat Detection Added Post-Hoc

```cpp
// Line 595: AUTECHRE algorithm
bool isBeatStart = (_stepIndex % 4) == 0;  // Hardcoded 4-step grid!
```

**The C++ port hardcoded `% 4` for beat detection**, while the original used configurable TPB (ticks per beat). This is **even worse** than the original analysis claimed - it's not just a different timing model, it's a **hardcoded assumption** that doesn't respect the original parameter system.

**Verdict: TRUE and WORSE - Hardcoded beat grid**

---

## 5. **Gesture Logic Simplified** - **PARTIALLY TRUE**

### Original C Cooldown

```c
if (Tick->vel >= T->CoolDown)  // Direct velocity comparison (0-255 scale)
{
    T->CoolDown = CoolDownMax;
    // Fire gate
}
```

### C++ Port Cooldown

```cpp
int velDensity = result.velocity / 16;  // Scale to 0-15
if (velDensity >= _coolDown) {
    densityGate = true;
}
```

The C++ implementation is actually **nearly correct** - it scales velocity to match cooldown range. The real problem is that algorithms don't generate varying velocities:

```cpp
// AUTECHRE line 610
result.velocity = 160;  // Always 160!
```

**Verdict: PARTIALLY TRUE - Gate system is correct, but algorithms don't use it properly**

---

## 6. **Mutation Density Altered** - **CONFIRMED TRUE**

### Original C - Probabilistic Everything

Algorithms used `Tuesday_BoolChance()`, `Tuesday_PercChance()`, and velocity variation to create **probabilistic patterns**. Example from Algo_SaikoSet.h:330:

```c
if (PS->GENERIC.b1 && I <= 7)  // b1 is probabilistically set
{
    Output->note += 3;
}

if (Tuesday_BoolChance(R))  // 50% chance
{
    Pattern_Transpose(T, 0, 7, 3);
}
```

### C++ Port - Deterministic Polyrhythm

```cpp
// AUTECHRE line 599
if (isBeatStart && tupleN != 4) {
    result.chaos = 100;  // ALWAYS 100!
    result.polyCount = tupleN;
}

// Line 910
if (result.polyCount > 0 && result.chaos > 50) {  // Always true
    // POLYRHYTHM MODE: fires every time
}
```

**Verdict: CONFIRMED TRUE - Polyrhythm is deterministic, not probabilistic**

---

## 7. **Dead Code & Uninitialized State** - **CONFIRMED TRUE**

AUTECHRE, APHEX, and STEPWAVE algorithms **don't exist in the original C code**. They were added to the C++ port without corresponding state initialization in `reset()`:

```cpp
// TuesdayTrackEngine.h declares:
uint8_t _aphex_track1_pattern[4];
uint8_t _aphex_track2_pattern[3];
uint8_t _aphex_track3_pattern[5];
uint8_t _aphex_pos1, _aphex_pos2, _aphex_pos3;

// But TuesdayTrackEngine.cpp:206 reset() never initializes them!
```

**Verdict: CONFIRMED TRUE - New algorithms have uninitialized state**

---

## **FINAL VERDICT**

| Claim | Verdict | Severity |
|-------|---------|----------|
| Fatal Model Shift | **TRUE (CRITICAL)** | **FATAL** |
| RNG Decorrelation | **TRUE (CRITICAL)** | **FATAL** |
| Scale Quantization Bypassed | FALSE | N/A |
| Timing Model Replaced | **TRUE (WORSE)** | **HIGH** |
| Gesture Logic Simplified | PARTIALLY TRUE | MEDIUM |
| Mutation Density Altered | **TRUE** | **HIGH** |
| Dead Code & Uninitialized State | **TRUE** | **HIGH** |

## **Critical Findings**

1. **Buffer model was completely eliminated** - Original algorithms expect to fill a buffer and have it post-processed. C++ port generates in real-time.

2. **RNG seeding is completely different** - Original uses `X + Y * 32` with 4-stream dithering. C++ uses independent `(param - 1) << 4` seeds.

3. **AUTECHRE, APHEX, STEPWAVE don't exist in original** - These were created for the C++ port without proper initialization.

4. **Polyrhythm is deterministic** - Should be probabilistic based on `chaos` parameter.

5. **No loop boundary RNG reset** - Original regenerates buffer on loop, C++ never resets RNG.

6. **Hardcoded beat grid** - Original used configurable TPB (ticks per beat), C++ hardcodes `% 4`.

## **Recommendations**

### Immediate Fixes (for current architecture)

1. **Initialize all algorithm state in reset()**
   - Add initialization for APHEX, AUTECHRE, STEPWAVE state variables
   - Prevent undefined behavior from uninitialized memory

2. **Make polyrhythm probabilistic**
   - Change `result.chaos = 100` to vary based on flow/ornament
   - Use chaos as probability: `if (_rng.nextRange(100) < result.chaos)`

3. **Add velocity variation to algorithms**
   - AUTECHRE and STEPWAVE should generate varying velocities
   - Use velocity to modulate density gating naturally

4. **Use configurable beat grid**
   - Replace hardcoded `% 4` with divisor-based calculation
   - Respect the sequence's timing configuration

### Architectural Restoration (to match original)

1. **Restore buffer-based generation**
   - Implement `Tuesday_PatternContainer` equivalent
   - Generate entire loop upfront, play from buffer
   - Apply transformations (rotate, transpose, reverse) to buffer

2. **Restore RNG correlation structure**
   - Implement 4-stream RNG with `X + Y * 32` + offsets
   - Add dithering system for smooth parameter transitions
   - Reset RNG state at loop boundaries

3. **Restore probabilistic pattern mutations**
   - Add `BoolChance()` and `PercChance()` helpers
   - Use probabilistic triggers for transformations
   - Let algorithms vary behavior stochastically

This would require a **major refactor** but would restore the original Tuesday behavior.

## **Date**

Analysis completed: 2025-12-03
