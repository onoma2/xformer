#include "TeletypeScriptViewPage.h"

#include "Pages.h"

#include "engine/TT2TrackEngine.h"
#include "engine/TT2Printer.h"
#include "engine/TT2ScriptLoader.h"
#include "ui/LedPainter.h"
#include "ui/MatrixMap.h"

#include "core/utils/StringBuilder.h"

#include <cstring>
#include <cstdint>
#include <cstdio>

#include "os/os.h"

extern "C" {
#include "command.h"
#include "teletype.h"
}

namespace {
constexpr int kLineCount = 6;
constexpr int kRowStartY = 4; // Right position do not touch!
constexpr int kRowStepY = 8;
constexpr int kEditLineY = 54;
constexpr int kLabelX = 4;
constexpr int kTextX = 16;
constexpr int kHudX = 192;       // live-value HUD region (script text clamps to its left)
constexpr int kHudColPitch = 8;
constexpr int kHudColW = 6;
// Trigger scripts get a function key each; metro/init are reached via [ ] nav.
constexpr int kTriggerScriptCount = 4;
} // namespace

void clampTextToWidth(Canvas &canvas, char *text, int maxWidth) {
    if (!text || maxWidth <= 0) {
        return;
    }
    int length = static_cast<int>(std::strlen(text));
    while (length > 0 && canvas.textWidth(text) > maxWidth) {
        text[--length] = '\0';
    }
}

TeletypeScriptViewPage::TeletypeScriptViewPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context) {
}

void TeletypeScriptViewPage::enter() {
    _scriptIndex = 0;
    _liveMode = true;
    _selectedLine = 0;
    _hasLiveResult = false;
    setEditBuffer("");
}

void TeletypeScriptViewPage::draw(Canvas &canvas) {
    if (_project.selectedTrack().trackMode() != Track::TrackMode::TeletypeV2) {
        close();
        return;
    }

    // Engine track mode may lag the model after a mode change; guard the cast.
    if (_engine.selectedTrackEngine().trackMode() != Track::TrackMode::TeletypeV2) {
        close();
        return;
    }

    canvas.setColor(Color::None);
    canvas.fill();
    canvas.setFont(_liveMode ? Font::Small : Font::Tele);
    canvas.setBlendMode(BlendMode::Set);

    auto &track = _project.selectedTrack().tt2Track();
    auto &program = track.program();
    const int scriptIndex = _scriptIndex;
    const uint8_t len = program.scripts[scriptIndex].length;

    FixedStringBuilder<4> scriptLabel;
    if (_liveMode) {
        scriptLabel("L");
    } else if (scriptIndex == TT2_METRO_SCRIPT) {
        scriptLabel("M");
    } else if (scriptIndex == TT2_INIT_SCRIPT) {
        scriptLabel("I");
    } else {
        scriptLabel("S%d", scriptIndex + 1);
    }
    canvas.setColor(Color::Medium);
    int scriptWidth = canvas.textWidth(scriptLabel);
    int scriptX = Width - 2 - scriptWidth;
    canvas.drawText(scriptX, 8, scriptLabel);

    if (_liveMode) {
        auto &trackEngine = _engine.selectedTrackEngine().as<TT2TrackEngine>();
        auto &runtime = track.runtime();
        const auto &output = trackEngine.output();
        bool slewActive = false;
        for (int i = 0; i < TT2_OUTPUT_CV_COUNT; ++i) {
            if (output.cv[i].remainingMs > 0) { slewActive = true; break; }
        }
        const int iconY = 8;
        int x = kLabelX;
        const char *icons[] = { "M", "S", "D", "St" };
        bool states[] = {
            runtime.variables.m_act && program.scripts[TT2_METRO_SCRIPT].length > 0,
            slewActive,
            runtime.delay.count > 0,
            runtime.stack.top > 0
        };
        for (int i = 0; i < 4; ++i) {
            canvas.setColor(states[i] ? Color::Bright : Color::Low);
            canvas.drawText(x, iconY, icons[i]);
            x += canvas.textWidth(icons[i]) + 4;
        }
    }

    const int maxTextWidth = kHudX - 2 - kTextX;
    for (int i = 0; i < kLineCount; ++i) {
        int y = kRowStartY + i * kRowStepY;
        char lineText[TT2_PRINT_LINE_MAX] = {};

        if (_liveMode) {
            if (i == 4) {
                if (_hasLiveResult) {
                    std::snprintf(lineText, sizeof(lineText), "%d", _liveResult);
                }
            } else if (i == 3 && _historyCount > 0 && _historyHead >= 0) {
                std::strncpy(lineText, _history[_historyHead], sizeof(lineText) - 1);
                lineText[sizeof(lineText) - 1] = '\0';
            } else {
                lineText[0] = '\0';
            }
        } else if (i < len) {
            tt2PrintCommand(program.scripts[scriptIndex].commands[i], lineText, sizeof(lineText));
        }

        if (_liveMode) {
            if (i == 3 && _historyCount > 0 && _historyHead >= 0) {
                canvas.setColor(Color::Low);
            } else {
                canvas.setColor(Color::Medium);
            }
        } else if (i < len && program.scripts[scriptIndex].commands[i].commented) {
            canvas.setColor(Color::Low);
        } else {
            canvas.setColor(i == _selectedLine ? Color::Bright : Color::Medium);
        }
        if (!_liveMode) {
            FixedStringBuilder<4> lineLabel("%d", i + 1);
            canvas.drawText(kLabelX, y + 4, lineLabel);
        }
        int textY = y + 4;
        if (_liveMode && i == 3 && _historyCount > 0 && _historyHead >= 0) {
            textY -= 4;
        }
        clampTextToWidth(canvas, lineText, maxTextWidth);
        canvas.drawText(kTextX, textY, lineText);
    }

    char editText[TT2_PRINT_LINE_MAX] = {};
    std::snprintf(editText, sizeof(editText), "> %s", _editBuffer);
    canvas.setColor(Color::Bright);
    const int maxEditWidth = Width - 2 - kLabelX;
    clampTextToWidth(canvas, editText, maxEditWidth);
    canvas.drawText(kLabelX, kEditLineY + 4, editText);

    int prefixWidth = canvas.textWidth("> ");
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
    const int maxCursorX = Width - 2;
    if (os::ticks() % os::time::ms(800) < os::time::ms(400) &&
        cursorX + cursorWidth <= maxCursorX) {
        canvas.setColor(Color::Medium);
        canvas.fillRect(cursorX, cursorY, cursorWidth - 1, kRowStepY - 1);
        canvas.setBlendMode(BlendMode::Sub);
        canvas.setColor(Color::Bright);
        canvas.drawText(cursorX, kEditLineY, cursorStr);
        canvas.setBlendMode(BlendMode::Set);
    }

    drawHud(canvas);
}

// Bipolar value bar: raw 0..16383, 8192 = center. Frame + inner level fill.
void TeletypeScriptViewPage::drawBipolarBar(Canvas &canvas, int x, int y, int w, int h, int raw, Color fill, Color outline) {
    canvas.setColor(outline);
    canvas.drawRect(x, y, w, h);
    int val = raw - 8192;
    int mag = val < 0 ? -val : val;
    int half = (h - 2) / 2;
    int bar = clamp(mag * half / 8192, 0, half);
    int cy = y + h / 2;
    if (bar > 0) {
        canvas.setColor(fill);
        if (val >= 0) {
            canvas.fillRect(x + 1, cy - bar, w - 2, bar);
        } else {
            canvas.fillRect(x + 1, cy + 1, w - 2, bar);
        }
    }
}

// Live-value HUD: CV(8) bars + TR(8) gate squares + TI(4) input squares +
// IN/PARAM/BUS bars. Activity readout only — no routing assignment (deferred).
void TeletypeScriptViewPage::drawHud(Canvas &canvas) {
    auto &track = _project.selectedTrack().tt2Track();
    auto &runtime = track.runtime();
    auto &trackEngine = _engine.selectedTrackEngine().as<TT2TrackEngine>();
    const auto &output = trackEngine.output();

    // CV row: 8 bipolar bars (live value).
    for (int i = 0; i < 8; ++i) {
        int x = kHudX + i * kHudColPitch;
        int raw = clamp(int(output.cv[i].currentQ >> 8), 0, 16383);
        drawBipolarBar(canvas, x, 12, kHudColW, 15, raw, Color::MediumBright, Color::Low);
    }
    // TR row: 8 gate squares (frame + inner surface lit when high).
    for (int i = 0; i < 8; ++i) {
        int x = kHudX + i * kHudColPitch;
        canvas.setColor(Color::Low);
        canvas.drawRect(x, 30, kHudColW, kHudColW);
        if (output.tr[i].level != 0) {
            canvas.setColor(Color::Bright);
            canvas.fillRect(x + 1, 31, kHudColW - 2, kHudColW - 2);
        }
    }
    // TI: 4 trigger-input squares, under columns 0-3 (left edge flush with top).
    for (int i = 0; i < TT2_TRIGGER_INPUT_COUNT; ++i) {
        int x = kHudX + i * kHudColPitch;
        canvas.setColor(Color::Low);
        canvas.drawRect(x, 43, kHudColW, kHudColW);
        if (trackEngine.inputState(i)) {
            canvas.setColor(Color::Bright);
            canvas.fillRect(x + 1, 44, kHudColW - 2, kHudColW - 2);
        }
    }
    // IN / PARAM (bright) + BUS x4 (dim); last BUS edge flush with the top row.
    drawBipolarBar(canvas, 225, 41, 4, 11, clamp(int(runtime.variables.in), 0, 16383), Color::MediumBright, Color::Low);
    drawBipolarBar(canvas, 230, 41, 4, 11, clamp(int(runtime.variables.param), 0, 16383), Color::MediumBright, Color::Low);
    static const int kBusX[4] = { 235, 240, 245, 250 };
    for (int i = 0; i < 4; ++i) {
        int raw = clamp(int((_engine.busCv(i) + 5.f) / 10.f * 16383.f), 0, 16383);
        drawBipolarBar(canvas, kBusX[i], 41, 4, 11, raw, Color::MediumLow, Color::Low);
    }
}

void TeletypeScriptViewPage::updateLeds(Leds &leds) {
    const bool shift = globalKeyState()[Key::Shift];
    const bool page = globalKeyState()[Key::Page];

    // Default mode: Character input + control keys
    // Steps 0-12: Character input (green)
    for (int i = 0; i < 13; ++i) {
        leds.set(MatrixMap::fromStep(i), false, true);
    }
    // Step 13: Backspace (red - destructive)
    leds.set(MatrixMap::fromStep(13), true, false);
    // Step 14: Space (yellow - control)
    leds.set(MatrixMap::fromStep(14), true, true);
    // Step 15: Commit+Advance (yellow - action)
    leds.set(MatrixMap::fromStep(15), true, true);

    // Show script indicator on function keys if in script edit mode
    if (!_liveMode) {
        LedPainter::drawSelectedSequenceSection(leds, 0);
    }

    // Shift mode: Symbol/command input
    if (shift && !page) {
        // Steps 0-7: Symbols (+/-, *//, =!, etc.)
        for (int i = 0; i < 8; ++i) {
            leds.set(MatrixMap::fromStep(i), true, true);
        }
        // Steps 8-15: Commands (CV, TR., PARAM, etc.)
        for (int i = 8; i < 16; ++i) {
            leds.set(MatrixMap::fromStep(i), false, true);
        }
    }

    // Override with Page mode actions (using unmask/mask pattern)
    if (page && !shift) {
        // Page mode: Clipboard/edit actions on steps 8-12 (no Comment; TT2 drops comments)
        const bool pageStepRed[8] = {
            true,   // Step 8: Copy (yellow)
            false,  // Step 9: Paste (green)
            true,   // Step 10: Duplicate (yellow)
            true,   // Step 11: Comment (red)
            true,   // Step 12: Delete (red)
            false,  // Step 13
            false,  // Step 14
            false   // Step 15
        };
        const bool pageStepGreen[8] = {
            true,           // Step 8: Copy (yellow)
            _hasClipboard,  // Step 9: Paste (green if valid)
            true,           // Step 10: Duplicate (yellow)
            false,          // Step 11: unused
            false,          // Step 12: Delete (red)
            false,          // Step 13
            false,          // Step 14
            false           // Step 15
        };

        for (int i = 0; i < 8; ++i) {
            int index = MatrixMap::fromStep(i + 8);
            leds.unmask(index);
            leds.set(index, pageStepRed[i], pageStepGreen[i]);
            leds.mask(index);
        }
    }
}

void TeletypeScriptViewPage::setLiveMode(bool enabled) {
    _liveMode = enabled;
    _hasLiveResult = false;
    _historyCursor = -1;
    if (!_liveMode) {
        loadEditBuffer(_selectedLine);
    }
}

void TeletypeScriptViewPage::keyPress(KeyPressEvent &event) {
    // Input can arrive at this (pushed) page before draw() closes it on a mode
    // change; bail before any tt2Track() deref on a non-TT2 track.
    if (_project.selectedTrack().trackMode() != Track::TrackMode::TeletypeV2) {
        return;
    }

    const auto &key = event.key();

    if (key.pageModifier()) {
        if (key.isLeft()) {
            recallHistory(-1);
            event.consume();
        } else if (key.isRight()) {
            recallHistory(1);
            event.consume();
        } else if (key.isStep()) {
            if (key.step() == 8) {
                copyLine();
                event.consume();
            } else if (key.step() == 9) {
                pasteLine();
                event.consume();
            } else if (key.step() == 10) {
                duplicateLine();
                event.consume();
            } else if (key.step() == 11) {
                commentLine();
                event.consume();
            } else if (key.step() == 12) {
                deleteLine();
                event.consume();
            }
        } else if (key.isFunction()) {
            int fn = key.function();
            if (key.shiftModifier()) {          // shift+page+F1..F4 -> fire S5..S8
                if (fn >= 0 && fn < kTriggerScriptCount &&
                    _engine.selectedTrackEngine().trackMode() == Track::TrackMode::TeletypeV2) {
                    _engine.selectedTrackEngine().as<TT2TrackEngine>().triggerScript(4 + fn);
                }
                event.consume();
            } else if (fn == 4) {               // page+F5 -> live/pattern
                if (_liveMode) {
                    setLiveMode(false);
                } else {
                    _manager.push(&_manager.pages().teletypePatternView);
                }
                event.consume();
            }
        }
        return;
    }

    if (key.isFunction()) {
        int fn = key.function();
        if (key.shiftModifier()) {              // shift+F1..F4 -> fire S1..S4
            if (fn >= 0 && fn < kTriggerScriptCount) {
                // Guard against race condition - engine may not be rebuilt yet
                if (_engine.selectedTrackEngine().trackMode() != Track::TrackMode::TeletypeV2) {
                    event.consume();
                    return;
                }
                _engine.selectedTrackEngine().as<TT2TrackEngine>().triggerScript(fn);
                event.consume();
                return;
            }
        }
        if (fn >= 0 && fn < 4) {                // F1..F4 cycle Sn <-> S(n+4)
            setScriptIndex(_scriptIndex == fn ? 4 + fn : fn);
            event.consume();
            return;
        }
        if (fn == 4) {                          // F5 cycle Metro <-> Init
            setScriptIndex(_scriptIndex == TT2_METRO_SCRIPT ? TT2_INIT_SCRIPT : TT2_METRO_SCRIPT);
            event.consume();
            return;
        }
    }

    if (key.isStep()) {
        handleStepKey(key.step(), key.shiftModifier());
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
        if (key.shiftModifier()) {
            commitLine();
        } else {
            loadEditBuffer(_selectedLine);
        }
        event.consume();
        return;
    }
}

void TeletypeScriptViewPage::encoder(EncoderEvent &event) {
    if (_project.selectedTrack().trackMode() != Track::TrackMode::TeletypeV2) {
        return;
    }
    if (globalKeyState()[Key::Shift]) {
        int next = _selectedLine + event.value();
        if (next < 0) {
            next = 0;
        } else if (next >= kLineCount) {
            next = kLineCount - 1;
        }
        if (next != _selectedLine) {
            loadEditBuffer(next);
        }
    } else {
        int steps = std::abs(event.value());
        if (steps == 0) {
            event.consume();
            return;
        }
        if (event.value() > 0) {
            for (int i = 0; i < steps; ++i) {
                moveCursorRight();
            }
        } else {
            for (int i = 0; i < steps; ++i) {
                moveCursorLeft();
            }
        }
    }
    event.consume();
}

void TeletypeScriptViewPage::handleStepKey(int step, bool shift) {
    if (step < 0 || step >= 16) {
        return;
    }

    // Handle special duplication functions for steps 14, 15, 16 (docs: 14, 15, 16)
    if (!shift && step >= 13 && step <= 15) {
        switch (step) {
            case 13: // Step 14 in docs - backspace
                backspace();
                return;
            case 14: // Step 15 in docs - insert space
                insertChar(' ');
                return;
            case 15: // Step 16 in docs - commit and advance
                commitLineAndAdvance();
                return;
        }
    }

    if (shift) {
        // Define 2-symbol rotation for steps 0-15 (Steps 1-16 in documentation)
        static const char *const kShiftMap[16][2] = {
            {"+", "-"},           // Step 0: +, -
            {"*", "/"},           // Step 1: *, /
            {"=", "!"},           // Step 2: =, !
            {"<", ">"},           // Step 3: <, >
            {"%", "^"},           // Step 4: %, ^
            {"&", "|"},           // Step 5: &, |
            {"$", "@"},           // Step 6: $, @
            {"?", ";"},           // Step 7: ?, ;
            {"CV", "CV.SLEW"},    // Step 8: CV, CV.SLEW
            {"TR.", "TR.TIME"},   // Step 9: TR., TR.TIME
            {"PARAM", "SCL"},     // Step 10: PARAM, SCL
            {"P.NEXT", "P.HERE"}, // Step 11: P.NEXT, P.HERE
            {"ELIF", "OTHER"},    // Step 12: ELIF, OTHER
            {"RRAND", "RND.P"},   // Step 13: RRAND, RND.P
            {"DRUNK", "WRAP"},    // Step 14: DRUNK, WRAP
            {"M.ACT", "M.RESET"}  // Step 15: M.ACT, M.RESET
        };
        static const int kShiftCount[16] = {
            2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
        };

        // Enable rotation for all steps 0-15 (Steps 1-16 in documentation)
        const uint32_t now = os::ticks();
        const bool canCycle = (_lastStepKey == step && _lastKeyShift &&
                               (now - _lastKeyTime) < os::time::ms(700));

        int index = 0;
        if (canCycle) {
            index = (_lastKeyIndex + 1) % kShiftCount[step];
            removeLastInsert(_lastInsertLength);
        }

        const char *token = kShiftMap[step][index];
        if (!token) {
            return;
        }
        const bool addSpace = std::strlen(token) > 1;
        insertText(token, addSpace);

        _lastStepKey = step;
        _lastKeyIndex = index;
        _lastKeyTime = now;
        _lastKeyShift = true;
        _lastInsertLength = int(std::strlen(token) + (addSpace ? 1 : 0));
        return;
    }

    static const char *const kBaseMap[16][3] = {
        { "1", "A", "B" },
        { "2", "C", "D" },
        { "3", "E", "F" },
        { "4", "G", "H" },
        { "5", "I", "J" },
        { "6", "K", "L" },
        { "7", "M", "N" },
        { "8", "O", "P" },
        { "9", "Q", "R" },
        { "0", "S", "T" },
        { ".", "U", "V" },
        { ":", "W", "X" },
        { ";", "Y", "Z" },
        { "CV", "CV.SLEW", "RRAND" },
        { "TR.P", "PARAM", "ELIF" },
        { "M.ACT", "P.NEXT", "BUS" }
    };
    static const int kBaseCount[16] = {
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
    };

    const uint32_t now = os::ticks();
    const bool canCycle = (_lastStepKey == step && !_lastKeyShift &&
                           (now - _lastKeyTime) < os::time::ms(700));

    int index = 0;
    if (canCycle) {
        index = (_lastKeyIndex + 1) % kBaseCount[step];
        removeLastInsert(_lastInsertLength);
    }

    const char *token = kBaseMap[step][index];
    if (!token) {
        return;
    }
    const bool addSpace = std::strlen(token) > 1;
    insertText(token, addSpace);

    _lastStepKey = step;
    _lastKeyIndex = index;
    _lastKeyTime = now;
    _lastInsertLength = int(std::strlen(token) + (addSpace ? 1 : 0));
    _lastKeyShift = false;
}

void TeletypeScriptViewPage::loadEditBuffer(int line) {
    _selectedLine = clamp(line, 0, kLineCount - 1);
    _editBuffer[0] = '\0';
    _cursor = 0;

    auto &track = _project.selectedTrack().tt2Track();
    auto &program = track.program();
    const int scriptIndex = _scriptIndex;
    const uint8_t len = program.scripts[scriptIndex].length;
    if (_selectedLine < len) {
        tt2PrintCommand(program.scripts[scriptIndex].commands[_selectedLine], _editBuffer, EditBufferSize);
        _cursor = int(std::strlen(_editBuffer));
    }
}

void TeletypeScriptViewPage::setScriptIndex(int scriptIndex) {
    if (scriptIndex < 0 || scriptIndex >= TT2_SCRIPT_COUNT) {
        return;
    }
    _liveMode = false;
    if (_scriptIndex != scriptIndex) {
        _scriptIndex = scriptIndex;
        loadEditBuffer(0);
    }
}

void TeletypeScriptViewPage::insertText(const char *text, bool addSpace) {
    if (!text) {
        return;
    }
    const int len = int(std::strlen(_editBuffer));
    const int insertLen = int(std::strlen(text)) + (addSpace ? 1 : 0);
    if (len + insertLen >= EditBufferSize) {
        return;
    }
    std::memmove(_editBuffer + _cursor + insertLen, _editBuffer + _cursor, len - _cursor + 1);
    std::memcpy(_editBuffer + _cursor, text, std::strlen(text));
    if (addSpace) {
        _editBuffer[_cursor + std::strlen(text)] = ' ';
    }
    _cursor += insertLen;
}

void TeletypeScriptViewPage::removeLastInsert(int count) {
    if (count <= 0 || _cursor < count) {
        return;
    }
    int len = int(std::strlen(_editBuffer));
    int start = _cursor - count;
    std::memmove(_editBuffer + start, _editBuffer + _cursor, len - _cursor + 1);
    _cursor = start;
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

    pushHistory(_editBuffer);

    if (_liveMode) {
        // Guard against race condition - engine may not be rebuilt yet
        if (_engine.selectedTrackEngine().trackMode() != Track::TrackMode::TeletypeV2) {
            showMessage("ENGINE NOT READY");
            return;
        }
        TT2Command lowered = {};
        if (!lowerCommand(parsed, lowered)) {
            showMessage("TOO LONG");
            return;
        }
        auto &trackEngine = _engine.selectedTrackEngine().as<TT2TrackEngine>();
        TT2EvalResult result = trackEngine.runLiveCommand(lowered);
        _hasLiveResult = result.stackSize > 0;
        if (_hasLiveResult) {
            _liveResult = result.value;
        }
        return;
    }

    auto &track = _project.selectedTrack().tt2Track();
    _engine.lock();
    setScriptCommand(track.program(), _scriptIndex, _selectedLine, _editBuffer);
    _engine.unlock();
    // Commit succeeded; no UI message per current workflow.
}

void TeletypeScriptViewPage::copyLine() {
    std::strncpy(_clipboard, _editBuffer, EditBufferSize - 1);
    _clipboard[EditBufferSize - 1] = '\0';
    _hasClipboard = true;
    showMessage("Line copied");
}

void TeletypeScriptViewPage::pasteLine() {
    if (!_hasClipboard) {
        return;
    }
    setEditBuffer(_clipboard);
    showMessage("Line pasted");
}

void TeletypeScriptViewPage::duplicateLine() {
    if (_liveMode) {
        return;
    }
    auto &track = _project.selectedTrack().tt2Track();
    auto &program = track.program();
    if (_selectedLine >= program.scripts[_scriptIndex].length) {
        return;
    }
    char lineBuffer[TT2_PRINT_LINE_MAX] = {};
    tt2PrintCommand(program.scripts[_scriptIndex].commands[_selectedLine], lineBuffer, sizeof(lineBuffer));
    _engine.lock();
    insertScriptCommand(program, _scriptIndex, _selectedLine + 1, lineBuffer);
    if (_selectedLine < kLineCount - 1) {
        _selectedLine += 1;
    }
    _engine.unlock();
    loadEditBuffer(_selectedLine);
}

void TeletypeScriptViewPage::commentLine() {
    if (_liveMode) {
        return;
    }
    auto &program = _project.selectedTrack().tt2Track().program();
    TT2Script &script = program.scripts[_scriptIndex];
    if (_selectedLine >= script.length) {
        return;
    }
    _engine.lock();
    uint8_t &c = script.commands[_selectedLine].commented;
    c = c ? 0 : 1;
    _engine.unlock();
}

void TeletypeScriptViewPage::deleteLine() {
    if (_liveMode) {
        return;
    }
    auto &track = _project.selectedTrack().tt2Track();
    auto &program = track.program();
    if (_selectedLine < program.scripts[_scriptIndex].length) {
        char lineBuffer[TT2_PRINT_LINE_MAX] = {};
        tt2PrintCommand(program.scripts[_scriptIndex].commands[_selectedLine], lineBuffer, sizeof(lineBuffer));
        setEditBuffer(lineBuffer);
        copyLine();
    }
    _engine.lock();
    deleteScriptCommand(program, _scriptIndex, _selectedLine);
    _engine.unlock();
    loadEditBuffer(_selectedLine);
    showMessage("Line deleted");
}

void TeletypeScriptViewPage::pushHistory(const char *line) {
    if (!line || line[0] == '\0') {
        return;
    }
    _historyHead = (_historyHead + 1) % HistorySize;
    std::strncpy(_history[_historyHead], line, EditBufferSize - 1);
    _history[_historyHead][EditBufferSize - 1] = '\0';
    if (_historyCount < HistorySize) {
        _historyCount += 1;
    }
    _historyCursor = -1;
}

void TeletypeScriptViewPage::recallHistory(int direction) {
    if (_historyCount == 0) {
        return;
    }
    const int oldest = (_historyHead - (_historyCount - 1) + HistorySize) % HistorySize;
    if (_historyCursor < 0) {
        _historyCursor = _historyHead;
    } else if (direction < 0) {
        if (_historyCursor != oldest) {
            _historyCursor = (_historyCursor - 1 + HistorySize) % HistorySize;
        }
    } else if (direction > 0) {
        if (_historyCursor != _historyHead) {
            _historyCursor = (_historyCursor + 1) % HistorySize;
        }
    }
    setEditBuffer(_history[_historyCursor]);
}

void TeletypeScriptViewPage::setEditBuffer(const char *text) {
    if (!text) {
        _editBuffer[0] = '\0';
        _cursor = 0;
        return;
    }
    std::strncpy(_editBuffer, text, EditBufferSize - 1);
    _editBuffer[EditBufferSize - 1] = '\0';
    _cursor = int(std::strlen(_editBuffer));
}

void TeletypeScriptViewPage::keyboard(KeyboardEvent &event) {
    if (_project.selectedTrack().trackMode() != Track::TrackMode::TeletypeV2) {
        return;
    }

    const uint8_t keycode = event.keycode();

    // F1-F8: fire S1-S8; F9: fire Metro
    if (!event.ctrl() && !event.alt() && !event.shift()) {
        if (keycode >= KeyboardEvent::KeyF1 && keycode <= KeyboardEvent::KeyF8) {
            const int scriptIdx = keycode - KeyboardEvent::KeyF1;  // F1 → 0, F2 → 1, ...
            if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::TeletypeV2) {
                auto &trackEngine = _engine.selectedTrackEngine().as<TT2TrackEngine>();
                trackEngine.triggerScript(scriptIdx);
            }
            event.consume();
            return;
        }
        if (keycode == KeyboardEvent::KeyF9) {
            if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::TeletypeV2) {
                auto &trackEngine = _engine.selectedTrackEngine().as<TT2TrackEngine>();
                trackEngine.triggerScript(TT2_METRO_SCRIPT);
            }
            event.consume();
            return;
        }
        if (keycode == KeyboardEvent::KeyF10) {
            if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::TeletypeV2) {
                auto &trackEngine = _engine.selectedTrackEngine().as<TT2TrackEngine>();
                trackEngine.triggerScript(TT2_INIT_SCRIPT);
            }
            event.consume();
            return;
        }
    }

    // Alt+F1-F8: edit S1-S8; Alt+F9: Metro; Alt+F10: Init
    if (event.alt() && !event.ctrl() && !event.shift()) {
        if (keycode >= KeyboardEvent::KeyF1 && keycode <= KeyboardEvent::KeyF8) {
            setScriptIndex(keycode - KeyboardEvent::KeyF1);
            event.consume();
            return;
        }
        if (keycode == KeyboardEvent::KeyF9) {
            setScriptIndex(TT2_METRO_SCRIPT);
            event.consume();
            return;
        }
        if (keycode == KeyboardEvent::KeyF10) {
            setScriptIndex(TT2_INIT_SCRIPT);
            event.consume();
            return;
        }
        if (keycode == 0x38) {  // HID_SLASH — toggle line comment
            commentLine();
            event.consume();
            return;
        }
    }

    if (event.ctrl()) {
        // Ctrl+F1-F8: mute/unmute trigger script; Ctrl+F9: stop/start metro
        if (keycode >= KeyboardEvent::KeyF1 && keycode <= KeyboardEvent::KeyF8) {
            if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::TeletypeV2) {
                _engine.selectedTrackEngine().as<TT2TrackEngine>().toggleScriptMute(keycode - KeyboardEvent::KeyF1);
            }
            event.consume();
            return;
        }
        if (keycode == KeyboardEvent::KeyF9) {
            if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::TeletypeV2) {
                _engine.selectedTrackEngine().as<TT2TrackEngine>().toggleMetroActive();
            }
            event.consume();
            return;
        }
        // Ctrl+shortcuts
        switch (keycode) {
        case KeyboardEvent::KeyHome:
            _cursor = 0;
            event.consume();
            return;
        case KeyboardEvent::KeyEnd:
            _cursor = int(std::strlen(_editBuffer));
            event.consume();
            return;
        case 0x06: // C
            copyLine();
            event.consume();
            return;
        case 0x19: // V
            pasteLine();
            event.consume();
            return;
        case 0x1B: // X
            copyLine();
            _editBuffer[0] = '\0';
            _cursor = 0;
            event.consume();
            return;
        default:
            return;
        }
    }

    // Special keys
    if (keycode == KeyboardEvent::KeyEnter) {
        if (event.shift()) {
            // Shift+Enter: commit then insert blank line
            commitLine();
            if (!_liveMode && _selectedLine < kLineCount - 1) {
                _selectedLine += 1;
                loadEditBuffer(_selectedLine);
            }
        } else {
            commitLineAndAdvance();
        }
        event.consume();
        return;
    }
    if (keycode == KeyboardEvent::KeyBackspace || keycode == KeyboardEvent::KeyDelete) {
        if (keycode == KeyboardEvent::KeyDelete) {
            // Delete: remove character after cursor
            int len = int(std::strlen(_editBuffer));
            if (_cursor < len) {
                std::memmove(_editBuffer + _cursor, _editBuffer + _cursor + 1, len - _cursor);
            }
        } else {
            backspace();
        }
        event.consume();
        return;
    }
    if (keycode == KeyboardEvent::KeyLeft) {
        moveCursorLeft();
        event.consume();
        return;
    }
    if (keycode == KeyboardEvent::KeyRight) {
        moveCursorRight();
        event.consume();
        return;
    }
    if (keycode == KeyboardEvent::KeyUp) {
        if (event.shift() || _liveMode) {
            recallHistory(-1);
        } else if (_selectedLine > 0) {
            loadEditBuffer(_selectedLine - 1);
        }
        event.consume();
        return;
    }
    if (keycode == KeyboardEvent::KeyDown) {
        if (event.shift() || _liveMode) {
            recallHistory(1);
        } else if (_selectedLine < kLineCount - 1) {
            loadEditBuffer(_selectedLine + 1);
        }
        event.consume();
        return;
    }
    if (keycode == KeyboardEvent::KeyEscape) {
        // Escape: clear edit buffer
        _editBuffer[0] = '\0';
        _cursor = 0;
        event.consume();
        return;
    }
    if (keycode == KeyboardEvent::KeyTab) {
        // Tab: insert space
        insertChar(' ');
        event.consume();
        return;
    }
    // [ and ] navigate scripts (matching hardware Teletype behavior)
    if (event.ch() == '[') {
        if (_scriptIndex > 0) {
            setScriptIndex(_scriptIndex - 1);
        }
        event.consume();
        return;
    }
    if (event.ch() == ']') {
        if (_scriptIndex < TT2_SCRIPT_COUNT - 1) {
            setScriptIndex(_scriptIndex + 1);
        }
        event.consume();
        return;
    }
    if (keycode == KeyboardEvent::KeyHome) {
        _cursor = 0;
        event.consume();
        return;
    }
    if (keycode == KeyboardEvent::KeyEnd) {
        _cursor = int(std::strlen(_editBuffer));
        event.consume();
        return;
    }

    // Printable character — convert letters to uppercase since the Teletype
    // parser only accepts uppercase tokens (CV, TR, etc.)
    char ch = event.ch();
    if (ch != 0) {
        if (ch >= 'a' && ch <= 'z') {
            ch = ch - 'a' + 'A';
        }
        insertChar(ch);
        event.consume();
    }

    // Forward unconsumed events (including modifier-only combos like Shift+Alt)
    // to BasePage for context menu detection.
    BasePage::keyboard(event);
}

void TeletypeScriptViewPage::commitLineAndAdvance() {
    commitLine();
    if (_liveMode) {
        _editBuffer[0] = '\0';
        _cursor = 0;
        return;
    }
    // Move to the next line if possible
    if (_selectedLine < kLineCount - 1) {
        loadEditBuffer(_selectedLine + 1);
    }
}
