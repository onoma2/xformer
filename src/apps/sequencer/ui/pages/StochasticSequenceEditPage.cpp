#include "StochasticSequenceEditPage.h"

#include "ui/painters/WindowPainter.h"
#include "ui/MatrixMap.h"
#include "model/StochasticTrack.h"
#include "core/utils/Random.h"

#include "engine/StochasticTrackEngine.h"

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
    { "EVEN" },
    { "RAND" },
};

static const ContextMenuModel::Item durContextMenuItems[] = {
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

void StochasticSequenceEditPage::nextPage() {
    _currentPage = Page((int(_currentPage) + 1) % int(Page::Count));
}

//----------
// Draw
//----------

void StochasticSequenceEditPage::draw(Canvas &canvas) {
    switch (_currentPage) {
    case Page::Pitch:   drawPitchPage(canvas); break;
    case Page::Duration: drawDurationPage(canvas); break;
    case Page::Count:   break;
    }
}

void StochasticSequenceEditPage::drawPitchPage(Canvas &canvas) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &sequence = track.sequence(_project.selectedPatternIndex());
    auto &scale = sequence.selectedScale(_project.scale());
    int scaleSize = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);

    if (_selectedDegree >= scaleSize) {
        _selectedDegree = scaleSize - 1;
    }

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "SCALE TICKETS");

    int baseY = 46;
    int barMaxH = 26;
    int barW = std::max(3, (Width - 16) / scaleSize - 2);
    int gap = 2;
    if (scaleSize > 20) { gap = 1; barW = std::max(2, (Width - 16) / scaleSize - 1); }

    int totalW = scaleSize * (barW + gap) - gap;
    int xOffset = std::max(8, (Width - totalW) / 2);

    int maxTicket = 0, totalTickets = 0, activeSlotCount = 0;
    for (int i = 0; i < scaleSize; ++i) {
        int t = sequence.effectiveDegreeTicket(i, scaleSize);
        if (t >= 0) {
            if (t > maxTicket) maxTicket = t;
            totalTickets += t;
            ++activeSlotCount;
        }
    }
    bool uniformFallback = (totalTickets == 0 && activeSlotCount > 0);
    int denom = std::max(1, maxTicket);

    if (totalTickets > 0 && activeSlotCount > 0) {
        int avgH = (totalTickets * barMaxH) / (activeSlotCount * denom);
        if (avgH > 0 && avgH < barMaxH) {
            canvas.setBlendMode(BlendMode::Set);
            canvas.setColor(Color::Low);
            canvas.hline(xOffset - 2, baseY - avgH, totalW + 4);
        }
    }

    for (int i = 0; i < scaleSize; ++i) {
        int degreeIndex = i;
        int x = xOffset + i * (barW + gap);
        int tickets = sequence.effectiveDegreeTicket(degreeIndex, scaleSize);

        bool active = false;
        int activeDegree = -1;
        if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
            auto &stoEng = _engine.selectedTrackEngine().as<StochasticTrackEngine>();
            activeDegree = stoEng.lastDegree() % scaleSize;
            active = (degreeIndex == activeDegree);
        }
        bool selected = (_pitchSelectionMask & (1U << degreeIndex)) != 0;

        if (tickets < 0) {
            canvas.setBlendMode(BlendMode::Set);
            canvas.setColor(active ? Color::Bright : (selected ? Color::Medium : Color::Low));
            canvas.line(x, baseY - 5, x + barW - 1, baseY);
            canvas.line(x, baseY, x + barW - 1, baseY - 5);
        } else if (tickets == 0 && !uniformFallback) {
            canvas.setBlendMode(BlendMode::Set);
            canvas.setColor(active ? Color::Bright : (selected ? Color::Medium : Color::Low));
            canvas.drawRect(x, baseY - 2, barW - 1, 2);
        } else {
            int h = uniformFallback ? barMaxH : (tickets * barMaxH) / denom;
            h = std::max(1, h);
            canvas.setBlendMode(BlendMode::Set);
            if (active) canvas.setColor(Color::Bright);
            else if (selected) canvas.setColor(Color::MediumBright);
            else canvas.setColor(uniformFallback ? Color::Low : Color::Medium);
            canvas.fillRect(x, baseY - h, barW, h);
        }

        if (active) {
            canvas.setColor(Color::Bright);
            canvas.hline(x - 1, baseY + 2, barW + 2);
        } else if (degreeIndex == _selectedDegree) {
            canvas.setColor(Color::MediumBright);
            canvas.hline(x - 1, baseY + 2, barW + 2);
        }
    }

    FixedStringBuilder<32> str;
    str("DEG %d: ", _selectedDegree + 1);
    int t = sequence.degreeTicket(_selectedDegree);
    if (t < 0) str("EXCL");
    else str("%d", t);
    canvas.setFont(Font::Small);
    canvas.setColor(Color::Bright);
    canvas.drawText(8, 18, str);

    if (scaleSize > 16) {
        str.reset();
        str("%d total  [%d-%d]", scaleSize, _bank * 16 + 1, std::min((_bank + 1) * 16, scaleSize));
        canvas.setFont(Font::Tiny);
        canvas.setColor(Color::Low);
        canvas.drawText(8, 28, str);
    }

    str.reset();
    str("DROT:%+d MROT:%+d", sequence.degreeRotation(), sequence.maskRotation());
    int rw = canvas.textWidth(str);

    // Ticket active badge
    canvas.setFont(Font::Tiny);
    canvas.setColor(sequence.pitchTicketsActive() ? Color::Bright : Color::Medium);
    FixedStringBuilder<8> badge;
    badge(sequence.pitchTicketsActive() ? "ON" : "OFF");
    int bw = canvas.textWidth(badge);
    canvas.drawText(Width - rw - bw - 16, 18, badge);

    // Rotation info
    canvas.setColor(Color::Medium);
    canvas.drawText(Width - rw - 12, 18, str);

    const char *footer[] = { "TIX", "DROT", "MROT", nullptr, "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), int(_editFocus));
}

void StochasticSequenceEditPage::drawDurationPage(Canvas &canvas) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &sequence = track.sequence(_project.selectedPatternIndex());

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "DURATION TICKETS");

    const int numSlots = 8;
    const int baseY = 42;
    const int barMaxH = 26;
    const int barW = 12;
    const int gap = barW;

    int totalW = numSlots * (barW + gap) - gap;
    int xOffset = std::max(8, (Width - totalW) / 2);

    int maxWeight = 0, totalWeight = 0;
    for (int i = 0; i < numSlots; ++i) {
        int w = sequence.durationTicket(i);
        if (w > maxWeight) maxWeight = w;
        totalWeight += w;
    }
    bool uniformFallback = (totalWeight == 0);
    int denom = std::max(1, maxWeight);

    if (totalWeight > 0) {
        int avgH = (totalWeight * barMaxH) / (numSlots * denom);
        if (avgH > 0 && avgH < barMaxH) {
            canvas.setBlendMode(BlendMode::Set);
            canvas.setColor(Color::Low);
            canvas.hline(xOffset - 2, baseY - avgH, totalW + 4);
        }
    }

    for (int i = 0; i < numSlots; ++i) {
        int x = xOffset + i * (barW + gap);
        int weight = sequence.durationTicket(i);

        int activeIdx = -1;
        if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
            auto &stoEng = _engine.selectedTrackEngine().as<StochasticTrackEngine>();
            activeIdx = stoEng.lastDurationIndex();
        }
        bool active = (i == activeIdx);
        bool selected = (_durSelectionMask & (1U << i)) != 0;

        if (weight == 0 && !uniformFallback) {
            canvas.setBlendMode(BlendMode::Set);
            canvas.setColor(active ? Color::Bright : (selected ? Color::Medium : Color::Low));
            canvas.drawRect(x, baseY - 2, barW - 1, 2);
        } else {
            int h = uniformFallback ? barMaxH : (weight * barMaxH) / denom;
            h = std::max(1, h);
            canvas.setBlendMode(BlendMode::Set);
            if (active) canvas.setColor(Color::Bright);
            else if (selected) canvas.setColor(Color::MediumBright);
            else canvas.setColor(uniformFallback ? Color::Low : Color::Medium);
            canvas.fillRect(x, baseY - h, barW, h);
        }

        if (active) {
            canvas.setColor(Color::Bright);
            canvas.hline(x - 1, baseY + 2, barW + 2);
        } else if (i == _selectedDurSlot) {
            canvas.setColor(Color::MediumBright);
            canvas.hline(x - 1, baseY + 2, barW + 2);
        }

        canvas.setFont(Font::Tiny);
        canvas.setColor(Color::Low);
        int labelW = canvas.textWidth(durationTicketLabels[i]);
        canvas.drawText(x + (barW - labelW) / 2, baseY + 8, durationTicketLabels[i]);
    }

    // Left info text
    FixedStringBuilder<32> str;
    if (_durFocus == DurFocus::Rest) {
        str("REST: %d%%", sequence.rest());
        canvas.setFont(Font::Small);
        canvas.setColor(Color::Bright);
        canvas.drawText(8, 18, str);
    } else {
        str("%s: %d", durationTicketLabels[_selectedDurSlot], sequence.durationTicket(_selectedDurSlot));
        canvas.setFont(Font::Small);
        canvas.setColor(Color::Bright);
        canvas.drawText(8, 18, str);
    }

    // Duration tickets on/off
    str.reset();
    str(sequence.durationTicketsActive() ? "ON" : "OFF");
    canvas.setFont(Font::Small);
    canvas.setColor(sequence.durationTicketsActive() ? Color::Bright : Color::Medium);
    canvas.drawText(Width - canvas.textWidth(str) - 8, 18, str);

    const char *footer[] = { "DUR", "RST", nullptr, nullptr, "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), _durFocus == DurFocus::DurTicket ? 0 : (_durFocus == DurFocus::Rest ? 1 : -1));
}

//----------
// LEDs
//----------

void StochasticSequenceEditPage::updateLeds(Leds &leds) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &sequence = track.sequence(_project.selectedPatternIndex());

    switch (_currentPage) {
    case Page::Pitch: {
        auto &scale = sequence.selectedScale(_project.scale());
        int scaleSize = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);

        int activeDegree = -1;
        if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
            auto &stoEng = _engine.selectedTrackEngine().as<StochasticTrackEngine>();
            activeDegree = stoEng.lastDegree() % scaleSize;
        }

        for (int i = 0; i < 16; ++i) {
            int degreeIndex = _bank * 16 + i;
            bool bit = (degreeIndex < scaleSize);
            bool selected = (_pitchSelectionMask & (1U << degreeIndex)) != 0;
            bool active = (degreeIndex == activeDegree);
            leds.set(MatrixMap::fromStep(i), active || (bit && selected), bit);
        }

        if (globalKeyState()[Key::Page] && !globalKeyState()[Key::Shift]) {
            for (int i = 8; i < 11; ++i) {
                int index = MatrixMap::fromStep(i + 1);
                leds.unmask(index);
                leds.set(index, false, true);
                leds.mask(index);
            }
        }
        break;
    }
    case Page::Duration: {
        int activeIdx = -1;
        if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
            auto &stoEng = _engine.selectedTrackEngine().as<StochasticTrackEngine>();
            activeIdx = stoEng.lastDurationIndex();
        }
        for (int i = 0; i < 8; ++i) {
            bool active = (i == activeIdx);
            bool selected = (_durSelectionMask & (1U << i)) != 0;
            leds.set(MatrixMap::fromStep(i), active || (i == _selectedDurSlot), true);
        }
        break;
    }
    case Page::Count:
        break;
    }
}

//----------
// Key Input
//----------

void StochasticSequenceEditPage::keyDown(KeyEvent &event) {
    switch (_currentPage) {
    case Page::Pitch:   handlePitchKeyDown(event); break;
    case Page::Duration: handleDurationKeyDown(event); break;
    case Page::Count:   break;
    }
}

void StochasticSequenceEditPage::handlePitchKeyDown(KeyEvent &event) {
    const auto &key = event.key();
    if (key.isStep() && !key.pageModifier()) {
        auto &track = _project.selectedTrack().stochasticTrack();
        auto &sequence = track.sequence(_project.selectedPatternIndex());
        auto &scale = sequence.selectedScale(_project.scale());
        int scaleSize = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);
        int degreeIndex = _bank * 16 + key.step();
        if (degreeIndex < scaleSize) {
            _selectedDegree = degreeIndex;
            if (_persistMode) {
                _pitchSelectionMask ^= (1U << degreeIndex);
            } else {
                _pitchSelectionMask |= (1U << degreeIndex);
            }
        }
        event.consume();
        return;
    }

    if (key.isShift()) {
        _persistMode = !_persistMode;
        if (!_persistMode) _pitchSelectionMask = 0;
        event.consume();
        return;
    }

    {
        auto &track = _project.selectedTrack().stochasticTrack();
        auto &sequence = track.sequence(_project.selectedPatternIndex());
        auto &scale = sequence.selectedScale(_project.scale());
        int scaleSize = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);
        if (key.isLeft() && scaleSize > 16) {
            _bank = std::max(0, _bank - 1);
            event.consume();
            return;
        }
        if (key.isRight() && scaleSize > 16) {
            int maxBank = (scaleSize - 1) / 16;
            _bank = std::min(maxBank, _bank + 1);
            event.consume();
            return;
        }
    }

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    BasePage::keyDown(event);
}

void StochasticSequenceEditPage::handleDurationKeyDown(KeyEvent &event) {
    const auto &key = event.key();
    if (key.isStep() && !key.pageModifier()) {
        int step = key.step();
        if (step >= 0 && step < 8) {
            _selectedDurSlot = step;
            if (_persistMode) {
                _durSelectionMask ^= (1U << step);
            } else {
                _durSelectionMask |= (1U << step);
            }
        }
        event.consume();
        return;
    }

    if (key.isShift()) {
        _persistMode = !_persistMode;
        if (!_persistMode) _durSelectionMask = 0;
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

void StochasticSequenceEditPage::keyUp(KeyEvent &event) {
    const auto &key = event.key();
    if (key.isStep() && !_persistMode) {
        if (_currentPage == Page::Pitch) {
            int degreeIndex = _bank * 16 + key.step();
            _pitchSelectionMask &= ~(1U << degreeIndex);
        } else if (_currentPage == Page::Duration) {
            int step = key.step();
            if (step >= 0 && step < 8) {
                _durSelectionMask &= ~(1U << step);
            }
        }
        event.consume();
        return;
    }

    if (key.isShift()) {
        event.consume();
        return;
    }

    BasePage::keyUp(event);
}

void StochasticSequenceEditPage::keyPress(KeyPressEvent &event) {
    switch (_currentPage) {
    case Page::Pitch:   handlePitchKeyPress(event); break;
    case Page::Duration: handleDurationKeyPress(event); break;
    case Page::Count:   break;
    }
}

void StochasticSequenceEditPage::handlePitchKeyPress(KeyPressEvent &event) {
    const auto &key = event.key();
    if (key.isStep() && !key.pageModifier()) {
        event.consume();
        return;
    }

    if (key.isQuickEdit() && !key.shiftModifier()) {
        switch (key.quickEdit()) {
        case 1: contextAction(int(ContextAction::Init)); break;
        case 2: contextAction(int(ContextAction::Even)); break;
        case 3: contextAction(int(ContextAction::Random)); break;
        }
        event.consume();
        return;
    }

    if (key.isFunction()) {
        switch (key.function()) {
        case 0: _editFocus = EditFocus::Ticket; break;
        case 1: _editFocus = EditFocus::DegreeRotation; break;
        case 2: _editFocus = EditFocus::MaskRotation; break;
        case 4: // F5 = NEXT
            nextPage();
            event.consume();
            return;
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

void StochasticSequenceEditPage::handleDurationKeyPress(KeyPressEvent &event) {
    const auto &key = event.key();
    if (key.isStep() && !key.pageModifier()) {
        event.consume();
        return;
    }

    if (key.isQuickEdit() && !key.shiftModifier()) {
        switch (key.quickEdit()) {
        case 1: contextAction(int(ContextAction::Init)); break;
        case 2: contextAction(int(ContextAction::Even)); break;
        case 3: contextAction(int(ContextAction::Random)); break;
        }
        event.consume();
        return;
    }

    if (key.isFunction()) {
        switch (key.function()) {
        case 0: _durFocus = DurFocus::DurTicket; break;
        case 1: _durFocus = DurFocus::Rest; break;
        case 4:
            nextPage();
            event.consume();
            return;
        }
        event.consume();
        return;
    }

    BasePage::keyPress(event);
}

void StochasticSequenceEditPage::encoder(EncoderEvent &event) {
    switch (_currentPage) {
    case Page::Pitch:   handlePitchEncoder(event); break;
    case Page::Duration: handleDurationEncoder(event); break;
    case Page::Count:   break;
    }
}

void StochasticSequenceEditPage::handlePitchEncoder(EncoderEvent &event) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &sequence = track.sequence(_project.selectedPatternIndex());
    auto &scale = sequence.selectedScale(_project.scale());
    int scaleSize = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);

    switch (_editFocus) {
    case EditFocus::Ticket: {
        uint32_t mask = _pitchSelectionMask;
        if (_pitchSelectionMask == 0) mask = (1U << _selectedDegree);
        for (int i = 0; i < scaleSize; ++i) {
            if (mask & (1U << i)) {
                sequence.setDegreeTicket(i, sequence.degreeTicket(i) + event.value());
            }
        }
        break;
    }
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

void StochasticSequenceEditPage::handleDurationEncoder(EncoderEvent &event) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &sequence = track.sequence(_project.selectedPatternIndex());

    switch (_durFocus) {
    case DurFocus::DurTicket: {
        uint32_t mask = _durSelectionMask;
        if (_durSelectionMask == 0) mask = (1U << _selectedDurSlot);
        for (int i = 0; i < 8; ++i) {
            if (mask & (1U << i)) {
                sequence.setDurationTicket(i, sequence.durationTicket(i) + event.value());
            }
        }
        break;
    }
    case DurFocus::Rest:
        sequence.setRest(sequence.rest() + event.value());
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

    switch (_currentPage) {
    case Page::Pitch: {
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
        break;
    }
    case Page::Duration: {
        switch (ContextAction(index)) {
        case ContextAction::Init:
            for (int i = 0; i < 8; ++i) {
                sequence.setDurationTicket(i, 50);
            }
            sequence.setRest(0);
            showMessage("INIT DUR");
            break;
        case ContextAction::Even:
            for (int i = 0; i < 8; ++i) {
                sequence.setDurationTicket(i, 50);
            }
            sequence.setRest(0);
            showMessage("EVEN DUR");
            break;
        case ContextAction::Random: {
            static Random rng;
            for (int i = 0; i < 8; ++i) {
                sequence.setDurationTicket(i, rng.nextRange(101));
            }
            showMessage("RAND DUR");
            break;
        }
        case ContextAction::Last:
            break;
        }
        break;
    }
    case Page::Count:
        break;
    }
}
