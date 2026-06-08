#pragma once

// Clean consumers for the base-less shell targets (Run/Reset) routed off the legacy
// shaper branch. They read the raw normalized source (0..1) directly — no bias, depth,
// window, or shaper. Reset reuses RouteParam::gateRisingEdge; Run uses the threshold here.

// Run gate: on above the legacy run threshold (hysteresis-friendly 0.55, not 0.5, to
// avoid chatter when a source idles near center).
inline bool routeRunGate(float source) {
    return source > 0.55f;
}
