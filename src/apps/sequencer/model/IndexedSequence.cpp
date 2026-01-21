#include "IndexedSequence.h"
#include "ProjectVersion.h"

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
