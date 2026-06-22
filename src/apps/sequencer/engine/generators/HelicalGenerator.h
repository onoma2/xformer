#pragma once

#include "Generator.h"

#include "core/math/Math.h"

#include <cstdint>

// Helical autoregressive generator (Indexed-only). Wraps the pure HelicalAr map:
// fills each step's noteIndex / duration / gateLength from the deterministic
// coupled fold-feedback, reading the resolved scale/root off the
// IndexedSequenceBuilder. Seed-driven (reroll = new initial conditions).
class HelicalGenerator : public Generator {
public:
    enum class Param {
        Seed,
        OctaveRange,
        Base,
        LawDir,
        Helicity,
        Last
    };

    struct Params {
        uint32_t seed = 1;
        uint8_t octaveRange = 2;
        uint16_t base = 192;
        int8_t lawDir = 1;
        uint8_t helicity = 8;   // law depth x10: 8 => 0.8 (sqrt is 5)
    };

    HelicalGenerator(SequenceBuilder &builder, Params &params);

    Mode mode() const override { return Mode::Helical; }

    int paramCount() const override { return int(Param::Last); }
    const char *paramName(int index) const override;
    void editParam(int index, int value, bool shift) override;
    void printParam(int index, StringBuilder &str) const override;

    void init() override;
    void randomizeParams() override;
    void update() override;

    int displayValue(int index) const override;

    void randomizeSeed();

    int octaveRange() const { return _params.octaveRange; }
    void setOctaveRange(int v) { _params.octaveRange = clamp(v, 1, 4); }

    int base() const { return _params.base; }
    void setBase(int v) { _params.base = clamp(v, 24, 768); }

    int lawDir() const { return _params.lawDir; }
    void setLawDir(int v) { _params.lawDir = v >= 0 ? 1 : -1; }

    int helicity() const { return _params.helicity; }
    void setHelicity(int v) { _params.helicity = clamp(v, 1, 20); }

private:
    Params &_params;
    GeneratorPattern _pattern;
};
