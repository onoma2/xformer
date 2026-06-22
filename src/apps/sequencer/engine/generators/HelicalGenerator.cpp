#include "HelicalGenerator.h"

#include "HelicalAr.h"
#include "IndexedSequenceBuilder.h"

#include "model/IndexedSequence.h"
#include "model/Scale.h"

#include "os/os.h"

static constexpr int kMinTicks = 24;
static constexpr int kMaxTicks = 768;

HelicalGenerator::HelicalGenerator(SequenceBuilder &builder, Params &params) :
    Generator(builder),
    _params(params)
{
    _pattern.fill(0);
    update();
}

const char *HelicalGenerator::paramName(int index) const {
    switch (Param(index)) {
    case Param::Seed:        return "Seed";
    case Param::OctaveRange: return "Range";
    case Param::Base:        return "Base";
    case Param::LawDir:      return "Law";
    case Param::Last:        break;
    }
    return nullptr;
}

void HelicalGenerator::editParam(int index, int value, bool shift) {
    switch (Param(index)) {
    case Param::Seed:
        if (value != 0) {
            randomizeSeed();
        }
        break;
    case Param::OctaveRange:
        setOctaveRange(octaveRange() + value);
        break;
    case Param::Base:
        setBase(base() + value * (shift ? 24 : 6));
        break;
    case Param::LawDir:
        setLawDir(value);
        break;
    case Param::Last:
        break;
    }
}

void HelicalGenerator::printParam(int index, StringBuilder &str) const {
    switch (Param(index)) {
    case Param::Seed:        str("%08X", _params.seed); break;
    case Param::OctaveRange: str("%d", octaveRange()); break;
    case Param::Base:        str("%d", base()); break;
    case Param::LawDir:      str(_params.lawDir >= 0 ? "LONG" : "SHORT"); break;
    case Param::Last:        break;
    }
}

void HelicalGenerator::init() {
    _params = Params();
    update();
}

void HelicalGenerator::randomizeParams() {
    randomizeSeed();
}

void HelicalGenerator::randomizeSeed() {
    _params.seed = uint32_t(os::ticks());
}

void HelicalGenerator::update() {
    auto *indexedBuilder = dynamic_cast<IndexedSequenceBuilder *>(&_builder);
    if (!indexedBuilder) {
        return;
    }
    auto &seq = indexedBuilder->editSequence();

    int scaleSize = indexedBuilder->scale().notesPerOctave();
    if (scaleSize < 1) {
        scaleSize = 12;
    }
    int span = _params.octaveRange * scaleSize;

    int stepCount = _builder.length();

    HelicalAr ar;
    ar.seed(_params.seed);

    for (int i = 0; i < stepCount; ++i) {
        auto r = ar.step(span, scaleSize, _params.base, _params.lawDir, kMinTicks, kMaxTicks);
        auto &step = seq.step(i);
        step.setNoteIndex(int8_t(r.noteIndex));
        step.setDuration(uint16_t(r.durationTicks));
        step.setGateLength(uint16_t(r.gateLength));
        _pattern[i] = clamp(r.noteIndex * 255 / (span > 1 ? span - 1 : 1), 0, 255);
    }
}

int HelicalGenerator::displayValue(int index) const {
    if (!showingPreview()) {
        return clamp(int(_builder.originalValue(index) * 255.f + 0.5f), 0, 255);
    }
    return _pattern[index];
}
