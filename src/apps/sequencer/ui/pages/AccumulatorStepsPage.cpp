#include "AccumulatorStepsPage.h"

#include "Pages.h"
#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

AccumulatorStepsPage::AccumulatorStepsPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{
}

void AccumulatorStepsPage::enter() {
    updateListModel();
}

void AccumulatorStepsPage::exit() {
    _listModel.setSequence(nullptr);
}

void AccumulatorStepsPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "ACCST");
    WindowPainter::drawActiveFunction(canvas, "ACCU STEPS");
    WindowPainter::drawFooter(canvas);

    ListPage::draw(canvas);
}

void AccumulatorStepsPage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}

void AccumulatorStepsPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }

    if (!event.consumed()) {
        ListPage::keyPress(event);
    }
}

void AccumulatorStepsPage::encoder(EncoderEvent &event) {
    if (!event.consumed()) {
        ListPage::encoder(event);
    }
}

void AccumulatorStepsPage::updateListModel() {
    auto &sequence = _project.selectedNoteSequence();
    _listModel.setSequence(&sequence);
}