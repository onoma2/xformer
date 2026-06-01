#pragma once

#include "RouteParam.h"

// MidiCv per-type param table (U4 of the routing mod-matrix overhaul).
//
// Track-scope: Scope.object is a Track* in MidiCv mode. MidiCv is a track-only
// mode (no sequences); both rows write track-level fields, routed slot, no
// fan-out -- mirroring MidiCvTrack::writeRouted (Transpose/SlideTime). Reuses the
// shared Transpose/SlideTime keys. Built alongside the live old dispatch; nothing
// wired live yet.

struct MidiCvParamTable {
    static const RouteParam::Table &table();
};
