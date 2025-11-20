#include "AccumulatorPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

#include "core/math/Math.h"

AccumulatorPage::AccumulatorPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{
}

void AccumulatorPage::enter() {
    updateListModel();
}

void AccumulatorPage::exit() {
    _listModel.setSequence(nullptr);
}

void AccumulatorPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "ACCUM");
    WindowPainter::drawActiveFunction(canvas, "ACCUMULATOR");

    const auto &sequence = _project.selectedNoteSequence();
    WindowPainter::drawAccumulatorValue(canvas, sequence.accumulator().currentValue(), sequence.accumulator().enabled());

    WindowPainter::drawFooter(canvas);

    ListPage::draw(canvas);
}

void AccumulatorPage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}

void AccumulatorPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }

    if (!event.consumed()) {
        ListPage::keyPress(event);
    }
}

void AccumulatorPage::encoder(EncoderEvent &event) {
    if (!event.consumed()) {
        ListPage::encoder(event);
    }
}

void AccumulatorPage::updateListModel() {
    auto &sequence = _project.selectedNoteSequence();
    _listModel.setSequence(&sequence);
}