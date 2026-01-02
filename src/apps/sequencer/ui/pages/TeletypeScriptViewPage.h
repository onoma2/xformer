#pragma once

#include "BasePage.h"

class TeletypeScriptViewPage : public BasePage {
public:
    TeletypeScriptViewPage(PageManager &manager, PageContext &context);

    void draw(Canvas &canvas) override;
    void updateLeds(Leds &leds) override;
    void keyPress(KeyPressEvent &event) override;
};
