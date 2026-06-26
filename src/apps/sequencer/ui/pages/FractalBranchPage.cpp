#include "FractalBranchPage.h"

#include "ui/painters/WindowPainter.h"
#include "Pages.h"

#include "model/FractalTrack.h"

#include "core/utils/StringBuilder.h"
#include "core/utils/Random.h"

#include <algorithm>

// 8-slot transform pool (matches branchPool bitmask order, KD-12).
static const char *kPoolNames[8] = { "Tr", "Rv", "In", "RI", "Ro", "Co", "Ex", "Gt" };

FractalBranchPage::FractalBranchPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void FractalBranchPage::enter() {}
void FractalBranchPage::exit() {}

bool FractalBranchPage::isActiveForSelectedTrack() const {
    return _project.selectedTrack().trackMode() == Track::TrackMode::Fractal &&
        _engine.selectedTrackEngine().trackMode() == Track::TrackMode::Fractal;
}

void FractalBranchPage::draw(Canvas &canvas) {
    if (!isActiveForSelectedTrack()) return;

    auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "FRACTAL");
    WindowPainter::drawActiveFunction(canvas, "BRANCH");

    int activeFn = int(_focus);
    const char *footer[] = { "CNT", "PATH", "POOL", "SEED", "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), activeFn);

    const int N = seq.branchCount();

    canvas.setFont(Font::Tiny);
    FixedStringBuilder<32> str;

    // route readout row
    canvas.setColor(Color::Medium);
    canvas.drawText(2, 17, "Route");
    canvas.setColor(Color::Bright);
    str.reset();
    str("T");
    for (int j = 1; j <= N; ++j) str(">B%d", j);
    canvas.drawText(40, 17, str);
    str.reset();
    str("N=%d", N);
    canvas.setColor(Color::MediumBright);
    canvas.drawText(Width - 2 - canvas.textWidth(str), 17, str);

    // chain blocks: T + B1..BN
    const int cols = N + 1;
    const int bw = Width / cols;
    const int y = 26;
    for (int j = 0; j < cols; ++j) {
        int x = j * bw;
        bool isTrunk = j == 0;
        canvas.setColor(isTrunk ? Color::Bright : Color::Medium);
        canvas.drawRect(x + 1, y, bw - 2, 10);
        str.reset();
        if (isTrunk) str("T"); else str("B%d", j);
        canvas.setColor(isTrunk ? Color::Bright : Color::MediumBright);
        canvas.drawText(x + 1 + (bw - 2 - canvas.textWidth(str)) / 2, y + 6, str);
    }

    // transform pool toggles
    const int pw = Width / 8;
    const int py = 45;
    const int pool = seq.branchPool();
    for (int k = 0; k < 8; ++k) {
        int x = k * pw;
        bool en = (pool >> k) & 1;
        bool sel = (_focus == Focus::Pool) && (k == _poolIndex);
        canvas.setColor(sel ? Color::Bright : (en ? Color::Bright : Color::Low));
        canvas.drawRect(x + 1, py, 6, 6);
        if (en) canvas.fillRect(x + 2, py + 1, 4, 4);
        canvas.setColor(en ? Color::MediumBright : Color::Low);
        canvas.drawText(x + 10, py + 5, kPoolNames[k]);
    }
}

void FractalBranchPage::encoder(EncoderEvent &event) {
    if (!isActiveForSelectedTrack()) return;
    auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());
    int v = event.value();

    switch (_focus) {
    case Focus::Count:
        seq.setBranchCount(seq.branchCount() + v);
        break;
    case Focus::Path:
        seq.setPath(seq.path() + v);
        break;
    case Focus::Pool:
        _poolIndex = std::max(0, std::min(7, _poolIndex + v));
        break;
    case Focus::Seed:
        seq.setBranchSeed(seq.branchSeed() + v);
        break;
    }
}

void FractalBranchPage::keyPress(KeyPressEvent &event) {
    if (!isActiveForSelectedTrack()) { event.consume(); return; }

    const auto &key = event.key();
    if (key.isFunction()) {
        switch (key.function()) {
        case 0: _focus = Focus::Count; break;
        case 1: _focus = Focus::Path; break;
        case 2: {
            auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());
            if (_focus == Focus::Pool) {
                // toggle the selected pool bit on a second press
                seq.setBranchPool(seq.branchPool() ^ (1 << _poolIndex));
            } else {
                _focus = Focus::Pool;
            }
            break;
        }
        case 3: {
            // SEED: reseed to a fresh random value.
            auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());
            Random rng(seq.branchSeed() ^ 0x9e3779b9u);
            seq.setBranchSeed(rng.next() & 0xffff);
            _focus = Focus::Seed;
            break;
        }
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

void FractalBranchPage::updateLeds(Leds &leds) {}
