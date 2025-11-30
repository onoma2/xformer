#pragma once

#include <cmath>
#include <vector>
#include <functional>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Curve {

    enum class Type : int8_t {
        Off = 0,
        Linear,
        Exponential,
        Logarithmic,
        Sine,
        Cosine,
        Tangent,
        Bell,
        Sigmoid,
        StepUp,
        StepDown,
        RampUp,
        RampDown,
        Triangle,
        SmoothStep,
        Parabolic,
        Circular,
        Square,
        Saw,
        InverseSaw,
        Last
    };

    typedef std::function<float(float)> Function;

    inline Function function(Type type) {
        switch (type) {
            case Type::Off:          return [](float) { return 0.f; };
            case Type::Linear:       return [](float x) { return x; };
            case Type::Exponential:  return [](float x) { return x * x; };
            case Type::Logarithmic:  return [](float x) { return x < 0.001f ? 0.f : logf(x + 0.001f) / logf(1.001f); };
            case Type::Sine:         return [](float x) { return 0.5f + 0.5f * sinf(x * 2.f * float(M_PI) - float(M_PI) / 2.f); };
            case Type::Cosine:       return [](float x) { return 0.5f + 0.5f * cosf(x * 2.f * float(M_PI) - float(M_PI) / 2.f); };
            case Type::Tangent:      return [](float x) { float t = tanf(x * float(M_PI) - float(M_PI) / 2.f); return (t + 10.f) / 20.f; };
            case Type::Bell:         return [](float x) { return 0.5f + 0.5f * cosf(x * 2.f * float(M_PI) - float(M_PI)); };
            case Type::Sigmoid:      return [](float x) { return 1.f / (1.f + expf(-10.f * (x - 0.5f))); };
            case Type::StepUp:       return [](float x) { return x > 0.5f ? 1.f : 0.f; };
            case Type::StepDown:     return [](float x) { return x < 0.5f ? 1.f : 0.f; };
            case Type::RampUp:       return [](float x) { return x; };
            case Type::RampDown:     return [](float x) { return 1.f - x; };
            case Type::Triangle:     return [](float x) { return x < 0.5f ? 2.f * x : 2.f - 2.f * x; };
            case Type::SmoothStep:   return [](float x) { return x * x * (3.f - 2.f * x); };
            case Type::Parabolic:    return [](float x) { return x < 0.5f ? 2.f * x * x : 1.f - 2.f * (1.f - x) * (1.f - x); };
            case Type::Circular:     return [](float x) { return x < 0.5f ? 0.5f - sqrtf(0.25f - (x - 0.5f) * (x - 0.5f)) : 0.5f + sqrtf(0.25f - (x - 0.5f) * (x - 0.5f)); };
            case Type::Square:       return [](float x) { return x < 0.5f ? 0.f : 1.f; };
            case Type::Saw:          return [](float x) { return x; };
            case Type::InverseSaw:   return [](float x) { return 1.f - x; };
            case Type::Last:         return [](float) { return 0.f; };
        }
        return [](float) { return 0.f; };
    }

    inline const char* name(Type type) {
        switch (type) {
            case Type::Off:          return "Off";
            case Type::Linear:       return "Linear";
            case Type::Exponential:  return "Exponential";
            case Type::Logarithmic:  return "Logarithmic";
            case Type::Sine:         return "Sine";
            case Type::Cosine:       return "Cosine";
            case Type::Tangent:      return "Tangent";
            case Type::Bell:         return "Bell";
            case Type::Sigmoid:      return "Sigmoid";
            case Type::StepUp:       return "StepUp";
            case Type::StepDown:     return "StepDown";
            case Type::RampUp:       return "RampUp";
            case Type::RampDown:     return "RampDown";
            case Type::Triangle:     return "Triangle";
            case Type::SmoothStep:   return "SmoothStep";
            case Type::Parabolic:    return "Parabolic";
            case Type::Circular:     return "Circular";
            case Type::Square:       return "Square";
            case Type::Saw:          return "Saw";
            case Type::InverseSaw:   return "InverseSaw";
            case Type::Last:         return "Last";
        }
        return "Unknown";
    }

} // namespace Curve