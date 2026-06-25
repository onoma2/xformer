#include "TT2IoConfigPage.h"

#include "ui/pages/Pages.h"
#include "ui/painters/WindowPainter.h"

#include "engine/TT2UiAccess.h"

#include "model/TeletypeProgram.h"
#include "model/Types.h"
#include "model/Scale.h"
#include "model/ModelUtils.h"
#include "model/FileManager.h"

#include "core/fs/FileSystem.h"
#include "core/utils/StringBuilder.h"
#include "core/Debug.h"

#include <cstdio>
#include <algorithm>

enum class SceneAction {
    Copy,
    Paste,
    Load,
    Save,
    Last
};

static const ContextMenuModel::Item sceneMenuItems[] = {
    { "COPY" },
    { "PASTE" },
    { "TTLOAD" },
    { "TTSAVE" },
};

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

// 2-letter source codes: IN=CvIn, GT=GateOut, LG=LogicalGate, None=--.
void trigSourceCode(TT2TriggerSource s, char *out) {
    int i = int(s);
    if (s == TT2TriggerSource::None) { out[0] = '-'; out[1] = '-'; out[2] = '\0'; return; }
    char a = 'I', b = 'N'; int base = int(TT2TriggerSource::CvIn1);
    if (i >= int(TT2TriggerSource::GateOut1) && i <= int(TT2TriggerSource::GateOut8)) {
        a = 'G'; b = 'T'; base = int(TT2TriggerSource::GateOut1);
    } else if (i >= int(TT2TriggerSource::LogicalGate1) && i <= int(TT2TriggerSource::LogicalGate8)) {
        a = 'L'; b = 'G'; base = int(TT2TriggerSource::LogicalGate1);
    }
    out[0] = a; out[1] = b; out[2] = char('1' + (i - base)); out[3] = '\0';
}

// 2-letter source codes: IN=CvIn, CV=CvOut, RT=CvRoute, LC=LogicalCv, None=--.
void cvSourceCode(TT2CvInputSource s, char *out) {
    int i = int(s);
    if (s == TT2CvInputSource::None) { out[0] = '-'; out[1] = '-'; out[2] = '\0'; return; }
    char a = 'I', b = 'N'; int base = int(TT2CvInputSource::CvIn1);
    if (i >= int(TT2CvInputSource::CvOut1) && i <= int(TT2CvInputSource::CvOut8)) {
        a = 'C'; b = 'V'; base = int(TT2CvInputSource::CvOut1);
    } else if (i >= int(TT2CvInputSource::CvRoute1) && i <= int(TT2CvInputSource::CvRoute4)) {
        a = 'R'; b = 'T'; base = int(TT2CvInputSource::CvRoute1);
    } else if (i >= int(TT2CvInputSource::LogicalCv1) && i <= int(TT2CvInputSource::LogicalCv8)) {
        a = 'L'; b = 'C'; base = int(TT2CvInputSource::LogicalCv1);
    }
    out[0] = a; out[1] = b; out[2] = char('1' + (i - base)); out[3] = '\0';
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
    case 0: return tt2TriggerInputCount(_project.selectedTrack());
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
    auto &track = _project.selectedTrack();
    _engine.lock();
    if (_view == View::Outputs) {
        int i = _row;
        switch (_col) {
        case 0:
            tt2SetCvOutputRange(track, i, ModelUtils::adjustedEnum(tt2CvOutputRange(track, i), delta));
            break;
        case 1:
            tt2SetCvOutputQuantizeScale(track, i, int8_t(clamp(int(tt2CvOutputQuantizeScale(track, i)) + delta, -1, Scale::Count - 1)));
            break;
        case 2:
            tt2SetCvOutputOffset(track, i, int16_t(clamp(int(tt2CvOutputOffset(track, i)) + delta * 5, -500, 500)));
            break;
        case 3:
            tt2SetCvOutputRootNote(track, i, int8_t(clamp(int(tt2CvOutputRootNote(track, i)) + delta, -1, 11)));
            break;
        }
    } else {
        switch (_col) {
        case 0:
            tt2SetTriggerSource(track, _row, ModelUtils::adjustedEnum(tt2TriggerSource(track, _row), delta));
            break;
        case 1:
            tt2SetCvInputSource(track, _row, ModelUtils::adjustedEnum(tt2CvInputSource(track, _row), delta));
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
    auto &track = _project.selectedTrack();

    const int top = 17, rowH = 7;

    auto cellColor = [&](bool selected) {
        if (!selected) return Color::Medium;
        return _edit ? Color::Bright : Color::MediumBright;
    };

    if (_view == View::Outputs) {
        const char *cols[4] = { "RNG", "QNT", "OFFST", "ROOT" };
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
                if (c == 0) rangeCode(tt2CvOutputRange(track, r), buf);
                else if (c == 1) {
                    int q = tt2CvOutputQuantizeScale(track, r);
                    if (q < 0) std::snprintf(buf, sizeof(buf), "-");
                    else { const char *nm = Scale::name(q); buf[0] = nm[0]; buf[1] = nm[1]; buf[2] = nm[2]; buf[3] = '\0'; }
                } else if (c == 2) stbsp_snprintf(buf, sizeof(buf), "%+.2f", tt2CvOutputOffset(track, r) * 0.01f);  // newlib-nano has no float printf; stb does
                else { int rn = tt2CvOutputRootNote(track, r); if (rn < 0) std::snprintf(buf, sizeof(buf), "*"); else std::snprintf(buf, sizeof(buf), "%s", kNoteNames[rn]); }
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
        // INPUTS: TRIG (left) | CV IN (centre) | MIDI (right), spread full-width.
        const int gridTop = top + 7;
        canvas.setColor(Color::Low);
        canvas.drawText(2, top + 4, "TRIG");
        canvas.drawText(98, top + 4, "CV IN");
        canvas.drawText(208, top + 4, "MIDI");
        char buf[8];
        const char *cvNames[6] = { "IN", "PRM", "X", "Y", "Z", "T" };
        for (int r = 0; r < tt2TriggerInputCount(track); ++r) {
            int col = r / 4, row = r % 4;  // 2-up: T1-4 | T5-8
            int nx = 2 + col * 42, vx = nx + 16;
            int y = gridTop + row * rowH;
            FixedStringBuilder<4> nm("T%d", r + 1);
            canvas.setColor(Color::Low); canvas.drawText(nx, y + 5, nm);
            bool sel = (_col == 0 && r == _row);
            trigSourceCode(tt2TriggerSource(track, r), buf);
            if (sel) { canvas.setColor(cellColor(true)); canvas.fillRect(vx - 2, y, 18, rowH); canvas.setBlendMode(BlendMode::Sub); }
            canvas.setColor(sel ? Color::Bright : Color::Medium); canvas.drawText(vx, y + 5, buf);
            if (sel) canvas.setBlendMode(BlendMode::Set);
        }
        for (int r = 0; r < TT2_CV_INPUT_COUNT; ++r) {
            int sub = r / 3, subrow = r % 3;  // 2-up: IN/PRM/X | Y/Z/T
            int nx = 98 + sub * 54, vx = nx + 20;
            int y = gridTop + subrow * rowH;
            canvas.setColor(Color::Low); canvas.drawText(nx, y + 5, cvNames[r]);
            bool sel = (_col == 1 && r == _row);
            cvSourceCode(tt2CvInputSource(track, r), buf);
            if (sel) { canvas.setColor(cellColor(true)); canvas.fillRect(vx - 2, y, 18, rowH); canvas.setBlendMode(BlendMode::Sub); }
            canvas.setColor(sel ? Color::Bright : Color::Medium); canvas.drawText(vx, y + 5, buf);
            if (sel) canvas.setBlendMode(BlendMode::Set);
        }
        // MIDI source (display only for now)
        bool midiSel = (_col == 2);
        canvas.setColor(midiSel ? Color::Bright : Color::Medium);
        canvas.drawText(208, gridTop + 5, "OMNI");
    }

    const char *outFooter[5] = { "RNG", "QNT", "OFFST", "ROOT", "NEXT" };
    const char *inFooter[5] = { "TRIG", "CV IN", "MIDI", "", "NEXT" };
    WindowPainter::drawFooter(canvas, _view == View::Outputs ? outFooter : inFooter, pageKeyState(), _col);
}

void TT2IoConfigPage::updateLeds(Leds &leds) {
    (void)leds;
}

void TT2IoConfigPage::keyPress(KeyPressEvent &event) {
    if (!isTt2()) return;
    const auto &key = event.key();

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }
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

void TT2IoConfigPage::contextShow(bool doubleClick) {
    showContextMenu(ContextMenu(
        sceneMenuItems,
        int(SceneAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return SceneAction(index) != SceneAction::Paste || _model.clipBoard().canPasteTrack(); },
        doubleClick
    ));
}

void TT2IoConfigPage::contextAction(int index) {
    switch (SceneAction(index)) {
    case SceneAction::Copy: _model.clipBoard().copyTrack(_project.selectedTrack()); break;
    case SceneAction::Paste: _model.clipBoard().pasteTrack(_project.selectedTrack()); break;
    case SceneAction::Load: loadScene(); break;
    case SceneAction::Save: saveScene(); break;
    case SceneAction::Last: break;
    }
}

void TT2IoConfigPage::loadScene() {
    _manager.pages().fileSelect.show("LOAD TT2 SCENE", FileType::TeletypeV2Program, 0, false, [this] (bool result, int slot) {
        if (result) {
            _manager.pages().confirmation.show("ARE YOU SURE?", [this, slot] (bool result) {
                if (result) loadSceneFromSlot(slot);
            });
        }
    });
}

void TT2IoConfigPage::saveScene() {
    _manager.pages().fileSelect.show("SAVE TT2 SCENE", FileType::TeletypeV2Program, 0, true, [this] (bool result, int slot) {
        if (result) {
            if (FileManager::slotUsed(FileType::TeletypeV2Program, slot)) {
                _manager.pages().confirmation.show("ARE YOU SURE?", [this, slot] (bool result) {
                    if (result) saveSceneToSlot(slot);
                });
            } else {
                saveSceneToSlot(slot);
            }
        }
    });
}

void TT2IoConfigPage::saveSceneToSlot(int slot) {
    _engine.suspend();
    _manager.pages().busy.show("SAVING TT2 SCENE ...");
    FileManager::task([this, slot] () {
        return FileManager::writeTt2Program(_project.selectedTrack().tt2Track(), nullptr, slot);
    }, [this] (fs::Error result) {
        if (result == fs::OK) {
            showMessage("TT2 SCENE SAVED");
        } else {
            showMessage(FixedStringBuilder<32>("FAILED (%s)", fs::errorToString(result)));
        }
        _manager.pages().busy.close();
        _engine.resume();
    });
}

void TT2IoConfigPage::loadSceneFromSlot(int slot) {
    _engine.suspend();
    _manager.pages().busy.show("LOADING TT2 SCENE ...");
    FileManager::task([this, slot] () {
        return FileManager::readTt2Program(_project.selectedTrack().tt2Track(), slot);
    }, [this] (fs::Error result) {
        if (result == fs::OK) {
            showMessage("TT2 SCENE LOADED");
        } else {
            showMessage(FixedStringBuilder<32>("FAILED (%s)", fs::errorToString(result)));
        }
        _manager.pages().busy.close();
        _engine.resume();
    });
}
