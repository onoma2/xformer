#pragma once

#include "BasePage.h"

// Hero-ring ornament page (KD-19): Rate + Intensity bars, Scale + Root, the
// ornament zone (shared with the Trunk page's ORN bracket), last-fired
// placeholder. Edits ornamentRate, ornamentIntensity, scale, rootNote.
class FractalOrnamentPage : public BasePage {
public:
    FractalOrnamentPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    enum class Focus { Rate, Intensity, Scale, Root };

    bool isActiveForSelectedTrack() const;

    Focus _focus = Focus::Rate;
};
