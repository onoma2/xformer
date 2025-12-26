#include "IndexedSequenceEditPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"
#include "ui/painters/SequencePainter.h"

#include "model/Scale.h"
#include "model/Routing.h"

#include "core/utils/StringBuilder.h"

enum class ContextAction {
    Init,
    Copy,
    Paste,
    Route,
    Insert,
    MakeFirst,
    Delete,
    Last
};

static const ContextMenuModel::Item seqContextMenuItems[] = {
    { "INIT" },
    { "COPY" },
    { "PASTE" },
    { "ROUTE" },
};

static const ContextMenuModel::Item stepContextMenuItems[] = {
    { "INSERT" },
    { "MAKE 1ST" },
    { "DELETE" },
    { "COPY" },
    { "PASTE" },
};

static const ContextAction stepContextActions[] = {
    ContextAction::Insert,
    ContextAction::MakeFirst,
    ContextAction::Delete,
    ContextAction::Copy,
    ContextAction::Paste,
};

struct Voicing {
    const char *name;
    int8_t semis[6];
    uint8_t count;
};

static const Voicing kPianoVoicings[] = {
    { "MAJ13",   { 0, 4, 7, 11, 14, 21 }, 6 },
    { "MAJ6/9",  { 0, 4, 7, 9, 14, 0 },   5 },
    { "MIN13",   { 0, 3, 7, 10, 14, 21 }, 6 },
    { "MIN6/9",  { 0, 3, 7, 9, 14, 0 },   5 },
    { "MINMAJ9", { 0, 3, 7, 11, 14, 0 },  5 },
    { "DOM13",   { 0, 4, 7, 10, 14, 21 }, 6 },
    { "M9B5",    { 0, 3, 6, 10, 14, 0 },  5 },
    { "DIM7",    { 0, 3, 6, 9, 0, 0 },    4 },
    { "AUG9",    { 0, 4, 8, 10, 14, 0 },  5 },
    { "AUGMAJ9", { 0, 4, 8, 11, 14, 0 },  5 },
    { "SUS2(9)", { 0, 2, 7, 10, 14, 0 },  5 },
    { "SUS4(11)",{ 0, 5, 7, 10, 14, 17 }, 6 },
};

static const Voicing kGuitarVoicings[] = {
    { "MAJ",   { 0, 4, 7, 12, 16, 0 }, 5 },
    { "MIN",   { 0, 7, 12, 15, 19, 0 }, 5 },
    { "7",     { 0, 4, 10, 12, 16, 0 }, 5 },
    { "MAJ7",  { 0, 7, 11, 16, 19, 0 }, 5 },
    { "MIN7",  { 0, 7, 10, 15, 19, 0 }, 5 },
    { "6",     { 0, 4, 9, 12, 16, 0 }, 5 },
    { "MIN6",  { 0, 7, 12, 15, 21, 0 }, 5 },
    { "9",     { 0, 7, 10, 16, 26, 0 }, 5 },
    { "13",    { 0, 7, 10, 16, 21, 0 }, 5 },
    { "SUS2",  { 0, 7, 12, 14, 19, 0 }, 5 },
    { "SUS4",  { 0, 7, 12, 17, 19, 0 }, 5 },
    { "ADD9",  { 0, 4, 7, 14, 19, 0 }, 5 },
    { "AUG",   { 0, 8, 12, 16, 20, 0 }, 5 },
    { "M7B5",  { 0, 6, 10, 15, 22, 0 }, 5 },
    { "DIM7",  { 0, 6, 12, 15, 21, 0 }, 5 },
};

static const int kPianoVoicingCount = int(sizeof(kPianoVoicings) / sizeof(kPianoVoicings[0]));
static const int kGuitarVoicingCount = int(sizeof(kGuitarVoicings) / sizeof(kGuitarVoicings[0]));

enum {
    QuickEditNone = -1,
    QuickEditSplit = -2,
    QuickEditSwap = -3,
    QuickEditMerge = -4,
    QuickEditSetFirst = -5,
    QuickEditPiano = -6,
    QuickEditGuitar = -7,
};

static const int quickEditItems[8] = {
    QuickEditSplit,                                   // Step 9
    QuickEditMerge,                                   // Step 10
    QuickEditSetFirst,                                // Step 11
    QuickEditPiano,                                   // Step 12
    QuickEditGuitar,                                  // Step 13
    QuickEditNone,                                    // Step 14 (free for macro)
    QuickEditNone,                                    // Step 15
    QuickEditNone
};

IndexedSequenceEditPage::IndexedSequenceEditPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{
}

void IndexedSequenceEditPage::enter() {
}

void IndexedSequenceEditPage::exit() {
}

void IndexedSequenceEditPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "INDEXED EDIT");

    const auto &sequence = _project.selectedIndexedSequence();
    const auto &trackEngine = _engine.selectedTrackEngine().as<IndexedTrackEngine>();

    // 1. Top Section: Timeline Bar
    int totalTicks = 0;
    int activeLength = sequence.activeLength();
    int nonzeroSteps = 0;
    for (int i = 0; i < activeLength; ++i) {
        int duration = sequence.step(i).duration();
        totalTicks += duration;
        if (duration > 0) {
            nonzeroSteps++;
        }
    }

    if (totalTicks > 0 && nonzeroSteps > 0) {
        const int barX = 8;
        const int barY = 14;
        const int barW = 240;
        const int barH = 16;
        const int minStepW = 7;

        int currentX = barX;
        int extraPixels = barW - minStepW * nonzeroSteps;
        if (extraPixels < 0) {
            extraPixels = 0;
        }
        int error = 0;

        for (int i = 0; i < activeLength; ++i) {
            const auto &step = sequence.step(i);
            int stepW = 0;
            if (step.duration() > 0) {
                int scaled = extraPixels * step.duration() + error;
                int extraW = scaled / totalTicks;
                error = scaled % totalTicks;
                stepW = minStepW + extraW;
            }

            bool selected = _stepSelection[i];
            bool active = (trackEngine.currentStep() == i);

            canvas.setColor(selected ? Color::Bright : (active ? Color::MediumBright : Color::Medium));
            canvas.drawRect(currentX, barY, stepW, barH);

            int gateW = 0;
            if (step.gateLength() == IndexedSequence::GateLengthTrigger) {
                gateW = std::min(2, stepW);
            } else {
                gateW = (int)(stepW * (step.gateLength() / 100.0f));
            }
            if (gateW > 0) {
                canvas.setColor(selected ? Color::Bright : (active ? Color::MediumBright : Color::Low));
                canvas.fillRect(currentX + 1, barY + 1, gateW, barH - 2);
            }

            currentX += stepW;
        }
    }

    // 2. Bottom Section: Info & Edit (F1, F2, F3) or Group Indicators (F1-F4)
    if (_functionMode == FunctionMode::Groups) {
        const int y = 40;
        canvas.setFont(Font::Small);
        canvas.setBlendMode(BlendMode::Set);
        canvas.setColor(Color::Bright);

        uint8_t groupMask = 0;
        if (_stepSelection.any()) {
            int stepIndex = _stepSelection.first();
            groupMask = sequence.step(stepIndex).groupMask();
        }

        int groupCounts[4] = {0, 0, 0, 0};
        int activeLength = sequence.activeLength();
        for (int i = 0; i < activeLength; ++i) {
            uint8_t mask = sequence.step(i).groupMask();
            for (int g = 0; g < 4; ++g) {
                if (mask & (1 << g)) {
                    groupCounts[g]++;
                }
            }
        }

        // F1-F4: Groups A-D
        const char* groupLabels[] = {"A", "B", "C", "D"};
        for (int i = 0; i < 4; ++i) {
            bool inGroup = (groupMask & (1 << i)) != 0;
            canvas.setColor(Color::Medium);
            FixedStringBuilder<6> countText;
            countText("%d", groupCounts[i]);
            canvas.drawTextCentered(i * 51, y - 8, 51, 8, countText);
            FixedStringBuilder<4> groupText;
            if (inGroup) {
                groupText("[%s]", groupLabels[i]);
            } else {
                groupText("[-]");
            }
            canvas.drawTextCentered(i * 51, y, 51, 16, groupText);
        }
    } else if (_stepSelection.any()) {
        // Step selected: Show bars/deltas on row 1, selected step values on row 2
        const auto &timeSig = _project.timeSignature();
        uint32_t measureTicks = timeSig.measureDivisor();
        uint32_t beatTicks = timeSig.noteDivisor();
        float bars = (float)totalTicks / measureTicks;

        canvas.setFont(Font::Tiny);
        canvas.setColor(Color::Medium);
        FixedStringBuilder<48> infoStr;
        auto deltaToBoundary = [](int ticks, int period) -> int {
            if (period <= 0) {
                return 0;
            }
            if (ticks < period) {
                return period - ticks;
            }
            int remainder = ticks % period;
            if (remainder == 0) {
                return 0;
            }
            int toPrev = -remainder;
            int toNext = period - remainder;
            return (abs(toPrev) <= abs(toNext)) ? toPrev : toNext;
        };

        int deltaBar = deltaToBoundary(totalTicks, int(measureTicks));
        int deltaBeat = deltaToBoundary(totalTicks, int(beatTicks));
        infoStr("BARS %.1f  DT %+d / %+d", bars, deltaBar, deltaBeat);
        canvas.drawTextCentered(0, 32, 256, 8, infoStr);

        const int y = 40;
        canvas.setFont(Font::Small);
        canvas.setBlendMode(BlendMode::Set);
        canvas.setColor(Color::Bright);

        // Edit mode: Show note/duration/gate values
        int stepIndex = _stepSelection.first();
        const auto &step = sequence.step(stepIndex);

        // F1: Duration
        FixedStringBuilder<16> durStr;
        durStr("%d", step.duration());
        canvas.drawTextCentered(0, y, 51, 16, durStr);

        // F2: Gate
        FixedStringBuilder<16> gateStr;
        if (step.gateLength() == IndexedSequence::GateLengthTrigger) {
            gateStr("T");
        } else {
            gateStr("%d%%", step.gateLength());
        }
        canvas.drawTextCentered(51, y, 51, 16, gateStr);

        // F3: Note
        FixedStringBuilder<8> noteName;
        const auto &scale = sequence.selectedScale(_project.selectedScale());
        int rootNote = sequence.rootNote() < 0 ? _project.rootNote() : sequence.rootNote();
        const auto &track = _project.selectedTrack().indexedTrack();
        int shift = track.octave() * scale.notesPerOctave() + track.transpose();
        int noteIndex = step.noteIndex() + shift;
        scale.noteName(noteName, noteIndex, rootNote, Scale::Format::Short1);
        float volts = scale.noteToVolts(noteIndex);
        if (scale.isChromatic()) {
            volts += rootNote * (1.f / 12.f);
        }
        FixedStringBuilder<24> noteStr;
        if (step.slide()) {
            noteStr("%.2f %s/", volts, static_cast<const char *>(noteName));
        } else {
            noteStr("%.2f %s", volts, static_cast<const char *>(noteName));
        }
        const int noteX = 102;
        const int noteW = 51;
        canvas.drawTextCentered(noteX, y, noteW, 16, noteStr);
    } else {
        // No step selected: Show "STEP N/N" with playing step info on row 1
        int currentStep = trackEngine.currentStep() + 1;
        int totalSteps = sequence.activeLength();

        if (trackEngine.currentStep() >= 0 && trackEngine.currentStep() < sequence.activeLength()) {
            const auto &step = sequence.step(trackEngine.currentStep());
            const auto &scale = sequence.selectedScale(_project.selectedScale());
            int rootNote = sequence.rootNote() < 0 ? _project.rootNote() : sequence.rootNote();
            const auto &track = _project.selectedTrack().indexedTrack();
            int shift = track.octave() * scale.notesPerOctave() + track.transpose();
            int noteIndex = step.noteIndex() + shift;

            FixedStringBuilder<8> noteName;
            scale.noteName(noteName, noteIndex, rootNote, Scale::Format::Short1);
            float volts = scale.noteToVolts(noteIndex);
            if (scale.isChromatic()) {
                volts += rootNote * (1.f / 12.f);
            }

            FixedStringBuilder<12> gateStr;
            if (step.gateLength() == IndexedSequence::GateLengthTrigger) {
                gateStr("T");
            } else {
                gateStr("%d%%", step.gateLength());
            }

            canvas.setFont(Font::Tiny);
            canvas.setColor(Color::MediumBright);
            FixedStringBuilder<48> info;
            info("STEP %d/%d  %d  %s  %.2f %s", currentStep, totalSteps, step.duration(), static_cast<const char *>(gateStr), volts, static_cast<const char *>(noteName));
            canvas.drawTextCentered(0, 32, 256, 8, info);
        }
    }

    // Footer Labels
    // F4 toggles between SEQ and STEP context mode (Edit mode) OR shows "GRPS" in Groups mode
    // F5 navigates to math page, SHIFT+F5 to route config
    const char *footerLabels[5];
    bool shift = pageKeyState()[Key::Shift];
    if (_functionMode == FunctionMode::Groups) {
        footerLabels[0] = "A";
        footerLabels[1] = "B";
        footerLabels[2] = "C";
        footerLabels[3] = "D";
        footerLabels[4] = "BACK";
    } else {
        footerLabels[0] = _durationTransfer ? "DUR-TR" : "DUR";
        footerLabels[1] = "GATE";
        footerLabels[2] = _noteSlideEdit ? "SLIDE" : "NOTE";
        footerLabels[3] = (_contextMode == ContextMode::Sequence) ? "SEQ" : "STEP";
        footerLabels[4] = shift ? "ROUTES" : "MATH";
    }
    int footerHighlight = -1;
    if (_functionMode != FunctionMode::Groups) {
        switch (_editMode) {
        case EditMode::Duration:
            footerHighlight = 0;
            break;
        case EditMode::Gate:
            footerHighlight = 1;
            break;
        case EditMode::Note:
            footerHighlight = 2;
            break;
        }
    }
    WindowPainter::drawFooter(canvas, footerLabels, pageKeyState(), footerHighlight);
}

void IndexedSequenceEditPage::updateLeds(Leds &leds) {
    const auto &sequence = _project.selectedIndexedSequence();
    const auto &trackEngine = _engine.selectedTrackEngine().as<IndexedTrackEngine>();
    int currentStep = trackEngine.currentStep();

    int stepOffset = this->stepOffset();

    for (int i = 0; i < 16; ++i) {
        int stepIndex = stepOffset + i;
        if (stepIndex >= sequence.activeLength()) break;

        bool selected = _stepSelection[stepIndex];
        bool playing = (stepIndex == currentStep);

        bool green = playing;
        bool red = selected;

        leds.set(MatrixMap::fromStep(i), red, green);
    }

    LedPainter::drawSelectedSequenceSection(leds, _section);

    if (globalKeyState()[Key::Page] && !globalKeyState()[Key::Shift]) {
        for (int i = 0; i < 8; ++i) {
            int index = MatrixMap::fromStep(i + 8);
            leds.unmask(index);
            leds.set(index, false, quickEditItems[i] != QuickEditNone);
            leds.mask(index);
        }

        // Indexed Macro Shortcuts - YELLOW
        // Step 4: Rhythm Generators
        // Step 5: Waveforms
        // Step 6: Melodic Generators
        // Step 14: Duration & Transform
        const int macroShortcuts[] = { 4, 5, 6, 14 };
        for (int step : macroShortcuts) {
            int index = MatrixMap::fromStep(step);
            leds.unmask(index);
            leds.set(index, true, true);
            leds.mask(index);
        }
    }
}

void IndexedSequenceEditPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.isQuickEdit() && !key.shiftModifier()) {
        quickEdit(key.quickEdit());
        event.consume();
        return;
    }

    if (key.pageModifier() && key.is(Key::Step4)) {
        rhythmContextShow();
        event.consume();
        return;
    }

    if (key.pageModifier() && key.is(Key::Step5)) {
        waveformContextShow();
        event.consume();
        return;
    }

    if (key.pageModifier() && key.is(Key::Step6)) {
        melodicContextShow();
        event.consume();
        return;
    }

    if (key.pageModifier() && key.is(Key::Step14)) {
        durationTransformContextShow();
        event.consume();
        return;
    }

    if (key.pageModifier()) {
        return;
    }

    // Double-press step to toggle gate between 0 and Trigger
    if (!key.shiftModifier() && key.isStep() && event.count() == 2) {
        auto &sequence = _project.selectedIndexedSequence();
        int stepIndex = stepOffset() + key.step();
        if (stepIndex < sequence.activeLength()) {
            auto &step = sequence.step(stepIndex);
            if (step.gateLength() == 0) {
                step.setGateLength(IndexedSequence::GateLengthTrigger);
                showMessage("GATE: TRIGGER");
            } else {
                step.setGateLength(0);
                showMessage("GATE: OFF");
            }
        }
        event.consume();
        return;
    }

    if (key.isStep()) {
        int stepIndex = stepOffset() + key.step();
        if (stepIndex < _project.selectedIndexedSequence().activeLength()) {
            _stepSelection.keyPress(event, stepOffset());
        }
        event.consume();
        return;
    }

    if (key.isFunction()) {
        int fn = key.function();
        bool shift = globalKeyState()[Key::Shift];

        if (fn == 3) {
            // F4: Cycle contexts Sequence -> Step -> Groups -> Sequence
            if (_functionMode == FunctionMode::Groups) {
                _functionMode = FunctionMode::Edit;
                _contextMode = ContextMode::Sequence;
            } else if (_contextMode == ContextMode::Sequence) {
                _contextMode = ContextMode::Step;
            } else {
                _functionMode = FunctionMode::Groups;
            }
        }

        if (fn == 4) {
            if (_functionMode == FunctionMode::Groups) {
                _functionMode = FunctionMode::Edit;
            } else {
                // F5: Navigate to Math page, SHIFT+F5 to Route Config
                if (shift) {
                    _manager.pages().top.editIndexedRouteConfig();
                } else {
                    _manager.pages().top.editIndexedMath();
                }
            }
        }

        event.consume();
        return;
    }

    if (key.isLeft()) {
        _section = std::max(0, _section - 1);
        event.consume();
    }
    if (key.isRight()) {
        _section = std::min(1, _section + 1);
        event.consume();
    }
}

void IndexedSequenceEditPage::encoder(EncoderEvent &event) {
    auto &sequence = _project.selectedIndexedSequence();

    if (_swapQuickEditActive) {
        int maxOffset = sequence.activeLength() - 1 - _swapQuickEditBaseIndex;
        if (maxOffset <= 0) {
            showMessage("NO NEXT");
            _swapQuickEditActive = false;
            event.consume();
            return;
        }
        int nextOffset = _swapQuickEditOffset;
        if (_swapQuickEditOffset == 0 && event.value() > 0 && _swapQuickEditPreferredOffset > 0) {
            nextOffset = _swapQuickEditPreferredOffset;
        } else {
            nextOffset += event.value();
        }
        _swapQuickEditOffset = clamp(nextOffset, 0, maxOffset);
        if (_swapQuickEditOffset == 0) {
            showMessage("NO SWAP");
        } else if (_swapQuickEditPreferredOffset > 0 &&
                   _swapQuickEditOffset == _swapQuickEditPreferredOffset) {
            showMessage("SELECTED");
        } else {
            FixedStringBuilder<16> msg("SWAP +%d", _swapQuickEditOffset);
            showMessage(msg);
        }
        event.consume();
        return;
    }

    if (!_stepSelection.any()) return;

    if (_editMode == EditMode::Duration && _durationTransfer) {
        int stepIndex = _stepSelection.first();
        int activeLength = sequence.activeLength();
        if (activeLength > 0) {
            int nextIndex = (stepIndex + 1) % activeLength;
            auto &step = sequence.step(stepIndex);
            auto &nextStep = sequence.step(nextIndex);
            bool shift = globalKeyState()[Key::Shift];
            int stepVal = shift ? sequence.divisor() : 1;
            int delta = event.value() * stepVal;

            int cur = step.duration();
            int next = nextStep.duration();
            int minDelta = std::max(-cur, next - int(IndexedSequence::MaxDuration));
            int maxDelta = std::min(int(IndexedSequence::MaxDuration) - cur, next);
            int clampedDelta = clamp(delta, minDelta, maxDelta);

            step.setDuration(cur + clampedDelta);
            nextStep.setDuration(next - clampedDelta);
        }
        event.consume();
        return;
    }

    if (_editMode == EditMode::Note && _noteSlideEdit) {
        if (event.value() != 0) {
            bool enable = event.value() > 0;
            for (int i = 0; i < IndexedSequence::MaxSteps; ++i) {
                if (_stepSelection[i]) {
                    sequence.step(i).setSlide(enable);
                }
            }
        }
        event.consume();
        return;
    }

    bool shift = globalKeyState()[Key::Shift];
    int selectionCount = _stepSelection.count();
    const auto &scale = sequence.selectedScale(_project.selectedScale());

    // Gradient Editing: Shift + multiple steps = linear ramp
    if (shift && selectionCount > 1) {
        int firstIndex = _stepSelection.firstSetIndex();
        int lastIndex = _stepSelection.lastSetIndex();
        auto &firstStep = sequence.step(firstIndex);
        auto &lastStep = sequence.step(lastIndex);

        static int multiStepsProcessed = 0;

        for (int i = 0; i < IndexedSequence::MaxSteps; ++i) {
            if (_stepSelection[i]) {
                auto &step = sequence.step(i);

                // Update the "target" (last step) value with encoder input (first iteration only)
                if (i == lastIndex && multiStepsProcessed == 0) {
                    switch (_editMode) {
                    case EditMode::Note:
                        lastStep.setNoteIndex(lastStep.noteIndex() + event.value());
                        break;
                    case EditMode::Duration: {
                        int newDur = lastStep.duration() + event.value();
                        lastStep.setDuration(clamp(newDur, 0, int(IndexedSequence::MaxDuration)));
                        break;
                    }
                    case EditMode::Gate: {
                        int currentGate = lastStep.gateLength();
                        int newGate = currentGate + event.value();
                        if (currentGate == IndexedSequence::GateLengthTrigger && event.value() < 0) {
                            newGate = 100;
                        } else if (currentGate <= 100 && newGate > 100) {
                            newGate = IndexedSequence::GateLengthTrigger;
                        }
                        lastStep.setGateLength(clamp(newGate, 0, int(IndexedSequence::GateLengthTrigger)));
                        break;
                    }
                    }
                }

                // Calculate interpolated value for current step
                float t = float(i - firstIndex) / (lastIndex - firstIndex);

                switch (_editMode) {
                case EditMode::Note: {
                    int startNote = firstStep.noteIndex();
                    int endNote = lastStep.noteIndex();
                    int interpolated = startNote + std::round(t * (endNote - startNote));
                    step.setNoteIndex(interpolated);
                    break;
                }
                case EditMode::Duration: {
                    int startDur = firstStep.duration();
                    int endDur = lastStep.duration();
                    int interpolated = startDur + std::round(t * (endDur - startDur));
                    step.setDuration(interpolated);
                    break;
                }
                case EditMode::Gate: {
                    int startGate = firstStep.gateLength();
                    int endGate = lastStep.gateLength();
                    int interpolated = startGate + std::round(t * (endGate - startGate));
                    step.setGateLength(interpolated);
                    break;
                }
                }

                multiStepsProcessed++;
            }
        }
        multiStepsProcessed = 0;
    } else {
        // Normal editing: apply same value to all selected steps
        for (int i = 0; i < IndexedSequence::MaxSteps; ++i) {
            if (_stepSelection[i]) {
                auto &step = sequence.step(i);

                switch (_editMode) {
                case EditMode::Note:
                    step.setNoteIndex(step.noteIndex() + event.value() * ((shift && scale.isChromatic()) ? scale.notesPerOctave() : 1));
                    break;
                case EditMode::Duration: {
                    int div = sequence.divisor();
                    int stepVal = shift ? div : 1;
                    int newDur = step.duration() + event.value() * stepVal;
                    step.setDuration(clamp(newDur, 0, int(IndexedSequence::MaxDuration)));
                    break;
                }
                case EditMode::Gate:
                    {
                        int stepSize = shift ? 1 : 5;
                        int currentGate = step.gateLength();
                        int newGate = currentGate + event.value() * stepSize;
                        if (currentGate == IndexedSequence::GateLengthTrigger && event.value() < 0) {
                            newGate = 100;
                        } else if (currentGate <= 100 && newGate > 100) {
                            newGate = IndexedSequence::GateLengthTrigger;
                        }
                        step.setGateLength(clamp(newGate, 0, int(IndexedSequence::GateLengthTrigger)));
                    }
                    break;
                }
            }
        }
    }

    event.consume();
}

void IndexedSequenceEditPage::keyDown(KeyEvent &event) {
    const auto &key = event.key();
    if (key.isQuickEdit() && !key.shiftModifier()) {
        if (key.quickEdit() == 1) {
            startSwapQuickEdit();
            event.consume();
            return;
        }
    }

    if (key.isStep()) {
        _stepSelection.keyDown(event, stepOffset());
    }

    if (key.isFunction()) {
        int fn = key.function();

        if (_functionMode == FunctionMode::Groups) {
            // Groups mode: F1-F4 toggle group membership (A-D)
            if (fn >= 0 && fn < 4) {
                auto &sequence = _project.selectedIndexedSequence();
                for (int i = 0; i < IndexedSequence::MaxSteps; ++i) {
                    if (_stepSelection[i]) {
                        sequence.step(i).toggleGroup(fn);
                    }
                }
            }
        } else {
            // Edit mode: F1-F3 select edit mode
            if (fn == 0) {
                if (_editMode == EditMode::Duration) {
                    _durationTransfer = !_durationTransfer;
                } else {
                    _editMode = EditMode::Duration;
                    _durationTransfer = false;
                }
                _noteSlideEdit = false;
            }
            if (fn == 1) {
                _editMode = EditMode::Gate;
                _durationTransfer = false;
                _noteSlideEdit = false;
            }
            if (fn == 2) {
                if (_editMode == EditMode::Note) {
                    _noteSlideEdit = !_noteSlideEdit;
                } else {
                    _editMode = EditMode::Note;
                    _noteSlideEdit = false;
                }
                _durationTransfer = false;
            }
        }
    }
}

void IndexedSequenceEditPage::keyUp(KeyEvent &event) {
    const auto &key = event.key();
    if (_swapQuickEditActive) {
        if (key.isPage() || (key.isStep() && key.step() == 9)) {
            finishSwapQuickEdit();
            event.consume();
            return;
        }
    }

    if (key.isStep()) {
        _stepSelection.keyUp(event, stepOffset());
    }
}

void IndexedSequenceEditPage::contextShow() {
    if (_contextMode == ContextMode::Sequence) {
        showContextMenu(ContextMenu(
            seqContextMenuItems,
            4, // count
            [&] (int index) { contextAction(index); },
            [&] (int index) { return contextActionEnabled(index); }
        ));
    } else {
        showContextMenu(ContextMenu(
            stepContextMenuItems,
            5, // count
            [&] (int index) { contextAction(static_cast<int>(stepContextActions[index])); },
            [&] (int index) { return contextActionEnabled(static_cast<int>(stepContextActions[index])); }
        ));
    }
}

void IndexedSequenceEditPage::contextAction(int index) {
    switch (ContextAction(index)) {
    // SEQ Actions
    case ContextAction::Init:
        initSequence();
        break;
    case ContextAction::Copy:
        if (_contextMode == ContextMode::Sequence) {
            copySequence();
        } else {
            copyStep();
        }
        break;
    case ContextAction::Paste:
        if (_contextMode == ContextMode::Sequence) {
            pasteSequence();
        } else {
            pasteStep();
        }
        break;
    case ContextAction::Route:
        routeSequence();
        break;
    // STEP Actions
    case ContextAction::Insert:
        insertStep();
        break;
    case ContextAction::MakeFirst:
        rotateToFirstSelected();
        break;
    case ContextAction::Delete:
        deleteStep();
        break;
    default:
        break;
    }
}

bool IndexedSequenceEditPage::contextActionEnabled(int index) const {
    auto &sequence = _project.selectedIndexedSequence();
    switch (ContextAction(index)) {
    case ContextAction::Copy:
        return _contextMode == ContextMode::Sequence ? true : _stepSelection.any();
    case ContextAction::Paste:
        return _contextMode == ContextMode::Sequence ?
            _model.clipBoard().canPasteIndexedSequence() :
            _model.clipBoard().canPasteIndexedSequenceSteps();
    case ContextAction::Insert:
        return sequence.canInsert();
    case ContextAction::MakeFirst:
        return _stepSelection.any() && _stepSelection.first() > 0;
    case ContextAction::Delete:
        return sequence.canDelete();
    default:
        return true;
    }
}

void IndexedSequenceEditPage::initSequence() {
    _project.selectedIndexedSequence().clear();
    showMessage("SEQUENCE CLEARED");
}

void IndexedSequenceEditPage::routeSequence() {
    _manager.pages().top.editRoute(Routing::Target::Divisor, _project.selectedTrackIndex());
}

void IndexedSequenceEditPage::insertStep() {
    auto &sequence = _project.selectedIndexedSequence();
    if (_stepSelection.any()) {
        sequence.insertStep(_stepSelection.first());
        // Auto-paste logic if clipboard valid
        if (_model.clipBoard().canPasteIndexedSequenceSteps()) {
            ClipBoard::SelectedSteps steps;
            steps.set(_stepSelection.first());
            _model.clipBoard().pasteIndexedSequenceSteps(sequence, steps);
            showMessage("STEP INSERTED (PASTE)");
        } else {
            showMessage("STEP INSERTED");
        }
    }
}

void IndexedSequenceEditPage::splitStep() {
    auto &sequence = _project.selectedIndexedSequence();
    if (!_stepSelection.any()) return;

    int selectedCount = _stepSelection.count();
    if (sequence.activeLength() + selectedCount > IndexedSequence::MaxSteps) {
        showMessage("CANNOT SPLIT: FULL");
        return;
    }

    // Iterate backwards to avoid index shifting issues
    bool splitAny = false;
    for (int i = sequence.activeLength() - 1; i >= 0; --i) {
        if (_stepSelection[i]) {
            sequence.splitStep(i);
            splitAny = true;
        }
    }

    if (splitAny) {
        // Clear selection because indices have shifted
        _stepSelection.clear();
        showMessage("STEPS SPLIT");
    }
}

void IndexedSequenceEditPage::rotateToFirstSelected() {
    auto &sequence = _project.selectedIndexedSequence();
    if (!_stepSelection.any()) return;

    int stepIndex = _stepSelection.first();
    if (stepIndex > 0 && stepIndex < sequence.activeLength()) {
        sequence.rotateSteps(stepIndex);
        _stepSelection.clear();
        FixedStringBuilder<32> msg("ROTATED TO STEP: %d", stepIndex + 1);
        showMessage(msg);
    }
}

void IndexedSequenceEditPage::deleteStep() {
    auto &sequence = _project.selectedIndexedSequence();
    if (!_stepSelection.any()) return;

    bool deletedAny = false;
    for (int i = sequence.activeLength() - 1; i >= 0; --i) {
        if (_stepSelection[i]) {
            sequence.deleteStep(i);
            deletedAny = true;
        }
    }

    if (deletedAny) {
        _stepSelection.clear();
        showMessage("STEPS DELETED");
    }
}

void IndexedSequenceEditPage::mergeStepWithNext() {
    auto &sequence = _project.selectedIndexedSequence();
    if (!_stepSelection.any()) {
        showMessage("NO STEP");
        return;
    }

    int stepIndex = _stepSelection.first();
    if (stepIndex < 0) {
        stepIndex = _stepSelection.firstSetIndex();
    }
    if (stepIndex < 0 || stepIndex >= sequence.activeLength() - 1) {
        showMessage("NO NEXT");
        return;
    }

    auto &current = sequence.step(stepIndex);
    const auto &next = sequence.step(stepIndex + 1);
    uint32_t mergedDuration = current.duration() + next.duration();
    current.setDuration(static_cast<uint16_t>(clamp<uint32_t>(mergedDuration, 0u, IndexedSequence::MaxDuration)));
    sequence.deleteStep(stepIndex + 1);
    _stepSelection.clear();
    showMessage("STEP MERGED");
}

void IndexedSequenceEditPage::swapStepWithOffset(int offset) {
    if (offset <= 0) {
        showMessage("NO SWAP");
        return;
    }

    auto &sequence = _project.selectedIndexedSequence();
    int baseIndex = _swapQuickEditBaseIndex;
    if (baseIndex < 0 || baseIndex >= sequence.activeLength()) {
        showMessage("NO STEP");
        return;
    }

    int targetIndex = baseIndex + offset;
    if (targetIndex >= sequence.activeLength()) {
        showMessage("NO NEXT");
        return;
    }

    auto &baseStep = sequence.step(baseIndex);
    auto &targetStep = sequence.step(targetIndex);

    int8_t baseNote = baseStep.noteIndex();
    uint16_t baseGate = baseStep.gateLength();
    uint16_t baseDuration = baseStep.duration();
    baseStep.setNoteIndex(targetStep.noteIndex());
    baseStep.setGateLength(targetStep.gateLength());
    baseStep.setDuration(targetStep.duration());
    targetStep.setNoteIndex(baseNote);
    targetStep.setGateLength(baseGate);
    targetStep.setDuration(baseDuration);

    showMessage("STEP SWAPPED");
}

void IndexedSequenceEditPage::copyStep() {
    auto &sequence = _project.selectedIndexedSequence();
    if (_stepSelection.any()) {
        ClipBoard::SelectedSteps steps = _stepSelection.selected().to_ullong();
        _model.clipBoard().copyIndexedSequenceSteps(sequence, steps);
        showMessage("STEPS COPIED");
    }
}

void IndexedSequenceEditPage::pasteStep() {
    auto &sequence = _project.selectedIndexedSequence();
    if (_stepSelection.any()) {
        ClipBoard::SelectedSteps steps = _stepSelection.selected().to_ullong();
        _model.clipBoard().pasteIndexedSequenceSteps(sequence, steps);
        showMessage("STEPS PASTED");
    }
}

void IndexedSequenceEditPage::copySequence() {
    _model.clipBoard().copyIndexedSequence(_project.selectedIndexedSequence());
    showMessage("SEQUENCE COPIED");
}

void IndexedSequenceEditPage::pasteSequence() {
    _model.clipBoard().pasteIndexedSequence(_project.selectedIndexedSequence());
    showMessage("SEQUENCE PASTED");
}

void IndexedSequenceEditPage::quickEdit(int index) {
    if (index < 0 || index >= 8) {
        return;
    }

    int item = quickEditItems[index];
    switch (item) {
    case QuickEditSplit:
        if (!_stepSelection.any()) {
            showMessage("NO STEP");
            return;
        }
        splitStep();
        return;
    case QuickEditPiano:
        applyVoicing(true);
        return;
    case QuickEditGuitar:
        applyVoicing(false);
        return;
    case QuickEditSwap:
        return;
    case QuickEditMerge:
        mergeStepWithNext();
        return;
    case QuickEditSetFirst: {
        if (!_stepSelection.any()) {
            showMessage("NO STEP");
            return;
        }
        int stepIndex = _stepSelection.first();
        if (stepIndex < 0) {
            stepIndex = _stepSelection.firstSetIndex();
        }
        if (stepIndex < 0) {
            showMessage("NO STEP");
            return;
        }
        _project.selectedIndexedSequence().setFirstStep(stepIndex);
        showMessage("FIRST STEP");
        return;
    }
    case QuickEditNone:
        return;
    default:
        _listModel.setSequence(&_project.selectedIndexedSequence());
        _manager.pages().quickEdit.show(_listModel, item);
        return;
    }
}

void IndexedSequenceEditPage::startSwapQuickEdit() {
    if (!_stepSelection.any()) {
        showMessage("NO STEP");
        return;
    }

    int stepIndex = -1;
    int preferredOffset = 0;
    if (_stepSelection.count() == 2) {
        int first = _stepSelection.firstSetIndex();
        int second = -1;
        for (int i = first + 1; i < int(_stepSelection.size()); ++i) {
            if (_stepSelection[i]) {
                second = i;
                break;
            }
        }
        if (first >= 0 && second >= 0) {
            stepIndex = first;
            preferredOffset = second - first;
        }
    }

    if (stepIndex < 0) {
        stepIndex = _stepSelection.first();
        if (stepIndex < 0) {
            stepIndex = _stepSelection.firstSetIndex();
        }
    }
    if (stepIndex < 0) {
        showMessage("NO STEP");
        return;
    }

    int maxOffset = _project.selectedIndexedSequence().activeLength() - 1 - stepIndex;
    if (maxOffset <= 0) {
        showMessage("NO NEXT");
        return;
    }

    _swapQuickEditActive = true;
    _swapQuickEditBaseIndex = stepIndex;
    _swapQuickEditOffset = 0;
    _swapQuickEditPreferredOffset = clamp(preferredOffset, 0, maxOffset);
    showMessage("NO SWAP");
}

void IndexedSequenceEditPage::finishSwapQuickEdit() {
    if (!_swapQuickEditActive) {
        return;
    }

    _swapQuickEditActive = false;
    swapStepWithOffset(_swapQuickEditOffset);
    _swapQuickEditBaseIndex = -1;
    _swapQuickEditOffset = 0;
}

IndexedSequence::Step& IndexedSequenceEditPage::step(int index) {
    return _project.selectedIndexedSequence().step(index);
}

const IndexedSequence::Step& IndexedSequenceEditPage::step(int index) const {
    return _project.selectedIndexedSequence().step(index);
}

void IndexedSequenceEditPage::applyVoicing(bool isPiano) {
    if (!_stepSelection.any()) {
        showMessage("NO STEP");
        return;
    }

    auto &sequence = _project.selectedIndexedSequence();

    // Find first selected step to use as root note
    int firstSelectedIndex = _stepSelection.firstSetIndex();
    if (firstSelectedIndex < 0) {
        showMessage("NO STEP");
        return;
    }

    int8_t rootNote = sequence.step(firstSelectedIndex).noteIndex();

    // Get voicing array
    const Voicing *voicings = isPiano ? kPianoVoicings : kGuitarVoicings;
    int voicingCount = isPiano ? kPianoVoicingCount : kGuitarVoicingCount;
    int &voicingIndex = isPiano ? _pianoVoicingIndex : _guitarVoicingIndex;

    // Cycle to next voicing
    voicingIndex = (voicingIndex + 1) % voicingCount;
    const Voicing &voicing = voicings[voicingIndex];

    // Apply voicing to selected steps
    int selectedCount = _stepSelection.count();
    int stepIndex = firstSelectedIndex;

    for (int i = 0; i < selectedCount && i < voicing.count; ++i) {
        // Find next selected step
        while (stepIndex < sequence.activeLength() && !_stepSelection[stepIndex]) {
            stepIndex++;
        }
        if (stepIndex >= sequence.activeLength()) break;

        // Apply interval from voicing relative to root note
        int8_t newNote = rootNote + voicing.semis[i];
        sequence.step(stepIndex).setNoteIndex(newNote);

        stepIndex++;
    }

    // Show voicing name
    FixedStringBuilder<16> msg;
    msg(isPiano ? "PIANO: " : "GUITAR: ");
    msg(voicing.name);
    showMessage(msg);
}

// ============================================================================
// Macro Context Menus
// ============================================================================

enum class RhythmContextAction {
    Euclidean,
    Clave,
    Tuplet,
    Poly,
    RandomRhythm,
    Last
};

static const ContextMenuModel::Item rhythmContextMenuItems[] = {
    { "EUCL" },
    { "CLAVE" },
    { "TUPLET" },
    { "POLY" },
    { "M-RHY" },
};

enum class WaveformContextAction {
    Triangle,
    Sine,
    Sawtooth,
    Pulse,
    Target,
    Last
};

static const ContextMenuModel::Item waveformContextMenuItems[] = {
    { "TRI" },
    { "SINE" },
    { "SAW" },
    { "PULSE" },
    { "TARGET" },
};

enum class MelodicContextAction {
    Scale,
    Arpeggio,
    Chord,
    Modal,
    RandomMelody,
    Last
};

static const ContextMenuModel::Item melodicContextMenuItems[] = {
    { "SCALE" },
    { "ARP" },
    { "CHORD" },
    { "MODAL" },
    { "M-MEL" },
};

enum class DurationTransformContextAction {
    DurationLog,
    DurationExp,
    DurationTriangle,
    Reverse,
    Mirror,
    Last
};

static const ContextMenuModel::Item durationTransformContextMenuItems[] = {
    { "D-LOG" },
    { "D-EXP" },
    { "D-TRI" },
    { "REV" },
    { "MIRR" },
};

void IndexedSequenceEditPage::rhythmContextShow() {
    showContextMenu(ContextMenu(
        rhythmContextMenuItems,
        int(RhythmContextAction::Last),
        [&] (int index) { rhythmContextAction(index); },
        [&] (int index) { return true; }
    ));
}

void IndexedSequenceEditPage::rhythmContextAction(int index) {
    auto &sequence = _project.selectedIndexedSequence();

    // Determine range: use selected steps if any, otherwise full active length
    int firstStep, lastStep;

    if (_stepSelection.any()) {
        firstStep = _stepSelection.firstSetIndex();
        lastStep = _stepSelection.lastSetIndex();
    } else {
        firstStep = 0;
        lastStep = sequence.activeLength() - 1;
    }

    switch (RhythmContextAction(index)) {
    case RhythmContextAction::Euclidean:
        // TODO: Implement Euclidean rhythm generator
        showMessage("EUCLIDEAN - NOT YET IMPLEMENTED");
        break;
    case RhythmContextAction::Clave:
        // TODO: Implement Clave patterns
        showMessage("CLAVE - NOT YET IMPLEMENTED");
        break;
    case RhythmContextAction::Tuplet:
        // TODO: Implement Tuplet subdivision
        showMessage("TUPLET - NOT YET IMPLEMENTED");
        break;
    case RhythmContextAction::Poly:
        // TODO: Implement Polyrhythmic subdivision
        showMessage("POLY - NOT YET IMPLEMENTED");
        break;
    case RhythmContextAction::RandomRhythm:
        // TODO: Implement Random rhythm generator
        showMessage("M-RHY - NOT YET IMPLEMENTED");
        break;
    case RhythmContextAction::Last:
        break;
    }
}

void IndexedSequenceEditPage::waveformContextShow() {
    showContextMenu(ContextMenu(
        waveformContextMenuItems,
        int(WaveformContextAction::Last),
        [&] (int index) { waveformContextAction(index); },
        [&] (int index) { return true; }
    ));
}

void IndexedSequenceEditPage::waveformContextAction(int index) {
    auto &sequence = _project.selectedIndexedSequence();

    // Determine range: use selected steps if any, otherwise full active length
    int firstStep, lastStep;

    if (_stepSelection.any()) {
        firstStep = _stepSelection.firstSetIndex();
        lastStep = _stepSelection.lastSetIndex();
    } else {
        firstStep = 0;
        lastStep = sequence.activeLength() - 1;
    }

    switch (WaveformContextAction(index)) {
    case WaveformContextAction::Triangle:
        // TODO: Implement Triangle waveform
        showMessage("TRI - NOT YET IMPLEMENTED");
        break;
    case WaveformContextAction::Sine:
        // TODO: Implement Sine waveform
        showMessage("SINE - NOT YET IMPLEMENTED");
        break;
    case WaveformContextAction::Sawtooth:
        // TODO: Implement Sawtooth waveform
        showMessage("SAW - NOT YET IMPLEMENTED");
        break;
    case WaveformContextAction::Pulse:
        // TODO: Implement Pulse waveform
        showMessage("PULSE - NOT YET IMPLEMENTED");
        break;
    case WaveformContextAction::Target:
        // TODO: Implement Target parameter selector
        showMessage("TARGET - NOT YET IMPLEMENTED");
        break;
    case WaveformContextAction::Last:
        break;
    }
}

void IndexedSequenceEditPage::melodicContextShow() {
    showContextMenu(ContextMenu(
        melodicContextMenuItems,
        int(MelodicContextAction::Last),
        [&] (int index) { melodicContextAction(index); },
        [&] (int index) { return true; }
    ));
}

void IndexedSequenceEditPage::melodicContextAction(int index) {
    auto &sequence = _project.selectedIndexedSequence();

    // Determine range: use selected steps if any, otherwise full active length
    int firstStep, lastStep;

    if (_stepSelection.any()) {
        firstStep = _stepSelection.firstSetIndex();
        lastStep = _stepSelection.lastSetIndex();
    } else {
        firstStep = 0;
        lastStep = sequence.activeLength() - 1;
    }

    switch (MelodicContextAction(index)) {
    case MelodicContextAction::Scale:
        // TODO: Implement Scale fill
        showMessage("SCALE - NOT YET IMPLEMENTED");
        break;
    case MelodicContextAction::Arpeggio:
        // TODO: Implement Arpeggio patterns
        showMessage("ARP - NOT YET IMPLEMENTED");
        break;
    case MelodicContextAction::Chord:
        // TODO: Implement Chord voicings
        showMessage("CHORD - NOT YET IMPLEMENTED");
        break;
    case MelodicContextAction::Modal:
        // TODO: Implement Modal melodies
        showMessage("MODAL - NOT YET IMPLEMENTED");
        break;
    case MelodicContextAction::RandomMelody:
        // TODO: Implement Random melody generator
        showMessage("M-MEL - NOT YET IMPLEMENTED");
        break;
    case MelodicContextAction::Last:
        break;
    }
}

void IndexedSequenceEditPage::durationTransformContextShow() {
    showContextMenu(ContextMenu(
        durationTransformContextMenuItems,
        int(DurationTransformContextAction::Last),
        [&] (int index) { durationTransformContextAction(index); },
        [&] (int index) { return true; }
    ));
}

void IndexedSequenceEditPage::durationTransformContextAction(int index) {
    auto &sequence = _project.selectedIndexedSequence();

    // Determine range: use selected steps if any, otherwise full active length
    int firstStep, lastStep;

    if (_stepSelection.any()) {
        firstStep = _stepSelection.firstSetIndex();
        lastStep = _stepSelection.lastSetIndex();
    } else {
        firstStep = 0;
        lastStep = sequence.activeLength() - 1;
    }

    switch (DurationTransformContextAction(index)) {
    case DurationTransformContextAction::DurationLog:
        // TODO: Implement Duration logarithmic curve
        showMessage("D-LOG - NOT YET IMPLEMENTED");
        break;
    case DurationTransformContextAction::DurationExp:
        // TODO: Implement Duration exponential curve
        showMessage("D-EXP - NOT YET IMPLEMENTED");
        break;
    case DurationTransformContextAction::DurationTriangle:
        // TODO: Implement Duration triangle curve
        showMessage("D-TRI - NOT YET IMPLEMENTED");
        break;
    case DurationTransformContextAction::Reverse:
        // TODO: Implement Reverse step order
        showMessage("REV - NOT YET IMPLEMENTED");
        break;
    case DurationTransformContextAction::Mirror:
        // TODO: Implement Mirror around midpoint
        showMessage("MIRR - NOT YET IMPLEMENTED");
        break;
    case DurationTransformContextAction::Last:
        break;
    }
}
