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
    void insertText(const char *text, bool addSpace);
    void removeLastInsert(int count);
    void insertChar(char c);
    void backspace();
    void moveCursorLeft();
    void moveCursorRight();
    void commitLine();

    int _selectedLine = 0;
    int _cursor = 0;
    int _lastStepKey = -1;
    int _lastKeyIndex = 0;
    uint32_t _lastKeyTime = 0;
    int _lastInsertLength = 0;
    bool _lastKeyShift = false;
    static constexpr int EditBufferSize = 96;
    char _editBuffer[EditBufferSize] = {};
};
