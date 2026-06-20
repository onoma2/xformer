#include "UnitTest.h"

#include "ui/KeyboardManager.h"
#include "ui/MatrixMap.h"
#include "ui/Key.h"

// Panel-faithful USB-keyboard -> control assignment (sim parity).
// hidKeycodeToButton() returns the final Key code for a panel key, or -1.

UNIT_TEST("KeyboardMapping") {

CASE("q-row maps to tracks 0-7") {
    expectEqual(KeyboardManager::hidKeycodeToButton(0x14), MatrixMap::fromTrack(0), "Q -> track 0");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x1A), MatrixMap::fromTrack(1), "W -> track 1");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x08), MatrixMap::fromTrack(2), "E -> track 2");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x15), MatrixMap::fromTrack(3), "R -> track 3");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x17), MatrixMap::fromTrack(4), "T -> track 4");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x1C), MatrixMap::fromTrack(5), "Y -> track 5");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x18), MatrixMap::fromTrack(6), "U -> track 6");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x0C), MatrixMap::fromTrack(7), "I -> track 7");
}

CASE("a-row maps to steps 0-7") {
    expectEqual(KeyboardManager::hidKeycodeToButton(0x04), MatrixMap::fromStep(0), "A -> step 0");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x16), MatrixMap::fromStep(1), "S -> step 1");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x07), MatrixMap::fromStep(2), "D -> step 2");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x09), MatrixMap::fromStep(3), "F -> step 3");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x0A), MatrixMap::fromStep(4), "G -> step 4");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x0B), MatrixMap::fromStep(5), "H -> step 5");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x0D), MatrixMap::fromStep(6), "J -> step 6");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x0E), MatrixMap::fromStep(7), "K -> step 7");
}

CASE("z-row maps to steps 8-15") {
    expectEqual(KeyboardManager::hidKeycodeToButton(0x1D), MatrixMap::fromStep(8), "Z -> step 8");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x1B), MatrixMap::fromStep(9), "X -> step 9");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x06), MatrixMap::fromStep(10), "C -> step 10");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x19), MatrixMap::fromStep(11), "V -> step 11");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x05), MatrixMap::fromStep(12), "B -> step 12");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x11), MatrixMap::fromStep(13), "N -> step 13");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x10), MatrixMap::fromStep(14), "M -> step 14");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x36), MatrixMap::fromStep(15), ", -> step 15");
}

CASE("number row 1-4 maps to mode buttons") {
    expectEqual(KeyboardManager::hidKeycodeToButton(0x1E), static_cast<int>(Key::Play), "1 -> Play");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x1F), static_cast<int>(Key::Tempo), "2 -> Tempo");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x20), static_cast<int>(Key::Pattern), "3 -> Pattern");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x21), static_cast<int>(Key::Performer), "4 -> Performer");
}

CASE("space maps to encoder press") {
    expectEqual(KeyboardManager::hidKeycodeToButton(0x2C), static_cast<int>(Key::Encoder), "space -> encoder");
}

CASE("unmapped keys return -1") {
    expectEqual(KeyboardManager::hidKeycodeToButton(0x28), -1, "Enter unmapped");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x12), -1, "O not in any panel row");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x22), -1, "5 unmapped (only 1-4 are modes)");
    expectEqual(KeyboardManager::hidKeycodeToButton(0x00), -1, "null keycode unmapped");
}

}
