#include "TeletypePatternViewPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/MatrixMap.h"
#include "ui/KeyboardManager.h"

#include "model/TT2Track.h"
#include "engine/TeletypeNativeOps.h"

#include "core/math/Math.h"
#include "core/utils/StringBuilder.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace {
constexpr int kRowCount = 8;
constexpr int kRowStartY = 4;
constexpr int kRowStepY = 7;
constexpr int kTextYOffset = 3;

constexpr int kGridX = -6;
constexpr int kGridWidth = 192;
constexpr int kColumnCount = 4;
constexpr int kColumnWidth = 48;
constexpr int kRowLabelX = kGridX + 8;
constexpr int kValueWidth = kColumnWidth - 6;
constexpr int kValueOffsetX = 2;

constexpr int kVarsX = kGridX + kGridWidth + 18;
} // namespace

TeletypePatternViewPage::TeletypePatternViewPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context) {
}

void TeletypePatternViewPage::enter() {
    _patternIndex = 0;
    _row = 0;
    _offset = 0;
    _editingNumber = false;
    _editBuffer = 0;
}

void TeletypePatternViewPage::draw(Canvas &canvas) {
    if (_project.selectedTrack().trackMode() != Track::TrackMode::TeletypeV2) {
        close();
        return;
    }

    canvas.setColor(Color::None);
    canvas.fill();
    canvas.setFont(Font::Tele);
    canvas.setBlendMode(BlendMode::Set);

    auto &track = _project.selectedTrack().tt2Track();
    auto &program = track.program();
    auto &runtime = track.runtime();

    int16_t *varPtr = &runtime.variables.a;
    const char *varNames[] = { "A", "B", "C", "D", "X", "Y", "Z", "T" };
    const int varIndices[] = { 0, 2, 4, 6, 1, 3, 5, 7 };
    const int varsValueRight = kVarsX + 18;
    const int varsDotX = varsValueRight + 2;
    const int varsLabelX = varsValueRight + 6;
    const bool turtleShown = runtime.turtle.shown != 0;
    const int turtleX = int(runtime.turtle.position.x);
    const int turtleY = int(runtime.turtle.position.y);

    for (int rowIndex = 0; rowIndex < kRowCount; ++rowIndex) {
        int row = _offset + rowIndex;
        int y = kRowStartY + rowIndex * kRowStepY;

        // Row number
        {
            char rowLabel[4];
            std::snprintf(rowLabel, sizeof(rowLabel), "%d", row);
            canvas.setColor(row == _row ? Color::Bright : Color::Medium);
            canvas.drawText(kRowLabelX, y + kTextYOffset, rowLabel);
        }

        // Pattern values
        for (int col = 0; col < kColumnCount; ++col) {
            const TT2Pattern &pat = program.patterns[col];
            char valueText[12];
            int16_t value = pat.val[row];
            int16_t len = int16_t(pat.len);
            if (_editingNumber && col == _patternIndex && row == _row) {
                std::snprintf(valueText, sizeof(valueText), "%d", int(_editBuffer));
            } else {
                std::snprintf(valueText, sizeof(valueText), "%d", value);
            }

            int textWidth = canvas.textWidth(valueText);
            int rightX = kGridX + col * kColumnWidth + kValueOffsetX + kValueWidth;
            int x = rightX - textWidth;
            Color baseColor = (row >= len) ? Color::Low : Color::Medium;
            Color valueColor = (col == _patternIndex && row == _row) ? Color::Bright : baseColor;
            canvas.setColor(valueColor);
            canvas.drawText(x, y + kTextYOffset, valueText);

            // Loop range indicator (dotted)
            int16_t start = pat.start;
            int16_t end = pat.end;
            if (row >= start && row <= end) {
                int glyphX = kGridX + col * kColumnWidth + kColumnWidth - 2;
                canvas.setColor(valueColor);
                canvas.drawText(glyphX, y + kTextYOffset, "|");
            }

            // Playhead marker
            int16_t idx = pat.idx;
            if (row == idx) {
                int headX = rightX;
                canvas.setColor(Color::Bright);
                canvas.point(headX, y + 2);
                canvas.point(headX, y + 3);
            }

            if (turtleShown && turtleX == col && turtleY == row) {
                int turtleDrawX = kGridX + col * kColumnWidth + 1;
                canvas.setColor(Color::Bright);
                canvas.drawText(turtleDrawX, y + kTextYOffset, "<");
            }
        }

        // Vars panel (8 rows)
        if (rowIndex < 8) {
            const char *label = varNames[rowIndex];
            int16_t value = varPtr[varIndices[rowIndex]];
            char valueText[12];
            std::snprintf(valueText, sizeof(valueText), "%d", value);
            int valueWidth = canvas.textWidth(valueText);
            canvas.setColor(Color::Medium);
            canvas.drawText(varsValueRight - valueWidth, y + kTextYOffset, valueText);
            canvas.drawText(varsDotX, y + kTextYOffset, ".");
            canvas.drawText(varsLabelX, y + kTextYOffset, label);
        }
    }
}

void TeletypePatternViewPage::updateLeds(Leds &leds) {
    const bool page = globalKeyState()[Key::Page];

    // Default mode: Digit input + control keys
    // Steps 0-9: Digit input (green) - maps to digits 1-9, 0
    for (int i = 0; i < 10; ++i) {
        leds.set(MatrixMap::fromStep(i), false, true);
    }
    // Steps 10-12: Unused (off)
    for (int i = 10; i < 13; ++i) {
        leds.set(MatrixMap::fromStep(i), false, false);
    }
    // Step 13: Backspace (red - destructive)
    leds.set(MatrixMap::fromStep(13), true, false);
    // Step 14: Unused (off)
    leds.set(MatrixMap::fromStep(14), false, false);
    // Step 15: Commit (yellow - action)
    leds.set(MatrixMap::fromStep(15), true, true);

    // Override with Page mode actions (using unmask/mask pattern)
    if (page) {
        // Page mode: Pattern structure controls on steps 4-6
        // Step 4: Set Length (yellow)
        // Step 5: Set Start (yellow)
        // Step 6: Set End (yellow)
        for (int i = 4; i <= 6; ++i) {
            int index = MatrixMap::fromStep(i);
            leds.unmask(index);
            leds.set(index, true, true);  // Yellow
            leds.mask(index);
        }
    }
}

void TeletypePatternViewPage::keyPress(KeyPressEvent &event) {
    // Input can arrive at this (pushed) page before draw() closes it on a mode
    // change; bail before any tt2Track() deref on a non-TT2 track.
    if (_project.selectedTrack().trackMode() != Track::TrackMode::TeletypeV2) {
        return;
    }

    const auto &key = event.key();

    if (key.pageModifier()) {
        if (key.isStep()) {
            int step = key.step();
            if (step == 4) {
                setLength();
                event.consume();
                return;
            } else if (step == 5) {
                setStart();
                event.consume();
                return;
            } else if (step == 6) {
                setEnd();
                event.consume();
                return;
            }
        }
        return;
    }

    if (key.isFunction()) {
        int fn = key.function();
        if (fn == 4) {
            _manager.pop();
            _manager.pages().teletypeScriptView.setLiveMode(true);
            event.consume();
            return;
        }
        if (fn >= 0 && fn < 4) {
            setPatternIndex(fn);
            event.consume();
            return;
        }
    }

    if (key.isLeft()) {
        if (key.shiftModifier()) {
            deleteRow();
        } else {
            backspaceDigit();
        }
        event.consume();
        return;
    }

    if (key.isRight()) {
        if (key.shiftModifier()) {
            toggleTurtle();
        } else {
            insertRow();
        }
        event.consume();
        return;
    }

    if (key.isStep()) {
        int step = key.step();
        int digit = -1;
        if (step >= 0 && step <= 9) {
            digit = (step == 9) ? 0 : step + 1;
        }
        if (digit >= 0) {
            insertDigit(digit);
            event.consume();
            return;
        }
        if (step == 13) {
            backspaceDigit();
            event.consume();
            return;
        } else if (step == 15) {
            commitEdit();
            event.consume();
            return;
        }
    }

    if (key.is(Key::Encoder)) {
        if (key.shiftModifier()) {
            commitEdit();
        } else {
            negateValue();
        }
        event.consume();
        return;
    }
}

void TeletypePatternViewPage::encoder(EncoderEvent &event) {
    if (_project.selectedTrack().trackMode() != Track::TrackMode::TeletypeV2) {
        return;
    }
    moveRow(event.value());
    event.consume();
}

void TeletypePatternViewPage::keyboard(KeyboardEvent &event) {
    if (_project.selectedTrack().trackMode() != Track::TrackMode::TeletypeV2) {
        return;
    }

    const uint8_t keycode = event.keycode();

    // Alt + track key (Q-I) selects the track; plain Q-I types in the editor.
    if (event.alt() && !event.ctrl() && !event.shift()) {
        int track = KeyboardManager::hidKeycodeToTrack(keycode);
        if (track >= 0) {
            _project.setSelectedTrackIndex(track);
            event.consume();
            return;
        }
    }

    // Bracket keys nudge the current cell (or the in-progress edit buffer). ch()
    // is nulled under Ctrl/Alt, so dispatch on the keycode + modifiers: plain =
    // +/-1 raw, Alt = +/-1 semitone, Ctrl = +/-fifth (7), Shift = +/-octave (12).
    if (keycode == 0x2F || keycode == 0x30) {
        int dir = (keycode == 0x30) ? 1 : -1;
        int interval = 0;
        if (event.alt() && !event.ctrl()) interval = dir;
        else if (event.ctrl()) interval = dir * 7;
        else if (event.shift()) interval = dir * 12;
        auto &pat = _project.selectedTrack().tt2Track().program().patterns[_patternIndex];
        if (interval != 0) {
            if (_editingNumber) {
                _editBuffer = tt2TransposeSemitones(int16_t(_editBuffer), interval);
            } else {
                _engine.lock();
                pat.val[_row] = tt2TransposeSemitones(pat.val[_row], interval);
                _engine.unlock();
            }
        } else {
            _engine.lock();
            int16_t v = pat.val[_row];
            pat.val[_row] = dir > 0 ? (v == INT16_MAX ? INT16_MIN : int16_t(v + 1))
                                    : (v == INT16_MIN ? INT16_MAX : int16_t(v - 1));
            _engine.unlock();
            _editingNumber = false;
        }
        event.consume();
        return;
    }

    // Alt+digit / Shift+Alt+digit: transpose the cell up/down by N semitones
    // (verbatim mapping: 0 = 10, 1 = 11, else the digit).
    if (event.alt() && !event.ctrl() && keycode >= 0x1E && keycode <= 0x27) {
        int n = keycode - 0x1E + 1;
        if (n == 1) n = 11;
        int interval = event.shift() ? -n : n;
        if (_editingNumber) {
            _editBuffer = tt2TransposeSemitones(int16_t(_editBuffer), interval);
        } else {
            auto &pat = _project.selectedTrack().tt2Track().program().patterns[_patternIndex];
            _engine.lock();
            pat.val[_row] = tt2TransposeSemitones(pat.val[_row], interval);
            _engine.unlock();
        }
        event.consume();
        return;
    }

    // Shift+L/S/E: set length/start/end to the cursor row.
    if (event.shift() && !event.alt() && !event.ctrl()) {
        if (keycode == 0x0F) { setLength(); event.consume(); return; }
        if (keycode == 0x16) { setStart(); event.consume(); return; }
        if (keycode == 0x08) { setEnd(); event.consume(); return; }
    }

    // Ctrl+shortcuts
    if (event.ctrl()) {
        switch (keycode) {
        case KeyboardEvent::KeyHome:
            _row = 0;
            _offset = 0;
            _editingNumber = false;
            event.consume();
            return;
        case KeyboardEvent::KeyEnd:
            _row = TT2_PATTERN_LENGTH - 1;
            ensureRowVisible();
            _editingNumber = false;
            event.consume();
            return;
        case 0x06: // C
            // Copy current cell value
            {
                auto &pat = _project.selectedTrack().tt2Track().program().patterns[_patternIndex];
                if (_editingNumber) {
                    _valueCopyBuffer = _editBuffer;
                } else {
                    _valueCopyBuffer = pat.val[_row];
                }
                showMessage("VALUE COPIED");
            }
            event.consume();
            return;
        case 0x19: // V
            // Paste value
            {
                auto &pat = _project.selectedTrack().tt2Track().program().patterns[_patternIndex];
                _engine.lock();
                pat.val[_row] = _valueCopyBuffer;
                _engine.unlock();
                _editingNumber = false;
            }
            event.consume();
            return;
        case 0x1B: // X
            // Cut: copy + delete
            {
                auto &pat = _project.selectedTrack().tt2Track().program().patterns[_patternIndex];
                _valueCopyBuffer = pat.val[_row];
                deleteRow();
                showMessage("VALUE CUT");
            }
            event.consume();
            return;
        default:
            return;
        }
    }

    // Arrow keys
    if (keycode == KeyboardEvent::KeyUp) {
        if (event.alt()) {
            // Alt+Up: page up
            _row = std::max(_row - 8, 0);
            ensureRowVisible();
        } else {
            moveRow(-1);
        }
        _editingNumber = false;
        event.consume();
        return;
    }
    if (keycode == KeyboardEvent::KeyDown) {
        if (event.alt()) {
            // Alt+Down: page down
            _row = std::min(_row + 8, TT2_PATTERN_LENGTH - 1);
            ensureRowVisible();
        } else {
            moveRow(1);
        }
        _editingNumber = false;
        event.consume();
        return;
    }
    if (keycode == KeyboardEvent::KeyLeft) {
        if (event.alt()) {
            setPatternIndex(0);  // Alt+Left: first pattern (hardware pattern_mode.c behavior)
        } else {
            setPatternIndex(std::max(_patternIndex - 1, 0));
        }
        _editingNumber = false;
        event.consume();
        return;
    }
    if (keycode == KeyboardEvent::KeyRight) {
        if (event.alt()) {
            setPatternIndex(kColumnCount - 1);  // Alt+Right: last pattern
        } else {
            setPatternIndex(std::min(_patternIndex + 1, kColumnCount - 1));
        }
        _editingNumber = false;
        event.consume();
        return;
    }

    // Enter
    if (keycode == KeyboardEvent::KeyEnter) {
        if (event.shift()) {
            insertRow();
        } else {
            commitEdit();
            // Move down after commit
            if (_row < TT2_PATTERN_LENGTH - 1) {
                _row++;
                ensureRowVisible();
            }
        }
        event.consume();
        return;
    }

    // Backspace
    if (keycode == KeyboardEvent::KeyBackspace) {
        backspaceDigit();
        event.consume();
        return;
    }

    // Delete
    if (keycode == KeyboardEvent::KeyDelete) {
        deleteRow();
        event.consume();
        return;
    }

    // Escape: cancel edit
    if (keycode == KeyboardEvent::KeyEscape) {
        _editingNumber = false;
        event.consume();
        return;
    }

    // Space: toggle between 0 and 1
    if (keycode == KeyboardEvent::KeySpace) {
        auto &pat = _project.selectedTrack().tt2Track().program().patterns[_patternIndex];
        _engine.lock();
        pat.val[_row] = pat.val[_row] ? 0 : 1;
        _engine.unlock();
        _editingNumber = false;
        event.consume();
        return;
    }

    // Minus/underscore: negate
    if (event.ch() == '-' || event.ch() == '_') {
        negateValue();
        event.consume();
        return;
    }

    // Digit input (0-9)
    char ch = event.ch();
    if (ch >= '0' && ch <= '9') {
        insertDigit(ch - '0');
        event.consume();
        return;
    }

    // Forward unconsumed events (including modifier-only combos like Shift+Alt)
    // to BasePage for context menu detection.
    BasePage::keyboard(event);
}

void TeletypePatternViewPage::setPatternIndex(int pattern) {
    if (pattern < 0 || pattern >= kColumnCount) {
        return;
    }
    _patternIndex = pattern;
    _editingNumber = false;
}

void TeletypePatternViewPage::moveRow(int delta) {
    if (delta == 0) {
        return;
    }
    _row = clamp(_row + delta, 0, TT2_PATTERN_LENGTH - 1);
    ensureRowVisible();
    _editingNumber = false;
}

void TeletypePatternViewPage::ensureRowVisible() {
    if (_row < _offset) {
        _offset = _row;
    } else if (_row >= _offset + kRowCount) {
        _offset = _row - (kRowCount - 1);
    }
    _offset = clamp(_offset, 0, TT2_PATTERN_LENGTH - kRowCount);
}

void TeletypePatternViewPage::commitEdit() {
    if (!_editingNumber) {
        return;
    }
    auto &pat = _project.selectedTrack().tt2Track().program().patterns[_patternIndex];
    int16_t value = clamp<int32_t>(_editBuffer, INT16_MIN, INT16_MAX);
    _engine.lock();
    pat.val[_row] = value;
    if (_row >= int(pat.len) && pat.len < TT2_PATTERN_LENGTH) {
        pat.len = uint16_t(_row + 1);
    }
    _engine.unlock();
    _editingNumber = false;
}

void TeletypePatternViewPage::backspaceDigit() {
    if (!_editingNumber) {
        auto &pat = _project.selectedTrack().tt2Track().program().patterns[_patternIndex];
        _editBuffer = pat.val[_row];
        _editingNumber = true;
    }
    _editBuffer /= 10;
}

void TeletypePatternViewPage::insertDigit(int digit) {
    if (!_editingNumber) {
        _editingNumber = true;
        _editBuffer = 0;
    }
    int32_t next = _editBuffer * 10 + digit;
    if (next <= INT16_MAX && next >= INT16_MIN) {
        _editBuffer = next;
    }
}

void TeletypePatternViewPage::insertRow() {
    auto &pat = _project.selectedTrack().tt2Track().program().patterns[_patternIndex];
    const int idx = _row;

    _engine.lock();
    const int len = int(pat.len);
    const int16_t value = pat.val[idx];
    const int maxIndex = std::min<int>(len, TT2_PATTERN_LENGTH - 1);

    if (maxIndex >= idx) {
        for (int i = maxIndex; i > idx; --i) {
            pat.val[i] = pat.val[i - 1];
        }
        if (len < TT2_PATTERN_LENGTH - 1) {
            pat.len = uint16_t(len + 1);
        }
    } else if (idx >= len && idx < TT2_PATTERN_LENGTH) {
        pat.len = uint16_t(idx + 1);
    }

    pat.val[idx] = value;
    _engine.unlock();
    _editingNumber = false;
}

void TeletypePatternViewPage::deleteRow() {
    auto &pat = _project.selectedTrack().tt2Track().program().patterns[_patternIndex];
    const int idx = _row;

    _engine.lock();
    const int len = int(pat.len);
    if (len <= 0) {
        _engine.unlock();
        return;
    }
    int newLen = len;
    if (idx < len) {
        for (int i = idx; i < len - 1; ++i) {
            pat.val[i] = pat.val[i + 1];
        }
        newLen = len - 1;
        pat.len = uint16_t(newLen);
    }
    _engine.unlock();
    int maxRow = std::max<int>(newLen - 1, 0);
    _row = clamp(_row, 0, maxRow);
    ensureRowVisible();
    _editingNumber = false;
}

void TeletypePatternViewPage::negateValue() {
    if (_editingNumber) {
        _editBuffer = -_editBuffer;
        return;
    }
    auto &pat = _project.selectedTrack().tt2Track().program().patterns[_patternIndex];
    _editBuffer = -pat.val[_row];
    _editingNumber = true;
}

void TeletypePatternViewPage::toggleTurtle() {
    auto &runtime = _project.selectedTrack().tt2Track().runtime();
    _engine.lock();
    runtime.turtle.shown = runtime.turtle.shown ? 0 : 1;
    _engine.unlock();
}

void TeletypePatternViewPage::setLength() {
    auto &pat = _project.selectedTrack().tt2Track().program().patterns[_patternIndex];
    _engine.lock();
    pat.len = uint16_t(clamp(_row + 1, 0, TT2_PATTERN_LENGTH));
    _engine.unlock();
    _editingNumber = false;
}

void TeletypePatternViewPage::setStart() {
    auto &pat = _project.selectedTrack().tt2Track().program().patterns[_patternIndex];
    _engine.lock();
    pat.start = int16_t(_row);
    _engine.unlock();
    _editingNumber = false;
}

void TeletypePatternViewPage::setEnd() {
    auto &pat = _project.selectedTrack().tt2Track().program().patterns[_patternIndex];
    _engine.lock();
    pat.end = int16_t(_row);
    _engine.unlock();
    _editingNumber = false;
}
