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
    int _selectedLine = 0;
};
