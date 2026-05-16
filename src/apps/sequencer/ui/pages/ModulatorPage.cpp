#include "ModulatorPage.h"

#include "Pages.h"
#include "Config.h"
#include "engine/ModulatorEngine.h"
#include "engine/generators/Lorenz.h"
#include "engine/generators/Latoocarfian.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

#include "core/math/Math.h"
#include "core/utils/StringBuilder.h"

static const ContextMenuModel::Item contextMenuItems[] = {
    { "Route" },
};

ModulatorPage::ModulatorPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{
}

void ModulatorPage::enter() {
    _selectedModulator = _project.selectedModulatorIndex();
    _waveformCacheValid = false;
    _currentPage = 0;
}

void ModulatorPage::exit() {
}

void ModulatorPage::draw(Canvas &canvas) {
    auto &modulator = _project.modulator(_selectedModulator);
    bool isRandom = (modulator.shape() == Modulator::Shape::Random);
    bool isTriggered = isRandom && (modulator.randomMode() == Modulator::RandomMode::Triggered);
    bool isADSR = (modulator.shape() == Modulator::Shape::ADSR);
    bool isChaos = Modulator::isChaosShape(modulator.shape());

    // Update total pages for ADSR and chaos
    if (isADSR || isChaos) {
        _totalPages = 2;
    } else {
        _totalPages = 1;
    }
    if (_currentPage >= _totalPages) {
        _currentPage = 0;
    }

    WindowPainter::clear(canvas);

    // Header
    FixedStringBuilder<32> title("MOD %d - %s", _selectedModulator + 1, _showRoutingOverlay ? "ROUTING" : "MODULATOR");
    WindowPainter::drawHeader(canvas, _model, _engine, title);

    // Footer
    if (_showRoutingOverlay) {
        const char *routingNames[5];
        routingNames[0] = "MODE";
        routingNames[1] = "GATE";
        routingNames[2] = "TARGET";
        routingNames[3] = "EVENT";
        routingNames[4] = "CC NUM";
        WindowPainter::drawFooter(canvas, routingNames, pageKeyState(), int(_selectedRoutingFunction));
    } else {
        WindowPainter::drawActiveFunction(canvas, "");
        const char *functionNames[5];
        if (isADSR) {
            if (_currentPage == 0) {
                functionNames[0] = "SHAPE";
                functionNames[1] = "ATTACK";
                functionNames[2] = "DECAY";
                functionNames[3] = "SUSTAIN";
                functionNames[4] = "RELEASE";
            } else {
                functionNames[0] = "AMPLIT";
                functionNames[1] = nullptr;
                functionNames[2] = nullptr;
                functionNames[3] = nullptr;
                functionNames[4] = nullptr;
            }
        } else if (isChaos) {
            if (_currentPage == 0) {
                functionNames[0] = "SHAPE";
                functionNames[1] = "RATE";
                functionNames[2] = "P1";
                functionNames[3] = "P2";
                functionNames[4] = "DEPTH";
            } else {
                functionNames[0] = "SLEW";
                functionNames[1] = "OFFSET";
                functionNames[2] = nullptr;
                functionNames[3] = nullptr;
                functionNames[4] = nullptr;
            }
        } else {
            functionNames[0] = "SHAPE";
            functionNames[1] = isRandom ? "R.MODE" : "RATE";
            functionNames[2] = isTriggered ? "G.TRACK" : "DEPTH";
            functionNames[3] = "OFFSET";
            functionNames[4] = isRandom ? "SLEW" : "PHASE";
        }
        int highlight = isADSR ? int(_selectedFunction) : int(_selectedFunction);
        WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), highlight);

        // Pagination dots for ADSR
        if (_totalPages > 1) {
            canvas.setColor(Color::Low);
            canvas.setFont(Font::Tiny);
            FixedStringBuilder<8> page("Pg %d/%d", _currentPage + 1, _totalPages);
            canvas.drawText(56, 12, page);
        }
    }

    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(Color::Bright);
    canvas.setFont(Font::Small);

    // Build parameter name/value strings
    FixedStringBuilder<16> paramName;
    FixedStringBuilder<32> paramValue;

    if (_showRoutingOverlay) {
        switch (_selectedRoutingFunction) {
        case RoutingFunction::Mode:
            paramName("MODE");
            modulator.printMode(paramValue);
            break;
        case RoutingFunction::Gate:
            paramName("GATE");
            modulator.printGateTrack(paramValue);
            break;
        case RoutingFunction::Target: {
            paramName("TARGET");
            if (_routingTargetType == RoutingTargetType::None) {
                paramValue("NONE");
            } else if (_routingTargetType == RoutingTargetType::Midi) {
                paramValue("MIDI %d", _routingTargetIndex + 1);
            } else {
                paramValue("CV %d", _routingTargetIndex + 1);
            }
            break;
        }
        case RoutingFunction::Event:
            paramName("EVENT");
            if (_routingTargetType == RoutingTargetType::CV) {
                paramValue("N/A");
            } else {
                paramValue(_routingEventIsCC ? "CC" : "Note");
            }
            break;
        case RoutingFunction::CCNumber:
            paramName("CC NUM");
            if (_routingTargetType == RoutingTargetType::CV || !_routingEventIsCC) {
                paramValue("N/A");
            } else {
                paramValue("CC %d", _routingCCNum);
            }
            break;
        }
    } else if (isADSR) {
        if (_currentPage == 0) {
            switch (_selectedFunction) {
            case Function::Shape:
                paramName("SHAPE");
                paramValue("ADSR");
                break;
            case Function::Rate:
                paramName("ATTACK");
                modulator.printAttack(paramValue);
                break;
            case Function::Depth:
                paramName("DECAY");
                modulator.printDecay(paramValue);
                break;
            case Function::Offset:
                paramName("SUSTAIN");
                modulator.printSustain(paramValue);
                break;
            case Function::Phase:
                paramName("RELEASE");
                modulator.printRelease(paramValue);
                break;
            }
        } else {
            switch (_selectedFunction) {
            case Function::Shape:
                paramName("AMPLITUDE");
                modulator.printAmplitude(paramValue);
                break;
            default:
                break;
            }
        }
    } else if (isChaos) {
        if (_currentPage == 0) {
            switch (_selectedFunction) {
            case Function::Shape:
                paramName("SHAPE");
                paramValue(Modulator::shapeName(modulator.shape()));
                break;
            case Function::Rate:
                paramName("RATE");
                modulator.printRate(paramValue);
                break;
            case Function::Depth:
                paramName("P1");
                paramValue("%d", modulator.attack() / 20);
                break;
            case Function::Offset:
                paramName("P2");
                paramValue("%d", modulator.decay() / 20);
                break;
            case Function::Phase:
                paramName("DEPTH");
                modulator.printDepth(paramValue);
                break;
            }
        } else {
            switch (_selectedFunction) {
            case Function::Shape:
                paramName("SLEW");
                modulator.printSmooth(paramValue);
                break;
            case Function::Rate:
                paramName("OFFSET");
                modulator.printOffset(paramValue);
                break;
            default:
                break;
            }
        }
    } else {
        switch (_selectedFunction) {
        case Function::Shape:
            paramName("SHAPE");
            paramValue(Modulator::shapeName(modulator.shape()));
            break;
        case Function::Rate:
            if (isRandom) {
                paramName("R.MODE");
                paramValue(Modulator::randomModeName(modulator.randomMode()));
            } else {
                paramName("RATE");
                modulator.printRate(paramValue);
            }
            break;
        case Function::Depth:
            if (isTriggered) {
                paramName("G.TRACK");
                modulator.printGateTrack(paramValue);
            } else {
                paramName("DEPTH");
                modulator.printDepth(paramValue);
            }
            break;
        case Function::Offset:
            paramName("OFFSET");
            modulator.printOffset(paramValue);
            break;
        case Function::Phase:
            if (isRandom) {
                paramName("SLEW");
                modulator.printSmooth(paramValue);
            } else {
                paramName("PHASE");
                modulator.printPhase(paramValue);
            }
            break;
        }
    }

    // --- Left half: waveform or ADSR envelope ---
    const int waveformX = 4;
    const int waveformY = 15;
    const int waveformW = 116;
    const int waveformH = 34;

    if (isChaos) {
        // Chaos: live output visualization (like Random — updates every frame)
        canvas.setColor(Color::Bright);
        canvas.drawRect(waveformX, waveformY, waveformW, waveformH);

        const int scopeX = waveformX + 2;
        const int scopeY = waveformY + (waveformH / 2);
        const int scopeWidth = waveformW - 4;
        const int scopeHeight = waveformH - 4;

        canvas.setColor(Color::Low);
        canvas.hline(scopeX, scopeY, scopeWidth);

        canvas.setColor(Color::Bright);
        int currentValue = _engine.modulatorEngine().currentValue(_selectedModulator);
        int y = scopeY - ((currentValue - 64) * scopeHeight / 2) / 127;
        canvas.hline(scopeX, y, scopeWidth);

        // Level bar (same as all other shapes)
        const int barX = waveformX + waveformW + 4;
        const int barWidth = 4;
        canvas.setColor(Color::Medium);
        canvas.drawRect(barX, waveformY, barWidth, waveformH);
        canvas.setColor(Color::Bright);
        int levelY = waveformY + waveformH - 2 - ((currentValue * (waveformH - 4)) / 127);
        canvas.hline(barX + 1, levelY, barWidth - 2);
        canvas.hline(barX + 1, levelY + 1, barWidth - 2);
    } else if (isADSR) {
        // ADSR envelope visualization
        int attackMs = modulator.attack();
        int decayMs = modulator.decay();
        int sustainLevel = modulator.sustain();
        int releaseMs = modulator.release();
        int amplitude = modulator.amplitude();

        int totalTime = attackMs + decayMs + 500 + releaseMs;
        if (totalTime == 0) totalTime = 1;
        int attackWidth = (attackMs * waveformW) / totalTime;
        int decayWidth = (decayMs * waveformW) / totalTime;
        int sustainWidth = (500 * waveformW) / totalTime;
        int releaseWidth = (releaseMs * waveformW) / totalTime;

        int x = waveformX;
        int bottomY = waveformY + waveformH - 2;
        int peakY = waveformY + 2;

        // Scale by amplitude
        int scaledPeakY = bottomY - ((bottomY - peakY) * amplitude) / 127;
        int scaledSustainY = bottomY - ((bottomY - peakY) * sustainLevel * amplitude) / (127 * 127);

        canvas.setColor(Color::Bright);
        // Attack
        canvas.line(x, bottomY, x + attackWidth, scaledPeakY);
        x += attackWidth;
        // Decay
        canvas.line(x, scaledPeakY, x + decayWidth, scaledSustainY);
        x += decayWidth;
        // Sustain
        if (sustainWidth > 0) {
            canvas.hline(x, scaledSustainY, sustainWidth);
        }
        x += sustainWidth;
        // Release
        canvas.line(x, scaledSustainY, x + releaseWidth, bottomY);

        // Draw playhead
        auto state = _engine.modulatorEngine().adsrState(_selectedModulator);
        uint32_t timer = _engine.modulatorEngine().adsrTimer(_selectedModulator);
        int playheadX = waveformX;

        switch (state) {
        case ModulatorEngine::ADSRState::Idle:
            playheadX = waveformX;
            break;
        case ModulatorEngine::ADSRState::Attack: {
            int attackTicks = (attackMs * CONFIG_PPQN) / 2500;
            if (attackTicks == 0) attackTicks = 1;
            int progress = clamp(static_cast<int>((timer * attackWidth) / attackTicks), 0, attackWidth);
            playheadX = waveformX + progress;
            break;
        }
        case ModulatorEngine::ADSRState::Decay: {
            int decayTicks = (decayMs * CONFIG_PPQN) / 2500;
            if (decayTicks == 0) decayTicks = 1;
            int progress = clamp(static_cast<int>((timer * decayWidth) / decayTicks), 0, decayWidth);
            playheadX = waveformX + attackWidth + progress;
            break;
        }
        case ModulatorEngine::ADSRState::Sustain:
            playheadX = waveformX + attackWidth + decayWidth;
            break;
        case ModulatorEngine::ADSRState::Release: {
            int releaseTicks = (releaseMs * CONFIG_PPQN) / 2500;
            if (releaseTicks == 0) releaseTicks = 1;
            int progress = clamp(static_cast<int>((timer * releaseWidth) / releaseTicks), 0, releaseWidth);
            playheadX = waveformX + attackWidth + decayWidth + sustainWidth + progress;
            break;
        }
        }

        canvas.setColor(Color::Medium);
        canvas.vline(playheadX, waveformY + 1, waveformH - 2);
    } else {
        // Waveform visualization
        canvas.setColor(Color::Bright);
        canvas.drawRect(waveformX, waveformY, waveformW, waveformH);

        const int scopeX = waveformX + 2;
        const int scopeY = waveformY + (waveformH / 2);
        const int scopeWidth = waveformW - 4;
        const int scopeHeight = waveformH - 4;

        canvas.setColor(Color::Low);
        canvas.hline(scopeX, scopeY, scopeWidth);

        canvas.setColor(Color::Bright);

        if (isRandom) {
            int currentValue = _engine.modulatorEngine().currentValue(_selectedModulator);
            int y = scopeY - ((currentValue - 64) * scopeHeight / 2) / 127;
            canvas.hline(scopeX, y, scopeWidth);
        } else {
            if (!_waveformCacheValid ||
                modulator.shape() != _lastShape ||
                modulator.depth() != _lastDepth ||
                modulator.offset() != _lastOffset ||
                modulator.phase() != _lastPhase ||
                modulator.attack() != _lastAttack ||
                modulator.decay() != _lastDecay) {
                updateWaveformCache();
            }

            for (int x = 0; x < WaveformCacheSize - 1; ++x) {
                int y1 = scopeY - (_waveformCache[x] * scopeHeight / 2) / 127;
                int y2 = scopeY - (_waveformCache[x + 1] * scopeHeight / 2) / 127;
                canvas.line(scopeX + x, y1, scopeX + x + 1, y2);
            }

            uint16_t currentPhase = _engine.modulatorEngine().currentPhase(_selectedModulator);
            int playheadX = (currentPhase * scopeWidth) / 65536;
            canvas.setColor(Color::Medium);
            canvas.vline(scopeX + playheadX, waveformY + 1, waveformH - 2);
        }

        // Level bar
        int currentValue = _engine.modulatorEngine().currentValue(_selectedModulator);
        const int barX = waveformX + waveformW + 4;
        const int barWidth = 4;

        canvas.setColor(Color::Medium);
        canvas.drawRect(barX, waveformY, barWidth, waveformH);

        canvas.setColor(Color::Bright);
        int levelY = waveformY + waveformH - 2 - ((currentValue * (waveformH - 4)) / 127);
        canvas.hline(barX + 1, levelY, barWidth - 2);
        canvas.hline(barX + 1, levelY + 1, barWidth - 2);
    }

    // --- Right half: parameter display ---
    const int paramX = 128;

    canvas.setFont(Font::Tiny);
    canvas.setColor(Color::Medium);
    canvas.drawText(paramX, 18, paramName);

    canvas.setFont(Font::Small);
    canvas.setColor(Color::Bright);
    canvas.drawText(paramX, 30, paramValue);

    if (!_showRoutingOverlay) {
        canvas.setFont(Font::Tiny);
        canvas.setColor(Color::Low);
        FixedStringBuilder<32> cvOut;
        bool first = true;
        for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
            if (_project.cvOutputModulator(i) == _selectedModulator + 1) {
                if (!first) cvOut(",");
                cvOut("CV%d", i + 1);
                first = false;
            }
        }
        if (!first) {
            FixedStringBuilder<32> label;
            label("-> %s", (const char *)cvOut);
            canvas.drawText(paramX, 44, label);
        }
    } else {
        canvas.setFont(Font::Tiny);
        canvas.setColor(Color::Low);
        canvas.drawTextCentered(paramX, 44, 128, 8, FixedStringBuilder<32>("Shift+Page to exit"));
    }
}

void ModulatorPage::updateLeds(Leds &leds) {
    for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
        bool isSelected = (i == _selectedModulator);
        leds.set(MatrixMap::fromTrack(i), false, isSelected);
    }
}

void ModulatorPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    // Shift+Page toggles routing overlay
    if (key.isPage() && key.shiftModifier()) {
        _showRoutingOverlay = !_showRoutingOverlay;
        if (_showRoutingOverlay) {
            _selectedRoutingFunction = RoutingFunction::Mode;
            loadRoutingFromMidiOutput();
        } else {
            _selectedFunction = Function::Shape;
            applyRoutingToMidiOutput();
        }
        event.consume();
        return;
    }

    if (key.pageModifier()) {
        return;
    }

    // Pagination for ADSR and chaos (Left/Right without shift)
    auto &modulator = _project.modulator(_selectedModulator);
    if ((modulator.shape() == Modulator::Shape::ADSR || Modulator::isChaosShape(modulator.shape()))
        && _totalPages > 1 && !_showRoutingOverlay) {
        if (key.is(Key::Left)) {
            if (_currentPage > 0) {
                _currentPage--;
                _selectedFunction = Function::Shape;
            }
            event.consume();
            return;
        }
        if (key.is(Key::Right)) {
            if (_currentPage < _totalPages - 1) {
                _currentPage++;
                _selectedFunction = Function::Shape;
            }
            event.consume();
            return;
        }
    }

    // Context menu
    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    // Track buttons select modulator
    if (key.isTrackSelect()) {
        setSelectedModulator(key.trackSelect());
        event.consume();
        return;
    }

    // Function buttons
    if (key.isFunction()) {
        if (_showRoutingOverlay) {
            int func = key.function();
            if (func >= 0 && func < 5) {
                // Skip EVENT and CC NUM if routing to CV (they're dimmed)
                if (_routingTargetType == RoutingTargetType::CV && func >= 3) {
                    return;
                }
                _selectedRoutingFunction = RoutingFunction(func);
            }
        } else {
            int func = key.function();
            if (func >= 0 && func < 5) {
                setSelectedFunction(Function(func));
            }
        }
        event.consume();
        return;
    }

    // Left/Right change modulator (only when not in ADSR pagination)
    if (key.isLeft()) {
        setSelectedModulator(_selectedModulator - 1);
        event.consume();
        return;
    }
    if (key.isRight()) {
        setSelectedModulator(_selectedModulator + 1);
        event.consume();
        return;
    }
}

void ModulatorPage::encoder(EncoderEvent &event) {
    auto &modulator = _project.modulator(_selectedModulator);
    bool pressed = event.pressed();

    if (_showRoutingOverlay) {
        switch (_selectedRoutingFunction) {
        case RoutingFunction::Mode:
            modulator.editMode(event.value(), pressed);
            break;
        case RoutingFunction::Gate:
            modulator.editGateTrack(event.value(), pressed);
            break;
        case RoutingFunction::Target: {
            int totalTargets = 1 + 16 + 8; // None + MIDI 1-16 + CV 1-8
            int currentGlobal;
            switch (_routingTargetType) {
                case RoutingTargetType::None: currentGlobal = 0; break;
                case RoutingTargetType::Midi: currentGlobal = 1 + _routingTargetIndex; break;
                case RoutingTargetType::CV: currentGlobal = 1 + 16 + _routingTargetIndex; break;
            }
            currentGlobal = (currentGlobal + event.value() + totalTargets) % totalTargets;
            if (currentGlobal == 0) {
                _routingTargetType = RoutingTargetType::None;
                _routingTargetIndex = 0;
            } else if (currentGlobal <= 16) {
                _routingTargetType = RoutingTargetType::Midi;
                _routingTargetIndex = currentGlobal - 1;
            } else {
                _routingTargetType = RoutingTargetType::CV;
                _routingTargetIndex = currentGlobal - 17;
            }
            break;
        }
        case RoutingFunction::Event:
            if (_routingTargetType == RoutingTargetType::Midi && event.value() != 0) {
                _routingEventIsCC = !_routingEventIsCC;
            }
            break;
        case RoutingFunction::CCNumber:
            if (_routingTargetType == RoutingTargetType::Midi && _routingEventIsCC) {
                _routingCCNum = clamp(_routingCCNum + event.value(), 0, 127);
            }
            break;
        }
    } else if (Modulator::isChaosShape(modulator.shape())) {
        if (_currentPage == 0) {
            switch (_selectedFunction) {
            case Function::Shape:
                modulator.editShape(event.value(), pressed);
                break;
            case Function::Rate:
                modulator.editRate(event.value(), pressed);
                break;
            case Function::Depth:
                modulator.editAttack(event.value(), pressed);
                break;
            case Function::Offset:
                modulator.editDecay(event.value(), pressed);
                break;
            case Function::Phase:
                modulator.editDepth(event.value(), pressed);
                break;
            }
        } else {
            switch (_selectedFunction) {
            case Function::Shape:
                modulator.editSmooth(event.value(), pressed);
                break;
            case Function::Rate:
                modulator.editOffset(event.value(), pressed);
                break;
            default:
                break;
            }
        }
    } else if (modulator.shape() == Modulator::Shape::ADSR) {
        if (_currentPage == 0) {
            switch (_selectedFunction) {
            case Function::Shape:
                modulator.editShape(event.value(), pressed);
                break;
            case Function::Rate:
                modulator.editAttack(event.value(), pressed);
                break;
            case Function::Depth:
                modulator.editDecay(event.value(), pressed);
                break;
            case Function::Offset:
                modulator.editSustain(event.value(), pressed);
                break;
            case Function::Phase:
                modulator.editRelease(event.value(), pressed);
                break;
            }
        } else {
            // ADSR page 2: only amplitude is meaningful; depth/offset are unused
            if (_selectedFunction == Function::Shape) {
                modulator.editAmplitude(event.value(), pressed);
            }
        }
    } else {
        bool isRandom = (modulator.shape() == Modulator::Shape::Random);
        bool isTriggered = isRandom && (modulator.randomMode() == Modulator::RandomMode::Triggered);

        switch (_selectedFunction) {
        case Function::Shape:
            modulator.editShape(event.value(), pressed);
            break;
        case Function::Rate:
            if (isRandom) {
                modulator.editRandomMode(event.value(), pressed);
            } else {
                modulator.editRate(event.value(), pressed);
            }
            break;
        case Function::Depth:
            if (isTriggered) {
                modulator.editGateTrack(event.value(), pressed);
            } else {
                modulator.editDepth(event.value(), pressed);
            }
            break;
        case Function::Offset:
            modulator.editOffset(event.value(), pressed);
            break;
        case Function::Phase:
            if (isRandom) {
                modulator.editSmooth(event.value(), pressed);
            } else {
                modulator.editPhase(event.value(), pressed);
            }
            break;
        }

        _waveformCacheValid = false;
    }

    event.consume();
}

void ModulatorPage::setSelectedModulator(int index) {
    _selectedModulator = clamp(index, 0, CONFIG_MODULATOR_COUNT - 1);
    _project.setSelectedModulatorIndex(_selectedModulator);
    _waveformCacheValid = false;
    _currentPage = 0;
    if (_showRoutingOverlay) {
        loadRoutingFromMidiOutput();
    }
}

void ModulatorPage::setSelectedFunction(Function function) {
    _selectedFunction = function;
}

void ModulatorPage::setSelectedRoutingFunction(RoutingFunction function) {
    _selectedRoutingFunction = function;
}

void ModulatorPage::updateWaveformCache() {
    auto &modulator = _project.modulator(_selectedModulator);

    _lastShape = modulator.shape();
    _lastDepth = modulator.depth();
    _lastOffset = modulator.offset();
    _lastPhase = modulator.phase();
    _lastAttack = modulator.attack();
    _lastDecay = modulator.decay();

    if (Modulator::isChaosShape(modulator.shape())) {
        if (modulator.shape() == Modulator::Shape::ChaosLorenz) {
            Lorenz lorenz;
            lorenz.reset();
            float p1n = modulator.attack() / 2000.f;
            float p2n = modulator.decay() / 2000.f;
            float p1c = p1n * (2.0f - p1n);
            float p2c = p2n * (2.0f - p2n);
            float rho  = 10.0f + p1c * 40.0f;
            float beta = 0.5f + p2c * 3.5f;
            for (int i = 0; i < 100; ++i) {
                lorenz.next(0.001f, rho, beta);
            }
            for (int x = 0; x < WaveformCacheSize; ++x) {
                float raw = lorenz.next(0.001f, rho, beta);
                int value = static_cast<int>(raw * 127.f);
                value = (value * modulator.depth()) / 127;
                value += modulator.offset();
                _waveformCache[x] = static_cast<int8_t>(clamp(value, -127, 127));
            }
        } else {
            Latoocarfian latoo;
            latoo.reset();
            float p1n = modulator.attack() / 2000.f;
            float p2n = modulator.decay() / 2000.f;
            float p1c = p1n * (2.0f - p1n);
            float p2c = p2n * (2.0f - p2n);
            float ab = 0.5f + p1c * 2.5f;
            float cd = 0.5f + p2c * 2.5f;
            for (int x = 0; x < WaveformCacheSize; ++x) {
                float raw = latoo.next(ab, ab, cd, cd);
                int value = static_cast<int>(raw * 127.f);
                value = (value * modulator.depth()) / 127;
                value += modulator.offset();
                _waveformCache[x] = static_cast<int8_t>(clamp(value, -127, 127));
            }
        }
        _waveformCacheValid = true;
        return;
    }

    for (int x = 0; x < WaveformCacheSize; ++x) {
        uint32_t phase = ((uint32_t)x * 65536) / WaveformCacheSize;
        phase = (phase + ((uint32_t)modulator.phase() * 65536 / 360)) & 0xFFFF;

        int value = WaveformGenerator::generate(modulator.shape(), (uint16_t)phase);
        value = (value * modulator.depth()) / 127;
        value += modulator.offset();
        value = clamp(value, -127, 127);

        _waveformCache[x] = value;
    }

    _waveformCacheValid = true;
}

void ModulatorPage::contextShow() {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [this] (int index) { contextAction(index); },
        [this] (int index) { return true; }
    ));
}

void ModulatorPage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Route: {
        for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
            if (_project.cvOutputModulator(i) == _selectedModulator + 1) {
                _project.setCvOutputModulator(i, 0);
                showMessage(FixedStringBuilder<32>("Mod %d -> CV %d removed", _selectedModulator + 1, i + 1));
                return;
            }
        }
        for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
            if (_project.cvOutputModulator(i) == 0) {
                _project.setCvOutputModulator(i, _selectedModulator + 1);
                showMessage(FixedStringBuilder<32>("Mod %d -> CV %d", _selectedModulator + 1, i + 1));
                return;
            }
        }
        _project.setCvOutputModulator(0, _selectedModulator + 1);
        showMessage(FixedStringBuilder<32>("Mod %d -> CV 1", _selectedModulator + 1));
        break;
    }
    case ContextAction::Last:
        break;
    }
}

void ModulatorPage::loadRoutingFromMidiOutput() {
    int targetModSource = int(MidiOutput::Output::ControlSource::FirstModulator) + _selectedModulator;

    bool foundInMidi = false;
    for (int i = 0; i < CONFIG_MIDI_OUTPUT_COUNT; ++i) {
        auto &output = _project.midiOutput().output(i);
        if (output.event() == MidiOutput::Output::Event::ControlChange &&
            int(output.controlSource()) == targetModSource) {
            _routingTargetType = RoutingTargetType::Midi;
            _routingTargetIndex = i;
            _routingCCNum = output.controlNumber();
            _routingEventIsCC = true;
            foundInMidi = true;
            break;
        }
    }

    if (!foundInMidi) {
        bool foundInCV = false;
        for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
            if (_project.cvOutputModulator(i) == _selectedModulator + 1) {
                _routingTargetType = RoutingTargetType::CV;
                _routingTargetIndex = i;
                _routingCCNum = 0;
                _routingEventIsCC = true;
                foundInCV = true;
                break;
            }
        }

        if (!foundInCV) {
            _routingTargetType = RoutingTargetType::None;
            _routingTargetIndex = 0;
            _routingCCNum = 0;
            _routingEventIsCC = true;
        }
    }
}

void ModulatorPage::applyRoutingToMidiOutput() {
    // Clear any existing old assignments first (fixes Modulove's leak where
    // switching MIDI->CV leaves the old MIDI assignment active)
    int targetModSource = int(MidiOutput::Output::ControlSource::FirstModulator) + _selectedModulator;
    for (int i = 0; i < CONFIG_MIDI_OUTPUT_COUNT; ++i) {
        auto &output = _project.midiOutput().output(i);
        if (output.event() == MidiOutput::Output::Event::ControlChange &&
            int(output.controlSource()) == targetModSource) {
            output.clear();
            break;
        }
    }
    for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
        if (_project.cvOutputModulator(i) == _selectedModulator + 1) {
            _project.setCvOutputModulator(i, 0);
            break;
        }
    }

    if (_routingTargetType == RoutingTargetType::None) {
        showMessage(FixedStringBuilder<32>("Mod %d unassigned", _selectedModulator + 1), 2000);
    } else if (_routingTargetType == RoutingTargetType::Midi) {
        auto &output = _project.midiOutput().output(_routingTargetIndex);
        if (_routingEventIsCC) {
            output.setEvent(MidiOutput::Output::Event::ControlChange);
            int modSource = int(MidiOutput::Output::ControlSource::FirstModulator) + _selectedModulator;
            output.setControlSource(MidiOutput::Output::ControlSource(modSource));
            output.setControlNumber(_routingCCNum);
            int channel = output.target().channel();
            const char *portName = (output.target().port() == Types::MidiPort::UsbMidi) ? "USB" : "MIDI";
            showMessage(FixedStringBuilder<32>("Mod %d > Out %d %s Ch%d CC%d",
                _selectedModulator + 1, _routingTargetIndex + 1, portName, channel + 1, _routingCCNum), 2000);
        }
    } else {
        _project.setCvOutputModulator(_routingTargetIndex, _selectedModulator + 1);
        showMessage(FixedStringBuilder<32>("Mod %d > CV %d",
            _selectedModulator + 1, _routingTargetIndex + 1), 2000);
    }
}
