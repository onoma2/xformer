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
    // Batch 0 / docs/stoch-review.md finding #3 — if the selected track is no
    // longer Stochastic, the list model still holds a raw pointer to the
    // previously-selected track. Bail before ListPage dereferences it.
    if (_project.selectedTrack().trackMode() != Track::TrackMode::Stochastic) {
        WindowPainter::clear(canvas);
        return;
    }
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "STOCH CFG");
    ListPage::draw(canvas);
}

void StochasticConfigPage::updateLeds(Leds &leds) {
    if (_project.selectedTrack().trackMode() != Track::TrackMode::Stochastic) return;
    ListPage::updateLeds(leds);
}
