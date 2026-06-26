#include "FractalSourcePage.h"

#include "ui/painters/WindowPainter.h"
#include "Pages.h"

#include "model/FractalTrack.h"
#include "model/Project.h"

#include "core/utils/StringBuilder.h"

#include <algorithm>

static const char *kGateLogic[6] = { "A", "B", "And", "Or", "Xor", "Nand" };
static const char *kCvLogic[7]   = { "A", "B", "Min", "Max", "Sum", "Avg", "Gat" };

FractalSourcePage::FractalSourcePage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void FractalSourcePage::enter() {}
void FractalSourcePage::exit() {}

bool FractalSourcePage::isActiveForSelectedTrack() const {
    return _project.selectedTrack().trackMode() == Track::TrackMode::Fractal &&
        _engine.selectedTrackEngine().trackMode() == Track::TrackMode::Fractal;
}

// Compact track ref: "<3-letter mode> T<n>" or "-" for none.
static void printSourceRef(StringBuilder &str, const Project &project, int index) {
    if (index < 0) { str("-"); return; }
    static const char *kShort[] = { "Not", "Crv", "Mid", "Tue", "Map", "Idx", "Sto", "Frc", "Ttv", "Ttm", "Phx" };
    int mode = int(project.track(index).trackMode());
    const char *m = (mode >= 0 && mode < int(sizeof(kShort) / sizeof(kShort[0]))) ? kShort[mode] : "?";
    str("%s T%d", m, index + 1);
}

// Single-select row of cells (mirrors the mockup _seg_row).
static void drawSegRow(Canvas &canvas, int x, int y, int w, const char **items, int n, int sel, bool focus) {
    int cw = w / n;
    for (int i = 0; i < n; ++i) {
        int bx = x + i * cw;
        bool on = i == sel;
        canvas.setColor(on ? Color::Bright : Color::Low);
        canvas.drawRect(bx, y, cw - 1, 9);
        canvas.setColor(on ? (focus ? Color::Bright : Color::MediumBright) : Color::MediumLow);
        canvas.drawText(bx + (cw - 1 - canvas.textWidth(items[i])) / 2, y + 7, items[i]);
    }
}

void FractalSourcePage::draw(Canvas &canvas) {
    if (!isActiveForSelectedTrack()) return;

    auto &track = _project.selectedTrack().fractalTrack();

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "FRACTAL");
    WindowPainter::drawActiveFunction(canvas, "MIX");

    int activeFn = int(_focus);
    const char *footer[] = { "SrcA", "SrcB", "GATE", "CV", "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), activeFn);

    canvas.setFont(Font::Tiny);
    FixedStringBuilder<24> str;

    // Source A / Source B refs
    canvas.setColor(Color::Medium);
    canvas.drawText(2, 16, "A");
    canvas.setColor(_focus == Focus::SourceA ? Color::Bright : Color::MediumBright);
    str.reset(); printSourceRef(str, _project, track.sourceA());
    canvas.drawText(14, 16, str);

    canvas.setColor(Color::Medium);
    canvas.drawText(200, 16, "B");
    canvas.setColor(_focus == Focus::SourceB ? Color::Bright : Color::MediumBright);
    str.reset(); printSourceRef(str, _project, track.sourceB());
    canvas.drawText(Width - 2 - canvas.textWidth(str), 16, str);

    // Gate logic row
    canvas.setColor(Color::Medium);
    canvas.drawText(2, 29, "Gate");
    drawSegRow(canvas, 40, 22, 214, kGateLogic, 6, int(track.gateLogic()), _focus == Focus::Gate);

    // CV logic row
    canvas.setColor(Color::Medium);
    canvas.drawText(2, 40, "CV");
    drawSegRow(canvas, 40, 33, 214, kCvLogic, 7, int(track.cvLogic()), _focus == Focus::Cv);

    // Result readout
    canvas.setColor(Color::Medium);
    str.reset();
    str("%s + %s", kGateLogic[int(track.gateLogic())], kCvLogic[int(track.cvLogic())]);
    canvas.drawText(2, 51, str);
}

void FractalSourcePage::encoder(EncoderEvent &event) {
    if (!isActiveForSelectedTrack()) return;
    auto &track = _project.selectedTrack().fractalTrack();
    int v = event.value();

    switch (_focus) {
    case Focus::SourceA: track.setSourceA(track.sourceA() + v); break;
    case Focus::SourceB: track.setSourceB(track.sourceB() + v); break;
    case Focus::Gate:    track.setGateLogic(FractalTrack::GateLogic(std::max(0, std::min(int(FractalTrack::GateLogic::Last) - 1, int(track.gateLogic()) + v)))); break;
    case Focus::Cv:      track.setCvLogic(FractalTrack::CvLogic(std::max(0, std::min(int(FractalTrack::CvLogic::Last) - 1, int(track.cvLogic()) + v)))); break;
    }
}

void FractalSourcePage::keyPress(KeyPressEvent &event) {
    if (!isActiveForSelectedTrack()) { event.consume(); return; }

    const auto &key = event.key();
    if (key.isFunction()) {
        switch (key.function()) {
        case 0: _focus = Focus::SourceA; break;
        case 1: _focus = Focus::SourceB; break;
        case 2: _focus = Focus::Gate; break;
        case 3: _focus = Focus::Cv; break;
        case 4: {
            auto &pages = _manager.pages();
            _manager.replace(_manager.stackSize() - 1, &pages.fractalTrunk);
            break;
        }
        default: break;
        }
        event.consume();
        return;
    }
}

void FractalSourcePage::updateLeds(Leds &leds) {}
