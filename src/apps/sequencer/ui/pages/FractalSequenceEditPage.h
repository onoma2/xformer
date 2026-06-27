#pragma once

#include "BasePage.h"

#include "ui/model/FractalSequenceListModel.h"

// Fractal hero-page ring (KD-19): one page object cycling Trunk -> Branch ->
// Ornament -> Source via F5=NEXT. Trunk is the captured loop "tape" with the
// live bracket editor (record/loop/ornament); Branch edits the transform
// chain; Ornament edits rate/intensity/scale; Source edits the A/B mix logic.
class FractalSequenceEditPage : public BasePage {
public:
    FractalSequenceEditPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    enum class ContextAction { Init, ClearBuffer, Copy, Paste, Last };

    enum class Page { Trunk, Branch, Ornament, Source, Last };

    enum class Bracket { Record, Loop, Ornament };
    enum class BranchFocus { Count, Path, Pool, Seed };
    enum class OrnamentFocus { Rate, Intensity, Scale };
    enum class SourceFocus { SourceA, SourceB, Gate, Cv };

    bool isActiveForSelectedTrack() const;
    void nextPage();

    void contextShow(bool doubleClick = false) override;
    void contextAction(int index);
    bool contextActionEnabled(int index) const;

    void drawTrunk(Canvas &canvas);
    void drawBranch(Canvas &canvas);
    void drawOrnament(Canvas &canvas);
    void drawSource(Canvas &canvas);

    void encoderTrunk(EncoderEvent &event);
    void encoderBranch(EncoderEvent &event);
    void encoderOrnament(EncoderEvent &event);
    void encoderSource(EncoderEvent &event);

    void keyPressTrunk(KeyPressEvent &event);
    void keyPressBranch(KeyPressEvent &event);
    void keyPressOrnament(KeyPressEvent &event);
    void keyPressSource(KeyPressEvent &event);

    void editBracket(int value, bool shift);

    void quickEdit(int index);

    FractalSequenceListModel _quickEditModel;

    Page _currentPage = Page::Trunk;

    Bracket _bracket = Bracket::Loop;
    bool _editRecordSkip = false;
    BranchFocus _branchFocus = BranchFocus::Count;
    int _poolIndex = 0;
    OrnamentFocus _ornamentFocus = OrnamentFocus::Rate;
    SourceFocus _sourceFocus = SourceFocus::Gate;
};
