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
    WindowPainter::drawHeader(canvas, _model, _engine, "MOD");

    FixedStringBuilder<32> title("MOD %d", _selectedModulator + 1);
    WindowPainter::drawActiveFunction(canvas, title);

    // Dynamic footer: function names change based on shape/mode
    const char *functionNames[5];
    functionNames[0] = "SHAPE";
    if (isRandom) {
        functionNames[1] = "R.MODE";
    } else {
        functionNames[1] = "RATE";
    }
    if (isTriggered) {
        functionNames[2] = "G.TRACK";
    } else {
        functionNames[2] = "DEPTH";
    }
    functionNames[3] = "OFFSET";
    if (isRandom) {
        functionNames[4] = "SLEW";
    } else {
        functionNames[4] = "PHASE";
    }

    WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), int(_selectedFunction));

    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(Color::Bright);
    canvas.setFont(Font::Small);

    // --- Left half: waveform visualization ---
    const int waveformX = 4;
    const int waveformY = 16;
    const int waveformW = 116;
    const int waveformH = 32;

    // Draw border box
    canvas.setColor(Color::Bright);
    canvas.drawRect(waveformX, waveformY, waveformW, waveformH);

    const int scopeX = waveformX + 2;
    const int scopeY = waveformY + (waveformH / 2);
    const int scopeWidth = waveformW - 4;
    const int scopeHeight = waveformH - 4;

    // Draw centerline
    canvas.setColor(Color::Low);
    canvas.hline(scopeX, scopeY, scopeWidth);

    // Draw waveform
    canvas.setColor(Color::Bright);

    if (isRandom) {
        // Random: show current value as horizontal line
        int currentValue = _engine.modulatorEngine().currentValue(_selectedModulator);
        int y = scopeY - ((currentValue - 64) * scopeHeight / 2) / 127;
        canvas.hline(scopeX, y, scopeWidth);
    } else {
        // LFO: draw cached waveform
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

        // Draw playhead
        uint16_t currentPhase = _engine.modulatorEngine().currentPhase(_selectedModulator);
        int playheadX = (currentPhase * scopeWidth) / 65536;
        canvas.setColor(Color::Medium);
        canvas.vline(scopeX + playheadX, waveformY + 1, waveformH - 2);
    }

    // Draw level bar on right side of waveform
    int currentValue = _engine.modulatorEngine().currentValue(_selectedModulator);
    const int barX = waveformX + waveformW + 2;
    const int barWidth = 4;
    const int barHeight = waveformH;

    canvas.setColor(Color::Medium);
    canvas.drawRect(barX, waveformY, barWidth, barHeight);

    canvas.setColor(Color::Bright);
    int levelY = waveformY + barHeight - 2 - ((currentValue * (barHeight - 4)) / 127);
    canvas.hline(barX + 1, levelY, barWidth - 2);
    canvas.hline(barX + 1, levelY + 1, barWidth - 2);

    // --- Right half: parameter name and value ---
    const int paramX = 128;
    const int paramW = 128;

    FixedStringBuilder<16> paramName;
    FixedStringBuilder<32> paramValue;

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

    canvas.setFont(Font::Tiny);
    canvas.setColor(Color::Medium);
    canvas.drawText(paramX, 18, paramName);

    canvas.setFont(Font::Small);
    canvas.setColor(Color::Bright);
    canvas.drawText(paramX, 30, paramValue);

    // Draw CV output assignment info below
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

    // Show mode line for LFO shapes
    if (!isRandom) {
        FixedStringBuilder<16> modeStr;
        switch (modulator.mode()) {
        case Modulator::Mode::Free:      modeStr("FREE"); break;
        case Modulator::Mode::Sync:      modeStr("SYNC"); break;
        case Modulator::Mode::Retrigger: modeStr("RETRIG"); break;
        default: break;
        }
        canvas.drawText(paramX, 54, modeStr);
    }
}

void ModulatorPage::updateLeds(Leds &leds) {
    // Track LEDs 1-8 show selected modulator
    for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
        bool isSelected = (i == _selectedModulator);
        leds.set(MatrixMap::fromTrack(i), false, isSelected);
    }
}

void ModulatorPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }

    // Context menu for quick mapping
    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    // Track buttons 1-8 select modulator
    if (key.isTrackSelect()) {
        setSelectedModulator(key.trackSelect());
        event.consume();
        return;
    }

    // Function buttons select parameter
    if (key.isFunction()) {
        int func = key.function();
        if (func >= 0 && func < 5) {
            setSelectedFunction(Function(func));
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
    bool isRandom = (modulator.shape() == Modulator::Shape::Random);
    bool isTriggered = isRandom && (modulator.randomMode() == Modulator::RandomMode::Triggered);
    bool pressed = event.pressed();

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
        // Cycle through CV output assignments for this modulator
        // Find first unassigned CV output, or toggle first assigned one
        for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
            if (_project.cvOutputModulator(i) == _selectedModulator + 1) {
                // Unassign: this modulator -> CV output
                _project.setCvOutputModulator(i, 0);
                showMessage(FixedStringBuilder<32>("Mod %d → CV %d removed", _selectedModulator + 1, i + 1));
                return;
            }
        }
        // No assignment found: assign to first available CV output
        for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
            if (_project.cvOutputModulator(i) == 0) {
                _project.setCvOutputModulator(i, _selectedModulator + 1);
                showMessage(FixedStringBuilder<32>("Mod %d → CV %d", _selectedModulator + 1, i + 1));
                return;
            }
        }
        // All CV outputs assigned, just assign to channel 0 (overwrite)
        _project.setCvOutputModulator(0, _selectedModulator + 1);
        showMessage(FixedStringBuilder<32>("Mod %d → CV 1", _selectedModulator + 1));
        break;
    }
    case ContextAction::Last:
        break;
    }
}

void ModulatorPage::quickMapToOutput(int outputIndex) {
    if (outputIndex < 0 || outputIndex >= CONFIG_CHANNEL_COUNT) return;
    _project.setCvOutputModulator(outputIndex, _selectedModulator + 1);
}
