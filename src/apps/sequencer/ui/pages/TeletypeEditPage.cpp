#include "TeletypeEditPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

TeletypeEditPage::TeletypeEditPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{
}

void TeletypeEditPage::enter() {
}

void TeletypeEditPage::exit() {
}

void TeletypeEditPage::draw(Canvas &canvas) {
    if (_project.selectedTrack().trackMode() != Track::TrackMode::Teletype) {
        close();
        return;
    }

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "TELETYPE");
}

void TeletypeEditPage::updateLeds(Leds &leds) {
    LedPainter::drawSelectedSequenceSection(leds, 0);
}

void TeletypeEditPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }
}

void TeletypeEditPage::encoder(EncoderEvent &event) {
    event.consume();
}
