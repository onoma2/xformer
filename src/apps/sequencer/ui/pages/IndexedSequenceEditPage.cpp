#include "IndexedSequenceEditPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"
#include "ui/painters/SequencePainter.h"

#include "model/Scale.h"
#include "model/Routing.h"

#include "engine/generators/Generator.h"

#include "core/utils/StringBuilder.h"
#include "core/utils/Random.h"
#include "core/math/Math.h"

#include <algorithm>
#include <cmath>


static void formatGateValue(StringBuilder &str, uint16_t gateValue, uint16_t durationTicks) {
    if (IndexedSequence::gateIsOff(gateValue)) {
        str("OFF");
    } else {
        str("%d", IndexedSequence::gateTicks(gateValue, durationTicks));
    }
}

enum class ContextAction {
    Init,
    Copy,
    Paste,
    Insert,
    MakeFirst,
    Delete,
    Generate,
    Last
};

static const ContextMenuModel::Item seqContextMenuItems[] = {
    { "INIT" },
    { "COPY" },
    { "PASTE" },
    { "GENER" },
};

static const ContextAction seqContextActions[] = {
    ContextAction::Init,
    ContextAction::Copy,
    ContextAction::Paste,
    ContextAction::Generate,
};

static const Generator::Mode indexedGeneratorModes[] = {
    Generator::Mode::InitLayer,
    Generator::Mode::InitSteps,
    Generator::Mode::Euclidean,
    Generator::Mode::Random,
    Generator::Mode::Helical,
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
    bool rootFromC0;
};

static const Voicing kPianoVoicings[] = {
    { "ROOT",    { 0, 0, 0, 0, 0, 0 },    0 },
    { "C0",      { 0, 0, 0, 0, 0, 0 },    0, true },
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
    { "ROOT",  { 0, 0, 0, 0, 0, 0 }, 0 },
    { "C0",    { 0, 0, 0, 0, 0, 0 }, 0, true },
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
    QuickEditSplit,                                   // Step 8
    QuickEditMerge,                                   // Step 9
    QuickEditSwap,                                    // Step 10
    int(IndexedSequenceListModel::Item::RunMode),      // Step 11
    QuickEditPiano,                                   // Step 12
    QuickEditGuitar,                                  // Step 13
    QuickEditNone,                                    // Step 14 (macro)
    QuickEditNone                                     // Step 15
};

IndexedSequenceEditPage::IndexedSequenceEditPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{
    _stepSelection.setStepCompare([this] (int a, int b) {
        const auto &sequence = _project.selectedIndexedSequence();
        const auto &stepA = sequence.step(a);
        const auto &stepB = sequence.step(b);

        switch (_editMode) {
        case EditMode::Note:
            if (_noteSlideEdit) {
                return stepA.slide() == stepB.slide();
            }
            return stepA.noteIndex() == stepB.noteIndex();
        case EditMode::Duration:
            return stepA.duration() == stepB.duration();
        case EditMode::Gate:
            return IndexedSequence::gateTicks(stepA.gateLength(), stepA.duration()) ==
                IndexedSequence::gateTicks(stepB.gateLength(), stepB.duration());
        }
        return false;
    });

}

void IndexedSequenceEditPage::enter() {
    updateMonitorStep();
}

void IndexedSequenceEditPage::exit() {
    _engine.selectedTrackEngine().as<IndexedTrackEngine>().setMonitorStep(-1);
}

void IndexedSequenceEditPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    FixedStringBuilder<20> header("INDEXED EDIT %d/%d", _section + 1, SectionCount);
    WindowPainter::drawHeader(canvas, _model, _engine, header);

    const auto &sequence = _project.selectedIndexedSequence();
    const auto &trackEngine = _engine.selectedTrackEngine().as<IndexedTrackEngine>();

    // 1. Top Section: Timeline Bar
    int totalTicks = 0;
    int activeLength = sequence.activeLength();
    int nonzeroSteps = 0;
    int currentStepIndex = trackEngine.currentStep();

    const int divisor = sequence.divisor();
    for (int i = 0; i < activeLength; ++i) {
        // Use modulated duration for currently playing step, programmed for others
        int duration = sequence.step(i).duration();
        if (i == currentStepIndex) {
            uint16_t effectiveDur = trackEngine.effectiveStepDuration();
            if (effectiveDur > 0) {
                duration = effectiveDur;
            }
        } else {
            duration *= divisor;
        }
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
        const bool twoRows = (minStepW * nonzeroSteps) > barW;
        const int rowH = twoRows ? (barH / 2) : barH;
        const int totalW = twoRows ? (barW * 2) : barW;

        int currentX = barX;
        int row = 0;
        int extraPixels = totalW - minStepW * nonzeroSteps;
        if (extraPixels < 0) {
            extraPixels = 0;
        }
        int error = 0;

        for (int i = 0; i < activeLength; ++i) {
            const auto &step = sequence.step(i);
            bool active = (i == currentStepIndex);

            // Use modulated duration for currently playing step, programmed for others
            int duration = step.duration();
            if (active) {
                uint16_t effectiveDur = trackEngine.effectiveStepDuration();
                if (effectiveDur > 0) {
                    duration = effectiveDur;
                }
            } else {
                duration *= divisor;
            }

            int stepW = 0;
            if (duration > 0) {
                int scaled = extraPixels * duration + error;
                int extraW = scaled / totalTicks;
                error = scaled % totalTicks;
                stepW = minStepW + extraW;
            }

            bool selected = _stepSelection[i];

            // Use modulated gate ticks for currently playing step, programmed for others
            int gateW = 0;
            if (duration > 0) {
                uint32_t gateTicks = active
                    ? trackEngine.effectiveGateTicks()
                    : std::min<uint32_t>(uint32_t(IndexedSequence::gateTicks(step.gateLength(), step.duration()) * divisor),
                                         uint32_t(duration));
                gateW = int((float(stepW) * gateTicks) / float(duration));
            }
            gateW = clamp(gateW, 0, std::max(0, stepW - 2));

            auto drawSegment = [&](int segX, int segY, int segW, int gateRemaining, int xOffset) -> int {
                if (segW <= 0) {
                    return gateRemaining;
                }
                int drawX = segX + xOffset;
                int drawW = segW - xOffset;
                if (drawX < barX) {
                    int shift = barX - drawX;
                    drawX = barX;
                    drawW = std::max(0, drawW - shift);
                }
                canvas.setColor(selected ? Color::Bright : (active ? Color::MediumBright : Color::Medium));
                canvas.drawRect(drawX, segY, drawW, rowH);

                if (gateRemaining > 0 && drawW > 2 && rowH > 2) {
                    int gateSeg = std::min(gateRemaining, drawW - 2);
                    canvas.setColor(selected ? Color::Bright : (active ? Color::MediumBright : Color::Low));
                    canvas.fillRect(drawX + 1, segY + 1, gateSeg, rowH - 2);
                    gateRemaining -= gateSeg;
                }
                return gateRemaining;
            };

            int segGate = gateW;
            int segRow = row;
            int segX = currentX;
            int remaining = stepW;
            const int xOffset = (i > 0) ? -1 : 0;
            while (remaining > 0) {
                int rowY = barY + segRow * rowH + (segRow == 1 ? -1 : 0);
                int rowRemaining = (barX + barW) - segX;
                int segW = std::min(remaining, rowRemaining);
                segGate = drawSegment(segX, rowY, segW, segGate, xOffset);
                remaining -= segW;
                if (remaining > 0 && twoRows && segRow == 0) {
                    segRow = 1;
                    segX = barX;
                    continue;
                }
                break;
            }

            currentX += stepW;
            if (twoRows && currentX >= barX + barW) {
                currentX = barX + (currentX - (barX + barW));
                row = 1;
            }
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
        formatGateValue(gateStr, step.gateLength(), step.duration());
        canvas.drawTextCentered(51, y, 51, 16, gateStr);

        // F3: Note
        FixedStringBuilder<8> noteName;
        const auto &scale = sequence.selectedScale(_project.scale(), _project.scaleRotate());
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
            const auto &scale = sequence.selectedScale(_project.scale(), _project.scaleRotate());
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
            formatGateValue(gateStr, step.gateLength(), step.duration());

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

    int stepOffset = this->stepOffset();

    for (int i = 0; i < 16; ++i) {
        int stepIndex = stepOffset + i;
        if (stepIndex >= sequence.activeLength()) break;

        int ledIndex = MatrixMap::fromStep(i);
        bool selected = _stepSelection[stepIndex];
        bool hasGate = sequence.step(stepIndex).gateLength() != 0;

        // Presence: green, gate: red, selection: yellow.
        leds.set(ledIndex, false, true);
        if (hasGate) {
            leds.set(ledIndex, true, false);
        }
        if (selected) {
            leds.set(ledIndex, true, true);
        }
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

    if (handleContextMenuKey(event)) return;

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

    if (key.isQuickEdit() && !key.shiftModifier()) {
        if (key.quickEdit() == 4 || key.quickEdit() == 5) {
            event.consume();
            return;
        }
        if (key.quickEdit() == 0 || key.quickEdit() == 1) {
            event.consume();
            return;
        }
        quickEdit(key.quickEdit());
        event.consume();
        return;
    }

    if (key.pageModifier()) {
        return;
    }

    // Double-press step to toggle gate between OFF and min ticks
    if (!key.shiftModifier() && key.isStep() && event.count() == 2) {
        auto &sequence = _project.selectedIndexedSequence();
        int stepIndex = stepOffset() + key.step();
        if (stepIndex < sequence.activeLength()) {
            auto &step = sequence.step(stepIndex);
            if (step.gateLength() == IndexedSequence::GateLengthOff) {
                uint16_t firstGate = IndexedSequence::gateEncodeTicksForDuration(
                    IndexedSequence::GateLengthTicksMin,
                    step.duration()
                );
                step.setGateLength(firstGate);
                FixedStringBuilder<16> msg("GATE: ");
                formatGateValue(msg, step.gateLength(), step.duration());
                showMessage(msg);
            } else {
                step.setGateLength(IndexedSequence::GateLengthOff);
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
            updateMonitorStep();
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
        _section = std::min(SectionCount - 1, _section + 1);
        event.consume();
    }
}

void IndexedSequenceEditPage::encoder(EncoderEvent &event) {
    auto &sequence = _project.selectedIndexedSequence();

    if (_splitQuickEditActive) {
        _splitQuickEditCount = clamp(_splitQuickEditCount + event.value(), 2, 8);
        FixedStringBuilder<16> msg("SPLIT: %d", _splitQuickEditCount);
        showMessage(msg);
        event.consume();
        return;
    }

    if (_mergeQuickEditActive) {
        int baseIndex = _stepSelection.first();
        int maxMerge = sequence.activeLength() - baseIndex;
        _mergeQuickEditCount = clamp(_mergeQuickEditCount + event.value(), 2, maxMerge);
        FixedStringBuilder<16> msg("MERGE: %d", _mergeQuickEditCount);
        showMessage(msg);
        event.consume();
        return;
    }

    if (_voicingQuickEditActive) {
        const int count = (_voicingQuickEditBank == VoicingBank::Piano) ? kPianoVoicingCount : kGuitarVoicingCount;
        int next = clamp(_voicingQuickEditIndex + event.value(), -1, count - 1);
        if (next != _voicingQuickEditIndex) {
            _voicingQuickEditIndex = next;
            _voicingQuickEditDirty = true;
            showVoicingMessage(_voicingQuickEditBank, _voicingQuickEditIndex);
        }
        event.consume();
        return;
    }

    if (_swapQuickEditActive) {
        int maxOffset = sequence.activeLength() - 1 - _swapQuickEditBaseIndex;
        if (maxOffset <= 0) {
            showMessage("NO NEXT");
            _swapQuickEditActive = false;
            event.consume();
            return;
        }
        int nextOffset = _swapQuickEditOffset + event.value();
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
    const auto &scale = sequence.selectedScale(_project.scale(), _project.scaleRotate());

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
                        uint16_t duration = lastStep.duration();
                        if (duration > 0 && currentGate >= duration && event.value() < 0) {
                            if (duration <= IndexedSequence::GateLengthTicksMin) {
                                lastStep.setGateLength(IndexedSequence::GateLengthOff);
                            } else {
                                lastStep.setGateLength(IndexedSequence::gateEncodeTicks(duration - 1));
                            }
                        } else {
                            int newGate = currentGate + event.value();
                            lastStep.setGateLength(clamp(newGate, 0, int(IndexedSequence::GateLengthMax)));
                        }
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
                    int startTicks = IndexedSequence::gateTicks(firstStep.gateLength(), firstStep.duration());
                    int endTicks = IndexedSequence::gateTicks(lastStep.gateLength(), lastStep.duration());
                    int interpolated = startTicks + std::round(t * (endTicks - startTicks));
                    step.setGateLength(IndexedSequence::gateEncodeTicksForDuration(interpolated, step.duration()));
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
                        uint16_t duration = step.duration();
                        if (duration > 0 && currentGate >= duration && event.value() < 0) {
                            if (duration <= IndexedSequence::GateLengthTicksMin) {
                                step.setGateLength(IndexedSequence::GateLengthOff);
                            } else {
                                step.setGateLength(IndexedSequence::gateEncodeTicks(duration - 1));
                            }
                        } else {
                            int newGate = currentGate + event.value() * stepSize;
                            step.setGateLength(clamp(newGate, 0, int(IndexedSequence::GateLengthMax)));
                        }
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
        if (key.quickEdit() == 0) {
            startSplitQuickEdit();
            event.consume();
            return;
        }
        if (key.quickEdit() == 1) {
            startMergeQuickEdit();
            event.consume();
            return;
        }
        if (key.quickEdit() == 4) {
            startVoicingQuickEdit(VoicingBank::Piano, key.quickEdit() + 8);
            event.consume();
            return;
        }
        if (key.quickEdit() == 5) {
            startVoicingQuickEdit(VoicingBank::Guitar, key.quickEdit() + 8);
            event.consume();
            return;
        }
        if (key.quickEdit() == 2) {
            startSwapQuickEdit();
            event.consume();
            return;
        }
    }

    if (key.isStep()) {
        _stepSelection.keyDown(event, stepOffset());
        updateMonitorStep();
    }

    if (key.isFunction()) {
        int fn = key.function();

        if (_functionMode == FunctionMode::Groups) {
            // Groups mode: F1-F4 toggle group membership (A-D)
            if (fn >= 0 && fn < 4) {
                auto &sequence = _project.selectedIndexedSequence();

                if (_stepSelection.any()) {
                    // Steps selected: toggle group membership
                    for (int i = 0; i < IndexedSequence::MaxSteps; ++i) {
                        if (_stepSelection[i]) {
                            sequence.step(i).toggleGroup(fn);
                        }
                    }
                } else {
                    // No steps selected: select all steps in this group
                    int groupBit = (1 << fn);
                    int selectedCount = 0;
                    for (int i = 0; i < sequence.activeLength(); ++i) {
                        if (sequence.step(i).groupMask() & groupBit) {
                            _stepSelection.select(i);
                            selectedCount++;
                        }
                    }
                    if (selectedCount > 0) {
                        FixedStringBuilder<16> msg;
                        msg("GRP %c: %d", 'A' + fn, selectedCount);
                        showMessage(msg);
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
        updateMonitorStep();
    }
}

void IndexedSequenceEditPage::keyUp(KeyEvent &event) {
    const auto &key = event.key();
    if (_splitQuickEditActive) {
        if (key.isPage() || (key.isStep() && key.step() == 8)) {
            finishSplitQuickEdit();
            event.consume();
            return;
        }
    }
    if (_mergeQuickEditActive) {
        if (key.isPage() || (key.isStep() && key.step() == 9)) {
            finishMergeQuickEdit();
            event.consume();
            return;
        }
    }
    if (_voicingQuickEditActive) {
        if (key.isPage() || (key.isStep() && key.step() == _voicingQuickEditStep)) {
            finishVoicingQuickEdit();
            event.consume();
            return;
        }
    }
    if (_swapQuickEditActive) {
        if (key.isPage() || (key.isStep() && key.step() == 10)) {
            finishSwapQuickEdit();
            event.consume();
            return;
        }
    }

    if (key.isStep()) {
        _stepSelection.keyUp(event, stepOffset());
        updateMonitorStep();
    }
}

void IndexedSequenceEditPage::updateMonitorStep() {
    auto &trackEngine = _engine.selectedTrackEngine().as<IndexedTrackEngine>();

    if (_editMode == EditMode::Note && !_stepSelection.isPersisted() && _stepSelection.any()) {
        trackEngine.setMonitorStep(_stepSelection.first());
    } else {
        trackEngine.setMonitorStep(-1);
    }
}

void IndexedSequenceEditPage::contextShow(bool doubleClick) {
    if (_contextMode == ContextMode::Sequence) {
        showContextMenu(ContextMenu(
            seqContextMenuItems,
            4, // count
            [&] (int index) { contextAction(static_cast<int>(seqContextActions[index])); },
            [&] (int index) { return contextActionEnabled(static_cast<int>(seqContextActions[index])); },
            doubleClick
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
    case ContextAction::Generate:
        generateSequence();
        break;
    default:
        break;
    }
}

void IndexedSequenceEditPage::generateSequence() {
    IndexedSequenceBuilder::Layer layer;
    switch (_editMode) {
    case EditMode::Duration: layer = IndexedSequenceBuilder::Layer::Duration; break;
    case EditMode::Gate:     layer = IndexedSequenceBuilder::Layer::GateLength; break;
    case EditMode::Note:
    default:                 layer = IndexedSequenceBuilder::Layer::NoteIndex; break;
    }

    _manager.pages().generatorSelect.show([this, layer] (bool success, Generator::Mode mode) {
        if (success) {
            auto &sequence = _project.selectedIndexedSequence();
            const auto &scale = sequence.selectedScale(_project.scale(), _project.scaleRotate());
            int rootNote = sequence.rootNote() < 0 ? _project.rootNote() : sequence.rootNote();
            auto builder = _builderContainer.create<IndexedSequenceBuilder>(sequence, layer, scale, rootNote);
            auto generator = Generator::execute(mode, *builder);
            if (generator) {
                _manager.pages().generator.show(generator, &_stepSelection);
            }
        }
    }, indexedGeneratorModes, int(sizeof(indexedGeneratorModes) / sizeof(indexedGeneratorModes[0])));
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
        return;
    case QuickEditPiano:
        applyVoicing(VoicingBank::Piano, _pianoVoicingIndex);
        return;
    case QuickEditGuitar:
        applyVoicing(VoicingBank::Guitar, _guitarVoicingIndex);
        return;
    case QuickEditSwap:
        return;
    case QuickEditMerge:
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

void IndexedSequenceEditPage::startSplitQuickEdit() {
    if (!_stepSelection.any()) {
        showMessage("NO STEP");
        return;
    }

    _splitQuickEditActive = true;
    _splitQuickEditCount = 2;
    showMessage("SPLIT: 2");
}

void IndexedSequenceEditPage::finishSplitQuickEdit() {
    if (!_splitQuickEditActive) {
        return;
    }

    _splitQuickEditActive = false;
    splitStepInto(_splitQuickEditCount);
    _splitQuickEditCount = 2;
}

void IndexedSequenceEditPage::splitStepInto(int pieces) {
    auto &sequence = _project.selectedIndexedSequence();

    // Check capacity
    int selectedCount = _stepSelection.count();
    int newSteps = selectedCount * (pieces - 1);
    if (sequence.activeLength() + newSteps > IndexedSequence::MaxSteps) {
        showMessage("CANNOT SPLIT: FULL");
        return;
    }

    // Iterate backwards to avoid index shifting issues
    bool splitAny = false;
    for (int i = sequence.activeLength() - 1; i >= 0; --i) {
        if (_stepSelection[i]) {
            auto &originalStep = sequence.step(i);
            uint16_t totalDur = originalStep.duration();
            uint16_t baseDur = totalDur / pieces;
            uint16_t remainder = totalDur % pieces;

            // First piece keeps the original position
            originalStep.setDuration(baseDur + (remainder > 0 ? 1 : 0));

            // Insert remaining pieces
            for (int p = 1; p < pieces; ++p) {
                sequence.insertStep(i + 1);
                auto &newStep = sequence.step(i + p);
                newStep.setNoteIndex(originalStep.noteIndex());
                newStep.setGateLength(originalStep.gateLength());
                newStep.setSlide(originalStep.slide());
                newStep.setGroupMask(originalStep.groupMask());
                newStep.setDuration(baseDur + (p < remainder ? 1 : 0));
            }
            splitAny = true;
        }
    }

    if (splitAny) {
        _stepSelection.clear();
        FixedStringBuilder<32> msg("SPLIT: %d PIECES", pieces);
        showMessage(msg);
    }
}

void IndexedSequenceEditPage::startMergeQuickEdit() {
    if (!_stepSelection.any()) {
        showMessage("NO STEP");
        return;
    }

    int baseIndex = _stepSelection.first();
    auto &sequence = _project.selectedIndexedSequence();
    int maxMerge = sequence.activeLength() - baseIndex;

    if (maxMerge < 2) {
        showMessage("NO NEXT");
        return;
    }

    _mergeQuickEditActive = true;
    _mergeQuickEditCount = 2;
    showMessage("MERGE: 2");
}

void IndexedSequenceEditPage::finishMergeQuickEdit() {
    if (!_mergeQuickEditActive) {
        return;
    }

    _mergeQuickEditActive = false;
    mergeSteps(_mergeQuickEditCount);
    _mergeQuickEditCount = 2;
}

void IndexedSequenceEditPage::mergeSteps(int count) {
    auto &sequence = _project.selectedIndexedSequence();
    int baseIndex = _stepSelection.first();

    if (baseIndex + count > sequence.activeLength()) {
        showMessage("NOT ENOUGH");
        return;
    }

    // Sum durations
    uint32_t totalDuration = 0;
    for (int i = 0; i < count; ++i) {
        totalDuration += sequence.step(baseIndex + i).duration();
    }

    // Update first step
    sequence.step(baseIndex).setDuration(
        clamp<uint32_t>(totalDuration, 0, IndexedSequence::MaxDuration)
    );

    // Delete following steps
    for (int i = 0; i < count - 1; ++i) {
        sequence.deleteStep(baseIndex + 1);
    }

    _stepSelection.clear();
    FixedStringBuilder<32> msg("MERGED: %d STEPS", count);
    showMessage(msg);
}

IndexedSequence::Step& IndexedSequenceEditPage::step(int index) {
    return _project.selectedIndexedSequence().step(index);
}

const IndexedSequence::Step& IndexedSequenceEditPage::step(int index) const {
    return _project.selectedIndexedSequence().step(index);
}

void IndexedSequenceEditPage::startVoicingQuickEdit(VoicingBank bank, int stepIndex) {
    _voicingQuickEditActive = true;
    _voicingQuickEditBank = bank;
    _voicingQuickEditStep = stepIndex;
    _voicingQuickEditIndex = (bank == VoicingBank::Piano) ? _pianoVoicingIndex : _guitarVoicingIndex;
    _voicingQuickEditDirty = false;
    showVoicingMessage(bank, _voicingQuickEditIndex);
}

void IndexedSequenceEditPage::finishVoicingQuickEdit() {
    if (!_voicingQuickEditActive) {
        return;
    }

    _voicingQuickEditActive = false;
    if (_voicingQuickEditBank == VoicingBank::Piano) {
        _pianoVoicingIndex = _voicingQuickEditIndex;
    } else {
        _guitarVoicingIndex = _voicingQuickEditIndex;
    }
    if (_voicingQuickEditDirty) {
        applyVoicing(_voicingQuickEditBank, _voicingQuickEditIndex);
    }
}

void IndexedSequenceEditPage::showVoicingMessage(VoicingBank bank, int voicingIndex) {
    FixedStringBuilder<16> msg;
    const char *bankName = (bank == VoicingBank::Piano) ? "PIANO" : "GUITAR";
    if (voicingIndex < 0) {
        msg("%s OFF", bankName);
        showMessage(msg);
        return;
    }

    const Voicing *voicings = (bank == VoicingBank::Piano) ? kPianoVoicings : kGuitarVoicings;
    const int count = (bank == VoicingBank::Piano) ? kPianoVoicingCount : kGuitarVoicingCount;
    int index = clamp(voicingIndex, 0, count - 1);
    msg("%s %s", bankName, voicings[index].name);
    showMessage(msg);
}

void IndexedSequenceEditPage::applyVoicing(VoicingBank bank, int voicingIndex) {
    auto &sequence = _project.selectedIndexedSequence();
    int activeLength = sequence.activeLength();
    if (activeLength <= 0) {
        showMessage("NO STEP");
        return;
    }

    int firstSelectedIndex = _stepSelection.firstSetIndex();
    if (firstSelectedIndex < 0 || firstSelectedIndex >= activeLength) {
        firstSelectedIndex = 0;
    }

    if (voicingIndex < 0) {
        showVoicingMessage(bank, voicingIndex);
        return;
    }

    const Voicing *voicings = (bank == VoicingBank::Piano) ? kPianoVoicings : kGuitarVoicings;
    const int count = (bank == VoicingBank::Piano) ? kPianoVoicingCount : kGuitarVoicingCount;
    int index = clamp(voicingIndex, 0, count - 1);
    const auto &voicing = voicings[index];

    const Scale &scale = sequence.selectedScale(_project.scale(), _project.scaleRotate());
    int rootIndex = sequence.step(firstSelectedIndex).noteIndex();
    if (voicing.rootFromC0) {
        int rootNote = sequence.rootNote();
        if (rootNote < 0) {
            rootNote = _project.rootNote();
        }
        rootIndex = scale.isChromatic() ? -rootNote : 0;
    }

    int selectionCount = _stepSelection.count();
    bool limitToSelection = selectionCount > 1;
    int targetIndices[IndexedSequence::MaxSteps];
    int targetCount = 0;

    for (int i = 0; i < activeLength; ++i) {
        if (limitToSelection && !_stepSelection[i]) {
            continue;
        }
        targetIndices[targetCount++] = i;
    }

    if (targetCount == 0) {
        showMessage("NO STEP");
        return;
    }

    for (int i = 0; i < targetCount; ++i) {
        int cycle = voicing.count > 0 ? i / voicing.count : 0;
        int pos = voicing.count > 0 ? i % voicing.count : 0;
        int transpose = 0;
        if (cycle % 3 == 1) {
            transpose = 7;
        } else if (cycle % 3 == 2) {
            transpose = -7;
        }
        int semis = int(voicing.semis[pos]) + transpose;
        float volts = float(semis) / 12.f;
        int degree = scale.noteFromVolts(volts);
        int noteIndex = rootIndex + degree;
        sequence.step(targetIndices[i]).setNoteIndex(noteIndex);
    }

    showVoicingMessage(bank, index);
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
    { "3/9" },
    { "5/20" },
    { "7/28" },
    { "3-5/5-7" },
    { "M-TALA" },
};

enum class WaveformContextAction {
    Triangle,
    Triangle2,
    Triangle3,
    Saw2,
    Saw3,
    Last
};

static const ContextMenuModel::Item waveformContextMenuItems[] = {
    { "TRI" },
    { "2TRI" },
    { "3TRI" },
    { "2SAW" },
    { "3SAW" },
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
    QuantizeMeasure,
    Reverse,
    Mirror,
    Last
};

static const ContextMenuModel::Item durationTransformContextMenuItems[] = {
    { "D-LOG" },
    { "D-EXP" },
    { "Q-MEAS" },
    { "REV" },
    { "MIRR" },
};

void IndexedSequenceEditPage::getMacroRange(int &firstStep, int &lastStep) const {
    auto &sequence = _project.selectedIndexedSequence();
    firstStep = 0;
    lastStep = sequence.activeLength() - 1;
    if (_stepSelection.any()) {
        firstStep = _stepSelection.firstSetIndex();
        lastStep = _stepSelection.lastSetIndex();
    }
}

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
    int firstStep, lastStep;
    getMacroRange(firstStep, lastStep);
    if (firstStep > lastStep) {
        showMessage("NO STEP");
        return;
    }

    switch (RhythmContextAction(index)) {
    case RhythmContextAction::Euclidean: {
        static bool longCycle = false;
        int steps = longCycle ? 9 : 3;
        sequence.populateWithMacroRhythm(firstStep, lastStep, &steps, 1);
        showMessage(longCycle ? "3/9 9" : "3/9 3");
        longCycle = !longCycle;
        break;
    }
    case RhythmContextAction::Clave: {
        static bool longCycle = false;
        int steps = longCycle ? 20 : 5;
        sequence.populateWithMacroRhythm(firstStep, lastStep, &steps, 1);
        showMessage(longCycle ? "5/20 20" : "5/20 5");
        longCycle = !longCycle;
        break;
    }
    case RhythmContextAction::Tuplet: {
        static bool longCycle = false;
        int steps = longCycle ? 28 : 7;
        sequence.populateWithMacroRhythm(firstStep, lastStep, &steps, 1);
        showMessage(longCycle ? "7/28 28" : "7/28 7");
        longCycle = !longCycle;
        break;
    }
    case RhythmContextAction::Poly: {
        static bool longCycle = false;
        if (longCycle) {
            int groups[] = {5, 7};
            sequence.populateWithMacroRhythm(firstStep, lastStep, groups, 2);
            showMessage("5-7");
        } else {
            int groups[] = {3, 5};
            sequence.populateWithMacroRhythm(firstStep, lastStep, groups, 2);
            showMessage("3-5");
        }
        longCycle = !longCycle;
        break;
    }
    case RhythmContextAction::RandomRhythm: {
        static int patternIndex = 0;
        switch (patternIndex) {
        case 0: {
            int groups[] = {5, 7};
            sequence.populateWithMacroRhythm(firstStep, lastStep, groups, 2);
            showMessage("M-KHAND");
            break;
        }
        case 1: {
            int groups[] = {2, 1, 2, 2, 1, 2, 2, 1, 2};
            sequence.populateWithMacroRhythm(firstStep, lastStep, groups, 9);
            showMessage("M-TIHAI");
            break;
        }
        case 2: {
            int groups[] = {5, 2, 3, 4};
            sequence.populateWithMacroRhythm(firstStep, lastStep, groups, 4);
            showMessage("M-DHAMAR");
            break;
        }
        }
        patternIndex = (patternIndex + 1) % 3;
        break;
    }
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
    int firstStep, lastStep;
    getMacroRange(firstStep, lastStep);
    if (firstStep > lastStep) {
        showMessage("NO STEP");
        return;
    }

    switch (WaveformContextAction(index)) {
    case WaveformContextAction::Triangle:
        sequence.populateWithMacroTriangle(firstStep, lastStep, 1);
        showMessage("WAVEFORM: TRI");
        break;
    case WaveformContextAction::Triangle2:
        sequence.populateWithMacroTriangle(firstStep, lastStep, 2);
        showMessage("WAVEFORM: 2TRI");
        break;
    case WaveformContextAction::Triangle3:
        sequence.populateWithMacroTriangle(firstStep, lastStep, 3);
        showMessage("WAVEFORM: 3TRI");
        break;
    case WaveformContextAction::Saw2:
        sequence.populateWithMacroSawtooth(firstStep, lastStep, 2);
        showMessage("WAVEFORM: 2SAW");
        break;
    case WaveformContextAction::Saw3:
        sequence.populateWithMacroSawtooth(firstStep, lastStep, 3);
        showMessage("WAVEFORM: 3SAW");
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
    int firstStep, lastStep;
    getMacroRange(firstStep, lastStep);
    if (firstStep > lastStep) {
        showMessage("NO STEP");
        return;
    }

    switch (MelodicContextAction(index)) {
    case MelodicContextAction::Scale: {
        static bool ascending = true;
        sequence.populateWithMacroScale(firstStep, lastStep, ascending);
        showMessage(ascending ? "SCALE: DESC" : "SCALE: ASC");
        ascending = !ascending;
        break;
    }
    case MelodicContextAction::Arpeggio: {
        static int patternIndex = 0;
        sequence.populateWithMacroArpeggio(firstStep, lastStep, patternIndex);
        const char *names[] = {"UP", "DOWN", "UP-DN", "TRIAD"};
        FixedStringBuilder<16> msg("ARP: ");
        msg(names[patternIndex]);
        showMessage(msg);
        patternIndex = (patternIndex + 1) % 4;
        break;
    }
    case MelodicContextAction::Chord: {
        static int progressionIndex = 0;
        sequence.populateWithMacroChord(firstStep, lastStep, progressionIndex);
        const char *names[] = {"I-IV-V", "I-V-vi", "ii-V-I"};
        FixedStringBuilder<16> msg("CHORD: ");
        msg(names[progressionIndex]);
        showMessage(msg);
        progressionIndex = (progressionIndex + 1) % 3;
        break;
    }
    case MelodicContextAction::Modal: {
        static int modeIndex = 0;
        sequence.populateWithMacroModal(firstStep, lastStep, modeIndex);
        const char *names[] = {"DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN"};
        FixedStringBuilder<16> msg("MODAL: ");
        msg(names[modeIndex]);
        showMessage(msg);
        modeIndex = (modeIndex + 1) % 4;
        break;
    }
    case MelodicContextAction::RandomMelody:
        sequence.populateWithMacroRandomMelody(firstStep, lastStep);
        showMessage("RANDOM MELODY");
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
    int firstStep, lastStep;
    getMacroRange(firstStep, lastStep);
    if (firstStep > lastStep) {
        showMessage("NO STEP");
        return;
    }

    switch (DurationTransformContextAction(index)) {
    case DurationTransformContextAction::DurationLog:
        sequence.transformDurationLog(firstStep, lastStep);
        showMessage("DURATION LOG");
        break;
    case DurationTransformContextAction::DurationExp:
        sequence.transformDurationExp(firstStep, lastStep);
        showMessage("DURATION EXP");
        break;
    case DurationTransformContextAction::QuantizeMeasure: {
        sequence.transformQuantizeMeasure(firstStep, lastStep);
        uint32_t totalDuration = 0;
        for (int i = firstStep; i <= lastStep; ++i) {
            totalDuration += sequence.step(i).duration();
        }
        const uint32_t barTicks = 768;
        int bars = int(totalDuration / barTicks);
        FixedStringBuilder<32> msg("Q-MEAS: %d BAR%s", bars, bars == 1 ? "" : "S");
        showMessage(msg);
        break;
    }
    case DurationTransformContextAction::Reverse:
        sequence.transformReverse(firstStep, lastStep);
        showMessage("REVERSED");
        break;
    case DurationTransformContextAction::Mirror:
        sequence.transformMirror(firstStep, lastStep);
        showMessage("MIRRORED");
        break;
    case DurationTransformContextAction::Last:
        break;
    }
}
