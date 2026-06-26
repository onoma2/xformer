#pragma once

#include "BasePage.h"

// Hero-ring branch page (KD-19): the Trunk->B1..BN chain, the Path route
// readout, the transform pool toggles, reseed. Edits the per-sequence branch
// fields only — the engine does not yet act on branches.
class FractalBranchPage : public BasePage {
public:
    FractalBranchPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    enum class Focus { Count, Path, Pool, Seed };

    bool isActiveForSelectedTrack() const;

    Focus _focus = Focus::Count;
    int _poolIndex = 0;
};
