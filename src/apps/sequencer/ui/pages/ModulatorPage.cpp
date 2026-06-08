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

    // Build all 5 parameter values for the grid display
    FixedStringBuilder<32> values[5];

    if (_showRoutingOverlay) {
        modulator.printMode(values[0]);
        modulator.printGateTrack(values[1]);
        if (_routingTargetType == RoutingTargetType::None) {
            values[2]("NONE");
        } else if (_routingTargetType == RoutingTargetType::Midi) {
            values[2]("MIDI %d", _routingTargetIndex + 1);
        } else {
            values[2]("CV %d", _routingTargetIndex + 1);
        }
        if (_routingTargetType == RoutingTargetType::CV) {
            values[3]("N/A");
        } else {
            values[3](_routingEventIsCC ? "CC" : "Note");
        }
        if (_routingTargetType == RoutingTargetType::CV || !_routingEventIsCC) {
            values[4]("N/A");
        } else {
            values[4]("CC %d", _routingCCNum);
        }
    } else if (isADSR) {
        if (_currentPage == 0) {
            values[0]("ADSR");
            modulator.printAttack(values[1]);
            modulator.printDecay(values[2]);
            modulator.printSustain(values[3]);
            modulator.printRelease(values[4]);
        } else {
            modulator.printAmplitude(values[0]);
        }
    } else if (isChaos) {
        if (_currentPage == 0) {
            values[0](Modulator::shapeName(modulator.shape()));
            modulator.printRate(values[1]);
            values[2]("%d", modulator.attack() / 20);
            values[3]("%d", modulator.decay() / 20);
            modulator.printDepth(values[4]);
        } else {
            modulator.printSmooth(values[0]);
            modulator.printOffset(values[1]);
        }
    } else {
        values[0](Modulator::shapeName(modulator.shape()));
        if (isRandom) {
            values[1](Modulator::randomModeName(modulator.randomMode()));
        } else {
            modulator.printRate(values[1]);
        }
        if (isTriggered) {
            modulator.printGateTrack(values[2]);
        } else {
            modulator.printDepth(values[2]);
        }
        modulator.printOffset(values[3]);
        if (isRandom) {
            modulator.printSmooth(values[4]);
        } else {
            modulator.printPhase(values[4]);
        }
    }

    // --- Left half: real-time scope of the modulator output (rolling trace, no level bar) ---
    {
        const int boxX = 1, boxY = 12, boxW = 119, boxH = 40;
        // Sample the live output (0..127, centered 64) into the rolling history, scaled to
        // +/-127 so the full swing fills the box. Same trace draw as the Monitor scope.
        int v = _engine.modulatorEngine().currentValue(_selectedModulator);
        _scope[_scopeWrite] = clamp((v - 64) * 2, -127, 127);
        _scopeWrite = (_scopeWrite + 1) % ScopeWidth;

        canvas.setBlendMode(BlendMode::Set);
        canvas.setColor(Color::Medium);
        canvas.drawRect(boxX, boxY, boxW, boxH);
        const int centerY = boxY + boxH / 2;
        const int halfH = boxH / 2 - 3;
        canvas.setColor(Color::Low);
        canvas.hline(boxX + 2, centerY, boxW - 4);
        canvas.setColor(Color::Bright);
        ScopePainter::drawTrace(canvas, boxX + 1, centerY, halfH, _scope, ScopeWidth, _scopeWrite);
    }

    // --- Right half: parameter display ---
    const int colL = 128;
    const int colM = 190;
    const int colR = 256;

    // Unified grid layout: all parameter values visible, no labels
    canvas.setBlendMode(BlendMode::Set);
    canvas.setFont(Font::Small);

    int selectedIdx = _showRoutingOverlay ? int(_selectedRoutingFunction) : int(_selectedFunction);

    // Row 1 (y=22): param 0 (left) + param 1 (right)
    for (int i = 0; i < 2; ++i) {
        if (values[i][0] == '\0') continue;
        canvas.setColor(i == selectedIdx ? Color::Bright : Color::Medium);
        if (i == 0) {
            canvas.drawText(colL, 22, values[0]);
        } else {
            int tw = canvas.textWidth(values[1]);
            canvas.drawText(colR - tw, 22, values[1]);
        }
    }

    // Row 2 (y=42): param 2 + param 3 + param 4
    for (int i = 2; i < 5; ++i) {
        if (values[i][0] == '\0') continue;
        canvas.setColor(i == selectedIdx ? Color::Bright : Color::Medium);
        if (i == 2) {
            canvas.drawText(colL, 42, values[2]);
        } else if (i == 3) {
            canvas.drawText(colM, 42, values[3]);
        } else {
            int tw = canvas.textWidth(values[4]);
            canvas.drawText(colR - tw, 42, values[4]);
        }
    }

    // CV routing info (only when NOT in routing overlay)
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
            canvas.drawText(colL, 54, label);
        }
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

    if (key.pageModifier() && event.count() == 2) {
        contextShow(true);
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
                // Re-pressing RATE (when it's already the selected function and the shape
                // actually shows RATE on F2) toggles the rate domain Free <-> Tempo.
                auto &modulator = _project.modulator(_selectedModulator);
                bool rateOnF2 = modulator.shape() != Modulator::Shape::Random &&
                                modulator.shape() != Modulator::Shape::ADSR;
                if (Function(func) == Function::Rate && _selectedFunction == Function::Rate && rateOnF2) {
                    modulator.cycleRateDomain();
                } else {
                    setSelectedFunction(Function(func));
                }
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
    }

    event.consume();
}

void ModulatorPage::setSelectedModulator(int index) {
    _selectedModulator = clamp(index, 0, CONFIG_MODULATOR_COUNT - 1);
    _project.setSelectedModulatorIndex(_selectedModulator);
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

void ModulatorPage::contextShow(bool doubleClick) {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [this] (int index) { contextAction(index); },
        [] (int index) { return true; },
        doubleClick
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

void ModulatorPage::keyboard(KeyboardEvent &event) {
    if (handleFunctionKeys(event)) return;
    BasePage::keyboard(event);
}
