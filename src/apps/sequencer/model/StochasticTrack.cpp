#include "StochasticTrack.h"

void StochasticTrack::writeRouted(Routing::Target target, int intValue, float floatValue) {
    switch (target) {
    case Routing::Target::SlideTime:
        setSlideTime(intValue, true);
        break;
    case Routing::Target::Octave:
        setOctave(intValue, true);
        break;
    case Routing::Target::Transpose:
        setTranspose(intValue, true);
        break;
    default:
        break;
    }
}
