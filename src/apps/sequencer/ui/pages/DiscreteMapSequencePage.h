#pragma once

#include "BasePage.h"

#include "engine/DiscreteMapTrackEngine.h"
#include "model/DiscreteMapSequence.h"

class DiscreteMapSequencePage : public BasePage {
public:
    DiscreteMapSequencePage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyDown(KeyEvent &event) override;
    virtual void keyUp(KeyEvent &event) override;
    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    enum class EditMode : uint8_t {
        None,
        Threshold,
        NoteValue,
        DualThreshold,
    };

    void refreshPointers();
    void drawThresholdBar(Canvas &canvas);
    void drawStageInfo(Canvas &canvas);
    void drawFooter(Canvas &canvas);

    void handleTopRowKey(int idx, bool shift);
    void handleBottomRowKey(int idx);
    void handleFunctionKey(int fnIndex);

    float getThresholdNormalized(int stageIndex) const;
    float rangeMin() const { return -5.f; }
    float rangeMax() const { return 5.f; }

    void contextShow();
    void contextAction(int index);
    bool contextActionEnabled(int index) const;

    DiscreteMapSequence *_sequence = nullptr;
    DiscreteMapTrackEngine *_enginePtr = nullptr;
    EditMode _editMode = EditMode::None;
    int _selectedStage = 0;
    int _secondaryStage = -1;
    bool _shiftHeld = false;
};
