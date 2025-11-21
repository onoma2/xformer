# Tuesday Track Algorithms

## Algorithm Catalog

### Original Tuesday Algorithms (from source code)

| # | Enum | Name | Type | State Structure | Description |
|---|------|------|------|-----------------|-------------|
| 0 | ALGO_TESTS | **TEST** | Utility | Mode, Note, Velocity | Calibration/test patterns (oct sweeps, min/max) |
| 1 | ALGO_TRITRANCE | **TRITRANCE** | Melodic | Generic | German-style minimal melodies |
| 2 | ALGO_STOMPER | **STOMPER** | Rhythmic | LastMode, CountDown, LowNote, HighNote[2] | Fishy patterns with slides, 14 actions |
| 3 | ALGO_MARKOV | **MARKOV** | Melodic | NoteHistory1/3, matrix[8][8][2] | 3rd-order Markov chain transitions |
| 4 | ALGO_WOBBLE | **WOBBLE** | Smooth | Phase, PhaseSpeed (×2), LastWasHigh | Dual LFO wobble patterns |
| 5 | ALGO_CHIPARP1 | **CHIPARP1** | Arpeggio | R, ChordSeed, Base, Dir | Chip-tune arpeggio style 1 |
| 6 | ALGO_CHIPARP2 | **CHIPARP2** | Arpeggio | R, ChordSeed, len, offset, etc. | Chip-tune arpeggio style 2 |
| 7 | ALGO_SNH | **SNH** | Random | Phase, LastVal, Target, Current, Filter | Sample & Hold with slew |
| 8 | ALGO_SAIKO_CLASSIC | **SAIKO** | Classic | Generic | Classic Saiko patterns reimagined |
| 9 | ALGO_SAIKO_LEAD | **SAIKOLEAD** | Lead | Generic | Saiko lead patterns |
| 10 | ALGO_SCALEWALKER | **SCALEWALK** | Melodic | R, WalkLen, Current | Sequential scale degree walking |
| 11 | ALGO_TOOEASY | **TOOEASY** | Simple | R, WalkLen, Current, Pattern[16] | Simple pre-made patterns |
| 12 | ALGO_RANDOM | **RANDOM** | Chaotic | (none) | Pure random notes |

### New Algorithms (from ALGO-RESEARCH)

| # | Name | Type | State Structure | Description |
|---|------|------|-----------------|-------------|
| 13 | **CELLAUTO** | Chaotic | cells[16], rule, generation | Cellular automata (Wolfram rules) |
| 14 | **CHAOS** | Chaotic | x, r, threshold (floats) | Logistic map chaos |
| 15 | **FRACTAL** | Chaotic | zoom, offset, depth (floats) | Mandelbrot-style patterns |
| 16 | **WAVE** | Smooth | phase1/2/3, freq1/2/3 (floats) | Triple wave interference |
| 17 | **DNA** | Evolving | genome[16], mutation_rate | Genetic mutation/crossover |
| 18 | **TURING** | Stateful | tape[32], head, state, rules | Turing machine sequencer |
| 19 | **PARTICLE** | Physics | particles[8] (floats) | Bouncing particle system |
| 20 | **NEURAL** | Adaptive | neurons[16], weights[16][16] | Neural network with learning |
| 21 | **QUANTUM** | Probabilistic | probability_map[16][8] | Quantum wave collapse |
| 22 | **LSYSTEM** | Generative | axiom, rules, string | L-System string rewriting |

### Genre-Inspired Algorithms

| # | Name | Genre | State Structure | Description |
|---|------|-------|-----------------|-------------|
| 23 | **TECHNO** | Minimal Techno | sequence[8], position, variation_seed | Hypnotic repetition, locked groove, subtle variation |
| 24 | **JUNGLE** | Drum & Bass | break_pattern[16], chop_position, roll_count | Breakbeat chops, syncopation, fast bursts |
| 25 | **AMBIENT** | Ambient/Drone | last_note, hold_timer, drift_direction | Sparse triggers, long holds, slow drift |
| 26 | **ACID** | Acid House | sequence[8], position, slide_flag, accent_pattern | 303-style patterns, slides, accents on octaves |
| 27 | **FUNK** | Funk/Disco | groove_template, ghost_probability, pocket_offset | Syncopated groove, ghost notes, off-beat emphasis |
| 28 | **DRILL** | UK Drill/Trap | hihat_pattern, slide_target, triplet_mode | Hi-hat rolls, bass slides, triplet subdivisions |
| 29 | **MINIMAL** | Minimal/Clicks | burst_length, silence_length, click_density | Staccato bursts, silence gaps, glitchy |

### Artist-Inspired Algorithms

| # | Name | Artist | State Structure | Description |
|---|------|--------|-----------------|-------------|
| 30 | **KRAFTWERK** | Kraftwerk | sequence[8], position, lock_timer | Precise mechanical sequences, robotic repetition |
| 31 | **APHEX** | Aphex Twin | pattern[16], time_sig_num, glitch_prob | Complex polyrhythms, acid lines, glitchy variations |
| 32 | **BOARDS** | Boards of Canada | base_note, detune_amount, wobble_phase | Nostalgic detuned textures, tape wobble, melancholic |
| 33 | **TANGERINE** | Tangerine Dream | arp_pattern[16], filter_pos, sequence_length | Berlin school motorik arpeggios, cosmic |
| 34 | **AUTECHRE** | Autechre | transform_matrix[4], mutation_rate, chaos_seed | Abstract constantly evolving, never repeats |
| 35 | **SQUAREPUSH** | Squarepusher | bass_position, run_length, jazz_mode | Virtuoso bass runs, jazz-influenced, slap patterns |
| 36 | **DAFT** | Daft Punk | loop_pattern[8], filter_cutoff, sweep_direction | Filtered disco loops, 4-on-floor, vocal rhythms |

### Parameter Response (Genre/Artist Algorithms)

| Algo | Power (density) | Flow (movement) | Ornament (decoration) |
|------|-----------------|-----------------|----------------------|
| TECHNO | Gates per bar | Variation frequency | Offbeat triggers |
| JUNGLE | Chop density | Syncopation | Rolls/ghosts |
| AMBIENT | Trigger rate | Note drift | Harmonics |
| ACID | Sequence length | Slide probability | Accents/octaves |
| FUNK | Ghost density | Pocket tightness | Approach notes |
| DRILL | Roll density | Slide frequency | Triplet feel |
| MINIMAL | Burst length | Silence duration | Glitch repeats |
| KRAFTWERK | Note density | Transposition freq | Mechanical ghosts |
| APHEX | Time sig complexity | Glitch probability | Acid accents |
| BOARDS | Trigger density | Detune amount | Degraded harmonics |
| TANGERINE | Arp speed | Filter movement | Cosmic octaves |
| AUTECHRE | Mutation intensity | Transform rate | Micro-timing |
| SQUAREPUSH | Run speed | Jazz balance | Articulations |
| DAFT | Filter resonance | Sweep speed | Disco fills |

### MVP Algorithm Set (8 algorithms)

| Priority | # | Name | Complexity | Why Include |
|----------|---|------|------------|-------------|
| **1** | 12 | RANDOM | Very Easy | Baseline test, no state needed |
| **2** | 3 | MARKOV | Medium | Classic Tuesday, intelligent melodic transitions |
| **3** | 2 | STOMPER | Medium | Rhythmic variety, slides, 14 actions |
| **4** | 26 | ACID | Medium | Recognizable 303-style, slides + accents |
| **5** | 30 | KRAFTWERK | Easy | Mechanical precision, robotic loops |
| **6** | 25 | AMBIENT | Easy | Sparse textures, slow evolution |
| **7** | 23 | TECHNO | Easy | Hypnotic repetition, locked grooves |
| **8** | 10 | SCALEWALK | Easy | Predictable baseline, testing reference |

**MVP Selection Rationale:**
- **Classic Tuesday**: RANDOM, MARKOV, STOMPER, SCALEWALK (proven algorithms)
- **Genre variety**: ACID (house), AMBIENT (drone), TECHNO (minimal)
- **Artist reference**: KRAFTWERK (mechanical/robotic)
- **Density spectrum**: AMBIENT (sparse) → TECHNO (medium) → STOMPER (dense)

### Deferred Algorithms (Post-MVP)

| # | Name | Reason |
|---|------|--------|
| 0 | TEST | Utility/calibration only |
| 1 | TRITRANCE | Need implementation analysis |
| 4 | WOBBLE | Dual phase LFOs, moderate complexity |
| 5-6 | CHIPARP1/2 | Chord awareness, arpeggiator logic |
| 7 | SNH | Filter state, slew calculations |
| 8-9 | SAIKO | Need implementation analysis |
| 11 | TOOEASY | Pre-made patterns, less interesting |
| 13-22 | Computational | Float math, complex state structures |
| 24 | JUNGLE | Complex breakbeat logic |
| 27 | FUNK | Groove template needed |
| 28 | DRILL | Triplet math, modern style |
| 29 | MINIMAL | Timing precision needed |
| 31 | APHEX | Complex time signatures |
| 32 | BOARDS | Detune/wobble LFO |
| 33 | TANGERINE | Filter sweep logic |
| 34 | AUTECHRE | Complex transformations |
| 35 | SQUAREPUSH | Jazz theory needed |
| 36 | DAFT | Filter sweep logic |

### Implementation Order

1. **RANDOM** - No state, pure random, baseline test
2. **SCALEWALK** - Minimal state (position), deterministic
3. **KRAFTWERK** - Simple loop + lock timer, mechanical
4. **AMBIENT** - Hold timer + drift, sparse output
5. **TECHNO** - Loop + variation timer, hypnotic
6. **MARKOV** - Matrix transitions, classic Tuesday
7. **ACID** - Sequence + slides + accents, 303-style
8. **STOMPER** - State machine, 14 actions, rhythmic

### Algorithm Source Files

Original Tuesday source code location: `ALGO-RESEARCH/Tuesday/Sources/`

| File | Algorithm |
|------|-----------|
| `Algo_Markov.h` | MARKOV implementation |
| `Algo_Stomper.h` | STOMPER implementation |
| `Algo_ScaleWalker.h` | SCALEWALK implementation |
| `Algo_SNH.h` | SNH implementation |
| `Algo_TooEasy.h` | TOOEASY implementation |
| `Algo_Wobble.h` | WOBBLE implementation |
| `Algo_ChipArps.h` | CHIPARP1/2 implementations |
| `Algo_TriTrance.h` | TRITRANCE implementation |
| `Algo_SaikoSet.h` | SAIKO implementations |
| `Algo_Test.h` | TEST implementation |
| `Algo.h` | Common structures and enum |
| `Algo.c` | Utility functions |