#pragma once

#include "BasePage.h"

// Hero-ring source/mix page (KD-19): Source A / Source B track refs, a
// single-select gateLogic row and a single-select cvLogic row. Edits sourceA,
// sourceB, gateLogic, cvLogic.
class FractalSourcePage : public BasePage {
public:
    FractalSourcePage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    enum class Focus { SourceA, SourceB, Gate, Cv };

    bool isActiveForSelectedTrack() const;

    Focus _focus = Focus::Gate;
};
