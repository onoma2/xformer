#pragma once

#include "BasePage.h"
#include "ui/StepSelection.h"
#include "ui/model/IndexedSequenceListModel.h"
#include "model/IndexedSequence.h"

class IndexedSequenceEditPage : public BasePage {
public:
    IndexedSequenceEditPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyDown(KeyEvent &event) override;
    virtual void keyUp(KeyEvent &event) override;
    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

    // Public accessor for step selection (used by math page)
    const StepSelection<IndexedSequence::MaxSteps> &stepSelection() const { return _stepSelection; }

private:
    enum class VoicingBank {
        Piano,
        Guitar
    };

    enum class EditMode {
        Note,
        Duration,
        Gate
    };

    enum class ContextMode {
        Sequence,
        Step
    };

    enum class FunctionMode {
        Edit,     // F1-F3 = Note/Duration/Gate editing
        Groups    // F1-F4 = Group A/B/C/D toggles
    };

    static const int StepCount = 16;
    static const int SectionCount = (IndexedSequence::MaxSteps + StepCount - 1) / StepCount;

    int stepOffset() const { return _section * StepCount; }

    void contextShow();
    void contextAction(int index);
    bool contextActionEnabled(int index) const;

    void insertStep();
    void splitStep();
    void rotateToFirstSelected();
    void deleteStep();
    void mergeStepWithNext();
    void swapStepWithOffset(int offset);
    void copyStep();
    void pasteStep();
    void copySequence();
    void pasteSequence();
    void initSequence();
    void routeSequence();

    IndexedSequence::Step& step(int index);
    const IndexedSequence::Step& step(int index) const;

    void quickEdit(int index);
    void startSplitQuickEdit();
    void finishSplitQuickEdit();
    void splitStepInto(int pieces);
    void startMergeQuickEdit();
    void finishMergeQuickEdit();
    void mergeSteps(int count);
    void startSwapQuickEdit();
    void finishSwapQuickEdit();
    void startVoicingQuickEdit(VoicingBank bank, int stepIndex);
    void finishVoicingQuickEdit();
    void showVoicingMessage(VoicingBank bank, int voicingIndex);
    void applyVoicing(VoicingBank bank, int voicingIndex);

    // Macro context menus
    void rhythmContextShow();
    void rhythmContextAction(int index);
    void waveformContextShow();
    void waveformContextAction(int index);
    void melodicContextShow();
    void melodicContextAction(int index);
    void durationTransformContextShow();
    void durationTransformContextAction(int index);

    int _section = 0;
    EditMode _editMode = EditMode::Note;
    ContextMode _contextMode = ContextMode::Sequence;
    FunctionMode _functionMode = FunctionMode::Edit;
    bool _durationTransfer = false;
    bool _noteSlideEdit = false;

    IndexedSequenceListModel _listModel;
    StepSelection<IndexedSequence::MaxSteps> _stepSelection;
    bool _splitQuickEditActive = false;
    int _splitQuickEditCount = 2;
    bool _mergeQuickEditActive = false;
    int _mergeQuickEditCount = 2;
    bool _swapQuickEditActive = false;
    int _swapQuickEditBaseIndex = -1;
    int _swapQuickEditOffset = 0;
    int _swapQuickEditPreferredOffset = 0;
    bool _voicingQuickEditActive = false;
    VoicingBank _voicingQuickEditBank = VoicingBank::Piano;
    int _voicingQuickEditIndex = -1;
    int _voicingQuickEditStep = 0;
    bool _voicingQuickEditDirty = false;
    int _pianoVoicingIndex = -1;
    int _guitarVoicingIndex = -1;
};
