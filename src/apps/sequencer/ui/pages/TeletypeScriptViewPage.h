#pragma once

#include "BasePage.h"

class TeletypeScriptViewPage : public BasePage {
public:
    TeletypeScriptViewPage(PageManager &manager, PageContext &context);

    void enter() override;
    void draw(Canvas &canvas) override;
    void updateLeds(Leds &leds) override;
    void keyPress(KeyPressEvent &event) override;
    void encoder(EncoderEvent &event) override;

private:
    void loadEditBuffer(int line);
    void insertChar(char c);
    void backspace();
    void moveCursorLeft();
    void moveCursorRight();
    void commitLine();

    int _selectedLine = 0;
    int _cursor = 0;
    static constexpr int EditBufferSize = 96;
    char _editBuffer[EditBufferSize] = {};
};
