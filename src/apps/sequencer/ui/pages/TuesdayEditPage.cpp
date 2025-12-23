#include "TuesdayEditPage.h"

#include "Pages.h"

#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"

#include "model/KnownDivisor.h"
#include "model/ModelUtils.h"

enum class ContextAction {
    Init,
    Reseed,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
    { "RESEED" },
};

TuesdayEditPage::TuesdayEditPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void TuesdayEditPage::enter() {
}

void TuesdayEditPage::exit() {
}

void TuesdayEditPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "STEPS");

    // Draw 4 parameters for current page
    // Use 51px columns to match F-key spacing (256/5 ≈ 51)
    const int colWidth = 51;
    for (int slot = 0; slot < ParamsPerPage; ++slot) {
        int param = paramForPage(_currentPage, slot);
        int x = slot * colWidth;
        drawParam(canvas, x, slot, param);
    }

    // Draw status box on right side
    drawStatusBox(canvas);

    // Draw page indicator between status box and footer, to the right of the status box
    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(Color::Medium);
    FixedStringBuilder<8> pageStr("[%d/%d]", _currentPage + 1, PageCount);
    // Center horizontally in F5 button region (x=204 to x=256, width=52)
    int textWidth = canvas.textWidth(pageStr);
    int centerX = 204 + (52 - textWidth) / 2;
    canvas.drawText(centerX, 50, pageStr);

    // Draw algorithm number indicator above F1 button (same style as page indicator)
    FixedStringBuilder<8> algoStr("[%d]", _project.selectedTuesdaySequence().algorithm());
    int algoTextWidth = canvas.textWidth(algoStr);
    // Center horizontally in F1 button region (x=0 to x=51, width=51)
    int algoCenterX = (51 - algoTextWidth) / 2;
    canvas.drawText(algoCenterX, 50, algoStr);

    // Draw footer with function key labels
    const char *functionNames[5];
    for (int i = 0; i < 4; ++i) {
        int param = paramForPage(_currentPage, i);
        functionNames[i] = (param >= 0 && param < ParamCount) ? paramShortName(param) : "-";
    }
    functionNames[4] = "NEXT";

    WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), _selectedSlot);
}

void TuesdayEditPage::updateLeds(Leds &leds) {
    const auto &sequence = _project.selectedTuesdaySequence();

    constexpr int kDefaultOctave = 0;
    constexpr int kDefaultTranspose = 0;
    constexpr int kDefaultRootNote = -1;
    constexpr int kDefaultDivisor = 12;
    constexpr int kDefaultMaskParam = 0;

    bool octaveUp = sequence.octave() > kDefaultOctave;
    bool octaveDown = sequence.octave() < kDefaultOctave;
    bool transposeUp = sequence.transpose() > kDefaultTranspose;
    bool transposeDown = sequence.transpose() < kDefaultTranspose;
    bool rootUp = sequence.rootNote() > kDefaultRootNote;
    bool rootDown = sequence.rootNote() < kDefaultRootNote;

    bool divisorFaster = sequence.divisor() < kDefaultDivisor;
    bool divisorSlower = sequence.divisor() > kDefaultDivisor;

    bool maskUp = sequence.maskParameter() > kDefaultMaskParam;
    bool maskDown = sequence.maskParameter() < kDefaultMaskParam;

    auto setTop = [&] (int step, bool on) {
        leds.set(MatrixMap::fromStep(step), false, on);
    };
    auto setBottom = [&] (int step, bool on) {
        leds.set(MatrixMap::fromStep(step), on, false);
    };

    setTop(0, octaveUp);
    setBottom(8, octaveDown);

    setTop(1, transposeUp);
    setBottom(9, transposeDown);

    setTop(2, rootUp);
    setBottom(10, rootDown);

    setTop(3, divisorFaster);
    setBottom(11, divisorSlower);

    setTop(4, divisorFaster);
    setBottom(12, divisorSlower);

    setTop(5, divisorFaster);
    setBottom(13, divisorSlower);

    setTop(6, maskUp);
    setBottom(14, maskDown);
}

void TuesdayEditPage::keyDown(KeyEvent &event) {
    const auto &key = event.key();

    if (key.isStep() && !key.pageModifier()) {
        handleStepKeyDown(key.step(), key.shiftModifier());
        event.consume();
        return;
    }

    event.consume();
}

void TuesdayEditPage::keyUp(KeyEvent &event) {
    const auto &key = event.key();

    if (key.isStep() && !key.pageModifier()) {
        handleStepKeyUp(key.step(), key.shiftModifier());
        event.consume();
        return;
    }

    event.consume();
}

void TuesdayEditPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isContextMenu()) {
        showContextMenu(ContextMenu(
            contextMenuItems,
            int(ContextAction::Last),
            [&] (int index) { contextAction(index); },
            [&] (int index) { return true; }
        ));
        event.consume();
        return;
    }

    if (key.isStep() && !key.pageModifier()) {
        handleStepKeyPress(key.step(), key.shiftModifier());
        event.consume();
        return;
    }

    if (key.isFunction()) {
        int fn = key.function();
        if (fn >= 0 && fn < 4) {
            selectParam(fn);
        } else if (fn == 4) {
            // F5 = NEXT page
            if (key.shiftModifier()) {
                // Shift+F5 = Reseed
                auto &engine = const_cast<TuesdayTrackEngine &>(trackEngine());
                engine.reseed();
            } else {
                nextPage();
            }
        }
        event.consume();
        return;
    }

    if (key.isEncoder()) {
        // Could add encoder push functionality here
        event.consume();
        return;
    }
}

void TuesdayEditPage::encoder(EncoderEvent &event) {
    int param = paramForPage(_currentPage, _selectedSlot);
    if (param >= 0 && param < ParamCount) {
        editParam(param, event.value(), event.pressed());
    }
    event.consume();
}

// Private methods

int TuesdayEditPage::paramForPage(int page, int slot) const {
    static const int pageParams[PageCount][ParamsPerPage] = {
        { Algorithm, Flow, Ornament, Power },           // Page 1
        { LoopLength, Rotate, Glide, Skew },            // Page 2
        { GateLength, GateOffset, Trill, Start },       // Page 3
    };

    if (page < 0 || page >= PageCount || slot < 0 || slot >= ParamsPerPage) {
        return -1;
    }
    return pageParams[page][slot];
}

const char *TuesdayEditPage::paramName(int param) const {
    switch (param) {
    case Algorithm:     return "Algorithm";
    case Flow:          return "Flow";
    case Ornament:      return "Ornament";
    case Power:         return "Power";
    case LoopLength:    return "Loop";
    case Rotate:        return "Rotate";
    case Glide:         return "Glide";
    case Skew:          return "Skew";
    case GateLength:    return "Gate Length";
    case GateOffset:    return "Gate Offset";
    case Trill:         return "Trill";
    case Start:         return "Start";
    default:            return "-";
    }
}

const char *TuesdayEditPage::paramShortName(int param) const {
    switch (param) {
    case Algorithm:     return "ALGO";
    case Flow:          return "FLOW";
    case Ornament:      return "ORN";
    case Power:         return "POWER";
    case LoopLength:    return "LOOP";
    case Rotate:        return "ROT";
    case Glide:         return "GLIDE";
    case Skew:          return "SKEW";
    case GateLength:    return "GATE";
    case GateOffset:    return "GOFS";
    case Trill:         return "TRILL";
    case Start:         return "START";
    default:            return "-";
    }
}

void TuesdayEditPage::formatParamValue(int param, StringBuilder &str) const {
    const auto &sequence = _project.selectedTuesdaySequence();

    switch (param) {
    case Algorithm: {
        // Truncate algorithm name to 7 chars max
        FixedStringBuilder<16> fullName;
        sequence.printAlgorithm(fullName);
        const char *name = fullName;
        int len = 0;
        while (name[len] && len < 7) len++;
        for (int i = 0; i < len; i++) str("%c", name[i]);
        break;
    }
    case Flow:
        str("%d", sequence.flow());
        break;
    case Ornament:
        str("%d", sequence.ornament());
        break;
    case Power:
        str("%d", sequence.power());
        break;
    case LoopLength:
        sequence.printLoopLength(str);
        break;
    case Rotate:
        if (sequence.loopLength() == 0) {
            str("N/A");
        } else {
            str("%+d", sequence.rotate());
        }
        break;
    case Glide:
        str("%d%%", sequence.glide());
        break;
    case Skew:
        str("%+d", sequence.skew());
        break;
    case GateLength:
        str("%d%%", sequence.gateLength());
        break;
    case GateOffset:
        str("%d%%", sequence.gateOffset());
        break;
    case Trill:
        str("%d%%", sequence.stepTrill());
        break;
    case Start:
        str("%d", sequence.start());
        break;
    default:
        str("-");
        break;
    }
}

int TuesdayEditPage::paramValue(int param) const {
    const auto &sequence = _project.selectedTuesdaySequence();

    switch (param) {
    case Algorithm:     return sequence.algorithm();
    case Flow:          return sequence.flow();
    case Ornament:      return sequence.ornament();
    case Power:         return sequence.power();
    case LoopLength:    return sequence.loopLength();
    case Rotate:        return sequence.rotate();
    case Glide:         return sequence.glide();
    case Skew:          return sequence.skew();
    case GateLength:    return sequence.gateLength();
    case GateOffset:    return sequence.gateOffset();
    case Trill:         return sequence.stepTrill();
    case Start:         return sequence.start();
    default:            return 0;
    }
}

int TuesdayEditPage::paramMax(int param) const {
    switch (param) {
    case Algorithm:     return 31;
    case Flow:          return 16;
    case Ornament:      return 16;
    case Power:         return 16;
    case LoopLength:    return 29;   // Index 0-29 (0=Inf, 29=128)
    case Rotate:        return 63;   // Bipolar: -63 to +63
    case Glide:         return 100;
    case Skew:          return 8;    // Bipolar: -8 to +8
    case GateLength:    return 100;  // Percentage: 0-100%
    case GateOffset:    return 100;  // Percentage: 0-100%
    case Trill:         return 100;
    case Start:         return 16;
    default:            return 0;
    }
}

bool TuesdayEditPage::paramIsBipolar(int param) const {
    return param == Rotate || param == Skew;
}

void TuesdayEditPage::editParam(int param, int value, bool shift) {
    auto &sequence = _project.selectedTuesdaySequence();

    switch (param) {
    case Algorithm:
        sequence.editAlgorithm(value, shift);
        break;
    case Flow:
        sequence.editFlow(value, shift);
        break;
    case Ornament:
        sequence.editOrnament(value, shift);
        break;
    case Power:
        sequence.editPower(value, shift);
        break;
    case LoopLength:
        sequence.editLoopLength(value, shift);
        break;
    case Rotate:
        if (sequence.loopLength() != 0) {
            sequence.editRotate(value, shift);
        }
        break;
    case Glide:
        sequence.editGlide(value, shift);
        break;
    case Skew:
        sequence.editSkew(value, shift);
        break;
    case GateLength:
        sequence.editGateLength(value, shift);
        break;
    case GateOffset:
        sequence.editGateOffset(value, shift);
        break;
    case Trill:
        sequence.editStepTrill(value, shift);
        break;
    case Start:
        sequence.editStart(value, shift);
        break;
    default:
        break;
    }
}

void TuesdayEditPage::drawParam(Canvas &canvas, int x, int slot, int param) {
    if (param < 0) {
        // Empty slot
        return;
    }

    const int colWidth = 51;  // Match F-key spacing
    // Vertically center in content area (Y=10 to Y=54)
    const int valueY = 26;  // Numbers
    const int barY = 32;    // Bar/algo name line
    const int barHeight = 4;
    const int barWidth = 40;
    const int barX = x + (colWidth - barWidth) / 2;  // Center bar horizontally

    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);

    // For Algorithm: draw name at bar level (not number level)
    if (param == Algorithm) {
        FixedStringBuilder<16> valueStr;
        formatParamValue(param, valueStr);
        int textWidth = canvas.textWidth(valueStr);
        int textX = x + (colWidth - textWidth) / 2;
        canvas.setColor(_selectedSlot == slot ? Color::Bright : Color::Medium);
        canvas.drawText(textX, barY + 3, valueStr);  // +3 to vertically center with bars
        return;
    }

    // For numeric params: draw number above, bar below
    FixedStringBuilder<16> valueStr;
    formatParamValue(param, valueStr);

    int textWidth = canvas.textWidth(valueStr);
    int textX = x + (colWidth - textWidth) / 2;

    canvas.setColor(_selectedSlot == slot ? Color::Bright : Color::Medium);
    canvas.drawText(textX, valueY, valueStr);

    // Draw bar
    int value = paramValue(param);
    int maxValue = paramMax(param);
    bool bipolar = paramIsBipolar(param);

    // Special case: Rotate is N/A for infinite loops
    if (param == Rotate && _project.selectedTuesdaySequence().loopLength() == 0) {
        return;
    }

    drawBar(canvas, barX, barY, barWidth, barHeight, value, maxValue, bipolar);
}

void TuesdayEditPage::drawBar(Canvas &canvas, int x, int y, int width, int height, int value, int maxValue, bool bipolar) {
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(Color::Bright);

    if (bipolar) {
        // Bipolar bar: center point, extends left or right
        int center = x + width / 2;
        if (value > 0) {
            int fillWidth = (value * width / 2) / maxValue;
            canvas.fillRect(center, y, fillWidth, height);
        } else if (value < 0) {
            int fillWidth = (-value * width / 2) / maxValue;
            canvas.fillRect(center - fillWidth, y, fillWidth, height);
        }
        // Draw center tick mark
        canvas.setColor(Color::Medium);
        canvas.vline(center, y, height);
    } else {
        // Unipolar bar: 0 to max
        int fillWidth = (value * width) / maxValue;
        if (fillWidth > 0) {
            canvas.fillRect(x, y, fillWidth, height);
        }
    }
}

void TuesdayEditPage::drawStatusBox(Canvas &canvas) {
    const int boxX = 204;  // Start after 4 columns (51*4=204)
    const int boxY = 14;
    const int boxW = 48;   // Narrower box
    const int boxH = 30;   // Reduced to avoid interference with page indicator

    // Draw box outline
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(Color::Medium);
    canvas.drawRect(boxX, boxY, boxW, boxH);

    canvas.setFont(Font::Tiny);

    const auto &engine = trackEngine();
    // Line 1: Note name + gate indicator
    int noteY = boxY + 7;

    // Get current note from engine
    float cv = engine.cvOutput(0);
    int semitone = int(cv * 12.0f + 0.5f);  // Convert voltage to semitone
    int octave = semitone / 12;
    int note = semitone % 12;
    if (note < 0) {
        note += 12;
        octave--;
    }

    static const char *noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    FixedStringBuilder<8> noteStr("%s%d", noteNames[note], octave);

    canvas.setColor(Color::Bright);
    canvas.drawText(boxX + 4, noteY, noteStr);

    // Gate indicator
    bool gate = engine.gateOutput(0);
    int gateX = boxX + boxW - 10;
    if (gate) {
        canvas.fillRect(gateX, noteY - 4, 5, 5);
    } else {
        canvas.drawRect(gateX, noteY - 4, 5, 5);
    }

    // Line 2: CV voltage
    int cvY = boxY + 15;
    FixedStringBuilder<8> cvStr("%.2fV", cv);
    canvas.setColor(Color::Medium);
    canvas.drawText(boxX + 4, cvY, cvStr);

    // Line 3: Step / Loop
    int stepY = boxY + 23;
    int currentStep = engine.currentStep();
    int loopLen = _project.selectedTuesdaySequence().actualLoopLength();

    FixedStringBuilder<12> stepStr;
    if (loopLen == 0) {
        stepStr("%d", currentStep + 1);
    } else {
        stepStr("%d/%d", (currentStep % loopLen) + 1, loopLen);
    }
    canvas.drawText(boxX + 4, stepY, stepStr);
}

void TuesdayEditPage::nextPage() {
    _currentPage = (_currentPage + 1) % PageCount;
    _selectedSlot = 0;  // Reset to first parameter on new page
}

void TuesdayEditPage::selectParam(int slot) {
    int param = paramForPage(_currentPage, slot);
    if (param >= 0) {
        _selectedSlot = slot;
    }
}

void TuesdayEditPage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Init:
        _project.selectedTuesdaySequence().clear();
        break;
    case ContextAction::Reseed:
        const_cast<TuesdayTrackEngine &>(trackEngine()).reseed();
        break;
    case ContextAction::Last:
        break;
    }
}

namespace {

int nextDivisorByType(int currentDivisor, int direction, char type) {
    if (direction > 0) {
        for (int i = 0; i < numKnownDivisors; ++i) {
            const auto &known = knownDivisors[i];
            if (known.type != type) {
                continue;
            }
            if (known.divisor > currentDivisor) {
                return known.divisor;
            }
        }
    } else if (direction < 0) {
        for (int i = numKnownDivisors - 1; i >= 0; --i) {
            const auto &known = knownDivisors[i];
            if (known.type != type) {
                continue;
            }
            if (known.divisor < currentDivisor) {
                return known.divisor;
            }
        }
    }
    return currentDivisor;
}

} // namespace

void TuesdayEditPage::handleStepKeyPress(int step, bool shift) {
    if (step < 0 || step > 15) {
        return;
    }

    if (step == 15 && shift) {
        return;
    }

    auto &sequence = _project.selectedTuesdaySequence();

    switch (step) {
    case 0: // Octave up
        sequence.editOctave(1, false);
        break;
    case 1: // Transpose up
        sequence.editTranspose(1, false);
        break;
    case 2: // Root note up
        sequence.editRootNote(1, false);
        break;
    case 3: { // Divisor up (straight)
        if (!sequence.isRouted(Routing::Target::Divisor)) {
            int next = nextDivisorByType(sequence.divisor(), -1, '\0');
            sequence.setDivisor(next);
        }
        break;
    }
    case 4: { // Divisor up (triplet)
        if (!sequence.isRouted(Routing::Target::Divisor)) {
            int next = nextDivisorByType(sequence.divisor(), -1, 'T');
            sequence.setDivisor(next);
        }
        break;
    }
    case 5: { // Divisor /2
        if (!sequence.isRouted(Routing::Target::Divisor)) {
            int next = ModelUtils::clampDivisor(sequence.divisor() / 2);
            sequence.setDivisor(next);
        }
        break;
    }
    case 6: // Mask up
        sequence.editMaskParameter(1, false);
        break;
    case 7: // Run momentary (handled in keyDown)
        break;
    case 8: // Octave down
        sequence.editOctave(-1, false);
        break;
    case 9: // Transpose down
        sequence.editTranspose(-1, false);
        break;
    case 10: // Root note down
        sequence.editRootNote(-1, false);
        break;
    case 11: { // Divisor down (straight)
        if (!sequence.isRouted(Routing::Target::Divisor)) {
            int next = nextDivisorByType(sequence.divisor(), 1, '\0');
            sequence.setDivisor(next);
        }
        break;
    }
    case 12: { // Divisor down (triplet)
        if (!sequence.isRouted(Routing::Target::Divisor)) {
            int next = nextDivisorByType(sequence.divisor(), 1, 'T');
            sequence.setDivisor(next);
        }
        break;
    }
    case 13: { // Divisor *2
        if (!sequence.isRouted(Routing::Target::Divisor)) {
            int next = ModelUtils::clampDivisor(sequence.divisor() * 2);
            sequence.setDivisor(next);
        }
        break;
    }
    case 14: // Mask down
        sequence.editMaskParameter(-1, false);
        break;
    case 15: { // Reset
        auto &engine = const_cast<TuesdayTrackEngine &>(trackEngine());
        engine.reset();
        break;
    }
    default:
        break;
    }
}

void TuesdayEditPage::handleStepKeyDown(int step, bool shift) {
    if (step == 7 && !_jamRunHeld) {
        _jamRunHeld = true;
        _jamRunTrack = _project.selectedTrackIndex();
        auto &track = _project.track(_jamRunTrack);
        _jamPrevRunGate = track.runGate();
        track.setRunGate(false);
    }

    if (step == 15 && shift && !_jamMuteHeld) {
        _jamMuteHeld = true;
        _jamMuteTrack = _project.selectedTrackIndex();
        auto &playState = _project.playState();
        _jamPrevMute = playState.trackState(_jamMuteTrack).mute();
        playState.muteTrack(_jamMuteTrack, PlayState::Immediate);
    }
}

void TuesdayEditPage::handleStepKeyUp(int step, bool shift) {
    if (step == 7 && _jamRunHeld) {
        _jamRunHeld = false;
        if (_jamRunTrack >= 0) {
            _project.track(_jamRunTrack).setRunGate(_jamPrevRunGate);
        }
        _jamRunTrack = -1;
    }

    if (step == 15 && shift && _jamMuteHeld) {
        _jamMuteHeld = false;
        if (_jamMuteTrack >= 0) {
            auto &playState = _project.playState();
            if (_jamPrevMute) {
                playState.muteTrack(_jamMuteTrack, PlayState::Immediate);
            } else {
                playState.unmuteTrack(_jamMuteTrack, PlayState::Immediate);
            }
        }
        _jamMuteTrack = -1;
    }
}

const TuesdayTrackEngine &TuesdayEditPage::trackEngine() const {
    return _engine.selectedTrackEngine().as<TuesdayTrackEngine>();
}

TuesdayTrack &TuesdayEditPage::tuesdayTrack() {
    return _project.selectedTrack().tuesdayTrack();
}

const TuesdayTrack &TuesdayEditPage::tuesdayTrack() const {
    return _project.selectedTrack().tuesdayTrack();
}
