#include "FractalOrnamentPage.h"

#include "ui/painters/WindowPainter.h"
#include "Pages.h"

#include "model/FractalTrack.h"
#include "model/Scale.h"
#include "model/Types.h"

#include "core/utils/StringBuilder.h"

#include <algorithm>

FractalOrnamentPage::FractalOrnamentPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void FractalOrnamentPage::enter() {}
void FractalOrnamentPage::exit() {}

bool FractalOrnamentPage::isActiveForSelectedTrack() const {
    return _project.selectedTrack().trackMode() == Track::TrackMode::Fractal &&
        _engine.selectedTrackEngine().trackMode() == Track::TrackMode::Fractal;
}

// Outlined bar with a filled portion (0..100) and optional tier ticks.
static void drawBar(Canvas &canvas, int x, int y, int w, int h, int pct, bool highlight) {
    canvas.setColor(Color::Low);
    canvas.drawRect(x, y, w, h);
    int fw = std::max(1, (w - 2) * std::max(0, std::min(100, pct)) / 100);
    canvas.setColor(highlight ? Color::Bright : Color::MediumBright);
    canvas.fillRect(x + 1, y + 1, fw, h - 2);
}

void FractalOrnamentPage::draw(Canvas &canvas) {
    if (!isActiveForSelectedTrack()) return;

    auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "FRACTAL");
    WindowPainter::drawActiveFunction(canvas, "ORN");

    int activeFn = int(_focus);
    const char *footer[] = { "RATE", "INT", "SCALE", "ZONE", "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), activeFn);

    canvas.setFont(Font::Tiny);
    FixedStringBuilder<24> str;

    const int rate = seq.ornamentRate();
    const int intensity = seq.ornamentIntensity();

    // Rate
    canvas.setColor(Color::Medium);
    canvas.drawText(2, 17, "Rate");
    drawBar(canvas, 40, 13, 96, 6, rate, _focus == Focus::Rate);
    str.reset(); str("%d%%", rate);
    canvas.setColor(Color::MediumBright);
    canvas.drawText(Width - 2 - canvas.textWidth(str), 17, str);

    // Intensity (tier label: off / 2-step / 4-step / 8-step trill)
    canvas.setColor(Color::Medium);
    canvas.drawText(2, 28, "Int");
    drawBar(canvas, 40, 24, 96, 6, intensity, _focus == Focus::Intensity);
    // tier ticks at 40% and 75%
    canvas.setColor(Color::MediumLow);
    canvas.vline(40 + 1 + (96 - 2) * 40 / 100, 24, 6);
    canvas.vline(40 + 1 + (96 - 2) * 75 / 100, 24, 6);
    const char *tier = intensity == 0 ? "off" : (intensity >= 75 ? "8-trill" : (intensity >= 40 ? "4-step" : "2-step"));
    str.reset(); str("%d%% %s", intensity, tier);
    canvas.setColor(Color::MediumBright);
    canvas.drawText(Width - 2 - canvas.textWidth(str), 28, str);

    // Scale + Root
    canvas.setColor(Color::Medium);
    canvas.drawText(2, 39, "Scale");
    canvas.setColor((_focus == Focus::Scale || _focus == Focus::Root) ? Color::Bright : Color::MediumBright);
    str.reset();
    Types::printNote(str, seq.rootNote());
    str(" %s", Scale::name(seq.scale()));
    canvas.drawText(40, 39, str);

    str.reset(); str("zone %d-%d", seq.ornFirst(), seq.ornLast());
    canvas.setColor(Color::MediumBright);
    canvas.drawText(Width - 2 - canvas.textWidth(str), 39, str);

    // Last-fired ornament (placeholder until ornament engine lands).
    canvas.setColor(Color::Medium);
    canvas.drawText(2, 50, "Last");
    canvas.setColor(Color::Medium);
    canvas.drawText(40, 50, "-");
}

void FractalOrnamentPage::encoder(EncoderEvent &event) {
    if (!isActiveForSelectedTrack()) return;
    auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());
    int v = event.value();

    switch (_focus) {
    case Focus::Rate:      seq.setOrnamentRate(seq.ornamentRate() + v); break;
    case Focus::Intensity: seq.setOrnamentIntensity(seq.ornamentIntensity() + v); break;
    case Focus::Scale:     seq.setScale(seq.scale() + v); break;
    case Focus::Root:      seq.setRootNote(seq.rootNote() + v); break;
    }
}

void FractalOrnamentPage::keyPress(KeyPressEvent &event) {
    if (!isActiveForSelectedTrack()) { event.consume(); return; }

    const auto &key = event.key();
    if (key.isFunction()) {
        switch (key.function()) {
        case 0: _focus = Focus::Rate; break;
        case 1: _focus = Focus::Intensity; break;
        // F3 cycles Scale <-> Root (zone is edited on the Trunk page).
        case 2: _focus = _focus == Focus::Scale ? Focus::Root : Focus::Scale; break;
        case 4: {
            auto &pages = _manager.pages();
            _manager.replace(_manager.stackSize() - 1, &pages.fractalOrnament);
            break;
        }
        default: break;
        }
        event.consume();
        return;
    }
}

void FractalOrnamentPage::updateLeds(Leds &leds) {}
