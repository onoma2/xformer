#include "apps/sequencer/ui/model/AccumulatorListModel.h"
#include "apps/sequencer/model/NoteSequence.h"

int main() {
    // Create a test sequence with accumulator
    NoteSequence sequence;
    
    // Test the AccumulatorListModel
    AccumulatorListModel model;
    model.setSequence(&sequence);
    
    // Test initial values
    printf("Initial Direction: %d (expected: 0 for Up)\n", (int)sequence.accumulator().direction());
    printf("Initial Order: %d (expected: 0 for Wrap)\n", (int)sequence.accumulator().order());
    
    // Simulate encoder change for Direction (should cycle Up -> Down -> Freeze)
    // Direction is at row 1 (second row)
    model.edit(1, 1, 1, false); // Move encoder forward once
    printf("After 1st encoder change - Direction: %d (expected: 1 for Down)\n", (int)sequence.accumulator().direction());
    
    model.edit(1, 1, 1, false); // Move encoder forward again
    printf("After 2nd encoder change - Direction: %d (expected: 2 for Freeze)\n", (int)sequence.accumulator().direction());
    
    model.edit(1, 1, 1, false); // Move encoder forward again (should wrap to Up)
    printf("After 3rd encoder change - Direction: %d (expected: 0 for Up)\n", (int)sequence.accumulator().direction());
    
    // Test Order cycling (Wrap -> Pendulum -> Random -> Hold -> Wrap)
    // Order is at row 2 (third row)
    model.edit(2, 1, 1, false); // Move encoder forward once
    printf("After 1st Order change - Order: %d (expected: 1 for Pendulum)\n", (int)sequence.accumulator().order());
    
    model.edit(2, 1, 1, false); // Move encoder forward again
    printf("After 2nd Order change - Order: %d (expected: 2 for Random)\n", (int)sequence.accumulator().order());
    
    model.edit(2, 1, 1, false); // Move encoder forward again
    printf("After 3rd Order change - Order: %d (expected: 3 for Hold)\n", (int)sequence.accumulator().order());
    
    model.edit(2, 1, 1, false); // Move encoder forward again (should wrap to Wrap)
    printf("After 4th Order change - Order: %d (expected: 0 for Wrap)\n", (int)sequence.accumulator().order());
    
    // Test backward cycling
    model.edit(1, 1, -1, false); // Move encoder backward
    printf("After backward cycle - Direction: %d (expected: 2 for Freeze)\n", (int)sequence.accumulator().direction());
    
    model.edit(2, 1, -1, false); // Move encoder backward
    printf("After backward cycle - Order: %d (expected: 3 for Hold)\n", (int)sequence.accumulator().order());
    
    printf("All tests completed successfully!\n");
    return 0;
}