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
    static constexpr int kSetCount = 4;  // TEMP, PTCH, ACCUM.N, ACCUM.P

    void drawGrid(Canvas &canvas);
    void drawGridCell(Canvas &canvas, int idx, bool isActive, bool isSelected);
    void drawTemporalScope(Canvas &canvas, int stageIdx, int scopeX);
    void drawPitchScope(Canvas &canvas, int stageIdx, int scopeX);
    void drawAccumDualStrip(Canvas &canvas, int scopeX, int activeSetIdx);
    void drawStageBadge(Canvas &canvas, int scopeX);
    void drawParamList(Canvas &canvas);
    void drawAccumPage(Canvas &canvas);

    void editSlot(int slot, int value, bool shift);
    void toggleShiftAt(int slot);
    void randomizeCurrentSet();

    const PhaseFluxTrackEngine *trackEngine() const;
    const PhaseFluxTrack &phaseFluxTrack() const;

    int _selectedCell = 0;
    int _currentSet = 0;    // 0 = TEMP, 1 = PTCH, 2 = ACCUM.N, 3 = ACCUM.P
    int _selectedSlot = 0;  // 0..4 (which F-slot is active)
    int _accumPage = 0;     // 0 = shape page, 1 = mode page (ACCUM.N/P only)
};
