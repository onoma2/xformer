#include "TT2IoConfigPage.h"

#include "ui/painters/WindowPainter.h"

#include "model/TeletypeProgram.h"
#include "model/Types.h"
#include "model/Scale.h"
#include "model/ModelUtils.h"

#include "core/Debug.h"

#include <cstdio>
#include <algorithm>

namespace {

constexpr int W = 256;

const char *kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// 2-char code for a CV output voltage range, e.g. Bipolar5V -> "5B".
void rangeCode(Types::VoltageRange r, char *out) {
    int i = int(r);
    int volts = (i < 5) ? (i + 1) : (i - 4);
    out[0] = char('0' + volts);
    out[1] = (i < 5) ? 'U' : 'B';
    out[2] = '\0';
}

void trigSourceCode(TT2TriggerSource s, char *out) {
    int i = int(s);
    if (s == TT2TriggerSource::None) { out[0] = '-'; out[1] = '\0'; return; }
    if (i >= int(TT2TriggerSource::CvIn1) && i <= int(TT2TriggerSource::CvIn4)) {
        out[0] = 'I'; out[1] = char('1' + (i - int(TT2TriggerSource::CvIn1)));
    } else if (i >= int(TT2TriggerSource::GateOut1) && i <= int(TT2TriggerSource::GateOut8)) {
        out[0] = 'G'; out[1] = char('1' + (i - int(TT2TriggerSource::GateOut1)));
    } else {
        out[0] = 'L'; out[1] = char('1' + (i - int(TT2TriggerSource::LogicalGate1)));
    }
    out[2] = '\0';
}

void cvSourceCode(TT2CvInputSource s, char *out) {
    int i = int(s);
    if (s == TT2CvInputSource::None) { out[0] = '-'; out[1] = '\0'; return; }
    char letter = 'I'; int base = int(TT2CvInputSource::CvIn1);
    if (i >= int(TT2CvInputSource::CvOut1) && i <= int(TT2CvInputSource::CvOut8)) {
        letter = 'O'; base = int(TT2CvInputSource::CvOut1);
    } else if (i >= int(TT2CvInputSource::CvRoute1) && i <= int(TT2CvInputSource::CvRoute4)) {
        letter = 'R'; base = int(TT2CvInputSource::CvRoute1);
    } else if (i >= int(TT2CvInputSource::LogicalCv1) && i <= int(TT2CvInputSource::LogicalCv8)) {
        letter = 'L'; base = int(TT2CvInputSource::LogicalCv1);
    }
    out[0] = letter; out[1] = char('1' + (i - base)); out[2] = '\0';
}

} // namespace

TT2IoConfigPage::TT2IoConfigPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context) {
}

bool TT2IoConfigPage::isTt2() const {
    return _project.selectedTrack().trackMode() == Track::TrackMode::TeletypeV2;
}

void TT2IoConfigPage::enter() {
    _view = View::Outputs;
    _col = 0;
    _row = 0;
    _scroll = 0;
    _edit = false;
}

int TT2IoConfigPage::columnCount() const {
    return _view == View::Outputs ? 4 : 3;  // OUTPUTS: RNG/QNT/OFF/ROOT; INPUTS: TRIG/CV IN/MIDI
}

int TT2IoConfigPage::rowCount() const {
    if (_view == View::Outputs) return TT2_CV_OUTPUT_COUNT;
    switch (_col) {
    case 0: return TT2_TRIGGER_INPUT_COUNT;
    case 1: return TT2_CV_INPUT_COUNT;
    default: return 1;  // MIDI
    }
}

void TT2IoConfigPage::moveRow(int delta) {
    int n = rowCount();
    _row = clamp(_row + delta, 0, n - 1);
}

void TT2IoConfigPage::editCell(int delta) {
    if (!isTt2()) return;
    auto &program = _project.selectedTrack().tt2Track().program();
    _engine.lock();
    if (_view == View::Outputs) {
        int i = _row;
        switch (_col) {
        case 0:
            program.cvOutputRange[i] = ModelUtils::adjustedEnum(program.cvOutputRange[i], delta);
            break;
        case 1:
            program.cvOutputQuantizeScale[i] = clamp(int(program.cvOutputQuantizeScale[i]) + delta, -1, Scale::Count - 1);
            break;
        case 2:
            program.cvOutputOffset[i] = clamp(int(program.cvOutputOffset[i]) + delta * 5, -500, 500);
            break;
        case 3:
            program.cvOutputRootNote[i] = clamp(int(program.cvOutputRootNote[i]) + delta, -1, 11);
            break;
        }
    } else {
        switch (_col) {
        case 0:
            program.triggerSource[_row] = ModelUtils::adjustedEnum(program.triggerSource[_row], delta);
            break;
        case 1:
            program.cvInputSource[_row] = ModelUtils::adjustedEnum(program.cvInputSource[_row], delta);
            break;
        default:
            break;  // MIDI source edit deferred
        }
    }
    _engine.unlock();
}

void TT2IoConfigPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    canvas.setFont(Font::Tiny);

    // header
    canvas.setColor(Color::Bright);
    canvas.drawText(2, 7, "TT2 I/O");
    const char *tag = _view == View::Outputs ? "OUTPUTS" : "IN+MIDI";
    canvas.setColor(Color::Medium);
    canvas.drawText((W - canvas.textWidth(tag)) / 2, 7, tag);
    canvas.setColor(Color::Low);
    canvas.hline(0, 10, W);

    if (!isTt2()) {
        WindowPainter::drawFooter(canvas);
        return;
    }
    auto &program = _project.selectedTrack().tt2Track().program();

    const int top = 17, rowH = 7;

    auto cellColor = [&](bool selected) {
        if (!selected) return Color::Medium;
        return _edit ? Color::Bright : Color::MediumBright;
    };

    if (_view == View::Outputs) {
        const char *cols[4] = { "RNG", "QNT", "OFF", "ROOT" };
        const int colX[4] = { 60, 104, 148, 200 };
        const int gridTop = 18;
        const int visible = 5;
        if (_row < _scroll) _scroll = _row;
        else if (_row >= _scroll + visible) _scroll = _row - visible + 1;
        _scroll = clamp(_scroll, 0, std::max(0, TT2_CV_OUTPUT_COUNT - visible));
        canvas.setColor(Color::Medium);
        for (int c = 0; c < 4; ++c) canvas.drawText(colX[c], 16, cols[c]);
        for (int vi = 0; vi < visible; ++vi) {
            int r = _scroll + vi;
            if (r >= TT2_CV_OUTPUT_COUNT) break;
            int y = gridTop + vi * rowH;
            FixedStringBuilder<4> gut("CV%d", r + 1);
            canvas.setColor(r == _row ? Color::Bright : Color::Medium);
            canvas.drawText(2, y + 5, gut);
            char buf[8];
            for (int c = 0; c < 4; ++c) {
                bool sel = (r == _row && c == _col);
                if (c == 0) rangeCode(program.cvOutputRange[r], buf);
                else if (c == 1) {
                    int q = program.cvOutputQuantizeScale[r];
                    if (q < 0) std::snprintf(buf, sizeof(buf), "-");
                    else { const char *nm = Scale::name(q); buf[0] = nm[0]; buf[1] = nm[1]; buf[2] = nm[2]; buf[3] = '\0'; }
                } else if (c == 2) std::snprintf(buf, sizeof(buf), "%+.2f", program.cvOutputOffset[r] * 0.01f);
                else { int rn = program.cvOutputRootNote[r]; if (rn < 0) std::snprintf(buf, sizeof(buf), "*"); else std::snprintf(buf, sizeof(buf), "%s", kNoteNames[rn]); }
                if (sel) { canvas.setColor(cellColor(true)); canvas.fillRect(colX[c] - 2, y, 30, rowH); canvas.setBlendMode(BlendMode::Sub); }
                canvas.setColor(sel ? Color::Bright : Color::Medium);
                canvas.drawText(colX[c], y + 5, buf);
                if (sel) canvas.setBlendMode(BlendMode::Set);
            }
        }
        if (TT2_CV_OUTPUT_COUNT > visible) {
            const int ty = gridTop, th = 52 - ty;
            canvas.setColor(Color::Low);
            canvas.vline(W - 2, ty, th);
            canvas.setColor(Color::Bright);
            canvas.fillRect(W - 3, ty + th * _scroll / TT2_CV_OUTPUT_COUNT, 3, std::max(4, th * visible / TT2_CV_OUTPUT_COUNT));
        }
    } else {
        // INPUTS: TRIG (col 0) | CV IN (col 1) | MIDI (col 2)
        const int gridTop = top + 7;
        canvas.setColor(Color::Low);
        canvas.drawText(2, top + 4, "TRIG");
        canvas.drawText(70, top + 4, "CV IN");
        canvas.drawText(170, top + 4, "MIDI");
        char buf[8];
        const char *cvNames[6] = { "IN", "PRM", "X", "Y", "Z", "T" };
        int visRows = 5;
        for (int r = 0; r < TT2_TRIGGER_INPUT_COUNT && r < visRows; ++r) {
            int y = gridTop + r * rowH;
            FixedStringBuilder<4> nm("T%d", r + 1);
            canvas.setColor(Color::Low); canvas.drawText(2, y + 5, nm);
            bool sel = (_col == 0 && r == _row);
            trigSourceCode(program.triggerSource[r], buf);
            if (sel) { canvas.setColor(cellColor(true)); canvas.fillRect(16, y, 20, rowH); canvas.setBlendMode(BlendMode::Sub); }
            canvas.setColor(sel ? Color::Bright : Color::Medium); canvas.drawText(18, y + 5, buf);
            if (sel) canvas.setBlendMode(BlendMode::Set);
        }
        for (int r = 0; r < TT2_CV_INPUT_COUNT; ++r) {
            int sub = r / 3, subrow = r % 3;  // 2-up: IN/PRM/X | Y/Z/T
            int nx = 70 + sub * 50, vx = nx + 20;
            int y = gridTop + subrow * rowH;
            canvas.setColor(Color::Low); canvas.drawText(nx, y + 5, cvNames[r]);
            bool sel = (_col == 1 && r == _row);
            cvSourceCode(program.cvInputSource[r], buf);
            if (sel) { canvas.setColor(cellColor(true)); canvas.fillRect(vx - 2, y, 18, rowH); canvas.setBlendMode(BlendMode::Sub); }
            canvas.setColor(sel ? Color::Bright : Color::Medium); canvas.drawText(vx, y + 5, buf);
            if (sel) canvas.setBlendMode(BlendMode::Set);
        }
        // MIDI source (display only for now)
        bool midiSel = (_col == 2);
        canvas.setColor(midiSel ? Color::Bright : Color::Medium);
        canvas.drawText(170, gridTop + 5, "OMNI");
    }

    const char *outFooter[5] = { "RNG", "QNT", "OFF", "ROOT", "NEXT" };
    const char *inFooter[5] = { "TRIG", "CV IN", "MIDI", "", "NEXT" };
    WindowPainter::drawFooter(canvas, _view == View::Outputs ? outFooter : inFooter, pageKeyState(), _col);
}

void TT2IoConfigPage::updateLeds(Leds &leds) {
    (void)leds;
}

void TT2IoConfigPage::keyPress(KeyPressEvent &event) {
    if (!isTt2()) return;
    const auto &key = event.key();

    if (key.isEncoder()) {
        _edit = !_edit;
        event.consume();
        return;
    }
    if (key.isFunction()) {
        int fn = key.function();
        if (fn == 4) {  // NEXT — toggle view
            _view = _view == View::Outputs ? View::Inputs : View::Outputs;
            _col = 0; _row = 0; _scroll = 0; _edit = false;
        } else if (fn < columnCount()) {
            _col = fn;
            _row = clamp(_row, 0, rowCount() - 1);
            _edit = false;
        }
        event.consume();
        return;
    }
}

void TT2IoConfigPage::encoder(EncoderEvent &event) {
    if (!isTt2()) return;
    if (_edit) editCell(event.value());
    else moveRow(event.value());
}
