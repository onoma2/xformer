#include "MonitorPage.h"

#include "ui/painters/WindowPainter.h"

#include "engine/CvInput.h"
#include "engine/CvOutput.h"

#include "core/math/Math.h"
#include "core/utils/StringBuilder.h"

enum class Function {
    CvIn    = 0,
    CvOut   = 1,
    Midi    = 2,
    Stats   = 3,
    Version = 4,
};

static const char *functionNames[] = { "CV IN", "CV OUT", "MIDI", "STATS", "Version", nullptr };

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
        case Function::Version:
            _mode = Mode::Version;
            break;
        }
    }
}

void MonitorPage::encoder(EncoderEvent &event) {
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
    const auto &trackEngine = _engine.selectedTrackEngine();
    _scopeCv[_scopeWriteIndex] = trackEngine.cvOutput(0);
    _scopeGate[_scopeWriteIndex] = trackEngine.gateOutput(0) ? 1 : 0;
    _scopeWriteIndex = (_scopeWriteIndex + 1) % ScopeWidth;
}

void MonitorPage::resetScope() {
    _scopeCv.fill(0.f);
    _scopeGate.fill(0);
    _scopeWriteIndex = 0;
}

void MonitorPage::drawScope(Canvas &canvas) {
    WindowPainter::clear(canvas);
    sampleScope();

    const int gateHeight = 3;
    const int gateTop = ScopeHeight - 3 - (gateHeight - 1);
    const int gateBottom = gateTop + gateHeight - 1;
    const int cvBottom = gateTop - 1;
    const float cvCenter = cvBottom * 0.5f;
    const float cvScale = cvBottom / (2.f * 6.f);

    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(Color::MediumBright);

    int lastY = 0;
    {
        int idx = _scopeWriteIndex % ScopeWidth;
        float cv = _scopeCv[idx];
        lastY = clamp(int(cvCenter - cv * cvScale), 0, cvBottom);
    }

    for (int x = 1; x < ScopeWidth; ++x) {
        int idx = (_scopeWriteIndex + x) % ScopeWidth;
        float cv = _scopeCv[idx];
        int y = clamp(int(cvCenter - cv * cvScale), 0, cvBottom);
        int y0 = std::min(lastY, y);
        int y1 = std::max(lastY, y);
        canvas.vline(x, y0, y1 - y0 + 1);
        lastY = y;
    }

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

    int lastIndex = (_scopeWriteIndex + ScopeWidth - 1) % ScopeWidth;
    float lastCv = _scopeCv[lastIndex];
    FixedStringBuilder<8> cvStr("%+5.2f", lastCv);
    canvas.setFont(Font::Tiny);
    canvas.setColor(Color::Low);
    int x = Width - 2 - canvas.textWidth(cvStr);
    canvas.drawText(x, 8, cvStr);
}
