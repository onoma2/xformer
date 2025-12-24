#include "CurveSequenceEditPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/SequencePainter.h"
#include "ui/painters/WindowPainter.h"

#include "model/Curve.h"

#include "core/utils/Random.h"
#include "core/utils/StringBuilder.h"

static Random rng;

enum class ContextAction {
    Init,
    Copy,
    Paste,
    Duplicate,
    Generate,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
    { "COPY" },
    { "PASTE" },
    { "DUPL" },
    { "GEN" },
};

enum class LfoContextAction {
    Triangle,
    Sine,
    Sawtooth,
    Square,
    RandomMinMax,
    Last
};

static const ContextMenuModel::Item lfoContextMenuItems[] = {
    { "TRI" },
    { "SINE" },
    { "SAW" },
    { "SQUA" },
    { "MM-RND" },
};

enum class SettingsContextAction {
    Init,
    Randomize,
    Copy,
    Paste,
    Last
};

static const ContextMenuModel::Item settingsContextMenuItems[] = {
    { "INIT" },
    { "RAND" },
    { "COPY" },
    { "PASTE" },
};

enum class Function {
    Shape   = 0,
    Min     = 1,
    Max     = 2,
    Gate    = 3,
    Phase   = 4,
};

static const char *functionNames[] = { "SHAPE", "MIN", "MAX", "GATE", "PHASE", nullptr };

static const CurveSequenceListModel::Item quickEditItems[8] = {
    CurveSequenceListModel::Item::FirstStep,
    CurveSequenceListModel::Item::LastStep,
    CurveSequenceListModel::Item::RunMode,
    CurveSequenceListModel::Item::Divisor,
    CurveSequenceListModel::Item::ResetMeasure,
    CurveSequenceListModel::Item::Range,
    CurveSequenceListModel::Item::Last,
    CurveSequenceListModel::Item::Last
};

static void drawCurve(Canvas &canvas, int x, int y, int w, int h, float &lastY, const Curve::Function function, float min, float max) {
    const int Step = 1;

    auto eval = [=] (float x) {
        return (1.f - (function(x) * (max - min) + min)) * h;
    };

    float fy0 = y + eval(0.f);

    if (lastY >= 0.f && lastY != fy0) {
        canvas.line(x, lastY, x, fy0);
    }

    for (int i = 0; i < w; i += Step) {
        float fy1 = y + eval((float(i) + Step) / w);
        canvas.line(x + i, fy0, x + i + Step, fy1);
        fy0 = fy1;
    }

    lastY = fy0;
}

static void drawMinMax(Canvas &canvas, int x, int y, int w, int h, float minMax) {
    y += std::round((1.f - minMax) * h);
    canvas.hline(x, y, w);
}

static void drawGatePattern(Canvas &canvas, int x, int y, int w, int h, int gate) {
    int gs = w / 4;
    int gw = w / 8;
    for (int i = 0; i < 4; ++i) {
        canvas.setColor((gate & (1 << i)) ? Color::Bright : Color::Medium);
        canvas.fillRect(x + i * gs, y, gw, h);
    }
}

static std::pair<int, int> calculateMultiStepShapeMinMax(size_t stepsSelected,
                                                         size_t multiStepsProcessed,
                                                         int shape,
                                                         bool reverse) {
    // If shift is pressed, reverse ascension
    int m = !reverse ? multiStepsProcessed : stepsSelected - multiStepsProcessed - 1;

    int min, max;
    if (shape == 0) {
        min = CurveSequence::Min::Min;
        max = CurveSequence::Max::Max;
    } else {
        min = std::ceil(float(m) * CurveSequence::Min::Max / stepsSelected);
        max = std::ceil(float(m + 1) * CurveSequence::Max::Max / stepsSelected);
    }

    return std::make_pair(min, max);
}

CurveSequenceEditPage::SettingsClipboard CurveSequenceEditPage::_settingsClipboard;

CurveSequenceEditPage::CurveSequenceEditPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{
    _stepSelection.setStepCompare([this] (int a, int b) {
        auto layer = _project.selectedCurveSequenceLayer();
        const auto &sequence = _project.selectedCurveSequence();
        return sequence.step(a).layerValue(layer) == sequence.step(b).layerValue(layer);
    });
}

void CurveSequenceEditPage::enter() {
    updateMonitorStep();

    _showDetail = false;
}

void CurveSequenceEditPage::exit() {
}

void CurveSequenceEditPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "STEPS");

    const auto &track = _project.selectedTrack().curveTrack();
    const auto &trackEngine = _engine.selectedTrackEngine().as<CurveTrackEngine>();
    const auto &sequence = _project.selectedCurveSequence();
    bool isActiveSequence = trackEngine.isActiveSequence(sequence);

    if (_editMode == EditMode::Wavefolder1) {
        // Draw Wavefolder UI
        WindowPainter::drawActiveFunction(canvas, "WAVEFOLDER");
        const char *wavefolderFunctionNames[5] = { "FOLD", "GAIN", "FILTER", "XFADE", "NEXT" };
        WindowPainter::drawFooter(canvas, wavefolderFunctionNames, pageKeyState(), _wavefolderRow);

        const int colWidth = 51;
        const int valueY = 26;
        const int barY = 32;
        const int barHeight = 4;
        const int barWidth = 40;

        for (int i = 0; i < 5; ++i) {
            int x = i * colWidth;
            int barX = x + (colWidth - barWidth) / 2;

            float value = 0.f;
            float max = 1.f;
            bool bipolar = false;
            FixedStringBuilder<16> valueStr;

            // Get data for the current parameter
            switch (i) {
            case 0: // FOLD
                value = sequence.wavefolderFold();
                sequence.printWavefolderFold(valueStr);
                break;
            case 1: // GAIN
                value = sequence.wavefolderGain();
                max = 2.f;  // New range is 0.0 to 2.0
                sequence.printWavefolderGain(valueStr);
                break;
            case 2: // FILTER
                value = sequence.djFilter();
                max = 1.f;
                bipolar = true;
                sequence.printDjFilter(valueStr);
                break;
            case 3: // XFADE
                value = sequence.xFade();
                max = 1.f;
                sequence.printXFade(valueStr);
                break;
            }

            // Draw numeric value
            canvas.setFont(Font::Tiny);
            canvas.setColor((i == _wavefolderRow) ? Color::Bright : Color::Medium);
            int textWidth = canvas.textWidth(valueStr);
            int textX = x + (colWidth - textWidth) / 2;
            canvas.drawText(textX, valueY, valueStr);

            // Draw bar
            canvas.setColor(Color::Bright);
            if (bipolar) {
                int center = barX + barWidth / 2;
                if (value > 0) {
                    int fillWidth = (value * barWidth / 2) / max;
                    canvas.fillRect(center, barY, fillWidth, barHeight);
                } else if (value < 0) {
                    int fillWidth = (-value * barWidth / 2) / max;
                    canvas.fillRect(center - fillWidth, barY, fillWidth, barHeight);
                }
                canvas.setColor(Color::Medium);
                canvas.vline(center, barY, barHeight);
            } else {
                int fillWidth = (value * barWidth) / max;
                if (fillWidth > 0) {
                    canvas.fillRect(barX, barY, fillWidth, barHeight);
                }
            }
        }

    } else if (_editMode == EditMode::Chaos) {
        WindowPainter::drawActiveFunction(canvas, "CHAOS");
        const char *chaosFunctionNames[5] = { "AMT", "HZ", "P1", "P2", "NEXT" };
        WindowPainter::drawFooter(canvas, chaosFunctionNames, pageKeyState(), _chaosRow);

        const int colWidth = 51;
        const int valueY = 26;
        const int barY = 32;
        const int barHeight = 4;
        const int barWidth = 40;

        // Draw Algo Name below "AMT"
        canvas.setFont(Font::Tiny);
        canvas.setColor(Color::Medium);
        // Assuming "AMT" is at index 0 (x=0)
        // Center text under the bar
        if (sequence.chaosAlgo() == CurveSequence::ChaosAlgorithm::Latoocarfian) {
             const char* line1 = "Latoo-";
             const char* line2 = "carfian";
             int x1 = 0 + (colWidth - canvas.textWidth(line1)) / 2;
             int x2 = 0 + (colWidth - canvas.textWidth(line2)) / 2;
             canvas.drawText(x1, 44, line1);
             canvas.drawText(x2, 50, line2);
        } else {
            FixedStringBuilder<16> algoName;
            sequence.printChaosAlgo(algoName);
            int algoX = 0 + (colWidth - canvas.textWidth(algoName)) / 2;
            canvas.drawText(algoX, 44, algoName);
        }


        for (int i = 0; i < 4; ++i) {
            int x = i * colWidth;
            int barX = x + (colWidth - barWidth) / 2;

            float value = 0.f;
            float max = 1.f;
            FixedStringBuilder<16> valueStr;

            switch (i) {
            case 0: // AMT
                value = sequence.chaosAmount();
                max = 100.f;
                sequence.printChaosAmount(valueStr);
                break;
            case 1: // HZ
                value = sequence.chaosRate();
                max = 127.f;
                sequence.printChaosRate(valueStr);
                break;
            case 2: // P1
                value = sequence.chaosParam1();
                max = 100.f;
                sequence.printChaosParam1(valueStr);
                break;
            case 3: // P2
                value = sequence.chaosParam2();
                max = 100.f;
                sequence.printChaosParam2(valueStr);
                break;
            }

            canvas.setFont(Font::Tiny);
            canvas.setColor((i == _chaosRow) ? Color::Bright : Color::Medium);
            int textWidth = canvas.textWidth(valueStr);
            int textX = x + (colWidth - textWidth) / 2;
            canvas.drawText(textX, valueY, valueStr);

            canvas.setColor(Color::Bright);
            int fillWidth = (value * barWidth) / max;
            if (fillWidth > 0) {
                canvas.fillRect(barX, barY, fillWidth, barHeight);
            }
        }

    } else {
        // Draw Step/Phase UI
        if (_editMode == EditMode::GlobalPhase) {
            FixedStringBuilder<16> str("PHASE: ");
            track.printGlobalPhase(str);
            WindowPainter::drawActiveFunction(canvas, str);
        } else {
            WindowPainter::drawActiveFunction(canvas, CurveSequence::layerName(layer()));
        }

        WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), activeFunctionKey());

        canvas.setBlendMode(BlendMode::Add);
        // Draw Step/Phase UI
        if (_editMode == EditMode::GlobalPhase) {
            FixedStringBuilder<16> str("PHASE: ");
            track.printGlobalPhase(str);
            WindowPainter::drawActiveFunction(canvas, str);
        } else {
            WindowPainter::drawActiveFunction(canvas, CurveSequence::layerName(layer()));
        }

        WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), activeFunctionKey());

        canvas.setBlendMode(BlendMode::Add);

        const int stepWidth = Width / StepCount;
        const int stepOffset = this->stepOffset();
        const int loopY = 16;
        const int curveY = 24;
        const int curveHeight = 20;
        const int bottomY = 48;
        bool drawShapeVariation = layer() == Layer::ShapeVariation || layer() == Layer::ShapeVariationProbability;

        // draw loop points
        canvas.setBlendMode(BlendMode::Set);
        canvas.setColor(Color::Bright);
        SequencePainter::drawLoopStart(canvas, (sequence.firstStep() - stepOffset) * stepWidth + 1, loopY, stepWidth - 2);
        SequencePainter::drawLoopEnd(canvas, (sequence.lastStep() - stepOffset) * stepWidth + 1, loopY, stepWidth - 2);

        // draw grid
        if (!drawShapeVariation) {
            canvas.setColor(Color::Low);
            for (int stepIndex = 1; stepIndex < StepCount; ++stepIndex) {
                int x = stepIndex * stepWidth;
                for (int y = 0; y <= curveHeight; y += 2) {
                    canvas.point(x, curveY + y);
                }
            }
        }

        // draw curve
        canvas.setColor(Color::Bright);
        float lastY = -1.f;
        float lastYVariation = -1.f;
        for (int i = 0; i < StepCount; ++i) {
            int stepIndex = stepOffset + i;
            const auto &step = sequence.step(stepIndex);
            float min = step.minNormalized();
            float max = step.maxNormalized();
            int x = i * stepWidth;
            int y = 20;
            canvas.setBlendMode(BlendMode::Set);

            if (stepIndex > sequence.firstStep() && stepIndex <= sequence.lastStep()) {
                canvas.setColor(Color::Bright);
                canvas.point(x, loopY);
            }

            {
                canvas.setColor(_stepSelection[stepIndex] ? Color::Bright : Color::Medium);
                FixedStringBuilder<8> str("%d", stepIndex + 1);
                canvas.drawText(x + (stepWidth - canvas.textWidth(str) + 1) / 2, y - 2, str);
            }

            {
                const auto function = Curve::function(Curve::Type(std::min(Curve::Last - 1, step.shape())));
                canvas.setColor(drawShapeVariation ? Color::MediumLow : Color::Bright);
                canvas.setBlendMode(BlendMode::Add);
                drawCurve(canvas, x, curveY, stepWidth, curveHeight, lastY, function, min, max);
            }

            if (drawShapeVariation) {
                const auto function = Curve::function(Curve::Type(std::min(Curve::Last - 1, step.shapeVariation())));
                canvas.setColor(Color::Bright);
                canvas.setBlendMode(BlendMode::Add);
                drawCurve(canvas, x, curveY, stepWidth, curveHeight, lastYVariation, function, min, max);
            }

            switch (layer()) {
            case Layer::Shape: break;
            case Layer::ShapeVariation: break;
            case Layer::ShapeVariationProbability:
                SequencePainter::drawProbability(canvas, x + 2, bottomY, stepWidth - 4, 2, step.shapeVariationProbability(), 8);
                break;
            case Layer::Min:
            case Layer::Max: {
                bool functionPressed = globalKeyState()[MatrixMap::fromFunction(activeFunctionKey())];
                canvas.setColor(Color::MediumLow);
                canvas.setBlendMode(BlendMode::Add);
                if (layer() == Layer::Min || functionPressed) {
                    drawMinMax(canvas, x, curveY, stepWidth, curveHeight, min);
                }
                if (layer() == Layer::Max || functionPressed) {
                    drawMinMax(canvas, x, curveY, stepWidth, curveHeight, max);
                }
                break;
            }
            case Layer::Gate:
                canvas.setColor(Color::Bright);
                canvas.setBlendMode(BlendMode::Set);
                drawGatePattern(canvas, x, bottomY, stepWidth, 2, step.gate());
                break;
            case Layer::GateProbability:
                SequencePainter::drawProbability(canvas, x + 2, bottomY, stepWidth - 4, 2, step.gateProbability() + 1, CurveSequence::GateProbability::Range);
                break;
            case Layer::Last: break;
            }
        }

        if (isActiveSequence) {
            canvas.setColor(Color::Bright);
            int x = ((trackEngine.currentStep() - stepOffset) + trackEngine.currentStepFraction()) * stepWidth;
            canvas.vline(x, curveY, curveHeight);
        }

        if (isActiveSequence && track.globalPhase() > 0.f) {
            canvas.setColor(Color::MediumLow);
            int x = ((trackEngine.phasedStep() - stepOffset) + trackEngine.phasedStepFraction()) * stepWidth;
            canvas.vline(x, curveY, curveHeight);
        }

        if (_showDetail) {
            if (!(layer() == Layer::ShapeVariationProbability || layer() == Layer::GateProbability) || _stepSelection.none()) {
                _showDetail = false;
            }
            if (_stepSelection.isPersisted() && os::ticks() > _showDetailTicks + os::time::ms(500)) {
                _showDetail = false;
            }
        }

        if (_showDetail) {
            drawDetail(canvas, sequence.step(_stepSelection.first()));
        }
    }
}

void CurveSequenceEditPage::updateLeds(Leds &leds) {
    const auto &trackEngine = _engine.selectedTrackEngine().as<CurveTrackEngine>();
    const auto &sequence = _project.selectedCurveSequence();
    int currentStep = trackEngine.isActiveSequence(sequence) ? trackEngine.currentStep() : -1;

    for (int i = 0; i < 16; ++i) {
        int stepIndex = stepOffset() + i;
        bool red = (stepIndex == currentStep) || _stepSelection[stepIndex];
        bool green = (stepIndex != currentStep) && (sequence.step(stepIndex).gate() > 0 || _stepSelection[stepIndex]);
        leds.set(MatrixMap::fromStep(i), red, green);
    }

    LedPainter::drawSelectedSequenceSection(leds, _section);

    LedPainter::drawSelectedSequenceSection(leds, _section);

    // show quick edit keys
    if (globalKeyState()[Key::Page] && !globalKeyState()[Key::Shift]) {
        for (int i = 0; i < 8; ++i) {
            int index = MatrixMap::fromStep(i + 8);
            leds.unmask(index);
            leds.set(index, false, quickEditItems[i] != CurveSequenceListModel::Item::Last);
            leds.mask(index);
        }
    }
}

void CurveSequenceEditPage::keyDown(KeyEvent &event) {
    _stepSelection.keyDown(event, stepOffset());
    updateMonitorStep();
}

void CurveSequenceEditPage::keyUp(KeyEvent &event) {
    _stepSelection.keyUp(event, stepOffset());
    updateMonitorStep();
}

void CurveSequenceEditPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();
    auto &sequence = _project.selectedCurveSequence();

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.isQuickEdit()) {
        quickEdit(key.quickEdit());
        event.consume();
        return;
    }

    if (key.pageModifier() && key.is(Key::Step5)) {
        lfoContextShow();
        event.consume();
        return;
    }

    if (key.pageModifier()) {
        return;
    }

    if (key.isEncoder() && layer() == Layer::Shape && globalKeyState()[Key::Shift] && _stepSelection.count() > 1) {
        auto firstStep = sequence.step(_stepSelection.firstSetIndex());
        auto lastStep = sequence.step(_stepSelection.lastSetIndex());
        bool isReversed = firstStep.max() > lastStep.max();

        for (size_t stepIndex = 0, multiStepsProcessed = 0; stepIndex < sequence.steps().size(); ++stepIndex) {
            if (_stepSelection[stepIndex]) {
                auto &step = sequence.step(stepIndex);

                float min, max;
                std::tie(min, max) = calculateMultiStepShapeMinMax(_stepSelection.count(), multiStepsProcessed, sequence.step(_stepSelection.firstSetIndex()).shape(), !isReversed);

                step.setMin(int(min));
                step.setMax(int(max));

                multiStepsProcessed++;
            }
        }
    }

    _stepSelection.keyPress(event, stepOffset());
    updateMonitorStep();

    if (key.isFunction()) {
        if (_editMode == EditMode::Wavefolder1) {
            int function = key.function();
            if (function >= 0 && function < 4) {
                _wavefolderRow = function;
                event.consume();
                return;
            }
        } else if (_editMode == EditMode::Chaos) {
        int function = key.function();
        if (function >= 0 && function < 4) {
            if (key.shiftModifier() && function == 0) {
                // Toggle Algo on Shift + F1
                auto algo = sequence.chaosAlgo();
                auto newAlgo = CurveSequence::ChaosAlgorithm((int(algo) + 1) % int(CurveSequence::ChaosAlgorithm::Last));
                sequence.setChaosAlgo(newAlgo);
            } else {
                _chaosRow = function;
            }
            event.consume();
            return;
        }
    }
        // For F5, or any F-key in other modes
        switchLayer(key.function(), key.shiftModifier());
        event.consume();
    }

    if (key.isLeft()) {
        if (key.shiftModifier()) {
            sequence.shiftSteps(_stepSelection.selected(), -1);
        } else {
            _section = std::max(0, _section - 1);
        }
        event.consume();
    }
    if (key.isRight()) {
        if (key.shiftModifier()) {
            sequence.shiftSteps(_stepSelection.selected(), 1);
        } else {
            _section = std::min(3, _section + 1);
        }
        event.consume();
    }
}

void CurveSequenceEditPage::encoder(EncoderEvent &event) {
    auto &sequence = _project.selectedCurveSequence();
    auto &track = _project.selectedTrack().curveTrack();
    bool shift = globalKeyState()[Key::Shift];

    switch (_editMode) {
    case EditMode::GlobalPhase:
        track.editGlobalPhase(event.value(), shift);
        event.consume();
        return;
    case EditMode::Wavefolder1:
        if (event.pressed()) {
            _wavefolderRow = clamp(_wavefolderRow + event.value(), 0, 3);
        } else {
            switch (_wavefolderRow) {
            case 0: sequence.editWavefolderFold(event.value(), shift); break;
            case 1: sequence.editWavefolderGain(event.value(), shift); break;
            case 2: sequence.editDjFilter(event.value(), shift); break;
            case 3: sequence.editXFade(event.value(), shift); break;
            }
        }
        event.consume();
        return;
    case EditMode::Chaos:
        if (event.pressed()) {
            _chaosRow = clamp(_chaosRow + event.value(), 0, 3);
        } else {
            switch (_chaosRow) {
            case 0: sequence.editChaosAmount(event.value(), shift); break;
            case 1: sequence.editChaosRate(event.value(), shift); break;
            case 2: sequence.editChaosParam1(event.value(), shift); break;
            case 3: sequence.editChaosParam2(event.value(), shift); break;
            }
        }
        event.consume();
        return;
    case EditMode::Step:
        // fall through to step edit logic
        break;
    }

    if (_stepSelection.any()) {
        _showDetail = true;
        _showDetailTicks = os::ticks();
    } else {
        return;
    }

    for (size_t stepIndex = 0, multiStepsProcessed = 0; stepIndex < sequence.steps().size(); ++stepIndex) {
        if (_stepSelection[stepIndex]) {
            auto &step = sequence.step(stepIndex);
            bool shift = globalKeyState()[Key::Shift];
            switch (layer()) {
            case Layer::Shape:
                if (_stepSelection.count() > 1 && shift) { // Create a multi-step shape
                    auto &firstStep = sequence.step(_stepSelection.firstSetIndex());
                    int firstStepShape = multiStepsProcessed == 0 ? std::max(firstStep.shape() + event.value(), 0) : firstStep.shape();
                    step.setShape(firstStepShape);

                    int min, max;
                    std::tie(min, max) = calculateMultiStepShapeMinMax(_stepSelection.count(), multiStepsProcessed, firstStepShape, false);

                    step.setMin(min);
                    step.setMax(max);
                } else {
                    step.setShape(step.shape() + event.value());
                }
                break;
            case Layer::ShapeVariation:
                step.setShapeVariation(step.shapeVariation() + event.value());
                break;
            case Layer::ShapeVariationProbability:
                step.setShapeVariationProbability(step.shapeVariationProbability() + event.value());
                break;
            case Layer::Min:
            case Layer::Max: {
                bool functionPressed = globalKeyState()[MatrixMap::fromFunction(activeFunctionKey())];
                int offset = event.value() * ((shift || event.pressed()) ? 1 : 8);
                if (functionPressed) {
                    // adjust both min and max
                    offset = clamp(offset, -step.min(), CurveSequence::Max::max() - step.max());
                    step.setMin(step.min() + offset);
                    step.setMax(step.max() + offset);
                } else {
                    // adjust min or max
                    if (layer() == Layer::Min) {
                        step.setMin(step.min() + offset);
                    } else {
                        step.setMax(step.max() + offset);
                    }
                }
                break;
            }
            case Layer::Gate:
                step.setGate(step.gate() + event.value());
                break;
            case Layer::GateProbability:
                step.setGateProbability(step.gateProbability() + event.value());
                break;
            case Layer::Last:
                break;
            }

            multiStepsProcessed++;
        }
    }

    event.consume();
}

void CurveSequenceEditPage::switchLayer(int functionKey, bool shift) {
    // Handle Phase function (F5)
    if (Function(functionKey) == Function::Phase) {
        switch (_editMode) {
        case EditMode::Step:
            _editMode = EditMode::GlobalPhase;
            break;
        case EditMode::GlobalPhase:
            _editMode = EditMode::Wavefolder1;
            _wavefolderRow = 0; // Reset row selection
            break;
        case EditMode::Wavefolder1:
            _editMode = EditMode::Chaos;
            _chaosRow = 0; // Reset row selection
            break;
        case EditMode::Chaos:
            _editMode = EditMode::Step;
            _stepSelection.clear();
            break;
        }
        return;
    }

    // Exit special modes when switching to other layers
    if (_editMode != EditMode::Step) {
        _editMode = EditMode::Step;
        _stepSelection.clear();
    }

    if (shift) {
        switch (Function(functionKey)) {
        case Function::Shape:
            setLayer(Layer::Shape);
            break;
        case Function::Min:
            setLayer(Layer::Min);
            break;
        case Function::Max:
            setLayer(Layer::Max);
            break;
        case Function::Gate:
            setLayer(Layer::Gate);
            break;
        case Function::Phase:
            break;
        }
        return;
    }

    switch (Function(functionKey)) {
    case Function::Shape:
        switch (layer()) {
        case Layer::Shape:
            setLayer(Layer::ShapeVariation);
            break;
        case Layer::ShapeVariation:
            setLayer(Layer::ShapeVariationProbability);
            break;
        case Layer::ShapeVariationProbability:
            setLayer(Layer::Shape);
            break;
        default:
            setLayer(Layer::Shape);
            break;
        }
        break;
    case Function::Min:
        setLayer(Layer::Min);
        break;
    case Function::Max:
        setLayer(Layer::Max);
        break;
    case Function::Gate:
        switch (layer()) {
        case Layer::Gate:
            setLayer(Layer::GateProbability);
            break;
        case Layer::GateProbability:
            setLayer(Layer::Gate);
            break;
        default:
            setLayer(Layer::Gate);
            break;
        }
        break;
    case Function::Phase:
        break;
    }
}

int CurveSequenceEditPage::activeFunctionKey() {
    if (_editMode == EditMode::GlobalPhase || _editMode == EditMode::Wavefolder1 || _editMode == EditMode::Chaos) {
        return 4; // Function::Phase
    }

    switch(layer()) {
    case Layer::Shape:
    case Layer::ShapeVariation:
    case Layer::ShapeVariationProbability:
        return 0;
    case Layer::Min:
        return 1;
    case Layer::Max:
        return 2;
    case Layer::Gate:
    case Layer::GateProbability:
        return 3;
    case Layer::Last:
        break;
    }

    return -1;
}

void CurveSequenceEditPage::updateMonitorStep() {
    auto &trackEngine = _engine.selectedTrackEngine().as<CurveTrackEngine>();

    if ((layer() == Layer::Min || layer() == Layer::Max) && !_stepSelection.isPersisted() && _stepSelection.any()) {
        trackEngine.setMonitorStep(_stepSelection.first());
        trackEngine.setMonitorStepLevel(layer() == Layer::Min ? CurveTrackEngine::MonitorLevel::Min : CurveTrackEngine::MonitorLevel::Max);
    } else {
        trackEngine.setMonitorStep(-1);
    }
}

void CurveSequenceEditPage::drawDetail(Canvas &canvas, const CurveSequence::Step &step) {

    FixedStringBuilder<16> str;

    WindowPainter::drawFrame(canvas, 64, 16, 128, 32);

    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(Color::Bright);
    canvas.vline(64 + 32, 16, 32);

    canvas.setFont(Font::Small);
    str("%d", _stepSelection.first() + 1);
    if (_stepSelection.count() > 1) {
        str("*");
    }
    canvas.drawTextCentered(64, 16, 32, 32, str);

    canvas.setFont(Font::Tiny);

    switch (layer()) {
    case Layer::Shape:
    case Layer::ShapeVariation:
        break;
    case Layer::ShapeVariationProbability:
        SequencePainter::drawProbability(
            canvas,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.shapeVariationProbability(), 8
        );
        str.reset();
        str("%.1f%%", 100.f * step.shapeVariationProbability() / 8.f);
        canvas.setColor(Color::Bright);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
        break;
    case Layer::Min:
    case Layer::Max:
    case Layer::Gate:
    case Layer::GateProbability:
        SequencePainter::drawProbability(
            canvas,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.gateProbability() + 1, CurveSequence::GateProbability::Range
        );
        str.reset();
        str("%.1f%%", 100.f * (step.gateProbability() + 1.f) / CurveSequence::GateProbability::Range);
        canvas.setColor(Color::Bright);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
        break;
    case Layer::Last:
        break;
    }
}

void CurveSequenceEditPage::contextShow() {
    if (_editMode == EditMode::Step || _editMode == EditMode::GlobalPhase) {
        showContextMenu(ContextMenu(
            contextMenuItems,
            int(ContextAction::Last),
            [&] (int index) { contextAction(index); },
            [&] (int index) { return contextActionEnabled(index); }
        ));
    } else {
        settingsContextShow();
    }
}

void CurveSequenceEditPage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Init:
        initSequence();
        break;
    case ContextAction::Copy:
        copySequence();
        break;
    case ContextAction::Paste:
        pasteSequence();
        break;
    case ContextAction::Duplicate:
        duplicateSequence();
        break;
    case ContextAction::Generate:
        generateSequence();
        break;
    case ContextAction::Last:
        break;
    }
}

bool CurveSequenceEditPage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    case ContextAction::Paste:
        return _model.clipBoard().canPasteCurveSequenceSteps();
    default:
        return true;
    }
}

void CurveSequenceEditPage::settingsContextShow() {
    showContextMenu(ContextMenu(
        settingsContextMenuItems,
        int(SettingsContextAction::Last),
        [&] (int index) { settingsContextAction(index); },
        [&] (int index) { return true; }
    ));
}

void CurveSequenceEditPage::settingsContextAction(int index) {
    switch (SettingsContextAction(index)) {
    case SettingsContextAction::Init:
        initSettings();
        break;
    case SettingsContextAction::Randomize:
        randomizeSettings();
        break;
    case SettingsContextAction::Copy:
        copySettings();
        break;
    case SettingsContextAction::Paste:
        pasteSettings();
        break;
    case SettingsContextAction::Last:
        break;
    }
}

void CurveSequenceEditPage::initSettings() {
    auto &sequence = _project.selectedCurveSequence();
    if (_editMode == EditMode::Wavefolder1) {
        sequence.setWavefolderFold(0.f);
        sequence.setWavefolderGain(0.f);
        sequence.setDjFilter(0.f);
        sequence.setXFade(1.f);
        showMessage("WAVEFOLDER INITIALIZED");
    } else if (_editMode == EditMode::Chaos) {
        sequence.setChaosAmount(0);
        sequence.setChaosRate(0);
        sequence.setChaosParam1(0);
        sequence.setChaosParam2(0);
        sequence.setChaosAlgo(CurveSequence::ChaosAlgorithm::Latoocarfian);
        showMessage("CHAOS INITIALIZED");
    }
}

void CurveSequenceEditPage::randomizeSettings() {
    auto &sequence = _project.selectedCurveSequence();
    if (_editMode == EditMode::Wavefolder1) {
        sequence.setWavefolderFold(float(rng.nextRange(100)) / 100.f);
        sequence.setWavefolderGain(float(rng.nextRange(200)) / 100.f);
        sequence.setDjFilter((float(rng.nextRange(200)) / 100.f) - 1.f);
        sequence.setXFade(float(rng.nextRange(100)) / 100.f);
        showMessage("WAVEFOLDER RANDOMIZED");
    } else if (_editMode == EditMode::Chaos) {
        sequence.setChaosAmount(rng.nextRange(101));
        sequence.setChaosRate(rng.nextRange(128));
        sequence.setChaosParam1(rng.nextRange(101));
        sequence.setChaosParam2(rng.nextRange(101));
        sequence.setChaosAlgo(CurveSequence::ChaosAlgorithm(rng.nextRange(int(CurveSequence::ChaosAlgorithm::Last))));
        showMessage("CHAOS RANDOMIZED");
    }
}

void CurveSequenceEditPage::copySettings() {
    auto &sequence = _project.selectedCurveSequence();
    _settingsClipboard.wavefolderFold = sequence.wavefolderFold();
    _settingsClipboard.wavefolderGain = sequence.wavefolderGain();
    _settingsClipboard.djFilter = sequence.djFilter();
    _settingsClipboard.xFade = sequence.xFade();
    _settingsClipboard.chaosAmount = sequence.chaosAmount();
    _settingsClipboard.chaosRate = sequence.chaosRate();
    _settingsClipboard.chaosParam1 = sequence.chaosParam1();
    _settingsClipboard.chaosParam2 = sequence.chaosParam2();
    // Cast sequence enum to track enum for clipboard compatibility (if needed, or update clipboard)
    // Actually clipboard uses CurveTrack::ChaosAlgorithm, but we moved it to CurveSequence.
    // We should update the Clipboard struct to use CurveSequence::ChaosAlgorithm.
    // For now, casting is safe as they are identical.
    _settingsClipboard.chaosAlgo = sequence.chaosAlgo();
    showMessage("SETTINGS COPIED");
}

void CurveSequenceEditPage::pasteSettings() {
    auto &sequence = _project.selectedCurveSequence();
    sequence.setWavefolderFold(_settingsClipboard.wavefolderFold);
    sequence.setWavefolderGain(_settingsClipboard.wavefolderGain);
    sequence.setDjFilter(_settingsClipboard.djFilter);
    sequence.setXFade(_settingsClipboard.xFade);
    sequence.setChaosAmount(_settingsClipboard.chaosAmount);
    sequence.setChaosRate(_settingsClipboard.chaosRate);
    sequence.setChaosParam1(_settingsClipboard.chaosParam1);
    sequence.setChaosParam2(_settingsClipboard.chaosParam2);
    sequence.setChaosAlgo(_settingsClipboard.chaosAlgo);
    showMessage("SETTINGS PASTED");
}

void CurveSequenceEditPage::initSequence() {
    _project.selectedCurveSequence().clearSteps();
    showMessage("STEPS INITIALIZED");
}

void CurveSequenceEditPage::copySequence() {
    _model.clipBoard().copyCurveSequenceSteps(_project.selectedCurveSequence(), _stepSelection.selected());
    showMessage("STEPS COPIED");
}

void CurveSequenceEditPage::pasteSequence() {
    _model.clipBoard().pasteCurveSequenceSteps(_project.selectedCurveSequence(), _stepSelection.selected());
    showMessage("STEPS PASTED");
}

void CurveSequenceEditPage::duplicateSequence() {
    _project.selectedCurveSequence().duplicateSteps();
    showMessage("STEPS DUPLICATED");
}

void CurveSequenceEditPage::generateSequence() {
    _manager.pages().generatorSelect.show([this] (bool success, Generator::Mode mode) {
        if (success) {
            auto builder = _builderContainer.create<CurveSequenceBuilder>(_project.selectedCurveSequence(), layer());
            auto generator = Generator::execute(mode, *builder);
            if (generator) {
                _manager.pages().generator.show(generator);
            }
        }
    });
}

void CurveSequenceEditPage::quickEdit(int index) {
    _listModel.setSequence(&_project.selectedCurveSequence());
    if (quickEditItems[index] != CurveSequenceListModel::Item::Last) {
        _manager.pages().quickEdit.show(_listModel, int(quickEditItems[index]));
    }
}

void CurveSequenceEditPage::lfoContextShow() {
    showContextMenu(ContextMenu(
        lfoContextMenuItems,
        int(LfoContextAction::Last),
        [&] (int index) { lfoContextAction(index); },
        [&] (int index) { return true; }
    ));
}

void CurveSequenceEditPage::lfoContextAction(int index) {
    auto &sequence = _project.selectedCurveSequence();

    // Determine range: use selected steps if any, otherwise all steps
    int firstStep = 0;
    int lastStep = CONFIG_STEP_COUNT - 1;

    if (_stepSelection.any()) {
        firstStep = _stepSelection.firstSetIndex();
        lastStep = _stepSelection.lastSetIndex();
    }

    switch (LfoContextAction(index)) {
    case LfoContextAction::Triangle:
        sequence.populateWithTriangleWaveLfo(firstStep, lastStep);
        showMessage("LFO TRIANGLE POPULATED");
        break;
    case LfoContextAction::Sine:
        sequence.populateWithSineWaveLfo(firstStep, lastStep);
        showMessage("LFO SINE POPULATED");
        break;
    case LfoContextAction::Sawtooth:
        sequence.populateWithSawtoothWaveLfo(firstStep, lastStep);
        showMessage("LFO SAWTOOTH POPULATED");
        break;
    case LfoContextAction::Square:
        sequence.populateWithSquareWaveLfo(firstStep, lastStep);
        showMessage("LFO SQUARE POPULATED");
        break;
    case LfoContextAction::RandomMinMax:
        sequence.populateWithRandomMinMax(firstStep, lastStep);
        showMessage("MIN/MAX RANDOMIZED");
        break;
    case LfoContextAction::Last:
        break;
    }
}
