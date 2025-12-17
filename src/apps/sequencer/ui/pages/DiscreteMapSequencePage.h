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

    enum class ContextAction : uint8_t {
        Init,
        Copy,
        Paste,
        Random,
        Route,
        Last
    };

    enum class GeneratorStage : uint8_t {
        Inactive,
        ChooseKind,
        Execute,
    };

    enum class GeneratorKind : uint8_t {
        Random,
        Linear,
        Logarithmic,
        Exponential,
    };

    void refreshPointers();
    void drawThresholdBar(Canvas &canvas);
    void drawStageInfo(Canvas &canvas);
    void drawFooter(Canvas &canvas);

    void handleTopRowKey(int idx);
    void handleBottomRowKey(int idx);
    void handleFunctionKey(int fnIndex);

    float getThresholdNormalized(int stageIndex) const;
    float rangeMin() const { return -5.f; }
    float rangeMax() const { return 5.f; }

    void contextShow();
    void contextAction(int index);
    bool contextActionEnabled(int index) const;
    void applyGenerator(bool applyThresholds, bool applyNotes, bool applyToggles = false);
    void generateThresholds(GeneratorKind kind);
    void generateNotes(GeneratorKind kind);
    float shapeValue(float t, GeneratorKind kind) const;

    DiscreteMapSequence *_sequence = nullptr;
    DiscreteMapTrackEngine *_enginePtr = nullptr;
    EditMode _editMode = EditMode::None;
    int _selectedStage = 0;
    uint32_t _selectionMask = 1;
    bool _shiftHeld = false;
    uint16_t _stepKeysHeld = 0;
    int _section = 0;
    GeneratorStage _generatorStage = GeneratorStage::Inactive;
    GeneratorKind _generatorKind = GeneratorKind::Random;
};
