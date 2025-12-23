#pragma once

#include "BasePage.h"
#include "ui/model/DiscreteMapSequenceListModel.h"

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

    enum class InitStage : uint8_t {
        Inactive,
        ChooseTarget,
    };

    enum class RangeMacro : uint8_t {
        Full,           // -5..+5
        Inv,            // +5..-5
        Pos,            // 0..+5
        Neg,            // -5..0
        Top,            // 4..5
        Btm,            // -5..-4
        Ass,            // -1..4
        BAss,           // -2..3
        Last
    };

    void refreshPointers();
    void drawThresholdBar(Canvas &canvas);
    void drawStageInfo(Canvas &canvas);
    void drawFooter(Canvas &canvas);
    void quickEdit(int index);

    void handleTopRowKey(int idx);
    void handleBottomRowKey(int idx);
    void handleFunctionKey(int fnIndex);
    void applyRangeMacro(RangeMacro macro);
    void getRangeMacroValues(RangeMacro macro, float &low, float &high) const;
    const char *getRangeMacroName(RangeMacro macro) const;

    float getThresholdNormalized(int stageIndex) const;
    float rangeMin() const { return -5.f; }
    float rangeMax() const { return 5.f; }

    void contextShow();
    void contextAction(int index);
    bool contextActionEnabled(int index) const;
    enum class NoteSpread : uint8_t {
        Wide,   // -63..+64 (approx -5..+5V depending on scale)
        Narrow, // -1..+31 (approx -2V)
    };

    void applyGenerator(bool applyThresholds, bool applyNotes, bool applyToggles = false, NoteSpread noteSpread = NoteSpread::Wide);
    void generateThresholds(GeneratorKind kind);
    void generateNotes(GeneratorKind kind, NoteSpread spread);
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
    InitStage _initStage = InitStage::Inactive;
    RangeMacro _currentRangeMacro = RangeMacro::Full;
    DiscreteMapSequenceListModel _listModel;
};
