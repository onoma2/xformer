#pragma once

#include "BasePage.h"
#include "engine/PhaseFluxTrackEngine.h"

class PhaseFluxEditPage : public BasePage {
public:
    PhaseFluxEditPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyDown(KeyEvent &event) override;
    virtual void keyUp(KeyEvent &event) override;
    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    void drawGrid(Canvas &canvas);
    void drawGridCell(Canvas &canvas, int idx, bool isActive, bool isSelected);
    void drawScopes(Canvas &canvas);
    void drawTemporalScope(Canvas &canvas, int stageIdx);
    void drawPitchScope(Canvas &canvas, int stageIdx);

    const PhaseFluxTrackEngine *trackEngine() const;
    const PhaseFluxTrack &phaseFluxTrack() const;

    int _selectedCell = 0;
};
