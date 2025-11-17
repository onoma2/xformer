include_directories(../../../apps/sequencer)
include_directories(../../../apps/sequencer/model) # Add include for model files

# Modify register_test to link against sequencer_shared
function(register_sequencer_test test file)
    add_executable(${test} ${file})
    target_link_libraries(${test} core sequencer_shared) # Link against sequencer_shared
    platform_postprocess_executable(${test})
    add_test(NAME ${test} COMMAND ${test})
endfunction(register_sequencer_test)

register_sequencer_test(TestCurve TestCurve.cpp)
register_sequencer_test(TestScale TestScale.cpp)
register_sequencer_test(TestAccumulator TestAccumulator.cpp)
register_sequencer_test(TestNoteSequence TestNoteSequence.cpp)
register_sequencer_test(TestNoteTrackEngine TestNoteTrackEngine.cpp)
register_sequencer_test(TestAccumulatorModulation TestAccumulatorModulation.cpp)
