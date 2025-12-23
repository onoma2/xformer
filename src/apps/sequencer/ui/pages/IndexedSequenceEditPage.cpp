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
    Split,
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
    { "SPLIT" },
    { "DELETE" },
    { "COPY" },
    { "PASTE" },
};

static const ContextAction stepContextActions[] = {
    ContextAction::Insert,
    ContextAction::Split,
    ContextAction::Delete,
    ContextAction::Copy,
    ContextAction::Paste,
};

enum {
    QuickEditNone = -1,
    QuickEditSplit = -2,
    QuickEditSwap = -3,
    QuickEditMerge = -4,
};

static const int quickEditItems[8] = {
    QuickEditSplit,                                   // Step 9
    QuickEditSwap,                                    // Step 10
    QuickEditMerge,                                   // Step 11
    int(IndexedSequenceListModel::Item::RunMode),     // Step 12
    QuickEditNone,
    QuickEditNone,
    QuickEditNone,
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

        // F1: Note
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
        noteStr("%.2f %s", volts, static_cast<const char *>(noteName));
        canvas.drawTextCentered(0, y, 51, 16, noteStr);

        // F2: Duration
        FixedStringBuilder<16> durStr;
        durStr("%d", step.duration());
        canvas.drawTextCentered(51, y, 51, 16, durStr);

        // F3: Gate
        FixedStringBuilder<16> gateStr;
        if (step.gateLength() == IndexedSequence::GateLengthTrigger) {
            gateStr("T");
        } else {
            gateStr("%d%%", step.gateLength());
        }
        canvas.drawTextCentered(102, y, 51, 16, gateStr);
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
            canvas.setColor(Color::Medium);
            FixedStringBuilder<48> info;
            info("STEP %d/%d  %.2f %s %d  %s", currentStep, totalSteps, volts, static_cast<const char *>(noteName), step.duration(), static_cast<const char *>(gateStr));
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
        footerLabels[0] = "NOTE";
        footerLabels[1] = _durationTransfer ? "DUR-TR" : "DUR";
        footerLabels[2] = "GATE";
        footerLabels[3] = (_contextMode == ContextMode::Sequence) ? "SEQ" : "STEP";
        footerLabels[4] = shift ? "ROUTES" : "MATH";
    }
    WindowPainter::drawFooter(canvas, footerLabels, pageKeyState(), (_functionMode == FunctionMode::Groups) ? -1 : (int)_editMode);
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

    if (key.pageModifier()) {
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
        _swapQuickEditOffset = clamp(_swapQuickEditOffset + event.value(), 0, maxOffset);
        if (_swapQuickEditOffset == 0) {
            showMessage("NO SWAP");
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
            int minDelta = std::max(-cur, next - 65535);
            int maxDelta = std::min(65535 - cur, next);
            int clampedDelta = clamp(delta, minDelta, maxDelta);

            step.setDuration(cur + clampedDelta);
            nextStep.setDuration(next - clampedDelta);
        }
        event.consume();
        return;
    }

    for (int i = 0; i < IndexedSequence::MaxSteps; ++i) {
        if (_stepSelection[i]) {
            auto &step = sequence.step(i);
            bool shift = globalKeyState()[Key::Shift];

            switch (_editMode) {
            case EditMode::Note:
                step.setNoteIndex(step.noteIndex() + event.value() * (shift ? 12 : 1));
                break;
            case EditMode::Duration: {
                int div = sequence.divisor();
                int stepVal = shift ? div : 1;
                int newDur = step.duration() + event.value() * stepVal;
                step.setDuration(clamp(newDur, 0, 65535));
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
                _editMode = EditMode::Note;
                _durationTransfer = false;
            }
            if (fn == 1) {
                if (_editMode == EditMode::Duration) {
                    _durationTransfer = !_durationTransfer;
                } else {
                    _editMode = EditMode::Duration;
                    _durationTransfer = false;
                }
            }
            if (fn == 2) {
                _editMode = EditMode::Gate;
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
    case ContextAction::Split:
        splitStep();
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
    case ContextAction::Split:
        return sequence.canInsert();
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
    current.setDuration(static_cast<uint16_t>(clamp<uint32_t>(mergedDuration, 0u, 65535u)));
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
    case QuickEditSwap:
        return;
    case QuickEditMerge:
        mergeStepWithNext();
        return;
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

    int stepIndex = _stepSelection.first();
    if (stepIndex < 0) {
        stepIndex = _stepSelection.firstSetIndex();
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
