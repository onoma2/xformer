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
    case Routing::Target::Rotate:
        setRotate(intValue, true);
        break;
    case Routing::Target::GateProbabilityBias:
        setGateBias(intValue, true);
        break;
    case Routing::Target::RetriggerProbabilityBias:
        setRetriggerBias(intValue, true);
        break;
    case Routing::Target::LengthBias:
        setLengthBias(intValue, true);
        break;
    case Routing::Target::NoteProbabilityBias:
        setNoteBias(intValue, true);
        break;
    case Routing::Target::StochasticDensity:
        setDensity(intValue, true);
        break;
    case Routing::Target::StochasticTilt:
        setTilt(intValue, true);
        break;
    case Routing::Target::StochasticJitter:
        setJitter(intValue, true);
        break;
    case Routing::Target::StochasticBurst:
        setBurst(intValue, true);
        break;
    default:
        break;
    }
}
