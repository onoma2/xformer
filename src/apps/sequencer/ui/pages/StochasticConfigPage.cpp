#include "StochasticConfigPage.h"

#include "ui/painters/WindowPainter.h"

StochasticConfigPage::StochasticConfigPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{}

void StochasticConfigPage::enter() {
    auto &track = _project.selectedTrack();
    if (track.trackMode() == Track::TrackMode::Stochastic) {
        _listModel.setTrack(track.stochasticTrack(), _project);
    }
    ListPage::enter();
}

void StochasticConfigPage::exit() {
    ListPage::exit();
}

void StochasticConfigPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "STOCHASTIC TRACK");
    ListPage::draw(canvas);
}

void StochasticConfigPage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}
