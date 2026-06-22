#include "Generator.h"

#include "EuclideanGenerator.h"
#include "RandomGenerator.h"
#include "AlgoGenerator.h"
#include "HelicalGenerator.h"
#include "IndexedSequenceBuilder.h"

#include "core/utils/Container.h"

static Container<EuclideanGenerator, RandomGenerator, AlgoGenerator, HelicalGenerator> generatorContainer;
static EuclideanGenerator::Params euclideanParams;
static RandomGenerator::Params randomParams;
static AlgoGenerator::Params algoParams;
static HelicalGenerator::Params helicalParams;

static void initLayer(SequenceBuilder &builder) {
    builder.clearLayer();
}

static void initSteps(SequenceBuilder &builder) {
    builder.clearSteps();
}

Generator *Generator::execute(Generator::Mode mode, SequenceBuilder &builder) {
    switch (mode) {
    case Mode::InitLayer:
        initLayer(builder);
        return nullptr;
    case Mode::InitSteps:
        initSteps(builder);
        return nullptr;
    case Mode::Euclidean:
        return generatorContainer.create<EuclideanGenerator>(builder, euclideanParams);
    case Mode::Random:
        return generatorContainer.create<RandomGenerator>(builder, randomParams);
    case Mode::Algo: {
        // Guard: Algo mode only works with NoteSequenceBuilder
        auto *noteBuilder = dynamic_cast<SequenceBuilderImpl<NoteSequence> *>(&builder);
        if (!noteBuilder) {
            return nullptr;
        }
        return generatorContainer.create<AlgoGenerator>(builder, algoParams);
    }
    case Mode::Helical: {
        // Guard: Helical mode only works with IndexedSequenceBuilder
        auto *indexedBuilder = dynamic_cast<IndexedSequenceBuilder *>(&builder);
        if (!indexedBuilder) {
            return nullptr;
        }
        return generatorContainer.create<HelicalGenerator>(builder, helicalParams);
    }
    case Mode::Last:
        break;
    }

    return nullptr;
}
