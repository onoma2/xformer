#include "TeletypeScriptViewPage.h"

#include "Pages.h"

#include "engine/TeletypeBridge.h"
#include "engine/TeletypeTrackEngine.h"
#include "model/FileManager.h"
#include "ui/LedPainter.h"

#include "core/utils/StringBuilder.h"

#include <cstring>
#include <cstdint>

#include "os/os.h"

extern "C" {
#include "command.h"
#include "script.h"
#include "state.h"
#include "teletype.h"
#include "teletype_io.h"
}

namespace {
constexpr int kLineCount = 6;
constexpr int kRowStartY = 4; // Right position do not touch!
constexpr int kRowStepY = 8;
constexpr int kEditLineY = 54;
constexpr int kLabelX = 4;
constexpr int kTextX = 16;
constexpr int kGridBusX = 196;
constexpr int kGridMainX = 214;
constexpr int kGridInParamX = 246;
constexpr int kGridY = 15;
constexpr int kGridColW = 8;
constexpr int kGridRowH = 8;
// Flip to false for hardware testing without engine suspend.
constexpr bool kSuspendEngineForScriptIO = true;
} // namespace

enum class ContextAction {
    Init,
    Load,
    Save,
    SaveAs,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
    { "LOAD" },
    { "SAVE" },
    { "SAVE AS" },
};

TeletypeScriptViewPage::TeletypeScriptViewPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context) {
}

void TeletypeScriptViewPage::enter() {
    _scriptIndex = 0;
    _liveMode = true;
    _selectedLine = 0;
    _hasLiveResult = false;
    _scriptSlot = 0;
    _scriptSlotAssigned = false;
    setEditBuffer("");
}

void TeletypeScriptViewPage::draw(Canvas &canvas) {
    if (_project.selectedTrack().trackMode() != Track::TrackMode::Teletype) {
        close();
        return;
    }

    // CRITICAL: Check engine track mode too - model may have changed but engines not rebuilt yet
    if (_engine.selectedTrackEngine().trackMode() != Track::TrackMode::Teletype) {
        close();
        return;
    }

    canvas.setColor(Color::None);
    canvas.fill();
    canvas.setFont(_liveMode ? Font::Small : Font::Tele);
    canvas.setBlendMode(BlendMode::Set);

    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    const int scriptIndex = _scriptIndex;
    const uint8_t len = ss_get_script_len(&state, scriptIndex);

    FixedStringBuilder<4> scriptLabel;
    if (_liveMode) {
        scriptLabel("L");
    } else if (scriptIndex == METRO_SCRIPT) {
        scriptLabel("M");
    } else {
        scriptLabel("S%d", scriptIndex + 1);
    }
    canvas.setColor(Color::Medium);
    int scriptWidth = canvas.textWidth(scriptLabel);
    int scriptX = Width - 2 - scriptWidth;
    if (scriptIndex == 0 || scriptIndex == METRO_SCRIPT) {
        FixedStringBuilder<4> slotLabel("P%d", track.activePatternSlot() + 1);
        int slotWidth = canvas.textWidth(slotLabel);
        int slotX = scriptX - slotWidth - 4;
        canvas.drawText(slotX, 8, slotLabel);
    }
    canvas.drawText(scriptX, 8, scriptLabel);

    if (_liveMode) {
        auto &trackEngine = _engine.selectedTrackEngine().as<TeletypeTrackEngine>();
        const int iconY = 8;
        int x = kLabelX;
        const char *icons[] = { "M", "S", "D", "St" };
        bool states[] = {
            state.variables.m_act && ss_get_script_len(&state, METRO_SCRIPT) > 0,
            trackEngine.anyCvSlewActive(),
            TeletypeBridge::hasDelays(),
            TeletypeBridge::hasStack()
        };
        for (int i = 0; i < 4; ++i) {
            canvas.setColor(states[i] ? Color::Bright : Color::Low);
            canvas.drawText(x, iconY, icons[i]);
            x += canvas.textWidth(icons[i]) + 4;
        }
    }

    for (int i = 0; i < kLineCount; ++i) {
        int y = kRowStartY + i * kRowStepY;
        char lineText[128] = {};

        if (_liveMode) {
            if (i == 4 && _hasLiveResult) {
                std::snprintf(lineText, sizeof(lineText), "%d", _liveResult);
            } else if (i == 3 && _historyCount > 0 && _historyHead >= 0) {
                std::strncpy(lineText, _history[_historyHead], sizeof(lineText) - 1);
                lineText[sizeof(lineText) - 1] = '\0';
            } else {
                lineText[0] = '\0';
            }
        } else if (i < len) {
            const tele_command_t *cmd = ss_get_script_command(&state, scriptIndex, i);
            if (cmd) {
                print_command(cmd, lineText);
            }
        }

        if (_liveMode) {
            if (i == 3 && _historyCount > 0 && _historyHead >= 0) {
                canvas.setColor(Color::Low);
            } else {
                canvas.setColor(Color::Medium);
            }
        } else if (ss_get_script_comment(&state, scriptIndex, i)) {
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
        canvas.drawText(kTextX, textY, lineText);
    }

    FixedStringBuilder<128> editLine("> %s", _editBuffer);
    canvas.setColor(Color::Bright);
    canvas.drawText(kLabelX, kEditLineY + 4, editLine);

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
    if (os::ticks() % os::time::ms(800) < os::time::ms(400)) {
        canvas.setColor(Color::Medium);
        canvas.fillRect(cursorX, cursorY, cursorWidth - 1, kRowStepY - 1);
        canvas.setBlendMode(BlendMode::Sub);
        canvas.setColor(Color::Bright);
        canvas.drawText(cursorX, kEditLineY, cursorStr);
        canvas.setBlendMode(BlendMode::Set);
    }

    drawIoGrid(canvas);
}

void TeletypeScriptViewPage::drawIoGrid(Canvas &canvas) {
    const int trackIndex = _project.selectedTrackIndex();
    const auto &track = _project.selectedTrack().teletypeTrack();
    const auto &trackEngine = _engine.selectedTrackEngine().as<TeletypeTrackEngine>();

    // --- BUS BLOCK (x=196) ---
    for (int i = 0; i < 4; ++i) {
        int x = kGridBusX + (i % 2) * kGridColW;
        int y = kGridY + (i / 2) * 16;
        float volts = trackEngine.busCv(i);
        uint16_t raw = clamp(int((volts + 5.0f) / 10.0f * 16383.0f), 0, 16383);
        drawBipolarBar(canvas, x, y, raw, Color::MediumBright, Color::Low);
    }

    // --- MAIN GRID (x=214) ---
    auto gateSlotForPhysical = [this, trackIndex] (int gateOutIndex) -> int {
        int slot = 0;
        const auto &gateOutputTracks = _project.gateOutputTracks();
        for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
            if (gateOutputTracks[i] == trackIndex) {
                if (i == gateOutIndex) {
                    return slot;
                }
                ++slot;
            }
        }
        return -1;
    };
    auto cvSlotForPhysical = [this, trackIndex] (int cvOutIndex) -> int {
        int slot = 0;
        const auto &cvOutputTracks = _project.cvOutputTracks();
        for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
            if (cvOutputTracks[i] == trackIndex) {
                if (i == cvOutIndex) {
                    return slot;
                }
                ++slot;
            }
        }
        return -1;
    };
    for (int i = 0; i < 4; ++i) {
        int x = kGridMainX + i * kGridColW;

        // Row 1: TI
        bool tiAssigned = track.triggerInputSource(i) != TeletypeTrack::TriggerInputSource::None;
        bool tiActive = trackEngine.inputState(i);
        canvas.setColor(tiAssigned ? (tiActive ? Color::Bright : Color::Medium) : Color::Low);
        canvas.fillRect(x + 1, kGridY + 1, 6, 6);

        // Row 2: TO
        auto toDest = track.triggerOutputDest(i);
        int gateIdx = int(toDest);
        bool toOwned = _project.gateOutputTrack(gateIdx) == trackIndex;
        int gateSlot = gateSlotForPhysical(gateIdx);
        bool toActive = gateSlot >= 0 ? trackEngine.gateOutput(gateSlot) : false;
        if (toOwned) {
            canvas.setColor(toActive ? Color::Bright : Color::Medium);
            canvas.fillRect(x + 1, kGridY + kGridRowH + 1, 6, 6);
        } else {
            // Outline indicates layout mismatch (mapped, but not owned by this track).
            canvas.setColor(Color::MediumLow);
            canvas.drawRect(x + 1, kGridY + kGridRowH + 1, 6, 6);
            if (toActive) {
                canvas.fillRect(x + 2, kGridY + kGridRowH + 2, 4, 4);
            }
        }

        // Row 3: CV
        auto cvDest = track.cvOutputDest(i);
        int cvIdx = int(cvDest);
        bool cvOwned = _project.cvOutputTrack(cvIdx) == trackIndex;
        uint16_t cvRaw = trackEngine.cvRaw(i);
        Color cvFill = cvOwned ? Color::MediumBright : Color::MediumLow;
        Color cvOutline = cvOwned ? cvFill : Color::MediumLow;
        drawBipolarBar(canvas, x, kGridY + kGridRowH * 2, cvRaw, cvFill, cvOutline);
    }

    // --- IN/PARAM COLUMN (x=246) ---
    // IN (Top)
    bool inAssigned = track.cvInSource() != TeletypeTrack::CvInputSource::None;
    drawBipolarBar(canvas, kGridInParamX, kGridY, static_cast<uint16_t>(track.state().variables.in), inAssigned ? Color::MediumBright : Color::Low, Color::Low);
    // PARAM (Bot)
    bool paramAssigned = track.cvParamSource() != TeletypeTrack::CvInputSource::None;
    drawBipolarBar(canvas, kGridInParamX, kGridY + 16, static_cast<uint16_t>(track.state().variables.param), paramAssigned ? Color::MediumBright : Color::Low, Color::Low);
}

void TeletypeScriptViewPage::drawBipolarBar(Canvas &canvas, int x, int y, uint16_t value, Color fillColor, Color outlineColor) {
    canvas.setColor(outlineColor);
    canvas.drawRect(x + 1, y + 1, 6, 14);

    int32_t val = int32_t(value) - 8192;
    int h = (std::abs(val) * 7) / 8192;
    h = clamp(h, 0, 7);

    int centerY = y + 7;

    if (h > 0) {
        canvas.setColor(fillColor);
        if (val >= 0) {
            canvas.fillRect(x + 2, centerY - h + 1, 4, h);
        } else {
            canvas.fillRect(x + 2, centerY + 1, 4, h);
        }
    }
}

void TeletypeScriptViewPage::updateLeds(Leds &leds) {
    LedPainter::drawSelectedSequenceSection(leds, 0);
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
    const auto &key = event.key();

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

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
        }
        return;
    }

    if (key.isFunction()) {
        int fn = key.function();
        if (key.shiftModifier()) {
            if (fn >= 0 && fn < TeletypeTrack::ScriptSlotCount) {
                // Guard against race condition - engine may not be rebuilt yet
                if (_engine.selectedTrackEngine().trackMode() != Track::TrackMode::Teletype) {
                    event.consume();
                    return;
                }
                auto &trackEngine = _engine.selectedTrackEngine().as<TeletypeTrackEngine>();
                trackEngine.triggerScript(fn);
                event.consume();
                return;
            }
        }
        if (fn == 4) {
            if (_liveMode) {
                setLiveMode(false);
            } else {
                _manager.push(&_manager.pages().teletypePatternView);
            }
            event.consume();
            return;
        }
        if (fn == 0) {
            setScriptIndex(0);
            event.consume();
            return;
        }
        if (fn == 3) {
            setScriptIndex(_scriptIndex == METRO_SCRIPT ? 3 : METRO_SCRIPT);
            event.consume();
            return;
        }
        if (fn >= 0 && fn < TeletypeTrack::EditableScriptCount) {
            setScriptIndex(fn);
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

void TeletypeScriptViewPage::contextShow() {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); }
    ));
}

void TeletypeScriptViewPage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Init:
        break;
    case ContextAction::Load:
        loadScript();
        break;
    case ContextAction::Save:
        saveScript();
        break;
    case ContextAction::SaveAs:
        saveScriptAs();
        break;
    case ContextAction::Last:
        break;
    }
}

bool TeletypeScriptViewPage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    case ContextAction::Load:
    case ContextAction::Save:
    case ContextAction::SaveAs:
        return FileManager::volumeMounted();
    default:
        return true;
    }
}

void TeletypeScriptViewPage::encoder(EncoderEvent &event) {
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

    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    const int scriptIndex = _scriptIndex;
    const uint8_t len = ss_get_script_len(&state, scriptIndex);
    if (_selectedLine < len) {
        if (const tele_command_t *cmd = ss_get_script_command(&state, scriptIndex, _selectedLine)) {
            print_command(cmd, _editBuffer);
            _cursor = int(std::strlen(_editBuffer));
        }
    }
}

void TeletypeScriptViewPage::setScriptIndex(int scriptIndex) {
    if (scriptIndex < 0 || scriptIndex >= TeletypeTrack::EditableScriptCount) {
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
        if (_engine.selectedTrackEngine().trackMode() != Track::TrackMode::Teletype) {
            showMessage("ENGINE NOT READY");
            return;
        }
        auto &track = _project.selectedTrack().teletypeTrack();
        scene_state_t &state = track.state();
        auto &trackEngine = _engine.selectedTrackEngine().as<TeletypeTrackEngine>();
        TeletypeBridge::ScopedEngine scope(trackEngine);
        exec_state_t es;
        es_init(&es);
        es_push(&es);
        es_set_script_number(&es, LIVE_SCRIPT);
        es_set_line_number(&es, 0);
        process_result_t result = process_command(&state, &es, &parsed);
        _hasLiveResult = result.has_value;
        if (result.has_value) {
            _liveResult = result.value;
        }
        return;
    }

    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    const int scriptIndex = _scriptIndex;
    ss_overwrite_script_command(&state, scriptIndex, _selectedLine, &parsed);
    if (scriptIndex == 0 || scriptIndex == METRO_SCRIPT) {
        track.syncActiveSlotScripts();
    }
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
    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    const int scriptIndex = _scriptIndex;
    const tele_command_t *cmd = ss_get_script_command(&state, scriptIndex, _selectedLine);
    if (!cmd) {
        return;
    }
    ss_insert_script_command(&state, scriptIndex, _selectedLine + 1, cmd);
    if (_selectedLine < kLineCount - 1) {
        _selectedLine += 1;
    }
    loadEditBuffer(_selectedLine);
    if (scriptIndex == 0 || scriptIndex == METRO_SCRIPT) {
        track.syncActiveSlotScripts();
    }
}

void TeletypeScriptViewPage::commentLine() {
    if (_liveMode) {
        return;
    }
    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    ss_toggle_script_comment(&state, _scriptIndex, _selectedLine);
    if (_scriptIndex == 0 || _scriptIndex == METRO_SCRIPT) {
        track.syncActiveSlotScripts();
    }
}

void TeletypeScriptViewPage::deleteLine() {
    if (_liveMode) {
        return;
    }
    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    if (const tele_command_t *cmd = ss_get_script_command(&state, _scriptIndex, _selectedLine)) {
        char lineBuffer[EditBufferSize] = {};
        print_command(cmd, lineBuffer);
        setEditBuffer(lineBuffer);
        copyLine();
    }
    ss_delete_script_command(&state, _scriptIndex, _selectedLine);
    loadEditBuffer(_selectedLine);
    if (_scriptIndex == 0 || _scriptIndex == METRO_SCRIPT) {
        track.syncActiveSlotScripts();
    }
    showMessage("Line deleted");
}

void TeletypeScriptViewPage::saveScript() {
    if (_scriptIndex >= TeletypeTrack::EditableScriptCount) {
        showMessage("SCRIPT ONLY");
        return;
    }
    if (!_scriptSlotAssigned) {
        saveScriptAs();
        return;
    }
    saveScriptToSlot(_scriptSlot);
}

void TeletypeScriptViewPage::saveScriptAs() {
    if (_scriptIndex >= TeletypeTrack::EditableScriptCount) {
        showMessage("SCRIPT ONLY");
        return;
    }
    _manager.pages().fileSelect.show("SAVE SCRIPT", FileType::TeletypeScript, _scriptSlotAssigned ? _scriptSlot : 0, true,
        [this] (bool result, int slot) {
            if (!result) {
                return;
            }
            if (FileManager::slotUsed(FileType::TeletypeScript, slot)) {
                _manager.pages().confirmation.show("ARE YOU SURE?", [this, slot] (bool result) {
                    if (result) {
                        saveScriptToSlot(slot);
                    }
                });
            } else {
                saveScriptToSlot(slot);
            }
        });
}

void TeletypeScriptViewPage::loadScript() {
    if (_scriptIndex >= TeletypeTrack::EditableScriptCount) {
        showMessage("SCRIPT ONLY");
        return;
    }
    _manager.pages().fileSelect.show("LOAD SCRIPT", FileType::TeletypeScript, _scriptSlotAssigned ? _scriptSlot : 0, false,
        [this] (bool result, int slot) {
            if (!result) {
                return;
            }
            _manager.pages().confirmation.show("ARE YOU SURE?", [this, slot] (bool result) {
                if (result) {
                    loadScriptFromSlot(slot);
                }
            });
        });
}

void TeletypeScriptViewPage::saveScriptToSlot(int slot) {
    if (kSuspendEngineForScriptIO) {
        _engine.suspend();
    }
    _manager.pages().busy.show("SAVING SCRIPT ...");

    FileManager::task([this, slot] () {
        auto &track = _project.selectedTrack().teletypeTrack();
        return FileManager::writeTeletypeScript(track, _scriptIndex, slot);
    }, [this, slot] (fs::Error result) {
        if (result == fs::OK) {
            showMessage("SCRIPT SAVED");
            _scriptSlot = slot;
            _scriptSlotAssigned = true;
        } else {
            showMessage(FixedStringBuilder<32>("FAILED (%s)", fs::errorToString(result)));
        }
        _manager.pages().busy.close();
        if (kSuspendEngineForScriptIO) {
            _engine.resume();
        }
    });
}

void TeletypeScriptViewPage::loadScriptFromSlot(int slot) {
    if (kSuspendEngineForScriptIO) {
        _engine.suspend();
    }
    _manager.pages().busy.show("LOADING SCRIPT ...");

    FileManager::task([this, slot] () {
        auto &track = _project.selectedTrack().teletypeTrack();
        return FileManager::readTeletypeScript(track, _scriptIndex, slot);
    }, [this, slot] (fs::Error result) {
        if (result == fs::OK) {
            showMessage("SCRIPT LOADED");
            _scriptSlot = slot;
            _scriptSlotAssigned = true;
            loadEditBuffer(_selectedLine);
        } else if (result == fs::INVALID_CHECKSUM) {
            showMessage("INVALID SCRIPT FILE");
        } else {
            showMessage(FixedStringBuilder<32>("FAILED (%s)", fs::errorToString(result)));
        }
        _manager.pages().busy.close();
        if (kSuspendEngineForScriptIO) {
            _engine.resume();
        }
    });
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

void TeletypeScriptViewPage::commitLineAndAdvance() {
    commitLine();
    // Move to the next line if possible
    if (_selectedLine < kLineCount - 1) {
        loadEditBuffer(_selectedLine + 1);
    }
}
