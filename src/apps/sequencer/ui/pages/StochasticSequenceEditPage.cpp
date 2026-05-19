#include "StochasticSequenceEditPage.h"

#include "ui/painters/WindowPainter.h"
#include "ui/MatrixMap.h"
#include "model/StochasticTrack.h"
#include "core/utils/Random.h"

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
    { "EVEN" },
    { "RAND" },
};

StochasticSequenceEditPage::StochasticSequenceEditPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void StochasticSequenceEditPage::enter() {
}

void StochasticSequenceEditPage::exit() {
}

void StochasticSequenceEditPage::draw(Canvas &canvas) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &sequence = track.sequence(_project.selectedPatternIndex());
    auto &scale = sequence.selectedScale(_project.scale());
    int scaleSize = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);

    if (_selectedDegree >= scaleSize) {
        _selectedDegree = scaleSize - 1;
    }

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "STOCHASTIC TICKETS");

    int baseY = 46;
    int barMaxH = 26;
    int barW = 5;
    int gap = 2;
    
    int bankSize = std::min(16, scaleSize - _bank * 16);
    if (bankSize < 0) {
        _bank = 0;
        bankSize = std::min(16, scaleSize);
    }

    int totalW = bankSize * (barW + gap) - gap;
    int xOffset = std::max(8, (Width - totalW) / 2);

    if (totalW > Width - 16) {
        barW = 3;
        gap = 1;
        totalW = bankSize * (barW + gap) - gap;
        xOffset = std::max(8, (Width - totalW) / 2);
    }
    
    for (int i = 0; i < bankSize; ++i) {
        int degreeIndex = _bank * 16 + i;
        int x = xOffset + i * (barW + gap);
        int tickets = sequence.degreeTicket(degreeIndex);

        if (tickets < 0) {
            // Excluded
            canvas.setBlendMode(BlendMode::Set);
            canvas.setColor(Color::Medium);
            canvas.line(x, baseY - 5, x + barW - 1, baseY);
            canvas.line(x, baseY, x + barW - 1, baseY - 5);
        } else if (tickets == 0) {
            // Included but 0 tickets
            canvas.setBlendMode(BlendMode::Set);
            canvas.setColor(Color::Low);
            canvas.drawRect(x, baseY - 2, barW - 1, 2);
        } else {
            // Bar
            int h = (tickets * barMaxH) / 100;
            h = std::max(1, h);
            canvas.setBlendMode(BlendMode::Set);
            canvas.setColor(degreeIndex == _selectedDegree ? Color::Bright : Color::Medium);
            canvas.fillRect(x, baseY - h, barW, h);
        }

        if (degreeIndex == _selectedDegree) {
            canvas.setColor(Color::Bright);
            canvas.hline(x - 1, baseY + 2, barW + 2);
        }
    }

    // Selected degree info
    FixedStringBuilder<32> str;
    str("DEG %d: ", _selectedDegree + 1);
    int t = sequence.degreeTicket(_selectedDegree);
    if (t < 0) str("EXCL");
    else str("%d", t);
    canvas.setFont(Font::Small);
    canvas.setColor(Color::Bright);
    canvas.drawText(8, 18, str);

    // Page indicator
    if (scaleSize > 16) {
        str.reset();
        str("P%d/%d", _bank + 1, (scaleSize + 15) / 16);
        canvas.setColor(Color::Low);
        canvas.drawText(8, 28, str);
    }

    // Rotation info
    str.reset();
    str("DROT:%+d MROT:%+d", sequence.degreeRotation(), sequence.maskRotation());
    canvas.setFont(Font::Tiny);
    canvas.setColor(Color::Medium);
    canvas.drawText(Width - canvas.textWidth(str) - 12, 18, str);

    // Footer
    const char *footer[] = { "TIX", "DROT", "MROT", nullptr, nullptr };
    WindowPainter::drawFooter(canvas, footer, int(_editFocus));
}

void StochasticSequenceEditPage::updateLeds(Leds &leds) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &sequence = track.sequence(_project.selectedPatternIndex());
    auto &scale = sequence.selectedScale(_project.scale());
    int scaleSize = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);

    for (int i = 0; i < 16; ++i) {
        int degreeIndex = _bank * 16 + i;
        bool bit = (degreeIndex < scaleSize);
        leds.set(MatrixMap::fromStep(i), bit && (degreeIndex == _selectedDegree), bit);
    }

    if (globalKeyState()[Key::Page] && !globalKeyState()[Key::Shift]) {
        for (int i = 8; i < 11; ++i) {
            int index = MatrixMap::fromStep(i + 1);
            leds.unmask(index);
            leds.set(index, false, true);
            leds.mask(index);
        }
    }
}

void StochasticSequenceEditPage::keyDown(KeyEvent &event) {
    const auto &key = event.key();
    if (key.isStep() && !key.pageModifier()) {
        auto &track = _project.selectedTrack().stochasticTrack();
        auto &sequence = track.sequence(_project.selectedPatternIndex());
        auto &scale = sequence.selectedScale(_project.scale());
        int scaleSize = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);
        int degreeIndex = _bank * 16 + key.step();
        if (degreeIndex < scaleSize) {
            _selectedDegree = degreeIndex;
        }
        event.consume();
        return;
    }

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    BasePage::keyDown(event);
}

void StochasticSequenceEditPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();
    if (key.isStep() && !key.pageModifier()) {
        // Handled in keyDown
        event.consume();
        return;
    }

    if (key.isQuickEdit() && !key.shiftModifier()) {
        switch (key.quickEdit()) {
        case 1: // Step 10: INIT
            contextAction(int(ContextAction::Init));
            break;
        case 2: // Step 11: EVEN
            contextAction(int(ContextAction::Even));
            break;
        case 3: // Step 12: RAND
            contextAction(int(ContextAction::Random));
            break;
        }
        event.consume();
        return;
    }

    if (key.isFunction()) {
        switch (key.function()) {
        case 0: _editFocus = EditFocus::Ticket; break;
        case 1: _editFocus = EditFocus::DegreeRotation; break;
        case 2: _editFocus = EditFocus::MaskRotation; break;
        }
        event.consume();
        return;
    }

    if (key.isLeft()) {
        auto &track = _project.selectedTrack().stochasticTrack();
        auto &sequence = track.sequence(_project.selectedPatternIndex());
        auto &scale = sequence.selectedScale(_project.scale());
        int scaleSize = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);
        if (scaleSize > 16) {
            _bank = std::max(0, _bank - 1);
        }
        event.consume();
    }
    if (key.isRight()) {
        auto &track = _project.selectedTrack().stochasticTrack();
        auto &sequence = track.sequence(_project.selectedPatternIndex());
        auto &scale = sequence.selectedScale(_project.scale());
        int scaleSize = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);
        if (scaleSize > 16) {
            _bank = std::min(1, _bank + 1);
        }
        event.consume();
    }

    BasePage::keyPress(event);
}

void StochasticSequenceEditPage::encoder(EncoderEvent &event) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &sequence = track.sequence(_project.selectedPatternIndex());

    switch (_editFocus) {
    case EditFocus::Ticket:
        sequence.setDegreeTicket(_selectedDegree, sequence.degreeTicket(_selectedDegree) + event.value());
        break;
    case EditFocus::DegreeRotation:
        sequence.setDegreeRotation(sequence.degreeRotation() + event.value());
        break;
    case EditFocus::MaskRotation:
        sequence.setMaskRotation(sequence.maskRotation() + event.value());
        break;
    default:
        break;
    }

    event.consume();
}

void StochasticSequenceEditPage::contextShow(bool doubleClick) {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return true; },
        doubleClick
    ));
}

void StochasticSequenceEditPage::contextAction(int index) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &sequence = track.sequence(_project.selectedPatternIndex());

    switch (ContextAction(index)) {
    case ContextAction::Init:
        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) {
            sequence.setDegreeTicket(i, 100);
        }
        sequence.setDegreeRotation(0);
        sequence.setMaskRotation(0);
        showMessage("INIT TICKETS");
        break;
    case ContextAction::Even: {
        int val = sequence.degreeTicket(_selectedDegree);
        if (val < 0) val = 50;
        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) {
            sequence.setDegreeTicket(i, val);
        }
        showMessage("EVEN TICKETS");
        break;
    }
    case ContextAction::Random: {
        static Random rng;
        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) {
            sequence.setDegreeTicket(i, rng.nextRange(101));
        }
        showMessage("RANDOM TICKETS");
        break;
    }
    case ContextAction::Last:
        break;
    }
}

