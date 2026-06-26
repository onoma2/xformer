#pragma once

#include "BasePage.h"

// Hero-ring headline: the captured loop as an adaptive "tape" (KD-19). Each
// cell is a bar (height = pitch, width = gate length); overlays the three
// brackets (record / loop / ornament), the playhead and the ornament band.
// The one hero page with real edit logic: F1/F2/F3 select a bracket, encoder
// moves its first edge, SHIFT+encoder its last edge, clamped to the nesting
// invariant recordFirst <= loopFirst <= ornFirst <= ornLast <= loopLast <= recordLast.
class FractalTrunkPage : public BasePage {
public:
    FractalTrunkPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    enum class Bracket { Record, Loop, Ornament };

    bool isActiveForSelectedTrack() const;
    void editBracket(int value, bool shift);

    Bracket _bracket = Bracket::Loop;
};
