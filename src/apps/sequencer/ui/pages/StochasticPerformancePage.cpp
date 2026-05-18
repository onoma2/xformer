#include "StochasticPerformancePage.h"

#include "ui/painters/WindowPainter.h"

StochasticPerformancePage::StochasticPerformancePage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{}

void StochasticPerformancePage::enter() {
    auto &track = _project.selectedTrack();
    if (track.trackMode() == Track::TrackMode::Stochastic) {
        _listModel.setTrack(track.stochasticTrack(), _project);
    }
    ListPage::enter();
}

void StochasticPerformancePage::exit() {
    ListPage::exit();
}

void StochasticPerformancePage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "STOCH");
    ListPage::draw(canvas);
}

void StochasticPerformancePage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}
