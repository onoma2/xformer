#include "TeletypeScriptViewPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"

#include <cstring>

#include "os/os.h"

extern "C" {
#include "command.h"
#include "state.h"
#include "teletype.h"
}

namespace {
constexpr int kLineCount = 6;
constexpr int kRowStartY = 4; // Right position do not touch!
constexpr int kRowStepY = 8;
constexpr int kEditLineY = 54;
constexpr int kLabelX = 4;
constexpr int kTextX = 16;
} // namespace

TeletypeScriptViewPage::TeletypeScriptViewPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context) {
}

void TeletypeScriptViewPage::enter() {
    loadEditBuffer(0);
}

void TeletypeScriptViewPage::draw(Canvas &canvas) {
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
    const int scriptIndex = 0; // S0
    const uint8_t len = ss_get_script_len(&state, scriptIndex);

    FixedStringBuilder<4> scriptLabel("%d", scriptIndex);
    canvas.setColor(Color::Medium);
    int scriptX = Width - 2 - canvas.textWidth(scriptLabel);
    canvas.drawText(scriptX, 6, scriptLabel);

    for (int i = 0; i < kLineCount; ++i) {
        int y = kRowStartY + i * kRowStepY;
        char lineText[128] = {};

        if (i < len) {
            const tele_command_t *cmd = ss_get_script_command(&state, scriptIndex, i);
            if (cmd) {
                print_command(cmd, lineText);
            }
        }

        FixedStringBuilder<4> lineLabel("%d", i + 1);
        canvas.setColor(i == _selectedLine ? Color::Bright : Color::Medium);
        canvas.drawText(kLabelX, y + 4, lineLabel);
        canvas.drawText(kTextX, y + 4, lineText);
    }

    FixedStringBuilder<128> editLine("E %s", _editBuffer);
    canvas.setColor(Color::Bright);
    canvas.drawText(kLabelX, kEditLineY + 4, editLine);

    int prefixWidth = canvas.textWidth("E ");
    int cursorOffset = 0;
    for (int i = 0; i < _cursor; ++i) {
        const char str[2] = { _editBuffer[i] == '\0' ? ' ' : _editBuffer[i], '\0' };
        cursorOffset += canvas.textWidth(str);
    }
    const char cursorChar = _editBuffer[_cursor] == '\0' ? ' ' : _editBuffer[_cursor];
    const char cursorStr[2] = { cursorChar, '\0' };
    int cursorWidth = canvas.textWidth(cursorStr);
    if (cursorWidth <= 0) {
        cursorWidth = canvas.textWidth(" ");
    }
    int cursorX = kLabelX + prefixWidth + cursorOffset;
    int cursorY = kEditLineY;
    if (os::ticks() % os::time::ms(600) < os::time::ms(300)) {
        canvas.setColor(Color::Medium);
        canvas.fillRect(cursorX, cursorY, cursorWidth - 1, kRowStepY - 1);
        canvas.setBlendMode(BlendMode::Sub);
        canvas.setColor(Color::Bright);
        canvas.drawText(cursorX, kEditLineY + 4, cursorStr);
        canvas.setBlendMode(BlendMode::Set);
    }
}

void TeletypeScriptViewPage::updateLeds(Leds &leds) {
    LedPainter::drawSelectedSequenceSection(leds, 0);
}

void TeletypeScriptViewPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }

    if (key.isFunction() && key.function() == 4) {
        _manager.pop();
        event.consume();
        return;
    }

    if (key.isLeft()) {
        if (key.shiftModifier()) {
            moveCursorLeft();
        } else {
            backspace();
        }
        event.consume();
        return;
    }

    if (key.isRight()) {
        if (key.shiftModifier()) {
            moveCursorRight();
        } else {
            insertChar(' ');
        }
        event.consume();
        return;
    }

    if (key.is(Key::Encoder)) {
        commitLine();
        event.consume();
        return;
    }
}

void TeletypeScriptViewPage::encoder(EncoderEvent &event) {
    int next = _selectedLine + event.value();
    if (next < 0) {
        next = 0;
    } else if (next >= kLineCount) {
        next = kLineCount - 1;
    }
    if (next != _selectedLine) {
        loadEditBuffer(next);
    }
    event.consume();
}

void TeletypeScriptViewPage::loadEditBuffer(int line) {
    _selectedLine = clamp(line, 0, kLineCount - 1);
    _editBuffer[0] = '\0';
    _cursor = 0;

    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    const int scriptIndex = 0; // S0
    const uint8_t len = ss_get_script_len(&state, scriptIndex);
    if (_selectedLine < len) {
        if (const tele_command_t *cmd = ss_get_script_command(&state, scriptIndex, _selectedLine)) {
            print_command(cmd, _editBuffer);
            _cursor = int(std::strlen(_editBuffer));
        }
    }
}

void TeletypeScriptViewPage::insertChar(char c) {
    int len = int(std::strlen(_editBuffer));
    if (len >= EditBufferSize - 1) {
        return;
    }
    std::memmove(_editBuffer + _cursor + 1, _editBuffer + _cursor, len - _cursor + 1);
    _editBuffer[_cursor] = c;
    _cursor += 1;
}

void TeletypeScriptViewPage::backspace() {
    if (_cursor <= 0) {
        return;
    }
    int len = int(std::strlen(_editBuffer));
    _cursor -= 1;
    std::memmove(_editBuffer + _cursor, _editBuffer + _cursor + 1, len - _cursor);
}

void TeletypeScriptViewPage::moveCursorLeft() {
    if (_cursor > 0) {
        _cursor -= 1;
    }
}

void TeletypeScriptViewPage::moveCursorRight() {
    if (_editBuffer[_cursor] != '\0') {
        _cursor += 1;
    }
}

void TeletypeScriptViewPage::commitLine() {
    if (_editBuffer[0] == '\0') {
        showMessage("EMPTY");
        return;
    }

    tele_command_t parsed = {};
    char errorMsg[TELE_ERROR_MSG_LENGTH] = {};
    tele_error_t error = parse(_editBuffer, &parsed, errorMsg);
    if (error != E_OK) {
        showMessage(tele_error(error));
        return;
    }
    error = validate(&parsed, errorMsg);
    if (error != E_OK) {
        showMessage(tele_error(error));
        return;
    }

    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    const int scriptIndex = 0; // S0
    ss_overwrite_script_command(&state, scriptIndex, _selectedLine, &parsed);
    // Commit succeeded; no UI message per current workflow.
}
