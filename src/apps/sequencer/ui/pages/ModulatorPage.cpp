#include "ModulatorPage.h"

#include "Pages.h"
#include "Config.h"
#include "engine/ModulatorEngine.h"

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
}

void ModulatorPage::exit() {
}

void ModulatorPage::draw(Canvas &canvas) {
    auto &modulator = _project.modulator(_selectedModulator);
    bool isRandom = (modulator.shape() == Modulator::Shape::Random);
    bool isTriggered = isRandom && (modulator.randomMode() == Modulator::RandomMode::Triggered);

    WindowPainter::clear(canvas);

    // Header
    FixedStringBuilder<32> title("MOD %d - %s", _selectedModulator + 1, _showRoutingOverlay ? "ROUTING" : "MODULATOR");
    WindowPainter::drawHeader(canvas, _model, _engine, title);

    // Footer
    if (_showRoutingOverlay) {
        const char *routingNames[5];
        routingNames[0] = "MODE";
        routingNames[1] = "GATE";
        routingNames[2] = "CV OUT";
        routingNames[3] = nullptr;
        routingNames[4] = nullptr;
        WindowPainter::drawFooter(canvas, routingNames, pageKeyState(), int(_selectedRoutingFunction));
    } else {
        WindowPainter::drawActiveFunction(canvas, "");
        const char *functionNames[5];
        functionNames[0] = "SHAPE";
        functionNames[1] = isRandom ? "R.MODE" : "RATE";
        functionNames[2] = isTriggered ? "G.TRACK" : "DEPTH";
        functionNames[3] = "OFFSET";
        functionNames[4] = isRandom ? "SLEW" : "PHASE";
        WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), int(_selectedFunction));
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
            paramName("CV OUT");
            bool first = true;
            for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
                if (_project.cvOutputModulator(i) == _selectedModulator + 1) {
                    if (!first) paramValue(",");
                    paramValue("CV%d", i + 1);
                    first = false;
                }
            }
            if (first) paramValue("--");
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

    // --- Draw waveform (left half) or CV output list (routing overlay) ---
    const int waveformX = 4;
    const int waveformY = 15;
    const int waveformW = 116;
    const int waveformH = 34;

    if (_showRoutingOverlay) {
        // Routing overlay: left half shows CV output assignments
        canvas.setColor(Color::Low);
        canvas.setFont(Font::Small);
        for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
            int y = 16 + i * 6;
            bool assigned = (_project.cvOutputModulator(i) == _selectedModulator + 1);
            canvas.setColor(assigned ? Color::Bright : Color::Low);
            FixedStringBuilder<16> label("CV%d", i + 1);
            canvas.drawText(6, y, label);
        }
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
                modulator.phase() != _lastPhase) {
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

    // --- Draw parameter display (right half) ---
    const int paramX = 128;

    canvas.setFont(Font::Tiny);
    canvas.setColor(Color::Medium);
    canvas.drawText(paramX, 18, paramName);

    canvas.setFont(Font::Small);
    canvas.setColor(Color::Bright);
    canvas.drawText(paramX, 30, paramValue);

    // CV output assignment hint (normal mode only)
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
        // Routing overlay hint
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

    // Shift+Page toggles routing overlay (must be before pageModifier check)
    if (key.isPage() && key.shiftModifier()) {
        _showRoutingOverlay = !_showRoutingOverlay;
        if (_showRoutingOverlay) {
            _selectedRoutingFunction = RoutingFunction::Mode;
            _routingCvOutputIndex = 0;
        } else {
            _selectedFunction = Function::Shape;
        }
        event.consume();
        return;
    }

    if (key.pageModifier()) {
        return;
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
            if (func >= 0 && func < 3) {
                _selectedRoutingFunction = RoutingFunction(func);
            }
        } else {
            int func = key.function();
            if (func >= 0 && func < 5) {
                _selectedFunction = Function(func);
            }
        }
        event.consume();
        return;
    }

    // Left/Right change modulator
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
        case RoutingFunction::Target:
            // Cycle CV output assignment
            _routingCvOutputIndex = (_routingCvOutputIndex + event.value() + CONFIG_CHANNEL_COUNT) % CONFIG_CHANNEL_COUNT;
            if (_project.cvOutputModulator(_routingCvOutputIndex) == _selectedModulator + 1) {
                _project.setCvOutputModulator(_routingCvOutputIndex, 0);
            } else {
                _project.setCvOutputModulator(_routingCvOutputIndex, _selectedModulator + 1);
            }
            break;
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
