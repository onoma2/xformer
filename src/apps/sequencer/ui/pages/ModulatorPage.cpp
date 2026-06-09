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
    { nullptr },
    { nullptr },
    { nullptr },
    { "CANCEL" },
    { "COMMIT" },
};

ModulatorPage::ModulatorPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{
}

void ModulatorPage::enter() {
    _selectedModulator = _project.selectedModulatorIndex();
    _currentPage = 0;
    _showRoutingOverlay = false;
}

void ModulatorPage::exit() {
    bakeJustf();   // leaving the page resolves JustF by baking the derived rates
}

void ModulatorPage::bakeJustf() {
    auto &me = _engine.modulatorEngine();
    if (!me.justfActive()) {
        return;
    }
    float m1Hz = _project.modulator(0).rateHz();
    float intone = me.intone();
    for (int i = 0; i < CONFIG_MODULATOR_COUNT; ++i) {
        auto &m = _project.modulator(i);
        if (m.rateDomain() != Modulator::RateDomain::Free) {
            continue;   // Free-domain only; Tempo modulators keep their rate
        }
        float hz = ModulatorEngine::justfEffectiveHz(m1Hz, intone, i + 1);
        m.setRate(int(hz * 100.f + 0.5f));   // -> centi-Hz; setRate clamps to the Free range
    }
    me.setJustfActive(false);
    me.setIntone(0.f);
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
        const char *routingNames[5] = { "RUN", "GATE", "CLEAR", "EVENT", "CC NUM" };
        int hl = -1;
        switch (_selectedRoutingFunction) {
        case RoutingFunction::Mode: hl = 0; break;
        case RoutingFunction::Gate: hl = 1; break;
        case RoutingFunction::Event: hl = 3; break;
        case RoutingFunction::CCNumber: hl = 4; break;
        default: break; // Grid -> no footer highlight
        }
        WindowPainter::drawFooter(canvas, routingNames, pageKeyState(), hl);
        drawDestinationsBody(canvas, _project.modulator(_selectedModulator));
        return;
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
            functionNames[2] = isTriggered ? "G.SRC" : "DEPTH";
            functionNames[3] = "OFFSET";
            functionNames[4] = isRandom ? "SLEW" : "PHASE";
        }
        // JustF: M2's RATE key becomes INTONE.
        if (_engine.modulatorEngine().justfActive() && !isADSR && !isChaos && !isRandom &&
            _selectedModulator == 1) {
            functionNames[1] = "INTONE";
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

    if (isADSR) {
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
            modulator.printGateSource(values[2]);
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

    // JustF: M2's RATE cell hosts INTONE; M3-M8 show their derived rate (read-only).
    {
        auto &me = _engine.modulatorEngine();
        if (me.justfActive() && !isADSR && !isChaos && !isRandom) {
            if (_selectedModulator == 1) {
                values[1]("%+.2f", me.intone());
            } else if (_selectedModulator > 1) {
                float hz = ModulatorEngine::justfEffectiveHz(
                    _project.modulator(0).rateHz(), me.intone(), _selectedModulator + 1);
                values[1]("%.2fHz", hz);
            }
            // M1 keeps its own printRate (the master).
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

    int selectedIdx = int(_selectedFunction);

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

    // JustF: M2's own derived rate, tiny, under the INTONE value.
    {
        auto &me = _engine.modulatorEngine();
        if (me.justfActive() && _selectedModulator == 1 && !isADSR && !isChaos && !isRandom) {
            float hz = ModulatorEngine::justfEffectiveHz(_project.modulator(0).rateHz(), me.intone(), 2);
            FixedStringBuilder<16> sub("%.1fHz", hz);
            canvas.setFont(Font::Tiny);
            canvas.setColor(Color::Medium);
            canvas.drawText(colR - canvas.textWidth(sub), 31, sub);
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

    // Context menu (Shift+Page) — must precede the page-modifier guard
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

    if (key.pageModifier()) {
        return;
    }

    // Left/Right walk the page cursor: param page(s) -> destinations.
    if (key.is(Key::Left)) {
        if (_showRoutingOverlay) {
            _showRoutingOverlay = false;
            _selectedFunction = Function::Shape;
        } else if (_currentPage > 0) {
            _currentPage--;
            _selectedFunction = Function::Shape;
        }
        event.consume();
        return;
    }
    if (key.is(Key::Right)) {
        if (!_showRoutingOverlay) {
            if (_currentPage < _totalPages - 1) {
                _currentPage++;
                _selectedFunction = Function::Shape;
            } else {
                _showRoutingOverlay = true;
                _selectedRoutingFunction = RoutingFunction::Grid;
                _destCursor = 0;
                loadRoutingFromMidiOutput();
            }
        }
        event.consume();
        return;
    }

    // Track buttons select modulator
    if (key.isTrackSelect()) {
        setSelectedModulator(key.trackSelect());
        event.consume();
        return;
    }

    // Encoder push on the destinations grid = add the cursored destination.
    if (key.isEncoder() && _showRoutingOverlay && _selectedRoutingFunction == RoutingFunction::Grid) {
        if (cursorIsCv()) {
            _cvSet |= (1 << cursorCvIndex());
        } else {
            _midiSet |= (1 << cursorMidiIndex());
        }
        event.consume();
        return;
    }

    // Function buttons
    if (key.isFunction()) {
        if (_showRoutingOverlay) {
            switch (key.function()) {
            case 0: _selectedRoutingFunction = RoutingFunction::Mode; break;
            case 1: _selectedRoutingFunction = RoutingFunction::Gate; break;
            case 2: // CLEAR the cursored destination
                if (cursorIsCv()) {
                    _cvSet &= ~(1 << cursorCvIndex());
                } else {
                    _midiSet &= ~(1 << cursorMidiIndex());
                }
                _selectedRoutingFunction = RoutingFunction::Grid;
                break;
            case 3: _selectedRoutingFunction = RoutingFunction::Event; break;
            case 4: _selectedRoutingFunction = RoutingFunction::CCNumber; break;
            }
        } else {
            int func = key.function();
            // Shift+RATE toggles JustF (Free-domain rate link). Toggling off bakes.
            if (func == int(Function::Rate) && key.shiftModifier()) {
                auto &me = _engine.modulatorEngine();
                if (me.justfActive()) {
                    bakeJustf();
                } else if (_project.modulator(0).rateDomain() == Modulator::RateDomain::Free) {
                    me.setIntone(0.f);
                    me.setJustfActive(true);
                    setSelectedFunction(Function::Rate);
                }
                event.consume();
                return;
            }
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
}

void ModulatorPage::encoder(EncoderEvent &event) {
    auto &modulator = _project.modulator(_selectedModulator);
    bool pressed = event.pressed();

    // JustF: RATE encoder edits INTONE on M2, is read-only on M3-M8, master on M1.
    {
        auto &me = _engine.modulatorEngine();
        if (me.justfActive() && !_showRoutingOverlay && _selectedFunction == Function::Rate) {
            if (_selectedModulator == 1) {
                float step = pressed ? 0.01f : 0.05f;   // finer when the encoder is pressed
                me.setIntone(me.intone() + event.value() * step);
                event.consume();
                return;
            }
            if (_selectedModulator >= 2) {
                event.consume();   // followers are read-only (derived)
                return;
            }
            // M1 (index 0) falls through to normal rate editing — the master.
        }
    }

    if (_showRoutingOverlay) {
        switch (_selectedRoutingFunction) {
        case RoutingFunction::Grid:
            if (event.value() != 0) {
                _destCursor = (_destCursor + event.value() + DestCount) % DestCount;
            }
            break;
        case RoutingFunction::Mode:
            modulator.editMode(event.value(), pressed);
            break;
        case RoutingFunction::Gate:
            modulator.setGateSource(Modulator::cycleGateSource(modulator.gateSource(), event.value(), _selectedModulator));
            break;
        case RoutingFunction::Event:
            if (!cursorIsCv() && event.value() != 0) {
                int idx = cursorMidiIndex();
                int n = int(ContEvent::Last);
                _midiEvent[idx] = ContEvent((int(_midiEvent[idx]) + event.value() + n) % n);
            }
            break;
        case RoutingFunction::CCNumber:
            if (!cursorIsCv() && _midiEvent[cursorMidiIndex()] == ContEvent::CC) {
                int idx = cursorMidiIndex();
                _midiCCNum[idx] = clamp(int(_midiCCNum[idx]) + event.value(), 0, 127);
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
                modulator.setGateSource(Modulator::cycleGateSource(modulator.gateSource(), event.value(), _selectedModulator));
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
        [this] (int index) {
            return _showRoutingOverlay &&
                   (index == int(ContextAction::Cancel) || index == int(ContextAction::Commit));
        },
        doubleClick
    ));
}

void ModulatorPage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Commit:
        applyRoutingToMidiOutput();
        break;
    case ContextAction::Cancel:
        loadRoutingFromMidiOutput();
        showMessage(FixedStringBuilder<32>("Mod %d routing reverted", _selectedModulator + 1), 2000);
        break;
    case ContextAction::Last:
        break;
    }
}

void ModulatorPage::loadRoutingFromMidiOutput() {
    int modSource = int(MidiOutput::Output::ControlSource::FirstModulator) + _selectedModulator;
    _cvSet = 0;
    _midiSet = 0;
    for (int i = 0; i < CONFIG_MIDI_OUTPUT_COUNT; ++i) {
        _midiEvent[i] = ContEvent::CC;
        _midiCCNum[i] = 0;
    }

    for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
        if (_project.cvOutputModulator(i) == _selectedModulator + 1) {
            _cvSet |= (1 << i);
        }
    }
    for (int i = 0; i < CONFIG_MIDI_OUTPUT_COUNT; ++i) {
        auto &output = _project.midiOutput().output(i);
        if (output.takesContinuousFromModulator(_selectedModulator)) {
            _midiSet |= (1 << i);
            _midiCCNum[i] = output.controlNumber();
            switch (output.event()) {
            case MidiOutput::Output::Event::PitchBend:       _midiEvent[i] = ContEvent::Bend; break;
            case MidiOutput::Output::Event::ChannelPressure: _midiEvent[i] = ContEvent::Pressure; break;
            default:                                         _midiEvent[i] = ContEvent::CC; break;
            }
        }
    }
}

void ModulatorPage::applyRoutingToMidiOutput() {
    int modSource = int(MidiOutput::Output::ControlSource::FirstModulator) + _selectedModulator;

    // CV jacks: assign members, release jacks dropped from the set.
    for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
        bool want = (_cvSet >> i) & 1;
        bool has = _project.cvOutputModulator(i) == _selectedModulator + 1;
        if (want && !has) {
            _project.setCvOutputModulator(i, _selectedModulator + 1);
        } else if (!want && has) {
            _project.setCvOutputModulator(i, 0);
        }
    }

    // MIDI outs: write continuous (CC/Bend/Aftertouch) members, clear dropped outs.
    for (int i = 0; i < CONFIG_MIDI_OUTPUT_COUNT; ++i) {
        auto &output = _project.midiOutput().output(i);
        bool has = output.takesContinuousFromModulator(_selectedModulator);
        bool want = (_midiSet >> i) & 1;
        if (want) {
            switch (_midiEvent[i]) {
            case ContEvent::Bend:
                output.setEvent(MidiOutput::Output::Event::PitchBend);
                break;
            case ContEvent::Pressure:
                output.setEvent(MidiOutput::Output::Event::ChannelPressure);
                break;
            default:
                output.setEvent(MidiOutput::Output::Event::ControlChange);
                output.setControlNumber(_midiCCNum[i]);
                break;
            }
            output.setControlSource(MidiOutput::Output::ControlSource(modSource));
        } else if (has) {
            output.clear();
        }
    }

    int cvCount = __builtin_popcount(_cvSet);
    int midiCount = __builtin_popcount((unsigned)_midiSet);
    showMessage(FixedStringBuilder<32>("Mod %d > %d CV, %d MIDI",
        _selectedModulator + 1, cvCount, midiCount), 2000);
}

void ModulatorPage::drawDestinationsBody(Canvas &canvas, Modulator &modulator) {
    drawMembershipGrid(canvas);

    const int colL = 128;
    canvas.setBlendMode(BlendMode::Set);

    // Top: cursored destination + its event/CC (the F4/F5 edit focus).
    canvas.setFont(Font::Small);
    FixedStringBuilder<16> dest;
    if (cursorIsCv()) {
        dest("CV %d", cursorCvIndex() + 1);
    } else {
        dest("MIDI %d", cursorMidiIndex() + 1);
    }
    canvas.setColor(Color::Medium);
    canvas.drawText(colL, 22, dest);
    if (!cursorIsCv()) {
        int idx = cursorMidiIndex();
        FixedStringBuilder<16> ev;
        switch (_midiEvent[idx]) {
        case ContEvent::Bend:     ev("Bend"); break;
        case ContEvent::Pressure: ev("AT"); break;
        default:                  ev("CC %d", _midiCCNum[idx]); break;
        }
        canvas.setColor(Color::Bright);
        canvas.drawText(colL, 34, ev);
    }

    canvas.setColor(Color::Low);
    canvas.hline(colL, 39, 124);

    // Bottom: modulator-wide RUN mode + GATE track, compact.
    canvas.setFont(Font::Tiny);
    FixedStringBuilder<16> modeStr; modulator.printMode(modeStr);
    FixedStringBuilder<16> gateStr; modulator.printGateSource(gateStr);
    canvas.setColor(_selectedRoutingFunction == RoutingFunction::Mode ? Color::Bright : Color::Medium);
    canvas.drawText(colL, 50, "RUN");
    canvas.setColor(Color::Bright);
    canvas.drawText(colL + 22, 50, modeStr);
    canvas.setColor(_selectedRoutingFunction == RoutingFunction::Gate ? Color::Bright : Color::Medium);
    canvas.drawText(colL + 64, 50, "GATE");
    canvas.setColor(Color::Bright);
    canvas.drawText(colL + 94, 50, gateStr);
}

void ModulatorPage::drawMembershipGrid(Canvas &canvas) {
    const int x0 = 1, y0 = 12;
    const int cellW = 10, cellH = 9, pitch = 12;
    const int labX = x0 + 2, gridX0 = x0 + 17;
    const int rowY[3] = { y0 + 2, y0 + 15, y0 + 28 };
    const char *rowLabel[3] = { "CV", "MIDI", "" };

    canvas.setBlendMode(BlendMode::Set);
    for (int r = 0; r < 3; ++r) {
        int y = rowY[r];
        if (rowLabel[r][0]) {
            canvas.setColor(Color::Medium);
            canvas.setFont(Font::Tiny);
            canvas.drawText(labX, y + 7, rowLabel[r]);
        }
        for (int c = 0; c < 8; ++c) {
            bool isCv = (r == 0);
            int midiIdx = (r == 1) ? c : (c + 8);
            int displayNum = isCv ? (c + 1) : (midiIdx + 1);
            bool member = isCv ? ((_cvSet >> c) & 1) : ((_midiSet >> midiIdx) & 1);
            int cursorCell = isCv ? c : (CONFIG_CHANNEL_COUNT + midiIdx);
            bool isCursor = (_destCursor == cursorCell);
            int x = gridX0 + c * pitch;

            canvas.setColor(member ? Color::Bright : Color::Low);
            canvas.drawRect(x, y, cellW, cellH);
            if (isCursor) {
                canvas.setColor(Color::Medium);
                canvas.fillRect(x + 1, y + 1, cellW - 2, cellH - 2);
                canvas.setColor(Color::None);
            } else {
                canvas.setColor(member ? Color::Bright : Color::Medium);
            }
            canvas.setFont(Font::Tiny);
            FixedStringBuilder<4> num("%d", displayNum);
            int tw = canvas.textWidth(num);
            canvas.drawText(x + (cellW - tw) / 2 + 1, y + 7, num);
        }
    }
}

void ModulatorPage::keyboard(KeyboardEvent &event) {
    if (handleFunctionKeys(event)) return;
    BasePage::keyboard(event);
}
