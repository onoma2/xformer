#include "IndexedSequence.h"
#include "ProjectVersion.h"

const uint16_t IndexedSequence::GateTickTable[IndexedSequence::GateLengthTableSize] = {
    4, 5, 6, 7, 8, 9, 10, 11,
    12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27,
    28, 29, 30, 31, 32, 33, 35, 37,
    40, 43, 46, 50, 54, 58, 62, 67,
    72, 77, 83, 89, 95, 103, 110, 118,
    127, 137, 147, 158, 170, 183, 196, 211,
    227, 244, 262, 281, 302, 325, 349, 375,
    403, 434, 466, 501, 538, 578, 622, 668,
    718, 772, 829, 891, 958, 1030, 1107, 1189,
    1278, 1374, 1477, 1587, 1705, 1833, 1970, 2117,
    2276, 2446, 2628, 2825, 3036, 3263, 3507, 3769,
    4051, 4354, 4679, 5029, 5405, 5809, 6243, 6709,
    7211, 7750, 8329, 8952, 9621, 10340, 11113, 11944,
    12836, 13796, 14827, 15936, 17127, 18407, 19783, 21262,
    22851, 24559, 26395, 28368, 30488, 32767,
};

void IndexedSequence::writeRouted(Routing::Target target, int intValue, float floatValue) {
    switch (target) {
    case Routing::Target::Divisor:
        setDivisor(intValue);
        break;
    case Routing::Target::ClockMult:
        setClockMultiplier(intValue, true);
        break;
    case Routing::Target::Scale:
        setScale(intValue);
        break;
    case Routing::Target::RootNote:
        setRootNote(intValue);
        break;
    case Routing::Target::FirstStep:
        setFirstStep(intValue, true);
        break;
    case Routing::Target::RunMode:
        setRunMode(Types::RunMode(intValue), true);
        break;
    case Routing::Target::IndexedA:
        _routedIndexedA = floatValue * 0.01f;  // Normalize from -100..100 to -1..1
        break;
    case Routing::Target::IndexedB:
        _routedIndexedB = floatValue * 0.01f;  // Normalize from -100..100 to -1..1
        break;
    default:
        break;
    }
}
