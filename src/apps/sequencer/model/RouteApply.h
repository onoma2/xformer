#pragma once

#include <cstdint>

// Name-agnostic value pipeline for the routing mod-matrix overhaul (U5 + U6b math).
//
// Turns a shaped, normalized source value into a signed delta to add to a param's
// base; the read combines clamp(base + delta) uniformly. Pure: no engine state, no
// neighbor/route coupling -- the explicit scaleSource replaces the old VcaNext
// `routeIndex+1` positional hack (R6: scaleSource is a plain Source value).
//
//   hs    = 0.5 + (h - 0.5) * scaleValue          // scaleSource: scale the centered output
//   u     = Modulate ? (hs - 0.5) * 2 : hs        // centered [-1,+1] (neutral at center) vs raw [0,1]
//   delta = (d / 100) * u * range                 // signed % depth/travel * source-form * param span
//
// Conventions: scaleValue is the scaleSource's normalized value in [0,1]; pass 1.0
// for "no scale source" (identity). dPercent is the per-track signed gain in
// [-100,100] (depth for Modulate, travel span for Absolute). range is the param's
// d=100% displacement from the registry ((max-min)/2 bipolar, (max-min) unipolar).
//
// Structural contracts (the apply pipeline relies on these):
//  - Modulate is neutral at source-center: h=0.5 -> hs=0.5 -> u=0 -> delta=0, for any
//    d, range, or scaleValue (center-preserving scale: 0.5 maps to 0.5).
//  - Absolute sweeps from base: source 0 -> delta 0 (base), source 1 -> d*range.
//
// See docs/plans/2026-05-31-002-routing-mod-matrix-overhaul-plan.md (apply pseudocode,
// R15/R16) and 2026-06-01-003-routed-value-storage-subspec.md (combine/override).

namespace RouteApply {

    enum class Combine : uint8_t {
        Modulate,   // centered source: bipolar around base, neutral at source-center
        Absolute,   // raw source: sweep from base, d*range travel
    };

    // Scale the centered deviation of h by scaleValue (preserves the 0.5 center).
    inline float scaled(float h, float scaleValue) {
        return 0.5f + (h - 0.5f) * scaleValue;
    }

    // Convert a shaped/scaled source [0,1] into the combine source-form.
    inline float toU(float hs, Combine combine) {
        return combine == Combine::Modulate ? (hs - 0.5f) * 2.f : hs;
    }

    // Full pipeline: shaped source -> signed delta. scaleValue = 1.0 means no scale source.
    inline float delta(float h, float scaleValue, Combine combine, int dPercent, float range) {
        return (dPercent * 0.01f) * toU(scaled(h, scaleValue), combine) * range;
    }

} // namespace RouteApply
