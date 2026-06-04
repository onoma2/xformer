#pragma once

#include "Routing.h"
#include "RouteApply.h"

// Draft/commit staging for one modulation, plus membership ops (is-modulated / remove-track).
// An existing Route is untouched until commit(); a freshly-created slot is reserved live but
// inert (source None) and freed on cancel().
namespace RouteDraft {

struct Draft {
    Routing::Route route;   // working copy (the staging buffer)
    int routeIndex = -1;    // slot in Routing (-1 = no slot)
    bool isNew = false;     // slot was freshly allocated by create()
};

// Stage a fresh modulation: allocate a slot, set target/tracks, combine=Modulate,
// all depth 0, source None (inert). routeIndex stays -1 if no empty slot.
inline Draft create(Routing &routing, Routing::Target target, int trackIndex) {
    Draft d;
    if (target == Routing::Target::None || trackIndex < 0 || trackIndex >= CONFIG_TRACK_COUNT) {
        return d;
    }
    int idx = routing.findEmptyRoute();
    if (idx < 0) {
        return d;
    }
    auto &r = routing.route(idx);
    r.clear();
    r.setTarget(target);
    r.setTracks(1 << trackIndex);
    r.setCombine(RouteApply::Combine::Modulate);
    for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
        r.setDepthPct(t, 0);
    }
    d.route = r;
    d.routeIndex = idx;
    d.isNew = true;
    return d;
}

// Begin editing an existing committed route (copy live -> draft).
inline Draft begin(const Routing &routing, int routeIndex) {
    Draft d;
    d.route = routing.route(routeIndex);
    d.routeIndex = routeIndex;
    d.isNew = false;
    return d;
}

// Source is required to commit.
inline bool canCommit(const Draft &d) {
    return d.routeIndex >= 0 && d.routeIndex < CONFIG_ROUTE_COUNT &&
           d.route.active() && d.route.source() != Routing::Source::None;
}

// Write draft -> live. Caller guarantees canCommit().
inline void commit(Routing &routing, const Draft &d) {
    routing.route(d.routeIndex) = d.route;
}

// Revert: existing route is already untouched; a new slot is freed.
inline void cancel(Routing &routing, const Draft &d) {
    if (d.isNew && d.routeIndex >= 0) {
        routing.route(d.routeIndex).clear();
    }
}

// Is this track currently modulated for the target? (label: MOD- when true, MOD+ when false)
inline bool isTrackModulated(const Routing &routing, Routing::Target target, int trackIndex) {
    if (Routing::isPerTrackTarget(target) && (trackIndex < 0 || trackIndex >= CONFIG_TRACK_COUNT)) {
        return false;
    }
    return routing.findRoute(target, trackIndex) >= 0;
}

// MOD- : remove this track from the modulation. Per-track: clear the track bit + zero its depth;
// when it was the last track, free the whole slot. Global target: free the slot. No-op if absent.
inline bool removeTrack(Routing &routing, Routing::Target target, int trackIndex) {
    if (Routing::isPerTrackTarget(target) && (trackIndex < 0 || trackIndex >= CONFIG_TRACK_COUNT)) {
        return false;
    }
    int r = routing.findRoute(target, trackIndex);
    if (r < 0) {
        return false;
    }
    auto &route = routing.route(r);
    if (Routing::isPerTrackTarget(target)) {
        route.setTracks(route.tracks() & ~(1 << trackIndex));
        route.setDepthPct(trackIndex, 0);
        if (route.tracks() == 0) {
            route.clear();
        }
    } else {
        route.clear();
    }
    return true;
}

} // namespace RouteDraft
