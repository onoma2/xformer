#include "StochasticSequenceEditPage.h"

#include "ui/painters/WindowPainter.h"
#include "ui/MatrixMap.h"

StochasticSequenceEditPage::StochasticSequenceEditPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{}

void StochasticSequenceEditPage::enter() {
    auto &track = _project.selectedTrack();
    if (track.trackMode() == Track::TrackMode::Stochastic) {
        _listModel.setSequence(track.stochasticTrack().sequence(_project.selectedPatternIndex()), _project);
        _listModel.setStepIndex(_stepIndex);
    }
    ListPage::enter();
}

void StochasticSequenceEditPage::exit() {
    ListPage::exit();
}

void StochasticSequenceEditPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "STOCHASTIC STEP");

    FixedStringBuilder<16> str("STEP %d", _stepIndex + 1);
    canvas.setFont(Font::Small);
    canvas.drawText(Width - canvas.textWidth(str) - 4, 12, str);

    ListPage::draw(canvas);
}

void StochasticSequenceEditPage::updateLeds(Leds &leds) {
    for (int i = 0; i < 16; ++i) {
        leds.set(MatrixMap::fromStep(i), i == _stepIndex, i == _stepIndex);
    }
}

void StochasticSequenceEditPage::keyDown(KeyEvent &event) {
    const auto &key = event.key();
    if (key.isStep()) {
        _stepIndex = key.step();
        _listModel.setStepIndex(_stepIndex);
        event.consume();
    }
    ListPage::keyDown(event);
}

void StochasticSequenceEditPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();
    if (key.isStep()) {
        _stepIndex = key.step();
        _listModel.setStepIndex(_stepIndex);
        event.consume();
    }
    ListPage::keyPress(event);
}
