#pragma once

#include "BasePage.h"
#include "ui/StepSelection.h"
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
    virtual void keyboard(KeyboardEvent &event) override;

    virtual void contextShow(bool doubleClick = false) override;

private:
    static constexpr int kSetCount = 5;  // TEMP, PTCH, ACCUM.N, ACCUM.P, MACRO

    void drawGrid(Canvas &canvas);
    void drawGridCell(Canvas &canvas, int idx, bool isActive, bool isSelected);
    void drawTemporalScope(Canvas &canvas, int stageIdx, int scopeX);
    void drawPitchScope(Canvas &canvas, int stageIdx, int scopeX);
    void drawAccumDualStrip(Canvas &canvas, int scopeX, int activeSetIdx);
    void drawStageBadge(Canvas &canvas, int scopeX);
    void drawParamList(Canvas &canvas);
    void drawAccumPage(Canvas &canvas);
    void drawMacroScope(Canvas &canvas);

    void editSlot(int slot, int value, bool shift);
    void togglePressSlot(int slot);
    void shake(bool wholeTopic);

    // Context menu — INIT (with sub: Stage/Topic/Sequence/Track) + COPY + PASTE.
    void contextAction(int index);
    bool contextActionEnabled(int index) const;
    void initStage();
    void initTopic();
    void initSequence();
    void initTrack();
    void copySequence();
    void pasteSequence();
    void duplicateSequence();

    const PhaseFluxTrackEngine *trackEngine() const;
    const PhaseFluxTrack &phaseFluxTrack() const;

    int _selectedCell = 0;
    int _currentSet = 0;    // 0=TEMP, 1=PTCH, 2=ACCUM.N, 3=ACCUM.P, 4=MACRO
    int _selectedSlot = 0;  // 0..3 (F1..F4) — F5 is Next
    int _topicPage = 0;     // P0/P1/(P2 for TEMP+PTCH)
    bool _lengthTransfer = false;  // TEMP P1 Len slot armed for Σ-conserving transfer (re-press F1)
    StepSelection<16> _stepSelection;  // multi-cell edit target (held + Persist via Shift)
};
