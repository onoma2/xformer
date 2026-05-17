#include "GeneratorPage.h"

#include "ui/painters/WindowPainter.h"
#include "ui/LedPainter.h"
#include "ui/pages/Pages.h"

#include "engine/generators/Generator.h"
#include "engine/generators/EuclideanGenerator.h"
#include "engine/generators/RandomGenerator.h"
#include "engine/generators/AlgoGenerator.h"

#include "engine/Engine.h"
#include "engine/NoteTrackEngine.h"
#include "engine/CurveTrackEngine.h"
#include "core/math/Math.h"

#include <cstddef>
#include <algorithm>
#include <cmath>

namespace {
namespace PlotArea {
constexpr int Top = 15;
constexpr int Bottom = 35;
constexpr int Height = Bottom - Top + 1;
constexpr int BankTop = 38;
}

int stepX(int step, int steps) {
    return (CONFIG_LCD_WIDTH * step) / steps;
}

int stepWidth(int step, int steps) {
    return std::max(1, stepX(step + 1, steps) - stepX(step, steps));
}

int stepCenterX(int step, int steps) {
    return stepX(step, steps) + stepWidth(step, steps) / 2;
}

int noteY(int note, int minNote, int maxNote, int top, int height) {
    if (maxNote <= minNote) {
        return top + height / 2;
    }
    int span = maxNote - minNote;
    int innerHeight = std::max(1, height - 1);
    int value = top + innerHeight - ((note - minNote) * innerHeight) / span;
    return clamp(value, top, top + innerHeight);
}

void drawStairStep(Canvas &canvas, int x0, int x1, int y, Color color) {
    canvas.setColor(color);
    canvas.hline(x0 + 1, y, std::max(1, x1 - x0 - 1));
}

} // namespace

void GeneratorContextQuickEditModel::configure(Generator *generator, int paramIndex, const char *label, const std::function<void()> &onEdited) {
    _generator = generator;
    _paramIndex = paramIndex;
    _label = label;
    _onEdited = onEdited;
}

int GeneratorContextQuickEditModel::rows() const {
    return 1;
}

int GeneratorContextQuickEditModel::columns() const {
    return 2;
}

void GeneratorContextQuickEditModel::cell(int row, int column, StringBuilder &str) const {
    if (row != 0 || !_generator) {
        return;
    }
    if (column == 0) {
        str("%s", _label);
    } else if (column == 1) {
        _generator->printParam(_paramIndex, str);
    }
}

void GeneratorContextQuickEditModel::edit(int row, int column, int value, bool shift) {
    if (row != 0 || column != 1 || !_generator) {
        return;
    }
    _generator->editParam(_paramIndex, value, shift);
    if (_onEdited) {
        _onEdited();
    }
}

void GeneratorContextQuickEditModel::setSelectedScale(int, bool) {}

static GeneratorContextQuickEditModel gGeneratorContextQuickEditModel;

enum class ContextAction {
    RandomizeSeed,
    Init,
    Revert,
    Commit,
    VariationInfo,
    Last
};

static bool seedDrivenGenerator(Generator::Mode mode) {
    return mode == Generator::Mode::Random || mode == Generator::Mode::Algo;
}

static bool abPreviewGenerator(Generator::Mode mode) {
    return seedDrivenGenerator(mode) || mode == Generator::Mode::Euclidean;
}

static int paramIndexForFunction(Generator::Mode mode, int functionIndex) {
    if (mode == Generator::Mode::Random) {
        switch (functionIndex) {
        case 1: return int(RandomGenerator::Param::Variation);
        case 2: return int(RandomGenerator::Param::Scale);
        case 3: return int(RandomGenerator::Param::Bias);
        default: return -1;
        }
    }

    if (mode == Generator::Mode::Euclidean) {
        switch (functionIndex) {
        case 1: return int(EuclideanGenerator::Param::Offset);
        case 2: return int(EuclideanGenerator::Param::Steps);
        case 3: return int(EuclideanGenerator::Param::Beats);
        default: return -1;
        }
    }

    if (mode == Generator::Mode::Algo) {
        switch (functionIndex) {
        case 1: return int(AlgoGenerator::Param::Algorithm);
        case 2: return int(AlgoGenerator::Param::Flow);
        case 3: return int(AlgoGenerator::Param::Ornament);
        case 4: return int(AlgoGenerator::Param::Power);
        default: return -1;
        }
    }

    return functionIndex;
}

GeneratorPage::GeneratorPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void GeneratorPage::show(Generator *generator, StepSelection<CONFIG_STEP_COUNT> *stepSelection) {
    _generator = generator;
    _stepSelection = stepSelection;
    _previewArmed = false;
    _applied = false;
    _boundTrackIndex = _project.selectedTrackIndex();
    _boundTrackMode = _project.selectedTrack().trackMode();

    BasePage::show();
}

bool GeneratorPage::boundTrackContextValid() const {
    return _project.selectedTrackIndex() == _boundTrackIndex &&
           _project.selectedTrack().trackMode() == _boundTrackMode;
}

bool GeneratorPage::ensureBoundTrackContext() {
    if (boundTrackContextValid()) {
        return true;
    }
    revert();
    showMessage("GEN CANCELED");
    return false;
}

int GeneratorPage::currentStep() const {
    switch (_project.selectedTrack().trackMode()) {
    case Track::TrackMode::Note: {
        const auto &trackEngine = _engine.selectedTrackEngine().as<NoteTrackEngine>();
        const auto &sequence = _project.selectedNoteSequence();
        return trackEngine.isActiveSequence(sequence) ? trackEngine.currentStep() : -1;
    }
    case Track::TrackMode::Curve: {
        const auto &trackEngine = _engine.selectedTrackEngine().as<CurveTrackEngine>();
        const auto &sequence = _project.selectedCurveSequence();
        return trackEngine.isActiveSequence(sequence) ? trackEngine.currentStep() : -1;
    }
    default:
        return -1;
    }
}

bool GeneratorPage::stepInCurrentBank(int step) const {
    return step / StepCount == _section;
}

void GeneratorPage::enter() {
    _valueRange.first = 0;
    _valueRange.second = 7;
    _previewArmed = false;
    _section = 0;

    // Enter in ORIGINAL state — first preview is created only on explicit reroll action
    _generator->revert();
    _generator->showOriginal();
}

void GeneratorPage::exit() {
    if (!_applied) {
        _generator->revert();
    }
}

void GeneratorPage::draw(Canvas &canvas) {
    if (!_generator) {
        return;
    }

    const char *activeFunction = _generator->name();
    const char *functionNames[5];
    for (int i = 0; i < 5; ++i) {
        functionNames[i] = nullptr;
    }

    if (_generator->mode() == Generator::Mode::Random) {
        functionNames[0] = "A/B";
        functionNames[1] = "VAR";
        functionNames[2] = "RANGE";
        functionNames[3] = "BIAS";
        functionNames[4] = "NEW RAND";
    } else if (_generator->mode() == Generator::Mode::Euclidean) {
        functionNames[0] = "A/B";
        functionNames[1] = "OFFSET";
        functionNames[2] = "STEPS";
        functionNames[3] = "BEATS";
        functionNames[4] = "NEW EUCL";
    } else if (_generator->mode() == Generator::Mode::Algo) {
        functionNames[0] = "A/B";
        functionNames[1] = "ALGO";
        functionNames[2] = "FLOW";
        functionNames[3] = "ORNMT";
        functionNames[4] = "POWER";
    } else {
        for (int i = 0; i < 5; ++i) {
            functionNames[i] = i < _generator->paramCount() ? _generator->paramName(i) : nullptr;
        }
    }

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "GENERATOR");
    WindowPainter::drawActiveFunction(canvas, activeFunction);
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState());

    canvas.setFont(Font::Small);
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(Color::Bright);

    auto drawValue = [&] (int index, const char *str, bool tiny = false) {
        Font prevFont = canvas.font();
        Color color = Color::Bright;
        Font font = tiny ? Font::Tiny : Font::Small;

        if (tiny && !_generator->showingPreview()) {
            color = Color::Medium;
        }

        canvas.setFont(font);
        canvas.setColor(color);

        int w = Width / 5;
        int x = (Width * index) / 5;
        int y = Height - 16;
        canvas.drawText(x + (w - canvas.textWidth(str)) / 2, y, str);

        canvas.setColor(Color::Bright);
        canvas.setFont(prevFont);
    };

    auto drawParamValue = [&] (int footerIndex, int paramIndex, bool tiny = false) {
        if (paramIndex < 0 || paramIndex >= _generator->paramCount()) {
            return;
        }

        FixedStringBuilder<16> str;
        if (seedDrivenGenerator(_generator->mode()) && paramIndex == 0 && !_generator->showingPreview()) {
            str("ORIGINAL");
        } else {
            _generator->printParam(paramIndex, str);
        }
        drawValue(footerIndex, str, tiny);
    };

    if (_generator->mode() == Generator::Mode::Random) {
        drawParamValue(0, int(RandomGenerator::Param::Seed), true);
        drawParamValue(1, int(RandomGenerator::Param::Variation));
        drawParamValue(2, int(RandomGenerator::Param::Scale));
        drawParamValue(3, int(RandomGenerator::Param::Bias));
    } else if (_generator->mode() == Generator::Mode::Euclidean) {
        drawValue(0, _generator->showingPreview() ? "CURRENT" : "ORIGINAL", true);
        drawParamValue(1, int(EuclideanGenerator::Param::Offset));
        drawParamValue(2, int(EuclideanGenerator::Param::Steps));
        drawParamValue(3, int(EuclideanGenerator::Param::Beats));
    } else if (_generator->mode() == Generator::Mode::Algo) {
        drawValue(0, _generator->showingPreview() ? "CURRENT" : "ORIGINAL", true);
        drawParamValue(1, int(AlgoGenerator::Param::Algorithm));
        drawParamValue(2, int(AlgoGenerator::Param::Flow));
        drawParamValue(3, int(AlgoGenerator::Param::Ornament));
        drawParamValue(4, int(AlgoGenerator::Param::Power));
    } else {
        for (int i = 0; i < _generator->paramCount(); ++i) {
            drawParamValue(i, i);
        }
    }

    switch (_generator->mode()) {
    case Generator::Mode::InitLayer:
    case Generator::Mode::InitSteps:
        // no page
        break;
    case Generator::Mode::Euclidean:
        drawEuclideanGenerator(canvas, *static_cast<const EuclideanGenerator *>(_generator));
        break;
    case Generator::Mode::Random:
    case Generator::Mode::Algo:
        drawValueGenerator(canvas, *_generator);
        break;
    case Generator::Mode::Last:
        break;
    }
}

void GeneratorPage::updateLeds(Leds &leds) {
    if (!_generator) {
        return;
    }

    int currentStep;
    switch (_project.selectedTrack().trackMode()) {
    case Track::TrackMode::Note: {
            const auto &trackEngine = _engine.selectedTrackEngine().as<NoteTrackEngine>();
            const auto &sequence = _project.selectedNoteSequence();
            currentStep = trackEngine.isActiveSequence(sequence) ? trackEngine.currentStep() : -1;
            for (int i = 0; i < 16; ++i) {
                int stepIndex = stepOffset() + i;
                bool red = (stepIndex == currentStep) || (_stepSelection && _stepSelection->selected()[stepIndex]);
                bool green = (stepIndex != currentStep) && (sequence.step(stepIndex).gate() || (_stepSelection && _stepSelection->selected()[stepIndex]));
                leds.set(MatrixMap::fromStep(i), red, green);
            }
        }
        break;
    case Track::TrackMode::Curve: {
            const auto &trackEngine = _engine.selectedTrackEngine().as<CurveTrackEngine>();
            const auto &sequence = _project.selectedCurveSequence();
            currentStep = trackEngine.isActiveSequence(sequence) ? trackEngine.currentStep() : -1;
            for (int i = 0; i < 16; ++i) {
                int stepIndex = stepOffset() + i;
                bool red = (stepIndex == currentStep) || (_stepSelection && _stepSelection->selected()[stepIndex]);
                bool green = (stepIndex != currentStep) && (sequence.step(stepIndex).gate() || (_stepSelection && _stepSelection->selected()[stepIndex]));
                leds.set(MatrixMap::fromStep(i), red, green);
            }
        }
        break;
    default:
        return;
    }

    LedPainter::drawSelectedSequenceSection(leds, _section);
}

void GeneratorPage::keyDown(KeyEvent &event) {
    const auto &key = event.key();

    if (key.isGlobal()) {
        return;
    }

    if (_stepSelection && key.isStep()) {
        _stepSelection->keyDown(event, stepOffset());
    }

    event.consume();
}

void GeneratorPage::keyUp(KeyEvent &event) {
    const auto &key = event.key();

    if (key.isGlobal()) {
        return;
    }

    if (_stepSelection && key.isStep()) {
        _stepSelection->keyUp(event, stepOffset());
    }

    event.consume();
}

void GeneratorPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (!ensureBoundTrackContext()) {
        event.consume();
        return;
    }

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.pageModifier() && event.count() == 2) {
        contextShow(true);
        event.consume();
        return;
    }

    if (key.isGlobal()) {
        return;
    }

    // Step + Shift triggers re-roll (for seed-driven generators)
    if (key.isStep() && key.shiftModifier()) {
        if (abPreviewGenerator(_generator->mode())) {
            _previewArmed = true;
        }
        if (_generator->mode() == Generator::Mode::Random) {
            static_cast<RandomGenerator *>(_generator)->randomizeContextParams();
            _generator->update();
        } else {
            _generator->randomizeParams();
            _generator->update();
        }
        if (abPreviewGenerator(_generator->mode())) {
            _generator->updatePreview();
            _generator->showPreview();
        }
        event.consume();
        return;
    }

    // F0 toggles A/B preview for seed-driven generators
    if (key.isFunction() && key.function() == 0) {
        if (abPreviewGenerator(_generator->mode()) && !_previewArmed && !_generator->showingPreview()) {
            // Block toggle if preview was never armed (user didn't generate yet)
            event.consume();
            return;
        }
        togglePreview();
        event.consume();
        return;
    }

    // F4 triggers re-roll only when the footer labels it as NEW RAND/NEW EUCL.
    if (key.isFunction() && key.function() == 4 && _generator->mode() != Generator::Mode::Algo) {
        if (abPreviewGenerator(_generator->mode())) {
            _previewArmed = true;
        }
        if (_generator->mode() == Generator::Mode::Random) {
            auto *random = static_cast<RandomGenerator *>(_generator);
            random->randomizeParams();
            random->update();
        } else {
            _generator->randomizeParams();
            _generator->update();
        }
        if (abPreviewGenerator(_generator->mode())) {
            _generator->updatePreview();
            _generator->showPreview();
        }
        event.consume();
        return;
    }

    // Step buttons still work for value range selection
    if (key.isStep()) {
        int secondStep = key.step();
        if (secondStep < 8) {
            int count = 0;
            int firstStep = -1;
            for (int step = 0; step < 8; ++step) {
                if (key.state()[MatrixMap::fromStep(step)]) {
                    ++count;
                    if (step != secondStep) {
                        firstStep = step;
                    }
                }
            }
            if (count == 2) {
                _valueRange.first = firstStep;
                _valueRange.second = secondStep;
            }
        }
        event.consume();
        return;
    }

    // Section navigation with Left/Right buttons
    if (key.isLeft()) {
        _section = (_section + 3) % 4;
        event.consume();
        return;
    }
    if (key.isRight()) {
        _section = (_section + 1) % 4;
        event.consume();
        return;
    }

    event.consume();
}

void GeneratorPage::togglePreview() {
    if (!ensureBoundTrackContext()) {
        return;
    }

    if (_generator->showingPreview()) {
        _generator->showOriginal();
        showMessage("ORIGINAL");
    } else {
        _generator->showPreview();
        if (_generator->showingPreview()) {
            showMessage("PREVIEW");
        }
    }
}

void GeneratorPage::encoder(EncoderEvent &event) {
    if (!ensureBoundTrackContext()) {
        return;
    }

    bool changed = false;
    bool functionKeyHeld = false;

    for (int functionIndex = 0; functionIndex < 5; ++functionIndex) {
        int paramIndex = paramIndexForFunction(_generator->mode(), functionIndex);
        if (paramIndex >= 0 && pageKeyState()[Key::F0 + functionIndex]) {
            functionKeyHeld = true;
            break;
        }
    }

    for (int functionIndex = 0; functionIndex < 5; ++functionIndex) {
        int paramIndex = paramIndexForFunction(_generator->mode(), functionIndex);
        if (paramIndex >= 0 && pageKeyState()[Key::F0 + functionIndex]) {
            _generator->editParam(paramIndex, event.value(), event.pressed());
            changed = true;
        }
    }

    if (_generator->mode() == Generator::Mode::Random && !functionKeyHeld && event.value() != 0) {
        static_cast<RandomGenerator *>(_generator)->randomizeSeed();
        changed = true;
    }

    if (_generator->mode() == Generator::Mode::Euclidean && !functionKeyHeld && event.value() != 0) {
        _generator->randomizeParams();
        changed = true;
    }

    if (_generator->mode() == Generator::Mode::Algo && !functionKeyHeld && event.value() != 0) {
        static_cast<AlgoGenerator *>(_generator)->randomizeSeed();
        changed = true;
    }

    if (changed) {
        if (abPreviewGenerator(_generator->mode())) {
            _previewArmed = true;
        }
        _generator->update();
        if (abPreviewGenerator(_generator->mode())) {
            _generator->updatePreview();
            _generator->showPreview();
        }
    }
}

void GeneratorPage::drawEuclideanGenerator(Canvas &canvas, const EuclideanGenerator &generator) const {
    auto steps = generator.steps();
    const auto &pattern = generator.pattern();

    int stepWidth = Width / steps;
    int stepHeight = 8;
    int x = (Width - steps * stepWidth) / 2;
    int y = Height / 2 - stepHeight / 2;

    for (int i = 0; i < steps; ++i) {
        canvas.setColor(Color::Medium);
        canvas.drawRect(x + 1, y + 1, stepWidth - 2, stepHeight - 2);
        if (pattern[i]) {
            canvas.setColor(Color::Bright);
            canvas.fillRect(x + 1, y + 1, stepWidth - 2, stepHeight - 2);
        }
        x += stepWidth;
    }
}

void GeneratorPage::drawValueGenerator(Canvas &canvas, const Generator &generator) const {
    const int steps = CONFIG_STEP_COUNT;
    const int baselineY = PlotArea::Top + PlotArea::Height / 2;
    const int amplitude = PlotArea::Height / 2;

    auto drawBankSeparators = [&] () {
        for (int bank = 0; bank <= steps / StepCount; ++bank) {
            const int separatorX = stepX(bank * StepCount, steps);
            canvas.setColor(bank == _section || bank == _section + 1 ? Color::Medium : Color::MediumLow);
            canvas.vline(separatorX, PlotArea::Top, PlotArea::BankTop - PlotArea::Top + 1);
        }
    };

    auto drawBankFrame = [&] () {
        if (steps <= StepCount) {
            return;
        }
        const int x0 = stepX(_section * StepCount, steps);
        const int x1 = stepX((_section + 1) * StepCount, steps);
        canvas.setColor(Color::Medium);
        canvas.hline(x0 + 1, PlotArea::Top, std::max(1, x1 - x0 - 1));
        canvas.hline(x0 + 1, PlotArea::BankTop, std::max(1, x1 - x0 - 1));
    };

    auto drawPlayhead = [&] () {
        const int playheadStep = currentStep();
        if (playheadStep >= 0 && playheadStep < steps) {
            const int playX = stepCenterX(playheadStep, steps);
            canvas.setColor(Color::MediumBright);
            canvas.vline(playX, PlotArea::Top, PlotArea::BankTop - PlotArea::Top + 1);
        }
    };

    auto drawProfile = [&] (bool centered, bool highlightMarkers) {
        if (centered) {
            canvas.setColor(Color::MediumLow);
            canvas.hline(0, baselineY, Width);
        }

        drawBankSeparators();

        int previousX = -1;
        int previousY = baselineY;
        for (int i = 0; i < steps; ++i) {
            const int centerX = stepCenterX(i, steps);
            const int rawValue = generator.displayValue(i);
            const int y = centered ?
                baselineY - ((rawValue - 127) * amplitude) / 127 :
                PlotArea::Bottom - (rawValue * (PlotArea::Height - 2)) / 255;
            const bool activeBank = steps <= StepCount || stepInCurrentBank(i);

            if (previousX >= 0) {
                canvas.setColor((activeBank || (steps <= StepCount || stepInCurrentBank(i - 1))) ? Color::Medium : Color::MediumLow);
                canvas.line(previousX, previousY, centerX, y);
            } else {
                canvas.setColor(activeBank ? Color::Bright : Color::Medium);
                canvas.fillRect(centerX, y, 1, 1);
            }

            if (highlightMarkers && activeBank) {
                canvas.setColor(Color::Bright);
                canvas.fillRect(std::max(0, centerX - 1), std::max(PlotArea::Top, y - 1), 3, 3);
            } else if (highlightMarkers) {
                canvas.setColor(Color::Medium);
                canvas.fillRect(centerX, y, 1, 1);
            }

            previousX = centerX;
            previousY = y;
        }
    };

    auto drawGateBlocks = [&] () {
        constexpr int gateTop = PlotArea::Top + 3;
        constexpr int gateHeight = 12;

        drawBankSeparators();
        for (int i = 0; i < steps; ++i) {
            const bool activeBank = steps <= StepCount || stepInCurrentBank(i);
            const int x0 = stepX(i, steps);
            const int width = std::max(1, stepWidth(i, steps) - 1);

            canvas.setColor(activeBank ? Color::Medium : Color::MediumLow);
            canvas.drawRect(x0 + 1, gateTop, width, gateHeight);
            if (generator.displayValue(i) >= 128) {
                canvas.setColor(activeBank ? Color::Bright : Color::Medium);
                canvas.fillRect(x0 + 1, gateTop + 1, width, gateHeight - 1);
            }
        }
    };

    auto drawNoteContour = [&] () {
        int minValue = 255;
        int maxValue = 0;
        for (int i = 0; i < steps; ++i) {
            const int value = generator.displayValue(i);
            minValue = std::min(minValue, value);
            maxValue = std::max(maxValue, value);
        }
        if (minValue == maxValue) {
            minValue = std::max(0, minValue - 1);
            maxValue = std::min(255, maxValue + 1);
        }

        int previousStep = -1;
        int previousY = 0;
        drawBankSeparators();

        for (int i = 0; i < steps; ++i) {
            const bool activeBank = steps <= StepCount || stepInCurrentBank(i);
            const int x0 = stepX(i, steps);
            const int x1 = stepX(i + 1, steps);
            const int y = noteY(generator.displayValue(i), minValue, maxValue, PlotArea::Top + 2, PlotArea::Height - 5);
            drawStairStep(canvas, x0, x1, y, activeBank ? Color::Bright : Color::Medium);

            if (previousStep >= 0) {
                canvas.setColor((activeBank || stepInCurrentBank(previousStep)) ? Color::Medium : Color::MediumLow);
                canvas.vline(x0, std::min(previousY, y), std::abs(y - previousY) + 1);
            }

            previousStep = i;
            previousY = y;
        }
    };

    auto drawSlideMarkers = [&] () {
        drawBankSeparators();
        for (int i = 0; i < steps; ++i) {
            if (generator.displayValue(i) < 128) {
                continue;
            }
            const bool activeBank = steps <= StepCount || stepInCurrentBank(i);
            const int x0 = stepX(i, steps);
            const int x1 = stepX(i + 1, steps);
            canvas.setColor(activeBank ? Color::Bright : Color::Medium);
            canvas.fillRect(x0 + 1, PlotArea::Bottom - 4, std::max(1, x1 - x0 - 2), 3);
        }
    };

    auto drawBooleanMarkers = [&] () {
        constexpr int markerY = PlotArea::Top + PlotArea::Height / 2 - 1;

        drawBankSeparators();
        for (int i = 0; i < steps; ++i) {
            if (generator.displayValue(i) < 128) {
                continue;
            }
            const bool activeBank = steps <= StepCount || stepInCurrentBank(i);
            const int centerX = stepCenterX(i, steps);
            canvas.setColor(activeBank ? Color::Bright : Color::Medium);
            canvas.fillRect(std::max(0, centerX - 1), markerY - 1, 3, 3);
        }
    };

    auto drawLengthBars = [&] () {
        constexpr int barTop = PlotArea::Top + 6;
        constexpr int barHeight = 8;

        drawBankSeparators();
        for (int i = 0; i < steps; ++i) {
            const bool activeBank = steps <= StepCount || stepInCurrentBank(i);
            const int x0 = stepX(i, steps);
            const int cellWidth = std::max(2, stepWidth(i, steps) - 1);
            const int barWidth = std::max(1, (cellWidth * generator.displayValue(i)) / 255);
            canvas.setColor(activeBank ? Color::Medium : Color::MediumLow);
            canvas.drawRect(x0 + 1, barTop, cellWidth, barHeight);
            canvas.setColor(activeBank ? Color::Bright : Color::Medium);
            canvas.fillRect(x0 + 1, barTop + 1, barWidth, barHeight - 1);
        }
    };

    auto drawRepeatStripes = [&] () {
        constexpr int stripeTop = PlotArea::Top + 4;
        constexpr int stripeHeight = 12;

        drawBankSeparators();
        for (int i = 0; i < steps; ++i) {
            const bool activeBank = steps <= StepCount || stepInCurrentBank(i);
            const int x0 = stepX(i, steps);
            const int width = std::max(2, stepWidth(i, steps) - 2);
            const int stripes = std::max(0, (generator.displayValue(i) * 4 + 127) / 255);

            canvas.setColor(activeBank ? Color::Medium : Color::MediumLow);
            canvas.drawRect(x0 + 1, stripeTop, width, stripeHeight);
            canvas.setColor(activeBank ? Color::Bright : Color::Medium);
            for (int stripe = 0; stripe < stripes; ++stripe) {
                const int stripeX = x0 + 2 + (stripe * width) / std::max(1, stripes);
                canvas.vline(stripeX, stripeTop + 1, stripeHeight - 1);
            }
        }
    };

    bool drewSpecialized = false;
    if (_project.selectedTrack().trackMode() == Track::TrackMode::Note) {
        switch (_project.selectedNoteSequenceLayer()) {
        case NoteSequence::Layer::Gate:
            drawGateBlocks();
            drewSpecialized = true;
            break;
        case NoteSequence::Layer::Note:
            drawNoteContour();
            drewSpecialized = true;
            break;
        case NoteSequence::Layer::Slide:
            drawSlideMarkers();
            drewSpecialized = true;
            break;
        case NoteSequence::Layer::Length:
            drawLengthBars();
            drewSpecialized = true;
            break;
        case NoteSequence::Layer::Retrigger:
        case NoteSequence::Layer::PulseCount:
            drawRepeatStripes();
            drewSpecialized = true;
            break;
        case NoteSequence::Layer::GateOffset:
            drawProfile(true, true);
            drewSpecialized = true;
            break;
        case NoteSequence::Layer::AccumulatorTrigger:
            drawBooleanMarkers();
            drewSpecialized = true;
            break;
        case NoteSequence::Layer::Condition:
        case NoteSequence::Layer::GateProbability:
        case NoteSequence::Layer::RetriggerProbability:
        case NoteSequence::Layer::LengthVariationRange:
        case NoteSequence::Layer::LengthVariationProbability:
        case NoteSequence::Layer::NoteVariationRange:
        case NoteSequence::Layer::NoteVariationProbability:
        case NoteSequence::Layer::GateMode:
        case NoteSequence::Layer::HarmonyRoleOverride:
        case NoteSequence::Layer::InversionOverride:
        case NoteSequence::Layer::VoicingOverride:
            drawProfile(false, true);
            drewSpecialized = true;
            break;
        default:
            break;
        }
    } else if (_project.selectedTrack().trackMode() == Track::TrackMode::Curve) {
        switch (_project.selectedCurveSequenceLayer()) {
        case CurveSequence::Layer::Gate:
            drawGateBlocks();
            drewSpecialized = true;
            break;
        case CurveSequence::Layer::Min:
        case CurveSequence::Layer::Max:
        case CurveSequence::Layer::Shape:
        case CurveSequence::Layer::ShapeVariation:
        case CurveSequence::Layer::ShapeVariationProbability:
        case CurveSequence::Layer::GateProbability:
            drawProfile(false, true);
            drewSpecialized = true;
            break;
        default:
            break;
        }
    }

    if (!drewSpecialized) {
        drawProfile(false, true);
    }

    drawBankFrame();
    drawPlayhead();
}

void GeneratorPage::contextShow(bool doubleClick) {
    if (_generator->mode() == Generator::Mode::Random) {
        const auto *random = static_cast<const RandomGenerator *>(_generator);
        snprintf(_contextMenuAuxLabel, sizeof(_contextMenuAuxLabel), "SMOOTH %d", random->smooth());
        _contextMenuItems[0] = { "" };
        _contextMenuItems[1] = { _contextMenuAuxLabel };
        _contextMenuItems[2] = { "REGEN" };
        _contextMenuItems[3] = { "CANCEL" };
        _contextMenuItems[4] = { "COMMIT" };
    } else if (_generator->mode() == Generator::Mode::Algo) {
        snprintf(_contextMenuAuxLabel, sizeof(_contextMenuAuxLabel), "VAR %d", static_cast<const AlgoGenerator *>(_generator)->variation());
        _contextMenuItems[0] = { "" };
        _contextMenuItems[1] = { _contextMenuAuxLabel };
        _contextMenuItems[2] = { "REGEN" };
        _contextMenuItems[3] = { "CANCEL" };
        _contextMenuItems[4] = { "COMMIT" };
    } else {
        _contextMenuItems[0] = { "NEW RAND" };
        _contextMenuItems[1] = { "RESEED" };
        _contextMenuItems[2] = { "REVERT" };
        _contextMenuItems[3] = { "COMMIT" };
        _contextMenuItems[4] = { "" };
    }

    showContextMenu(ContextMenu(
        _contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); },
        doubleClick
    ));
}

void GeneratorPage::contextAction(int index) {
    if (!ensureBoundTrackContext()) {
        return;
    }

    if (_generator->mode() == Generator::Mode::Random) {
        switch (index) {
        case 1:
            gGeneratorContextQuickEditModel.configure(_generator, int(RandomGenerator::Param::Smooth), "SMOOTH", [&] {
                _generator->update();
                _generator->updatePreview();
            });
            _manager.pages().quickEdit.show(gGeneratorContextQuickEditModel, 0);
            return;
        case 2:
            init();
            break;
        case 3:
            revert();
            break;
        case 4:
            commit();
            break;
        default:
            break;
        }
        return;
    }

    if (_generator->mode() == Generator::Mode::Algo) {
        switch (index) {
        case 1:
            gGeneratorContextQuickEditModel.configure(_generator, int(AlgoGenerator::Param::Variation), "VAR", [&] {
                _generator->update();
                _generator->updatePreview();
            });
            _manager.pages().quickEdit.show(gGeneratorContextQuickEditModel, 0);
            return;
        case 2:
            init();
            break;
        case 3:
            revert();
            break;
        case 4:
            commit();
            break;
        default:
            break;
        }
        return;
    }

    // Non-Random modes: NEW RAND, RESEED, REVERT, COMMIT
    switch (index) {
    case 0: // NEW RAND
        if (_generator->mode() == Generator::Mode::Random) {
            auto *random = static_cast<RandomGenerator *>(_generator);
            random->randomizeParams();
            random->update();
            if (abPreviewGenerator(_generator->mode())) {
                _previewArmed = true;
                _generator->updatePreview();
                _generator->showPreview();
            }
        } else if (_generator->mode() == Generator::Mode::Algo) {
            auto *algo = static_cast<AlgoGenerator *>(_generator);
            algo->randomizeParams();
            algo->update();
            if (abPreviewGenerator(_generator->mode())) {
                _previewArmed = true;
                _generator->updatePreview();
                _generator->showPreview();
            }
        }
        break;
    case 1: // RESEED
        if (_generator->mode() == Generator::Mode::Random) {
            auto *random = static_cast<RandomGenerator *>(_generator);
            random->randomizeSeed();
            random->update();
            if (abPreviewGenerator(_generator->mode())) {
                _previewArmed = true;
                _generator->updatePreview();
                _generator->showPreview();
            }
        } else if (_generator->mode() == Generator::Mode::Algo) {
            auto *algo = static_cast<AlgoGenerator *>(_generator);
            algo->randomizeSeed();
            algo->update();
            if (abPreviewGenerator(_generator->mode())) {
                _previewArmed = true;
                _generator->updatePreview();
                _generator->showPreview();
            }
        }
        break;
    case 2: // REVERT
        revert();
        break;
    case 3: // COMMIT
        commit();
        break;
    default:
        break;
    }
}

bool GeneratorPage::contextActionEnabled(int index) const {
    if (index >= int(ContextAction::Last)) {
        return false;
    }

    if (_generator->mode() == Generator::Mode::Random) {
        return index >= 1 && index <= 4;
    }

    if (_generator->mode() == Generator::Mode::Algo) {
        return index >= 1 && index <= 4;
    }

    return index >= 0 && index <= 3;
}

void GeneratorPage::init() {
    if (!ensureBoundTrackContext()) {
        return;
    }

    if (_stepSelection) {
        _stepSelection->clear();
    }
    _generator->init();
    _previewArmed = false;
    _generator->showOriginal();
}

void GeneratorPage::revert() {
    if (_stepSelection) {
        _stepSelection->clear();
    }
    _generator->revert();
    _applied = false;
    close();
}

void GeneratorPage::commit() {
    if (!ensureBoundTrackContext()) {
        return;
    }

    if (abPreviewGenerator(_generator->mode()) && !_previewArmed && !_generator->showingPreview()) {
        // Allow commit even if preview wasn't armed — user made changes
        _applied = true;
        close();
        return;
    }

    if (_stepSelection) {
        _stepSelection->clear();
    }
    _generator->apply();
    _applied = true;
    close();
}
