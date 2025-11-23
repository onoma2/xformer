#include "TuesdayEditPage.h"

#include "Pages.h"

#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"

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
    _currentPage = 0;
    _selectedSlot = 0;
}

void TuesdayEditPage::exit() {
}

void TuesdayEditPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "STEPS");

    const auto &track = tuesdayTrack();

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
    // LEDs handled by footer highlight mechanism
    (void)leds;
}

void TuesdayEditPage::keyDown(KeyEvent &event) {
    event.consume();
}

void TuesdayEditPage::keyUp(KeyEvent &event) {
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
        { LoopLength, Scan, Rotate, Glide },            // Page 2
        { Skew, GateOffset, CvUpdateMode, -1 },         // Page 3
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
    case Scan:          return "Scan";
    case Rotate:        return "Rotate";
    case Glide:         return "Glide";
    case Skew:          return "Skew";
    case GateOffset:    return "Gate Offset";
    case CvUpdateMode:  return "CV Mode";
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
    case Scan:          return "SCAN";
    case Rotate:        return "ROT";
    case Glide:         return "GLIDE";
    case Skew:          return "SKEW";
    case GateOffset:    return "GOFS";
    case CvUpdateMode:  return "CVUPD";
    default:            return "-";
    }
}

void TuesdayEditPage::formatParamValue(int param, StringBuilder &str) const {
    const auto &track = tuesdayTrack();

    switch (param) {
    case Algorithm: {
        // Truncate algorithm name to 7 chars max
        FixedStringBuilder<16> fullName;
        track.printAlgorithm(fullName);
        const char *name = fullName;
        int len = 0;
        while (name[len] && len < 7) len++;
        for (int i = 0; i < len; i++) str("%c", name[i]);
        break;
    }
    case Flow:
        str("%d", track.flow());
        break;
    case Ornament:
        str("%d", track.ornament());
        break;
    case Power:
        str("%d", track.power());
        break;
    case LoopLength:
        track.printLoopLength(str);
        break;
    case Scan:
        str("%d", track.scan());
        break;
    case Rotate:
        if (track.loopLength() == 0) {
            str("N/A");
        } else {
            str("%+d", track.rotate());
        }
        break;
    case Glide:
        str("%d%%", track.glide());
        break;
    case Skew:
        str("%+d", track.skew());
        break;
    case GateOffset:
        str("%d%%", track.gateOffset());
        break;
    case CvUpdateMode:
        track.printCvUpdateMode(str);
        break;
    default:
        str("-");
        break;
    }
}

int TuesdayEditPage::paramValue(int param) const {
    const auto &track = tuesdayTrack();

    switch (param) {
    case Algorithm:     return track.algorithm();
    case Flow:          return track.flow();
    case Ornament:      return track.ornament();
    case Power:         return track.power();
    case LoopLength:    return track.loopLength();
    case Scan:          return track.scan();
    case Rotate:        return track.rotate();
    case Glide:         return track.glide();
    case Skew:          return track.skew();
    case GateOffset:    return track.gateOffset();
    case CvUpdateMode:  return track.cvUpdateMode();
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
    case Scan:          return 127;
    case Rotate:        return 63;   // Bipolar: -63 to +63
    case Glide:         return 100;
    case Skew:          return 8;    // Bipolar: -8 to +8
    case GateOffset:    return 100;  // Percentage: 0-100%
    case CvUpdateMode:  return 1;
    default:            return 0;
    }
}

bool TuesdayEditPage::paramIsBipolar(int param) const {
    return param == Rotate || param == Skew;
}

void TuesdayEditPage::editParam(int param, int value, bool shift) {
    auto &track = tuesdayTrack();

    switch (param) {
    case Algorithm:
        track.editAlgorithm(value, shift);
        break;
    case Flow:
        track.editFlow(value, shift);
        break;
    case Ornament:
        track.editOrnament(value, shift);
        break;
    case Power:
        track.editPower(value, shift);
        break;
    case LoopLength:
        track.editLoopLength(value, shift);
        break;
    case Scan:
        track.editScan(value, shift);
        break;
    case Rotate:
        if (track.loopLength() != 0) {
            track.editRotate(value, shift);
        }
        break;
    case Glide:
        track.editGlide(value, shift);
        break;
    case Skew:
        track.editSkew(value, shift);
        break;
    case GateOffset:
        track.editGateOffset(value, shift);
        break;
    case CvUpdateMode:
        track.editCvUpdateMode(value, shift);
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

    // For CvUpdateMode: draw value at bar level
    if (param == CvUpdateMode) {
        FixedStringBuilder<16> valueStr;
        formatParamValue(param, valueStr);
        int textWidth = canvas.textWidth(valueStr);
        int textX = x + (colWidth - textWidth) / 2;
        canvas.setColor(_selectedSlot == slot ? Color::Bright : Color::Medium);
        canvas.drawText(textX, barY + 3, valueStr);
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
    if (param == Rotate && tuesdayTrack().loopLength() == 0) {
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
    const auto &track = tuesdayTrack();

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
    int loopLen = track.actualLoopLength();

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
        tuesdayTrack().clear();
        break;
    case ContextAction::Reseed:
        const_cast<TuesdayTrackEngine &>(trackEngine()).reseed();
        break;
    case ContextAction::Last:
        break;
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
