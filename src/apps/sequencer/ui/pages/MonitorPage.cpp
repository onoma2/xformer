#include "MonitorPage.h"

#include "Config.h"
#include "ui/painters/WindowPainter.h"

#include "engine/CvInput.h"
#include "engine/CvOutput.h"
#include "engine/Engine.h"
#include "engine/RoutingEngine.h"
#include "engine/NoteTrackEngine.h"
#include "engine/CurveTrackEngine.h"
#include "engine/MidiCvTrackEngine.h"
#include "engine/TuesdayTrackEngine.h"
#include "engine/DiscreteMapTrackEngine.h"
#include "engine/IndexedTrackEngine.h"
#include "engine/TeletypeTrackEngine.h"
#include "engine/TrackEngine.h"
#include "model/NoteSequence.h"
#include "model/CurveSequence.h"
#include "model/NoteTrack.h"
#include "model/CurveTrack.h"
#include "model/MidiCvTrack.h"
#include "model/TuesdayTrack.h"
#include "model/DiscreteMapTrack.h"
#include "model/IndexedTrack.h"
#include "model/TeletypeTrack.h"
#include "model/Track.h"
#include "model/Model.h"

#include "core/math/Math.h"
#include "core/utils/StringBuilder.h"

// ARM sizeof probes — temporary, remove after recording phase 2 measurements

enum class Function {
    CvIn    = 0,
    CvOut   = 1,
    Midi    = 2,
    Stats   = 3,
    Sizes   = 4,
    Version = 5,
};

static const char *functionNames[] = { "CV IN", "CV OUT", "MIDI", "STATS", "SIZES", "Ver", nullptr };

static void formatMidiMessage(StringBuilder &eventStr, StringBuilder &dataStr, const MidiMessage &msg) {
    if (msg.isChannelMessage()) {
        int channel = msg.channel() + 1;
        switch (msg.channelMessage()) {
        case MidiMessage::NoteOff:
            eventStr("NOTE OFF");
            dataStr("CH=%d NOTE=%d VEL=%d", channel, msg.note(), msg.velocity());
            return;
        case MidiMessage::NoteOn:
            eventStr("NOTE ON");
            dataStr("CH=%d NOTE=%d VEL=%d", channel, msg.note(), msg.velocity());
            return;
        case MidiMessage::KeyPressure:
            eventStr("KEY PRESSURE");
            dataStr("CH=%d NOTE=%d PRE=%d", channel, msg.note(), msg.keyPressure());
            return;
        case MidiMessage::ControlChange:
            eventStr("CONTROL CHANGE");
            dataStr("CH=%d NUM=%d VAL=%d", channel, msg.controlNumber(), msg.controlValue());
            return;
        case MidiMessage::ProgramChange:
            eventStr("PROGRAM CHANGE");
            dataStr("CH=%d NUM=%d", channel, msg.programNumber());
            return;
        case MidiMessage::ChannelPressure:
            eventStr("CHANNEL PRESSURE");
            dataStr("CH=%d PRE=%d", channel, msg.channelPressure());
            return;
        case MidiMessage::PitchBend:
            eventStr("PITCH BEND");
            dataStr("CH=%d VAL=%d", channel, msg.pitchBend());
            return;
        }
    } else if (msg.isSystemMessage()) {
        switch (msg.systemMessage()) {
        case MidiMessage::SystemExclusive:
            eventStr("SYSEX");
            return;
        case MidiMessage::TimeCode:
            eventStr("TIME CODE");
            dataStr("DATA=%02x", msg.data0());
            return;
        case MidiMessage::SongPosition:
            eventStr("SONG POSITION");
            dataStr("POS=%d", msg.songPosition());
            return;
        case MidiMessage::SongSelect:
            eventStr("SONG SELECT");
            dataStr("NUM=%d", msg.songNumber());
            return;
        case MidiMessage::TuneRequest:
            eventStr("TUNE REQUEST");
            return;
        default: break;
        }
    }
}

MonitorPage::MonitorPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void MonitorPage::enter() {
}

void MonitorPage::exit() {
}

void MonitorPage::draw(Canvas &canvas) {
    if (_scopeActive) {
        drawScope(canvas);
        return;
    }

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "MONITOR");
    WindowPainter::drawActiveFunction(canvas, functionNames[int(_mode)]);
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), int(_mode));

    canvas.setBlendMode(BlendMode::Set);
    canvas.setFont(Font::Tiny);
    canvas.setColor(Color::Bright);

    switch (_mode) {
    case Mode::CvIn:
        drawCvIn(canvas);
        break;
    case Mode::CvOut:
        drawCvOut(canvas);
        break;
    case Mode::Midi:
        drawMidi(canvas);
        break;
    case Mode::Stats:
        drawStats(canvas);
        break;
    case Mode::Sizes:
        drawSizes(canvas);
        break;
    case Mode::Version:
        drawVersion(canvas);
        break;
    }
}

void MonitorPage::updateLeds(Leds &leds) {
}

void MonitorPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }

    if (key.isFunction()) {
        switch (Function(key.function())) {
        case Function::CvIn:
            _mode = Mode::CvIn;
            break;
        case Function::CvOut:
            _mode = Mode::CvOut;
            break;
        case Function::Midi:
            _mode = Mode::Midi;
            break;
        case Function::Stats:
            _mode = Mode::Stats;
            break;
        case Function::Sizes:
            _mode = Mode::Sizes;
            break;
        case Function::Version:
            _mode = Mode::Version;
            break;
        }
    }
}

void MonitorPage::encoder(EncoderEvent &event) {
    if (_mode == Mode::Sizes) {
        int next = _sizePage + event.value();
        _sizePage = clamp(next, 0, SizePageCount - 1);
        return;
    }

    if (!_scopeActive) {
        return;
    }

    if (globalKeyState()[Key::Shift]) {
        int next = _scopeSecondaryChannel + event.value();
        if (next >= CvOutput::Channels) {
            next %= CvOutput::Channels;
        } else if (next < -1) {
            next = (next % CvOutput::Channels + CvOutput::Channels) % CvOutput::Channels;
            if (next == CvOutput::Channels - 1) {
                next = -1;
            }
        }
        _scopeSecondaryChannel = (int8_t)next;
    } else {
        int next = _scopeChannel + event.value();
        if (next >= CvOutput::Channels) {
            next %= CvOutput::Channels;
        } else if (next < 0) {
            next = (next % CvOutput::Channels + CvOutput::Channels) % CvOutput::Channels;
        }
        _scopeChannel = (int8_t)next;
    }
}

void MonitorPage::midi(MidiEvent &event) {
    _lastMidiMessage = event.message();
    _lastMidiMessagePort = event.port();
    _lastMidiMessageTicks = os::ticks();
}

void MonitorPage::setScopeActive(bool active) {
    if (_scopeActive == active) {
        return;
    }
    _scopeActive = active;
    if (_scopeActive) {
        resetScope();
    }
}

void MonitorPage::toggleScope() {
    setScopeActive(!_scopeActive);
}

void MonitorPage::drawCvIn(Canvas &canvas) {
    FixedStringBuilder<16> str;

    int w = Width / 4;
    int h = 8;

    for (size_t i = 0; i < CvInput::Channels; ++i) {
        int x = i * w;
        int y = 32;

        str.reset();
        str("CV%d", i + 1);
        canvas.drawTextCentered(x, y - h, w, h, str);

        str.reset();
        str("%.2fV", _engine.cvInput().channel(i));
        canvas.drawTextCentered(x, y, w, h, str);
    }
}

void MonitorPage::drawCvOut(Canvas &canvas) {
    FixedStringBuilder<16> str;

    int w = Width / 4;
    int h = 8;

    for (size_t i = 0; i < CvOutput::Channels; ++i) {
        int x = (i % 4) * w;
        int y = 20 + (i / 4) * 20;

        str.reset();
        str("CV%d", i + 1);
        canvas.drawTextCentered(x, y - h, w, h, str);

        str.reset();
        str("%.2fV", _engine.cvOutput().channel(i));
        canvas.drawTextCentered(x, y, w, h, str);
    }
}

void MonitorPage::drawMidi(Canvas &canvas) {

    if (os::ticks() - _lastMidiMessageTicks < os::time::ms(1000)) {
        FixedStringBuilder<32> eventStr;
        FixedStringBuilder<32> dataStr;
        formatMidiMessage(eventStr, dataStr, _lastMidiMessage);
        canvas.drawTextCentered(0, 24 - 8, Width, 16, midiPortName(_lastMidiMessagePort));
        canvas.drawTextCentered(0, 32 - 8, Width, 16, eventStr);
        canvas.drawTextCentered(0, 40 - 8, Width, 16, dataStr);
    }
}

void MonitorPage::drawStats(Canvas &canvas) {
    auto stats = _engine.stats();

    auto drawValue = [&] (int index, const char *name, const char *value) {
        canvas.drawText(10, 20 + index * 10, name);
        canvas.drawText(100, 20 + index * 10, value);
    };

    {
        int seconds = stats.uptime;
        int minutes = seconds / 60;
        int hours = minutes / 60;
        FixedStringBuilder<16> str("%d:%02d:%02d", hours, minutes % 60, seconds % 60);
        drawValue(0, "UPTIME:", str);
    }

    {
        FixedStringBuilder<16> str("%d", stats.midiRxOverflow);
        drawValue(1, "MIDI OVF:", str);
    }

    {
        FixedStringBuilder<16> str("%d", stats.usbMidiRxOverflow);
        drawValue(2, "USBMIDI OVF:", str);
    }

}

void MonitorPage::drawVersion(Canvas &canvas) {
    canvas.setFont(Font::Small);
    canvas.drawTextCentered(0, 10, Width, 16, CONFIG_VERSION_NAME);
    FixedStringBuilder<16> str("Version %d.%d.%d", CONFIG_VERSION_MAJOR, CONFIG_VERSION_MINOR, CONFIG_VERSION_REVISION);
    canvas.drawTextCentered(0, 25, Width, 16, str);
}

void MonitorPage::sampleScope() {
    // Store normalized int8 (+/-127 = +/-6V) for the shared scope trace; keep the raw
    // primary CV for the numeric readout.
    float cv = _engine.cvOutput().channel(_scopeChannel);
    _scopeLastCv = cv;
    _scopeCv[_scopeWriteIndex] = int8_t(clamp(int(cv / 6.f * 127.f), -127, 127));
    _scopeGate[_scopeWriteIndex] = (_engine.gateOutput() >> _scopeChannel) & 1;

    if (_scopeSecondaryChannel >= 0) {
        float cvSec = _engine.cvOutput().channel(_scopeSecondaryChannel);
        _scopeCvSecondary[_scopeWriteIndex] = int8_t(clamp(int(cvSec / 6.f * 127.f), -127, 127));
    } else {
        _scopeCvSecondary[_scopeWriteIndex] = 0;
    }
    _scopeWriteIndex = (_scopeWriteIndex + 1) % ScopeWidth;
}

void MonitorPage::resetScope() {
    _scopeCv.fill(0);
    _scopeGate.fill(0);
    _scopeCvSecondary.fill(0);
    _scopeLastCv = 0.f;
    _scopeWriteIndex = 0;
}

void MonitorPage::drawSizes(Canvas &canvas) {
    // ARM sizeof probe display — temporary measurement gate for Phase 2
    canvas.setFont(Font::Tiny);
    canvas.setColor(Color::Bright);

    auto drawRow = [&](int row, const char *label, size_t size) {
        int y = 12 + row * 8;
        FixedStringBuilder<24> str("%s:%u", label, (unsigned)size);
        canvas.drawText(2, y, str);
    };

    auto drawRow2 = [&](int row, const char *label, size_t size1, size_t size2) {
        int y = 12 + row * 8;
        FixedStringBuilder<24> str("%s:%u/%u", label, (unsigned)size1, (unsigned)size2);
        canvas.drawText(2, y, str);
    };

    switch (_sizePage) {
    case 0: // Track & container
        drawRow(0, "Track", sizeof(Track));
        drawRow(1, "NoteTrack", sizeof(NoteTrack));
        drawRow(2, "CurveTrack", sizeof(CurveTrack));
        drawRow(3, "MidiCvTrack", sizeof(MidiCvTrack));
        drawRow(4, "TuesdayTrack", sizeof(TuesdayTrack));
        drawRow(5, "DiscreteMapTrack", sizeof(DiscreteMapTrack));
        drawRow(6, "IndexedTrack", sizeof(IndexedTrack));
        drawRow(7, "TeletypeTrack", sizeof(TeletypeTrack));
        drawRow2(8, "Container", sizeof(Container<NoteTrack, CurveTrack, MidiCvTrack, TuesdayTrack, DiscreteMapTrack, IndexedTrack, TeletypeTrack>), sizeof(Engine::TrackEngineContainer));
        drawRow(9, "NoteSeq", sizeof(NoteSequence));
        drawRow(10, "CurveSeq", sizeof(CurveSequence));
        break;
    case 1: // Sequences
        drawRow(0, "NoteSeq", sizeof(NoteSequence));
        drawRow(1, "NoteSeq::Step", sizeof(NoteSequence::Step));
        drawRow2(2, "NoteStep[]", sizeof(NoteSequence::Step) * CONFIG_STEP_COUNT, CONFIG_STEP_COUNT);
        drawRow(3, "CurveSeq", sizeof(CurveSequence));
        drawRow(4, "CurveSeq::Step", sizeof(CurveSequence::Step));
        drawRow2(5, "CurveStep[]", sizeof(CurveSequence::Step) * CONFIG_STEP_COUNT, CONFIG_STEP_COUNT);
        break;
    case 2: // RoutingEngine
        drawRow(0, "RoutingEngine", sizeof(RoutingEngine));
        drawRow(1, "RouteState", sizeof(RoutingEngine::RouteState));
        drawRow(2, "Shaper", sizeof(Routing::Shaper));
        drawRow2(3, "RouteStates[]", sizeof(RoutingEngine::RouteState) * CONFIG_ROUTE_COUNT, CONFIG_ROUTE_COUNT);
        break;
    case 3: // TrackEngine container sizes
        drawRow(0, "NoteTrackEngine", sizeof(NoteTrackEngine));
        drawRow(1, "CurveTrackEngine", sizeof(CurveTrackEngine));
        drawRow(2, "MidiCvTrackEngine", sizeof(MidiCvTrackEngine));
        drawRow(3, "TuesdayTrackEngine", sizeof(TuesdayTrackEngine));
        drawRow(4, "DiscreteMapTE", sizeof(DiscreteMapTrackEngine));
        drawRow(5, "IndexedTrackEngine", sizeof(IndexedTrackEngine));
        drawRow(6, "TeletypeTE", sizeof(TeletypeTrackEngine));
        drawRow(7, "TrackEngine", sizeof(TrackEngine));
        break;
    case 4: // Engine & Model & global
        drawRow(0, "Engine", sizeof(Engine));
        drawRow(1, "Model", sizeof(Model));
        drawRow(2, "Project", sizeof(Project));
        drawRow(3, "Clock", sizeof(Clock));
        drawRow(4, "RoutingEngine", sizeof(RoutingEngine));
        drawRow2(5, "TEContainer[]", sizeof(Engine::TrackEngineContainerArray), sizeof(Engine::TrackEngineContainer));
        drawRow(6, "TEArray", sizeof(Engine::TrackEngineArray));
        drawRow(7, "MidiOutEngine", sizeof(MidiOutputEngine));
        break;
    case 5: // Teletype
        drawRow(0, "TTTrack", sizeof(TeletypeTrack));
        drawRow(1, "TTSlot", sizeof(TeletypeTrack::PatternSlot));
        drawRow(2, "scene_state", sizeof(scene_state_t));
        drawRow(3, "tele_cmd", sizeof(tele_command_t));
        drawRow(4, "scene_pat", sizeof(scene_pattern_t));
        drawRow(5, "TTTE", sizeof(TeletypeTrackEngine));
        drawRow(6, "TECont", sizeof(Engine::TrackEngineContainer));
        break;
    }

    // Page indicator
    FixedStringBuilder<8> pageStr("%d/%d", _sizePage + 1, SizePageCount);
    canvas.setColor(Color::Low);
    canvas.drawText(Width - canvas.textWidth(pageStr) - 2, 8, pageStr);
}

void MonitorPage::drawScope(Canvas &canvas) {
    WindowPainter::clear(canvas);
    sampleScope();

    const int gateHeight = 3;
    const int gateTop = ScopeHeight - 3 - (gateHeight - 1);
    const int cvBottom = gateTop - 1;
    const int cvCenter = cvBottom / 2;   // +/-127 sample maps to +/-cvCenter (= +/-6V)

    canvas.setBlendMode(BlendMode::Set);

    if (_scopeSecondaryChannel >= 0) {
        canvas.setColor(Color::Low);
        ScopePainter::drawTrace(canvas, 0, cvCenter, cvCenter, _scopeCvSecondary.data(), ScopeWidth, _scopeWriteIndex);
    }

    canvas.setColor(Color::MediumBright);
    ScopePainter::drawTrace(canvas, 0, cvCenter, cvCenter, _scopeCv.data(), ScopeWidth, _scopeWriteIndex);

    canvas.setColor(Color::Medium);
    int runStart = -1;
    for (int x = 0; x < ScopeWidth; ++x) {
        int idx = (_scopeWriteIndex + x) % ScopeWidth;
        bool gate = _scopeGate[idx] != 0;
        if (gate) {
            if (runStart < 0) {
                runStart = x;
            }
        } else if (runStart >= 0) {
            canvas.fillRect(runStart, gateTop, x - runStart, gateHeight);
            runStart = -1;
        }
    }
    if (runStart >= 0) {
        canvas.fillRect(runStart, gateTop, ScopeWidth - runStart, gateHeight);
    }

    FixedStringBuilder<8> cvStr("%+5.2f", _scopeLastCv);
    canvas.setFont(Font::Tiny);
    canvas.setColor(Color::Low);
    int x = Width - 2 - canvas.textWidth(cvStr);
    canvas.drawText(x, 8, cvStr);

    canvas.setColor(Color::MediumBright);
    FixedStringBuilder<8> primaryStr("CH%d", _scopeChannel + 1);
    canvas.drawText(2, 8, primaryStr);

    int secondaryX = 2 + canvas.textWidth(primaryStr) + 4;
    canvas.setColor(Color::Low);
    if (_scopeSecondaryChannel < 0) {
        canvas.drawText(secondaryX, 8, "OFF");
    } else {
        FixedStringBuilder<8> secondaryStr("CH%d", _scopeSecondaryChannel + 1);
        canvas.drawText(secondaryX, 8, secondaryStr);
    }
}

void MonitorPage::keyboard(KeyboardEvent &event) {
    if (handleFunctionKeys(event)) return;
    if (event.keycode() == KeyboardEvent::KeyF6) {
        pressFunctionButton(5, event.shift());
        event.consume();
        return;
    }
    BasePage::keyboard(event);
}
