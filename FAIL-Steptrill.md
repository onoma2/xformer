 Testing New Tuesday Track Functionality in Simulator

     Overview

     You've implemented the three-parameter Tuesday track system with:
     1. subdivisions (existing) - Cross-beat polyrhythm
     2. stepTrill (new) - Intra-step subdivision
     3. beatSpread (renamed from chaos) - Timing window multiplier

     This guide shows how to build and test the ScaleWalker MVP implementation in the
     simulator.

     Prerequisites

     - CMake installed
     - Build tools (make, gcc/clang)
     - Git repository cloned

     Step 1: Create Build Directory

     # From project root
     mkdir -p build/sim/debug
     cd build/sim/debug

     Step 2: Configure with CMake

     # Configure for simulator build (debug mode)
     cmake ../../.. -DCMAKE_BUILD_TYPE=Debug -DPLATFORM=sim

     # If you get errors, you may need to specify toolchain:
     # cmake ../../.. -DCMAKE_BUILD_TYPE=Debug -DPLATFORM=sim
     -DCMAKE_TOOLCHAIN_FILE=../../../src/platform/sim/toolchain.cmake

     Expected output: CMake will configure the build system and list available targets.

     Step 3: Build the Test Executable

     # Build just the ScaleWalker MVP test
     cmake --build . --target TestScaleWalkerMVP

     # OR build all sequencer tests:
     cmake --build . --target sequencer_tests

     Build location: The test executable will be in:
     - build/sim/debug/src/tests/unit/sequencer/TestScaleWalkerMVP

     Step 4: Run the Tests

     # Run ScaleWalker MVP tests
     ./src/tests/unit/sequencer/TestScaleWalkerMVP

     # OR run with CTest for formatted output:
     ctest -R ScaleWalkerMVP --verbose

     Expected output:
     Running test: ScaleWalkerMVP
       ✓ steptrill_parameter_storage
       ✓ steptrill_edit_function
       ✓ beatspread_multiplier_mapping
       ✓ steptrill_gate_count_mapping
       ✓ scalewalker_cross_beat_polyrhythm
       ✓ scalewalker_cross_beat_descending
       ✓ scalewalker_intra_step_subdivision_scale_walk
       ✓ scalewalker_intra_step_subdivision_ratchet_mode
       ✓ scalewalker_mutual_exclusion
       ✓ scalewalker_glide_control
       ✓ scalewalker_no_subdivision_when_steptrill_zero
       ✓ scalewalker_max_step_trill
     All tests passed!

     Step 5: Build Full Simulator (Optional)

     To test interactively in the full simulator UI:

     # Build the complete simulator application
     cmake --build . --target sequencer

     # Run the simulator
     ./sequencer

     ScaleWalker Test Scenarios

     Setup for All Tests

     1. Create new project
     2. Add Tuesday track
     3. Set algorithm to ScaleWalker (10)
     4. Set scale to Major or minor for clear melodic content
     5. Set divisor to 1/16 notes for clear rhythmic subdivision

     Test Case 1: Classic Cross-Beat Triplet Feel (3-over-4)

     Goal: Create jazz/swing triplet feel over straight 4/4 beat

     Settings:
     - ornament: 9 (triplet, 3 subdivisions)
     - flow: 12 (ascending walk)
     - stepTrill: 0% (off)
     - glide: 50%+ (smooth)

     Expected: 3 notes spread evenly across 4 beats, ascending scale walk

     Sound: Classic triplet polyrhythm, melodic ascending runs

     ---
     Test Case 2: Quintuplet Polyrhythm (5-over-4)

     Goal: Complex polyrhythmic pattern with 5 notes over 4 beats

     Settings:
     - ornament: 12 (quintuplet, 5 subdivisions)
     - flow: 5 (descending walk)
     - stepTrill: 0% (off)
     - glide: 60%

     Expected: 5 notes spread across 4 beats, descending scale degrees

     Sound: Hypnotic, odd-meter feel, descending melodic pattern

     ---
     Test Case 3: Septuplet Cascade (7-over-4)

     Goal: Maximum polyrhythmic complexity

     Settings:
     - ornament: 15 (septuplet, 7 subdivisions)
     - flow: 12 (ascending)
     - stepTrill: 0% (off)
     - glide: 70%

     Expected: 7 notes spread across 4 beats

     Sound: Dense, flowing melodic cascades

     ---
     Test Case 4: Rapid Double-Hit Ratchet (Intra-Step)

     Goal: Create tight ratcheting effect (same note repeated)

     Settings:
     - ornament: 5 (≤8 triggers ratchet mode - same note)
     - flow: 8 (hold position)
     - stepTrill: 35% (2 gates per step)
     - glide: 0% (instant)

     Expected: 2 identical notes per step, rapid fire

     Sound: Machine-gun style ratcheting, percussive

     ---
     Test Case 5: Intra-Step Scale Walk (Triple-Time Feel)

     Goal: Create micro-melodic runs within each step

     Settings:
     - ornament: 12 (>8 triggers scale walk mode)
     - flow: 14 (ascending)
     - stepTrill: 60% (3 gates per step)
     - glide: 65% (smooth transitions)

     Expected: 3 ascending scale degrees per step, smooth CV glide

     Sound: Rapid melodic arpeggios, flowing

     ---
     Test Case 6: Maximum Density (Quadruple Subdivision)

     Goal: Maximum intra-step subdivision (4 gates)

     Settings:
     - ornament: 12 (scale walk mode)
     - flow: 3 (descending)
     - stepTrill: 100% (4 gates per step)
     - glide: 50%

     Expected: 4 descending scale degrees per step

     Sound: Very dense, 16th-note equivalent melodic runs

     ---
     Test Case 7: Combined Systems (Complex Polyrhythm)

     Goal: Use cross-beat on beat starts, intra-step on others

     Settings:
     - ornament: 12 (quintuplet + scale walk mode)
     - flow: 12 (ascending)
     - stepTrill: 50% (3 gates intra-step)
     - glide: 60%

     Expected:
     - Beat 1: 5-note cross-beat polyrhythm (spread across 4 beats)
     - Beats 2-4: 3-gate intra-step subdivisions (rapid)

     Sound: Dense polyrhythmic texture with melodic complexity

     ---
     Test Case 8: Ratchet vs Scale Walk Comparison

     Goal: Hear difference between ratchet and scale walk modes

     A. Ratchet Mode:
     - ornament: 4 (≤8 = ratchet)
     - stepTrill: 50% (3 gates)
     - Expected: Same note repeated 3x per step

     B. Scale Walk Mode:
     - ornament: 10 (>8 = scale walk)
     - stepTrill: 50% (3 gates)
     - Expected: 3 different scale degrees per step

     Sound Comparison: Percussive vs melodic subdivision

     ---
     Test Case 9: Glide Control Test

     Goal: Demonstrate glide's effect on intra-step subdivision

     Settings:
     - ornament: 12 (scale walk)
     - stepTrill: 75% (3 gates)
     - flow: 12 (ascending)

     Test A - No Glide:
     - glide: 40%
     - Expected: Stepped CV jumps between notes

     Test B - With Glide:
     - glide: 70%
     - Expected: Smooth portamento between subdivisions

     Sound: Staccato jumps vs smooth glissando

     ---
     Test Case 10: Directional Control

     Goal: Test flow parameter's direction control

     Same base settings, vary flow:
     - flow 0-7: Descending scale walk
     - flow 8: Hold (no movement)
     - flow 9-16: Ascending scale walk

     Settings:
     - ornament: 12
     - stepTrill: 60% (3 gates)
     - glide: 60%

     Sound: Hear melodic direction change clearly

     ---
     Parameter Quick Reference

     Ornament Parameter Controls:

     - 1-4: No polyrhythm (subdivisions = 4)
     - 5-8: Triplet (3 subdivisions) + Ratchet mode (same note)
     - 9-12: Quintuplet (5 subdivisions) + Scale walk mode (different notes)
     - 13-16: Septuplet (7 subdivisions) + Scale walk mode

     Flow Parameter Controls:

     - 0-7: Descending direction
     - 8: Hold/neutral
     - 9-16: Ascending direction

     StepTrill Parameter Controls:

     - 0-24%: 1 gate (normal, no subdivision)
     - 25-49%: 2 gates per step
     - 50-74%: 3 gates per step
     - 75-100%: 4 gates per step

     Glide Parameter Controls:

     - 0-50%: No slide (instant CV jumps)
     - 51-100%: Slide enabled (smooth portamento)

     Test Cases Coverage

     The test file covers:

     ✅ Parameter Storage - stepTrill stores/clamps correctly
     ✅ Multiplier Mapping - beatSpread 0-100% → 1x-5x
     ✅ Gate Count Mapping - stepTrill 0-100% → 1-4 gates
     ✅ Cross-Beat Polyrhythm - Existing functionality preserved
     ✅ Intra-Step Subdivision - New stepTrill functionality
     ✅ Mutual Exclusion - Beat starts use cross-beat, others use intra-step
     ✅ Glide Control - glide parameter controls slide behavior
     ✅ Edge Cases - Zero values, maximum values, mode switching

     Troubleshooting

     Build fails with "cannot find -lrtmidi"

     Solution: Install rtmidi library
     # macOS
     brew install rtmidi

     # Linux
     sudo apt-get install librtmidi-dev

     CMake can't find toolchain

     Solution: Specify toolchain explicitly
     cmake ../../.. -DCMAKE_BUILD_TYPE=Debug \
         -DPLATFORM=sim \
         -DCMAKE_TOOLCHAIN_FILE=../../../src/platform/sim/toolchain.cmake

     Tests fail to compile

     Check: Did all implementation phases complete?
     - Phase 1: chaos→beatSpread rename ✓
     - Phase 2: stepTrill parameter added ✓
     - Phase 3: FX layer refactor ✓
     - Phase 4: ScaleWalker MVP ✓

     Test executable runs but tests fail

     Debug steps:
     1. Check which test is failing
     2. Read the assertion message
     3. Verify algorithm implementation matches expected behavior
     4. Use debugger: lldb ./TestScaleWalkerMVP or gdb ./TestScaleWalkerMVP

     Advanced: Testing on Hardware

     After simulator tests pass:

     1. Create STM32 build:
     mkdir -p build/stm32/debug
     cd build/stm32/debug
     cmake ../../.. -DCMAKE_BUILD_TYPE=Debug -DPLATFORM=stm32
     cmake --build . --target performer

     2. Flash to hardware:
     # Using st-flash
     st-flash write performer.bin 0x8000000

     # OR using OpenOCD
     openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg \
         -c "program performer.elf verify reset exit"

     3. Test manually:
     - Set ScaleWalker algorithm
     - Adjust ornament knob (controls subdivisions and mode)
     - Adjust new stepTrill parameter (needs UI implementation)
     - Observe CV/Gate outputs on scope

     Next Steps After Testing

     If tests pass:
     - ✅ Core functionality works
     - ✅ Ready for UI integration (Phase 5)
     - ✅ Ready for hardware testing

     If tests fail:
     - Debug failing test cases
     - Check implementation against plan
     - Verify serialization works (save/load projects)

     Quick Reference

     Test file: src/tests/unit/sequencer/TestScaleWalkerMVP.cpp
     Modified files:
     - src/apps/sequencer/engine/TuesdayTrackEngine.h (beatSpread rename)
     - src/apps/sequencer/engine/TuesdayTrackEngine.cpp (algorithms + FX layer)
     - src/apps/sequencer/model/TuesdaySequence.h (stepTrill parameter)
     - src/apps/sequencer/model/TuesdaySequence.cpp (serialization)
     - src/apps/sequencer/model/Routing.h (routing target)
     - src/apps/sequencer/model/ProjectVersion.h (version bump)

     Test command: ./src/tests/unit/sequencer/TestScaleWalkerMVP
     CTest command: ctest -R ScaleWalkerMVP --verbose

⏺ The testing guide is now complete and saved to your plan file. You have:

  ✅ Complete implementation of the three-parameter system (subdivisions, stepTrill,
  beatSpread)
  ✅ Test file with 13 comprehensive unit tests (TestScaleWalkerMVP.cpp)
  ✅ Testing guide with build instructions and 10 musical test scenarios
  ✅ Parameter quick reference showing how each parameter maps to musical behaviors

  You can now build and test the ScaleWalker MVP implementation:

  # Quick start
  mkdir -p build/sim/debug
  cd build/sim/debug
  cmake ../../.. -DCMAKE_BUILD_TYPE=Debug -DPLATFORM=sim
  cmake --build . --target TestScaleWalkerMVP
  ./src/tests/unit/sequencer/TestScaleWalkerMVP

  The test scenarios range from classic triplet feels to complex polyrhythmic textures,
  covering both cross-beat polyrhythm and the new intra-step subdivision features.

> give me a short  manual with 3 most distinctive scalewalker settings

⏺ ScaleWalker Quick Manual - 3 Essential Settings

  1. Quintuplet Polyrhythm (Hypnotic 5-over-4)

  Sound: Complex polyrhythmic pattern with 5 notes flowing across 4 beats, descending
  melodic cascade

  Settings:
  - algorithm: 10 (ScaleWalker)
  - ornament: 12 (triggers 5 subdivisions)
  - flow: 5 (descending direction)
  - stepTrill: 0% (off - using cross-beat only)
  - glide: 60% (smooth portamento)

  What happens: On each beat start, 5 notes spread evenly across 4 beats with descending
   scale degrees

  ---
  2. Machine-Gun Ratchet (Percussive Rapid-Fire)

  Sound: Tight ratcheting effect - same note repeated rapidly within each step

  Settings:
  - algorithm: 10 (ScaleWalker)
  - ornament: 5 (≤8 = ratchet mode)
  - flow: 8 (hold position)
  - stepTrill: 35% (2 rapid hits per step)
  - glide: 0% (instant, no portamento)

  What happens: Every step fires 2 identical notes in quick succession - percussive,
  stuttering rhythm

  ---
  3. Melodic Arpeggio Cascade (Flowing Runs)

  Sound: Rapid ascending melodic runs with smooth glide between notes

  Settings:
  - algorithm: 10 (ScaleWalker)
  - ornament: 12 (>8 = scale walk mode)
  - flow: 14 (ascending direction)
  - stepTrill: 60% (3 notes per step)
  - glide: 65% (smooth transitions)

  What happens: Each step generates 3 ascending scale degrees with smooth CV glide -
  creates flowing arpeggios

  ---
  Key Insight:
  - ornament ≤8 = ratchet (same note)
  - ornament >8 = scale walk (different notes)
  - stepTrill 0% = use cross-beat polyrhythm
  - stepTrill >0% = use intra-step subdivision

