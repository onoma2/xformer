#include "HarmonyPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

#include "core/math/Math.h"

HarmonyPage::HarmonyPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{
}

void HarmonyPage::enter() {
    updateListModel();
}

void HarmonyPage::exit() {
    _listModel.setSequence(nullptr);
    _listModel.setModel(nullptr);
}

void HarmonyPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "HARMONY");
    WindowPainter::drawActiveFunction(canvas, "HARMONY");

    const auto &sequence = _project.selectedNoteSequence();
    WindowPainter::drawAccumulatorValue(canvas, sequence.accumulator().currentValue(), sequence.accumulator().enabled());

    WindowPainter::drawFooter(canvas);

    ListPage::draw(canvas);
}

void HarmonyPage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}

void HarmonyPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }

    if (!event.consumed()) {
        ListPage::keyPress(event);
    }
}

void HarmonyPage::encoder(EncoderEvent &event) {
    if (!event.consumed()) {
        ListPage::encoder(event);
    }
}

void HarmonyPage::updateListModel() {
    auto &sequence = _project.selectedNoteSequence();
    _listModel.setSequence(&sequence);
    _listModel.setModel(&_model);
}
