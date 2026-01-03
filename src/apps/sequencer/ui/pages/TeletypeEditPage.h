#pragma once

#include "BasePage.h"

class TeletypeEditPage : public BasePage {
public:
    TeletypeEditPage(PageManager &manager, PageContext &context);

    void enter() override;
    void exit() override;
    void draw(Canvas &canvas) override;
    void updateLeds(Leds &leds) override;
    void keyPress(KeyPressEvent &event) override;
    void encoder(EncoderEvent &event) override;

private:
};
