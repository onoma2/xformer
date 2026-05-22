#include "StochasticPerformancePage.h"

#include "ui/painters/WindowPainter.h"

StochasticPerformancePage::StochasticPerformancePage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{}

void StochasticPerformancePage::enter() {
    auto &track = _project.selectedTrack();
    if (track.trackMode() == Track::TrackMode::Stochastic) {
        _listModel.setTrack(track.stochasticTrack(), _project, &_engine);
    }
    ListPage::enter();
}

void StochasticPerformancePage::exit() {
    ListPage::exit();
}

void StochasticPerformancePage::draw(Canvas &canvas) {
    // Batch 0 / docs/stoch-review.md finding #3 — see StochasticConfigPage.
    if (_project.selectedTrack().trackMode() != Track::TrackMode::Stochastic) {
        WindowPainter::clear(canvas);
        return;
    }
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "STOCH");
    ListPage::draw(canvas);
}

void StochasticPerformancePage::updateLeds(Leds &leds) {
    if (_project.selectedTrack().trackMode() != Track::TrackMode::Stochastic) return;
    ListPage::updateLeds(leds);
}
