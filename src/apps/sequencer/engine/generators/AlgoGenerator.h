#pragma once

#include "Generator.h"
#include "TuesdayAlgoCore.h"

#include "core/utils/Random.h"

class AlgoGenerator : public Generator {
public:
    enum class Param {
        Seed,
        Algorithm,
        Flow,
        Ornament,
        Power,
        Last
    };

    struct Params {
        uint32_t seed = 0;
        uint8_t algorithm = 0;
        uint8_t flow = 8;
        uint8_t ornament = 8;
        uint8_t power = 8;
        uint8_t variation = 100;
    };

    AlgoGenerator(SequenceBuilder &builder, Params &params);

    Mode mode() const override { return Mode::Algo; }

    int paramCount() const override { return int(Param::Last); }
    const char *paramName(int index) const override;
    void editParam(int index, int value, bool shift) override;
    void printParam(int index, StringBuilder &str) const override;

    void init() override;
    void randomizeParams() override;
    void update() override;

    void randomizeSeed();

    int displayValue(int index) const override;

    int algorithm() const { return _params.algorithm; }
    void setAlgorithm(int algorithm) { _params.algorithm = clamp(algorithm, 0, 14); }

    int flow() const { return _params.flow; }
    void setFlow(int flow) { _params.flow = clamp(flow, 0, 16); }

    int ornament() const { return _params.ornament; }
    void setOrnament(int ornament) { _params.ornament = clamp(ornament, 0, 16); }

    int power() const { return _params.power; }
    void setPower(int power) { _params.power = clamp(power, 0, 16); }

    int variation() const { return _params.variation; }
    void setVariation(int variation) { _params.variation = clamp(variation, 0, 100); }

private:
    int ornamentToSubdivisions(int ornament) const;

    Params &_params;
    TuesdayAlgoCore _algoCore;
    GeneratorPattern _pattern;
};
