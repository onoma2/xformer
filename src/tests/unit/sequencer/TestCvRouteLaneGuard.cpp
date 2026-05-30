#include "UnitTest.h"

#include "apps/sequencer/model/CvRoute.h"

UNIT_TEST("CvRouteLaneGuard") {

CASE("valid_lanes_return_set_values") {
    CvRoute route;
    route.clear();
    route.setInputSource(0, CvRoute::InputSource::Mod1);
    route.setOutputDest(0, CvRoute::OutputDest::Bus);
    expectEqual(int(route.inputSource(0)), int(CvRoute::InputSource::Mod1), "valid input lane returns set value");
    expectEqual(int(route.outputDest(0)), int(CvRoute::OutputDest::Bus), "valid output lane returns set value");
}

CASE("invalid_lane_getter_returns_safe_default") {
    CvRoute route;
    route.clear();
    route.setOutputDest(0, CvRoute::OutputDest::Bus); // aliases _inputs[LaneCount] if unguarded
    expectEqual(int(route.inputSource(-1)), int(CvRoute::InputSource::Off), "negative input lane guarded to Off");
    expectEqual(int(route.inputSource(CvRoute::LaneCount)), int(CvRoute::InputSource::Off), "over-bound input lane guarded to Off");
    expectEqual(int(route.outputDest(-1)), int(CvRoute::OutputDest::None), "negative output lane guarded to None");
    expectEqual(int(route.outputDest(CvRoute::LaneCount)), int(CvRoute::OutputDest::None), "over-bound output lane guarded to None");
}

}
