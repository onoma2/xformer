#include "TeletypePatternViewPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"

#include "core/math/Math.h"
#include "core/utils/StringBuilder.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

extern "C" {
#include "state.h"
}

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
    if (_project.selectedTrack().trackMode() != Track::TrackMode::Teletype) {
        close();
        return;
    }

    canvas.setColor(Color::None);
    canvas.fill();
    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);

    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();

    int16_t *varPtr = &state.variables.a;
    const char *varNames[] = { "A", "B", "C", "D", "X", "Y", "Z", "T" };
    const int varIndices[] = { 0, 2, 4, 6, 1, 3, 5, 7 };
    scene_turtle_t *turtle = ss_turtle_get(&state);
    const bool turtleShown = turtle_get_shown(turtle);
    const uint8_t turtleX = turtle_get_x(turtle);
    const uint8_t turtleY = turtle_get_y(turtle);

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
            char valueText[12];
            int16_t value = ss_get_pattern_val(&state, col, row);
            int16_t len = ss_get_pattern_len(&state, col);
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
            int16_t start = ss_get_pattern_start(&state, col);
            int16_t end = ss_get_pattern_end(&state, col);
            if (row >= start && row <= end) {
                int glyphX = kGridX + col * kColumnWidth + kColumnWidth - 2;
                canvas.setColor(valueColor);
                canvas.drawText(glyphX, y + kTextYOffset, "|");
            }

            // Playhead marker
            int16_t idx = ss_get_pattern_idx(&state, col);
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
            canvas.setColor(Color::Medium);
            canvas.drawText(kVarsX, y + kTextYOffset, label);
            char valueText[12];
            std::snprintf(valueText, sizeof(valueText), "%d", value);
            canvas.drawText(kVarsX + 6, y + kTextYOffset, valueText);
        }
    }
}

void TeletypePatternViewPage::updateLeds(Leds &leds) {
    (void)leds;
}

void TeletypePatternViewPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }

    if (key.isFunction()) {
        int fn = key.function();
        if (fn == 4) {
            _manager.pop();
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
        if (step >= 0 && step <= 8) {
            digit = step + 1;
        } else if (step == 9) {
            digit = 0;
        } else if (step == 13) {
            setLength();
            event.consume();
            return;
        } else if (step == 14) {
            setStart();
            event.consume();
            return;
        } else if (step == 15) {
            setEnd();
            event.consume();
            return;
        }
        if (digit >= 0) {
            insertDigit(digit);
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
    moveRow(event.value());
    event.consume();
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
    _row = clamp(_row + delta, 0, PATTERN_LENGTH - 1);
    ensureRowVisible();
    _editingNumber = false;
}

void TeletypePatternViewPage::ensureRowVisible() {
    if (_row < _offset) {
        _offset = _row;
    } else if (_row >= _offset + kRowCount) {
        _offset = _row - (kRowCount - 1);
    }
    _offset = clamp(_offset, 0, PATTERN_LENGTH - kRowCount);
}

void TeletypePatternViewPage::commitEdit() {
    if (!_editingNumber) {
        return;
    }
    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    int16_t value = clamp<int32_t>(_editBuffer, INT16_MIN, INT16_MAX);
    ss_set_pattern_val(&state, _patternIndex, _row, value);
    uint16_t len = ss_get_pattern_len(&state, _patternIndex);
    if (_row >= len && len < PATTERN_LENGTH) {
        ss_set_pattern_len(&state, _patternIndex, _row + 1);
    }
    syncPattern();
    _editingNumber = false;
}

void TeletypePatternViewPage::backspaceDigit() {
    if (!_editingNumber) {
        auto &track = _project.selectedTrack().teletypeTrack();
        scene_state_t &state = track.state();
        _editBuffer = ss_get_pattern_val(&state, _patternIndex, _row);
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
    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    const int16_t len = ss_get_pattern_len(&state, _patternIndex);
    const int16_t idx = _row;
    const int16_t value = ss_get_pattern_val(&state, _patternIndex, idx);
    const int16_t maxIndex = std::min<int16_t>(len, PATTERN_LENGTH - 1);

    if (maxIndex >= idx) {
        for (int16_t i = maxIndex; i > idx; --i) {
            int16_t v = ss_get_pattern_val(&state, _patternIndex, i - 1);
            ss_set_pattern_val(&state, _patternIndex, i, v);
        }
        if (len < PATTERN_LENGTH - 1) {
            ss_set_pattern_len(&state, _patternIndex, len + 1);
        }
    } else if (idx >= len && idx < PATTERN_LENGTH) {
        ss_set_pattern_len(&state, _patternIndex, idx + 1);
    }

    ss_set_pattern_val(&state, _patternIndex, idx, value);
    syncPattern();
    _editingNumber = false;
}

void TeletypePatternViewPage::deleteRow() {
    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    const int16_t len = ss_get_pattern_len(&state, _patternIndex);
    if (len <= 0) {
        return;
    }
    const int16_t idx = _row;
    int16_t newLen = len;
    if (idx < len) {
        for (int16_t i = idx; i < len - 1; ++i) {
            int16_t v = ss_get_pattern_val(&state, _patternIndex, i + 1);
            ss_set_pattern_val(&state, _patternIndex, i, v);
        }
        newLen = len - 1;
        ss_set_pattern_len(&state, _patternIndex, newLen);
    }
    int maxRow = std::max<int>(static_cast<int>(newLen) - 1, 0);
    _row = clamp(_row, 0, maxRow);
    ensureRowVisible();
    syncPattern();
    _editingNumber = false;
}

void TeletypePatternViewPage::negateValue() {
    if (_editingNumber) {
        _editBuffer = -_editBuffer;
        return;
    }
    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    _editBuffer = -ss_get_pattern_val(&state, _patternIndex, _row);
    _editingNumber = true;
}

void TeletypePatternViewPage::toggleTurtle() {
    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    scene_turtle_t *turtle = ss_turtle_get(&state);
    turtle_set_shown(turtle, !turtle_get_shown(turtle));
}

void TeletypePatternViewPage::setLength() {
    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    ss_set_pattern_len(&state, _patternIndex, _row + 1);
    syncPattern();
    _editingNumber = false;
}

void TeletypePatternViewPage::setStart() {
    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    ss_set_pattern_start(&state, _patternIndex, _row);
    syncPattern();
    _editingNumber = false;
}

void TeletypePatternViewPage::setEnd() {
    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    ss_set_pattern_end(&state, _patternIndex, _row);
    syncPattern();
    _editingNumber = false;
}

void TeletypePatternViewPage::syncPattern() {
    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    track.setPattern(_patternIndex, state.patterns[_patternIndex]);
}
