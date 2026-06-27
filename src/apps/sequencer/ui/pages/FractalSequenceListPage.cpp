#include "FractalSequenceListPage.h"

#include "ui/painters/WindowPainter.h"

FractalSequenceListPage::FractalSequenceListPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{}

void FractalSequenceListPage::enter() {
    auto &track = _project.selectedTrack();
    if (track.trackMode() == Track::TrackMode::Fractal) {
        _listModel.setSequence(&track.fractalTrack().sequence(_project.selectedPatternIndex()));
    }
    ListPage::enter();
}

void FractalSequenceListPage::exit() {
    ListPage::exit();
}

void FractalSequenceListPage::draw(Canvas &canvas) {
    // Bail before ListPage dereferences a stale sequence pointer if the selected
    // track is no longer Fractal.
    if (_project.selectedTrack().trackMode() != Track::TrackMode::Fractal) {
        WindowPainter::clear(canvas);
        return;
    }
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "FRACTAL SEQ");
    ListPage::draw(canvas);
}

void FractalSequenceListPage::updateLeds(Leds &leds) {
    if (_project.selectedTrack().trackMode() != Track::TrackMode::Fractal) return;
    ListPage::updateLeds(leds);
}
