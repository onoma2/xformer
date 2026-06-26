#include "FractalTrunkPage.h"

#include "ui/painters/WindowPainter.h"
#include "Pages.h"

#include "model/FractalTrack.h"
#include "engine/FractalTrackEngine.h"

#include "core/utils/StringBuilder.h"

#include <algorithm>

FractalTrunkPage::FractalTrunkPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void FractalTrunkPage::enter() {}
void FractalTrunkPage::exit() {}

bool FractalTrunkPage::isActiveForSelectedTrack() const {
    return _project.selectedTrack().trackMode() == Track::TrackMode::Fractal &&
        _engine.selectedTrackEngine().trackMode() == Track::TrackMode::Fractal;
}

void FractalTrunkPage::draw(Canvas &canvas) {
    if (!isActiveForSelectedTrack()) return;

    auto &track = _project.selectedTrack().fractalTrack();
    auto &seq = track.sequence(_project.selectedPatternIndex());
    const auto &eng = _engine.selectedTrackEngine().as<FractalTrackEngine>();

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "FRACTAL");
    WindowPainter::drawActiveFunction(canvas, "LOOP");

    int activeFn = _bracket == Bracket::Record ? 0 : (_bracket == Bracket::Loop ? 1 : 2);
    const char *footer[] = { "REC", "LOOP", "ORN", "DIV", "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), activeFn);

    const int N = std::max(1, track.bufferLength());
    const int loopFirst = seq.loopFirst(), loopLast = seq.loopLast();
    const int ornFirst = seq.ornFirst(), ornLast = seq.ornLast();
    const int recFirst = seq.recordFirst(), recLast = seq.recordLast();
    const int cur = eng.readPos();

    const int tapeTop = 22, tapeBot = 44;
    const int tapeH = tapeBot - tapeTop + 1;
    const int margin = 4;
    const int stepW = std::max(1, (Width - 2 * margin) / N);
    const int used = stepW * N;
    const int x0 = (Width - used) / 2;

    // tape outline
    canvas.setColor(Color::Low);
    canvas.drawRect(x0, tapeTop, used, tapeH);

    for (int i = 0; i < N; ++i) {
        if (!eng.cellValid(i)) continue;
        float semis; int gateLen; bool valid;
        FractalTrackEngine::decodeCell(eng.trunk()[i], semis, gateLen, valid);
        if (!valid || gateLen == 0) continue;

        // height = pitch: floor-based, ±5 octaves mapped into the tape height.
        float pitch = (semis + 60.f) / 120.f;
        pitch = std::max(0.05f, std::min(1.f, pitch));
        int barH = std::max(1, int((tapeH - 3) * pitch));

        // width = gate length (0..15 of the cell width).
        int bw = std::max(1, stepW * gateLen / 15);

        int x = x0 + i * stepW;
        bool inLoop = i >= loopFirst && i <= loopLast;
        bool isCur = i == cur;
        canvas.setColor(isCur ? Color::Bright : (inLoop ? Color::MediumBright : Color::Low));
        canvas.fillRect(x, tapeBot - barH, bw, barH);
    }

    // ornament zone — band above the tape
    canvas.setColor(_bracket == Bracket::Ornament ? Color::Bright : Color::MediumBright);
    canvas.hline(x0 + ornFirst * stepW, tapeTop - 3, (ornLast - ornFirst + 1) * stepW);

    // record extent — bracket ticks below the tape
    canvas.setColor(_bracket == Bracket::Record ? Color::Bright : Color::Medium);
    canvas.vline(x0 + recFirst * stepW, tapeBot + 1, 3);
    canvas.vline(x0 + (recLast + 1) * stepW - 1, tapeBot + 1, 3);

    // loop window — bracket ticks on the tape top edge
    canvas.setColor(_bracket == Bracket::Loop ? Color::Bright : Color::MediumBright);
    canvas.vline(x0 + loopFirst * stepW, tapeTop - 2, 4);
    canvas.vline(x0 + (loopLast + 1) * stepW - 1, tapeTop - 2, 4);

    // playhead
    canvas.setColor(Color::Bright);
    canvas.vline(x0 + cur * stepW + stepW / 2, tapeTop, tapeH);

    FixedStringBuilder<16> str;
    canvas.setFont(Font::Tiny);
    canvas.setColor(Color::Medium);
    str("%dc", N);
    canvas.drawText(2, 16, str);

    str.reset();
    str("orn %d-%d", ornFirst, ornLast);
    canvas.drawText(Width - 2 - canvas.textWidth(str), 16, str);

    str.reset();
    str("loop %d-%d", loopFirst, loopLast);
    canvas.drawText(90, 51, str);
}

void FractalTrunkPage::editBracket(int value, bool shift) {
    auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());
    const int N = std::max(1, _project.selectedTrack().fractalTrack().bufferLength());
    const int maxCell = N - 1;

    // Read current edges.
    int recF = seq.recordFirst(), recL = seq.recordLast();
    int loopF = seq.loopFirst(), loopL = seq.loopLast();
    int ornF = seq.ornFirst(), ornL = seq.ornLast();

    auto clampi = [](int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); };

    // Move the targeted edge, then enforce the nesting invariant:
    //   recF <= loopF <= ornF <= ornL <= loopL <= recL
    // Inner edges clamp against their outer neighbour; moving an outer edge
    // pushes the inners it would otherwise cross.
    switch (_bracket) {
    case Bracket::Record:
        if (!shift) { recF = clampi(recF + value, 0, recL); if (loopF < recF) loopF = recF; if (ornF < loopF) ornF = loopF; }
        else        { recL = clampi(recL + value, recF, maxCell); if (loopL > recL) loopL = recL; if (ornL > loopL) ornL = loopL; }
        break;
    case Bracket::Loop:
        if (!shift) { loopF = clampi(loopF + value, recF, loopL); if (ornF < loopF) ornF = loopF; }
        else        { loopL = clampi(loopL + value, loopF, recL); if (ornL > loopL) ornL = loopL; }
        break;
    case Bracket::Ornament:
        if (!shift) ornF = clampi(ornF + value, loopF, ornL);
        else        ornL = clampi(ornL + value, ornF, loopL);
        break;
    }

    seq.setRecordFirst(recF); seq.setRecordLast(recL);
    seq.setLoopFirst(loopF);  seq.setLoopLast(loopL);
    seq.setOrnFirst(ornF);    seq.setOrnLast(ornL);
}

void FractalTrunkPage::encoder(EncoderEvent &event) {
    if (!isActiveForSelectedTrack()) return;
    editBracket(event.value(), event.pressed());
}

void FractalTrunkPage::keyPress(KeyPressEvent &event) {
    if (!isActiveForSelectedTrack()) { event.consume(); return; }

    const auto &key = event.key();
    if (key.isFunction()) {
        switch (key.function()) {
        case 0: _bracket = Bracket::Record; break;
        case 1: _bracket = Bracket::Loop; break;
        case 2: _bracket = Bracket::Ornament; break;
        case 4: {
            auto &pages = _manager.pages();
            _manager.replace(_manager.stackSize() - 1, &pages.fractalBranch);
            break;
        }
        default: break;
        }
        event.consume();
        return;
    }
}

void FractalTrunkPage::updateLeds(Leds &leds) {}
