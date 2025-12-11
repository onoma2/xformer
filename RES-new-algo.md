# Window Algorithm Analysis and Implementation Plan

The "Window" algorithm creates a **polyrhythmic, evolving textural sequence** by using two independent time-cycles (phasors) running at different speeds.

## Musical Characteristics

*   **Structure:** It feels like a "Lead vs. Background" interplay. A slow, steady cycle drives strong, accented "anchor" notes (the Lead), while a faster cycle generates rhythmic fill and texture (the Background).
*   **Rhythm:** The relationship between the slow and fast cycles creates a **polyrhythmic feel** (e.g., 3-against-4 or 5-against-1 feel), governed by the `PhaseRatio` (3 to 6).
*   **Melody:**
    *   **Accents:** Deterministic and rising. They occur when the slow cycle wraps around, creating a predictable "downbeat" anchor in a high octave.
    *   **Texture (Voice A):** A "random walk" melody (Markov chain) in the middle register. It wanders step-wise based on previous notes.
    *   **Texture (Voice B):** Arpeggiated notes derived directly from the fast clock phase, sitting in a lower register.
*   **Dynamics:** High contrast. Accents are loud, long, and slide. Background notes are short and quiet, with "ghost notes" providing subtle rhythmic subdivision.
*   **Evolution:** The groove "breathes" over time as the `PhaseRatio` (rhythmic density) and `GhostThreshold` (note density) randomly mutate.

## Reference Code (Original C Implementation)

```cpp
struct Algo_Window_State {
	uint32_t SlowPhase, FastPhase;
	uint8_t NoteMemory, NoteHistory;
	uint8_t GhostThreshold; // 0-31, mutable
	uint8_t PhaseRatio;     // 3-6, mutable
};

void Algo_Window_Init(...) {
	Tuesday_RandomSeed(R, T->seed1 >> 4);
	Tuesday_RandomSeed(&Output->ExtraRandom, T->seed2 >> 4);
	Output->Window.SlowPhase = Tuesday_Rand(R) << 16;
	Output->Window.FastPhase = Tuesday_Rand(R) << 16;
	Output->Window.NoteMemory = Tuesday_Rand(R) & 0x7;
	Output->Window.NoteHistory = Tuesday_Rand(R) & 0x7;
	Output->Window.GhostThreshold = Tuesday_Rand(R) & 0x1f;
	Output->Window.PhaseRatio = 3 + (Tuesday_Rand(R) & 0x3);
}

void Algo_Window_Gen(...) {
	struct ScaledNote SN;
	DefaultTick(Output);

	// Advance phases at mutable ratio
	Output->Window.SlowPhase += 0xffffffff / __max(1, T->CurrentPattern.Length);
	Output->Window.FastPhase += 0xffffffff / (__max(1, T->CurrentPattern.Length / Output->Window.PhaseRatio));

	// Mutate parameters (like TriTrance's b2/b3)
	if ((I % 96) == 0 && Tuesday_BoolChance(&PS->ExtraRandom)) {
		Output->Window.PhaseRatio = 3 + (Tuesday_Rand(&PS->ExtraRandom) & 0x3);
	}
	if ((I % 64) == 0 && Tuesday_BoolChance(&PS->ExtraRandom)) {
		Output->Window.GhostThreshold = Tuesday_Rand(&PS->ExtraRandom) & 0x1f;
	}

	// Core decisions
	bool isAccent = ((Output->Window.SlowPhase & 0xff000000) == 0);
	bool isGhost = ((Output->Window.FastPhase >> 27) & 0x1f) > Output->Window.GhostThreshold;
	bool voiceA = (Output->Window.FastPhase >> 28) & 1;

	if (isAccent) {
		// Gate forced: high velocity, long gate
		Output->accent = 1;
		SN.note = Output->Window.NoteMemory;
		SN.oct = 2 + ((Output->Window.SlowPhase >> 26) & 1);
		Output->vel = 100 + (Tuesday_Rand(R) / 4);
		Output->maxsubticklength = ((3 + (Tuesday_Rand(R) % 2)) * TUESDAY_SUBTICKRES) - 2; // 3-4 steps
		Output->slide = (Tuesday_Rand(R) % 3) + 2;
		// Update memory
		Output->Window.NoteMemory = (Output->Window.NoteMemory + 1) & 0x7;
	} else {
		// Gate depends on deterministic voice logic
		Output->accent = 0;
		bool shouldPlay = voiceA ? !isGhost : ((Output->Window.FastPhase >> 29) & 1);

		if (!shouldPlay) {
			NOTEOFF();
			return;
		}

		if (voiceA) {
			// Order-2 Markov walk
			Tuesday_RandomSeed(&PS->ExtraRandom, Output->Window.NoteHistory + Output->Window.NoteMemory);
			int step = (Tuesday_Rand(&PS->ExtraRandom) % 3) - 1; // -1,0,+1
			SN.note = (Output->Window.NoteMemory + step) & 0x7;
			SN.oct = 1 + ((Output->Window.SlowPhase >> 26) & 1);
			Output->Window.NoteHistory = Output->Window.NoteMemory;
			Output->Window.NoteMemory = SN.note;
		} else {
			SN.note = ((Output->Window.FastPhase >> 25) & 0x7);
			SN.oct = 0;
		}

		// Low velocity, short gate
		Output->vel = isGhost ? (Tuesday_Rand(R) / 8) + 5 : (Tuesday_Rand(R) / 3) + 40;
		Output->maxsubticklength = ((1 + (Tuesday_Rand(R) % 2)) * TUESDAY_SUBTICKRES) - 2; // 1-2 steps
		Output->slide = (Tuesday_Rand(R) / 4) & 0x3; // 0-3 small slides
	}

	Output->note = ScaleToNote(&SN, T, P, S);
}
```

## Implementation Strategy

Since `TuesdayTrackEngine` stores algorithm state as member variables rather than a generic union, you will need to add specific members for the Window algorithm.

#### 1. Add State Variables to `TuesdayTrackEngine.h`

Add these member variables to the `TuesdayTrackEngine` class to hold the persistent state:

```cpp
    // WINDOW algorithm state
    uint32_t _windowSlowPhase = 0;
    uint32_t _windowFastPhase = 0;
    uint8_t _windowNoteMemory = 0;
    uint8_t _windowNoteHistory = 0;
    uint8_t _windowGhostThreshold = 0;
    uint8_t _windowPhaseRatio = 0;
```

#### 2. Initialization Logic (`initAlgorithm` in `.cpp`)

In `TuesdayTrackEngine::initAlgorithm`, add a case for `Algorithm::Window`. You need to map the external seed inputs to the internal state:

*   Seed the RNGs (`_rng` and `_extraRng`).
*   Initialize `_windowSlowPhase` and `_windowFastPhase` to random 32-bit values.
*   Initialize `_windowNoteMemory` and `_windowHistory` to random 3-bit values (0-7).
*   Set `_windowGhostThreshold` (0-31) and `_windowPhaseRatio` (3-6).

#### 3. Generation Logic (`generateStep` in `.cpp`)

Create a helper function `generateWindow(uint32_t tick)` or implement directly inside the `generateStep` switch.

**Logic Mapping:**
*   **Tick Advance:** Instead of `DefaultTick`, use the engine's `tick` counter to detect mutation points.
*   **Phase Advance:**
    *   `SlowPhase += 0xffffffff / loopLength`
    *   `FastPhase += 0xffffffff / (loopLength / PhaseRatio)`
*   **Mutation:** Check `tick % 96 == 0` and `tick % 64 == 0` to mutate `PhaseRatio` and `GhostThreshold` using `_extraRng`.
*   **Decision Tree:**
    *   **Accent:** Check if `SlowPhase` wrapped (or high byte is 0). If so, generate a high-velocity, high-octave note, set `result.accent = true`, and increment `_windowNoteMemory`.
    *   **Texture:** If not an accent, check `voiceA` bit (from `FastPhase`).
        *   **Gate:** Use `isGhost` threshold logic to determine if `result.velocity` is low (ghost) or medium.
        *   **Voice A:** Use `_extraRng` seeded with `NoteMemory + NoteHistory` to pick a step (-1, 0, +1) for the Markov walk.
        *   **Voice B:** Map `FastPhase` bits directly to a note index.
*   **Output:** Fill the `TuesdayTickResult` struct with the calculated note, octave, velocity, and gate length.

# Ganz Algorithm Analysis and Implementation Plan

The "Ganz" algorithm ("Whole" or "Complete") creates a **granular, glitchy, and structurally complex** sequence driven by multiple interfering prime-number cycles.

## Musical Characteristics

*   **Structure:** Glitchy, IDM-inspired rhythm. It combines a rigid 5-tuplet sub-grid (`TupletPos`) with larger, unpredictable phrase skipping (`CountDown`).
*   **Rhythm:** Highly syncopated.
    *   **Micro-rhythm:** Based on a 5-step cycle where only steps 0, 1, and 2 play notes, creating a "stuttering" 3+2 feel.
    *   **Macro-rhythm:** Prime-number phased cycles (1, 5, 7) create long-evolving interference patterns.
    *   **Phrase Skipping:** Randomly skips entire phrases (like Stomper), creating silence/tension.
*   **Melody:** Mode-dependent complexity.
    *   **Mode 0:** Purely phase-driven (chaotic arpeggio).
    *   **Mode 1:** History-driven (neighboring notes based on previous output).
    *   **Mode 2:** Mixed phase interaction.
    *   **Mode 3:** Feedback loop (replaying history).
*   **Dynamics:** Sample-and-hold velocity. The velocity is "decimated" (held constant for periods), creating stepped dynamic changes rather than smooth flows.
*   **Evolution:** `PhraseLength` and `ModeState` mutate rarely, shifting the entire character of the generation every few bars.

## Reference Code (Original C Implementation)

```cpp
struct Algo_Ganz_State {
	uint32_t PhaseA, PhaseB, PhaseC;
	uint8_t NoteBuf[3];
	uint8_t PhraseLength; // 2-5, mutable
	uint8_t TupletPos;
	uint8_t CountDown;
	uint8_t ModeState;    // 0-3, mutable
	uint8_t Velocity;     // Held, decimated
};

void Algo_Ganz_Init(...) {
	Tuesday_RandomSeed(R, T->seed1 >> 5);
	Tuesday_RandomSeed(&Output->ExtraRandom, T->seed2 >> 5);
	Output->Ganz.PhaseA = Tuesday_Rand(R) << 16;
	Output->Ganz.PhaseB = Tuesday_Rand(R) << 16;
	Output->Ganz.PhaseC = Tuesday_Rand(R) << 16;
	Output->Ganz.NoteBuf[0] = Tuesday_Rand(R) % 8;
	Output->Ganz.NoteBuf[1] = Tuesday_Rand(R) % 8;
	Output->Ganz.NoteBuf[2] = Tuesday_Rand(R) % 8;
	Output->Ganz.PhraseLength = 2 + (Tuesday_Rand(R) & 0x3);
	Output->Ganz.TupletPos = 0;
	Output->Ganz.CountDown = 0;
	Output->Ganz.ModeState = Tuesday_Rand(R) % 4;
	Output->Ganz.Velocity = Tuesday_Rand(R) / 4 + 30;
}

void Algo_Ganz_Gen(...) {
	struct ScaledNote SN;
	DefaultTick(Output);

	// Advance phases (prime ratios)
	Output->Ganz.PhaseA += 0xffffffff / __max(1, T->CurrentPattern.Length);
	Output->Ganz.PhaseB += 0xffffffff / (__max(1, T->CurrentPattern.Length / 5));
	Output->Ganz.PhaseC += 0xffffffff / (__max(1, T->CurrentPattern.Length / 7));

	// Tuplet counter
	Output->Ganz.TupletPos = (Output->Ganz.TupletPos + 1) % 5;

	// Mutate phrase structure (rare)
	if ((I % 96) == 0 && Tuesday_BoolChance(&PS->ExtraRandom)) {
		Output->Ganz.PhraseLength = 2 + (Tuesday_Rand(&PS->ExtraRandom) & 0x3);
		Output->Ganz.ModeState = Tuesday_Rand(&PS->ExtraRandom) % 4;
	}

	// Stomper-style phrase skip
	if (Output->Ganz.CountDown > 0) {
		Output->Ganz.CountDown--;
		NOTEOFF();
		return;
	}
	if ((Output->Ganz.TupletPos == 0) && ((Output->Ganz.PhaseC >> 29) & 1)) {
		Output->Ganz.CountDown = Output->Ganz.PhraseLength - 1;
		NOTEOFF();
		return;
	}

	// Accent: primary tuplet beat
	bool isAccent = (Output->Ganz.TupletPos == 0 && ((Output->Ganz.PhaseA >> 30) & 1));

	if (isAccent) {
		// Force gate, high velocity, long
		Output->accent = 1;
		switch(Output->Ganz.ModeState) {
			case 0: SN.note = ((Output->Ganz.PhaseA >> 24) & 0x7); SN.oct = 2; break;
			case 1: SN.note = (Output->Ganz.NoteBuf[2] + 1) & 0x7; SN.oct = 1; break;
			case 2: SN.note = ((Output->Ganz.PhaseB >> 23) & 0x7); SN.oct = 0; break;
			default: SN.note = Output->Ganz.NoteBuf[1]; SN.oct = 1; break;
		}
		Output->vel = 100 + (Tuesday_Rand(R) / 4);
		Output->maxsubticklength = ((3 + (Tuesday_Rand(R) % 2)) * TUESDAY_SUBTICKRES) - 2;
		Output->slide = (Tuesday_Rand(R) % 4) + 1;
	} else {
		// Secondary/ghost
		Output->accent = 0;
		bool shouldPlay = (Output->Ganz.TupletPos < 3); // Ticks 0,1,2 play (0 is accent)
		if (!shouldPlay) {
			NOTEOFF();
			return;
		}

		switch(Output->Ganz.ModeState) {
			case 0: SN.note = ((Output->Ganz.PhaseA >> 26) & 0x7); SN.oct = 1; break;
			case 1: SN.note = (Output->Ganz.NoteBuf[2] - 1) & 0x7; SN.oct = 0; break;
			case 2: SN.note = ((Output->Ganz.PhaseC >> 25) & 0x7); SN.oct = 0; break;
			default: SN.note = Output->Ganz.NoteBuf[0]; SN.oct = 0; break;
		}

		// Velocity decimation
		if ((Output->Ganz.PhaseC >> 28) & 1) {
			Output->Ganz.Velocity = Tuesday_Rand(&PS->ExtraRandom) / 4 + 30;
		}
		Output->vel = Output->Ganz.Velocity + (Tuesday_Rand(R) / 16);
		Output->maxsubticklength = ((1 + (Tuesday_Rand(R) % 2)) * TUESDAY_SUBTICKRES) - 2;
		Output->slide = 0;
	}

	// Update note history
	Output->Ganz.NoteBuf[0] = Output->Ganz.NoteBuf[1];
	Output->Ganz.NoteBuf[1] = Output->Ganz.NoteBuf[2];
	Output->Ganz.NoteBuf[2] = SN.note;

	Output->note = ScaleToNote(&SN, T, P, S);
}
```

## Implementation Strategy

Add these member variables to the `TuesdayTrackEngine` class:

```cpp
    // GANZ algorithm state
    uint32_t _ganzPhaseA = 0;
    uint32_t _ganzPhaseB = 0;
    uint32_t _ganzPhaseC = 0;
    uint8_t _ganzNoteBuf[3] = {0, 0, 0};
    uint8_t _ganzPhraseLength = 0;
    uint8_t _ganzTupletPos = 0;
    uint8_t _ganzCountDown = 0;
    uint8_t _ganzModeState = 0;
    uint8_t _ganzVelocity = 0;
```

#### Initialization (`initAlgorithm`):
*   Seed RNGs (shift seeds by 5 bits this time).
*   Initialize all phases and buffers with random values.
*   Set initial `Velocity` and `ModeState`.

#### Generation (`generateStep` / `generateGanz`):
*   **Phasors:** Update 3 phases using `loopLength`, `loopLength/5`, and `loopLength/7` divisors.
*   **Micro-Rhythm:** Increment `TupletPos` modulo 5.
*   **Mutation:** Occasional (every 96 ticks) updates to `PhraseLength` and `ModeState`.
*   **Phrase Skip:** Check `CountDown`. If active, decrement and return NO_GATE. If 0 and condition met (tuplet 0 + random check), set countdown and return NO_GATE.
*   **Note Logic:**
    *   **Accent (Tuplet 0 + PhaseA check):**
        *   Switch on `ModeState` to pick note source (PhaseA, History, PhaseB, etc.).
        *   High velocity, long gate, slide.
    *   **Standard (Tuplet < 3):**
        *   If `TupletPos >= 3`, return NO_GATE (creates the 3+2 gap).
        *   Switch on `ModeState` for note source.
        *   **Velocity:** Use `_ganzVelocity` (decimated/held value) + small random jitter. Update `_ganzVelocity` occasionally based on `PhaseC`.
*   **History:** Shift `NoteBuf` and push new note.

# Blake Algorithm Analysis and Implementation Plan

The "Blake" algorithm creates a **melodic, breathing sequence** centered around a small motif (4 notes) that evolves dynamically through a "breath cycle."

## Musical Characteristics

*   **Structure:** Motif-based. It relentlessly repeats a 4-note motif (`Motif[4]`), but varies *how* those notes are played based on a rhythmic breathing pattern.
*   **Rhythm:** Cyclical breathing.
    *   **BreathCycle:** The overall length of the rhythmic phrase (4-7 steps).
    *   **BreathPattern:** Determines the note density/articulation for each step in the cycle. Patterns include combinations of "Real notes," "Ghost notes," and "Whispers" (rests).
*   **Melody:** Stable but evolving.
    *   The pitch material is fixed (until re-seeded), but the *octave* and *articulation* change.
    *   **Real Note:** The actual motif note in a middle octave.
    *   **Ghost Note:** Same pitch, but high octave, extremely low velocity (very quiet), and short gate. Creates a shimmering "halo" around the motif.
    *   **Whisper:** Silence (rest).
*   **Dynamics:** Organic swell. The velocity has a bias (`VelocityBias`) controlled by a very slow LFO (`BreathPhase`), simulating long crescendos and decrescendos.
*   **Sub-Bass Drop:** Occasionally, a deep, accented sub-bass note (`SubCountDown`) interrupts the flow, acting as a structural punctuation or drop.

## Reference Code (Original C Implementation)

```cpp
struct Algo_Blake_State {
	uint8_t Motif[4];
	uint8_t MotifPos;
	uint8_t BreathCycle;    // 4-7, mutable
	uint8_t BreathPattern;  // 0-3, mutable
	uint8_t SubCountDown;
	uint8_t BreathPhase;
	uint8_t VelocityBias;
};

void Algo_Blake_Init(...) {
	Tuesday_RandomSeed(R, T->seed1 >> 3);
	Tuesday_RandomSeed(&Output->ExtraRandom, T->seed2 >> 3);
	for(int i = 0; i < 4; i++) {
		Output->Blake.Motif[i] = Tuesday_Rand(&Output->ExtraRandom) % 8;
	}
	Output->Blake.MotifPos = 0;
	Output->Blake.BreathCycle = 4 + (Tuesday_Rand(R) & 0x3);
	Output->Blake.BreathPattern = Tuesday_Rand(R) & 0x3;
	Output->Blake.SubCountDown = 0;
	Output->Blake.BreathPhase = 0;
	Output->Blake.VelocityBias = 0;
}

void Algo_Blake_Gen(...) {
	struct ScaledNote SN;
	DefaultTick(Output);

	// Advance breath phase (slow arc)
	Output->Blake.BreathPhase += 0xffffffff / (T->CurrentPattern.Length * 8);
	if ((Output->Blake.BreathPhase & 0xff000000) == 0) {
		Output->Blake.VelocityBias = (Output->Blake.BreathPhase >> 24) & 0xf;
	}

	// Mutate breathing pattern (rare)
	if ((I % 96) == 0 && Tuesday_PercChance(&PS->ExtraRandom, 20)) {
		Output->Blake.BreathCycle = 4 + (Tuesday_Rand(&PS->ExtraRandom) & 0x3);
		Output->Blake.BreathPattern = (Output->Blake.BreathPattern + 1) % 4;
	}

	// Sub-bass drop (Stomper-style phrase trigger)
	if (Output->Blake.SubCountDown > 0) {
		Output->Blake.SubCountDown--;
		Output->accent = 1;
		SN.note = 0;
		SN.oct = 0;
		Output->vel = 110 + (Tuesday_Rand(R) / 8);
		Output->maxsubticklength = TUESDAY_SUBTICKRES - 2;
		if (Output->Blake.SubCountDown == 0) {
			Output->slide = (Tuesday_Rand(R) % 3) + 2;
		}
		goto FINISH;
	}
	if ((I % 48) == 0 && ((Output->Blake.BreathPhase >> 25) & 1)) {
		Output->Blake.SubCountDown = 2 + (Tuesday_Rand(R) % 2);
	}

	// Deterministic breathing cycle
	int pos = I % Output->Blake.BreathCycle;
	Output->accent = 0;

	switch(Output->Blake.BreathPattern) {
		case 0: // Real, ghost, whisper, whisper
			if (pos == 0) { // Real note
				SN.note = Output->Blake.Motif[Output->Blake.MotifPos];
				SN.oct = 1 + ((Output->Blake.BreathPhase >> 27) & 1);
				Output->vel = 45 + (Tuesday_Rand(R) / 3) + Output->Blake.VelocityBias;
				Output->maxsubticklength = ((2 + (Tuesday_Rand(R) % 2)) * TUESDAY_SUBTICKRES) - 2; // 2-3 steps
				Output->slide = Tuesday_PercChance(R, 25) ? (Tuesday_Rand(R) % 2) + 1 : 0;
				Output->Blake.MotifPos = (Output->Blake.MotifPos + 1) % 4;
			} else if (pos == 1) { // Ghost
				SN.note = Output->Blake.Motif[Output->Blake.MotifPos];
				SN.oct = 2;
				Output->vel = 8 + (Tuesday_Rand(R) / 8);
				Output->maxsubticklength = TUESDAY_SUBTICKRES / 2;
				Output->slide = 0;
			} else { // Whisper rest
				NOTEOFF();
				return;
			}
			break;
		case 1: // Ghost, real, whisper, whisper
			// ... similar ...
			break;
		// cases 2,3 for variety
		default:
			NOTEOFF();
			return;
	}

FINISH:
	Output->note = ScaleToNote(&SN, T, P, S);
}
```

## Implementation Strategy

Add these member variables to the `TuesdayTrackEngine` class:

```cpp
    // BLAKE algorithm state
    uint8_t _blakeMotif[4] = {0, 0, 0, 0};
    uint8_t _blakeMotifPos = 0;
    uint8_t _blakeBreathCycle = 0;
    uint8_t _blakeBreathPattern = 0;
    uint8_t _blakeSubCountDown = 0;
    uint32_t _blakeBreathPhase = 0; // Promoted to uint32_t for finer resolution
    uint8_t _blakeVelocityBias = 0;
```

#### Initialization (`initAlgorithm`):
*   Seed RNGs (shift seeds by 3 bits).
*   Fill `_blakeMotif` array with 4 random notes (scale degrees 0-7).
*   Initialize `BreathCycle` (4-7) and `BreathPattern` (0-3).
*   Reset phase and counters.

#### Generation (`generateStep` / `generateBlake`):
*   **Breath Phase:** Increment `BreathPhase` very slowly (`0xffffffff / (loopLength * 8)`). This creates an 8-bar LFO.
*   **Velocity Bias:** Extract from `BreathPhase` high bits.
*   **Mutation:** Occasionally shuffle `BreathCycle` and `BreathPattern`.
*   **Sub-Bass Drop:**
    *   Check `SubCountDown`. If active, play accented root note (octave 0), decrement, return result.
    *   Trigger logic: Every 48 ticks, if `BreathPhase` aligns, randomly start countdown.
*   **Breath Pattern Logic:**
    *   Calculate `pos = tick % BreathCycle`.
    *   Switch `BreathPattern` to determine if `pos` corresponds to a **Real Note**, **Ghost Note**, or **Rest**.
    *   **Real Note:** Play current motif note. Advance `MotifPos`. Moderate velocity + `VelocityBias`. Occasional slide.
    *   **Ghost Note:** Play current motif note (don't advance pos). High octave (2). Very low velocity. Short gate.
    *   **Rest:** Return NO_GATE.
