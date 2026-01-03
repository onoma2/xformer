#pragma once

#include "BasePage.h"

#include <cstdint>

class TeletypeScriptViewPage : public BasePage {
public:
    TeletypeScriptViewPage(PageManager &manager, PageContext &context);

    void enter() override;
    void draw(Canvas &canvas) override;
    void updateLeds(Leds &leds) override;
    void keyPress(KeyPressEvent &event) override;
    void encoder(EncoderEvent &event) override;

private:
    void handleStepKey(int step, bool shift);
    void loadEditBuffer(int line);
    void setScriptIndex(int scriptIndex);
    void insertText(const char *text, bool addSpace);
    void removeLastInsert(int count);
    void insertChar(char c);
    void backspace();
    void moveCursorLeft();
    void moveCursorRight();
    void commitLine();
    void copyLine();
    void pasteLine();
    void pushHistory(const char *line);
    void recallHistory(int direction);
    void setEditBuffer(const char *text);

    int _selectedLine = 0;
    int _cursor = 0;
    int _scriptIndex = 0;
    int _lastStepKey = -1;
    int _lastKeyIndex = 0;
    uint32_t _lastKeyTime = 0;
    int _lastInsertLength = 0;
    bool _lastKeyShift = false;
    static constexpr int EditBufferSize = 96;
    char _editBuffer[EditBufferSize] = {};
    char _clipboard[EditBufferSize] = {};
    bool _hasClipboard = false;
    bool _liveMode = false;
    bool _hasLiveResult = false;
    int16_t _liveResult = 0;
    static constexpr int HistorySize = 4;
    char _history[HistorySize][EditBufferSize] = {};
    int _historyCount = 0;
    int _historyHead = -1;
    int _historyCursor = -1;
};
