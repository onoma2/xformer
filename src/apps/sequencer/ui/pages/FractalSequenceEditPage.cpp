#include "FractalSequenceEditPage.h"

#include "ui/painters/WindowPainter.h"
#include "ui/painters/SequencePainter.h"
#include "Pages.h"

#include "model/FractalTrack.h"
#include "model/ModelUtils.h"
#include "model/Project.h"
#include "model/Scale.h"
#include "model/Track.h"
#include "model/Types.h"
#include "engine/FractalTrackEngine.h"

#include "core/math/Math.h"
#include "core/utils/StringBuilder.h"
#include "core/utils/Random.h"

#include <algorithm>

// 8-slot transform pool (matches branchPool bitmask order, KD-12).
static const char *kPoolNames[8] = { "Tr", "Rv", "In", "RI", "Ro", "Co", "Ex", "Gt" };

// Source-page segmented logic rows source their labels from
// FractalTrack::gateLogicName/cvLogicName via these wrappers.
static const char *gateLogicLabel(int i) { return FractalTrack::gateLogicName(FractalTrack::GateLogic(i)); }
static const char *cvLogicLabel(int i) { return FractalTrack::cvLogicName(FractalTrack::CvLogic(i)); }

// Outlined bar with a filled portion (0..100) and optional tier ticks.
static void drawBar(Canvas &canvas, int x, int y, int w, int h, int pct, bool highlight) {
    canvas.setColor(Color::Low);
    canvas.drawRect(x, y, w, h);
    int fw = std::max(1, (w - 2) * std::max(0, std::min(100, pct)) / 100);
    canvas.setColor(highlight ? Color::Bright : Color::MediumBright);
    canvas.fillRect(x + 1, y + 1, fw, h - 2);
}

// Compact track ref: "<mode-letter> T<n>" or "-" for none.
static void printSourceRef(StringBuilder &str, const Project &project, int index) {
    if (index < 0) { str("-"); return; }
    str("%c T%d", Track::trackModeLetter(project.track(index).trackMode()), index + 1);
}

// Single-select row of cells (mirrors the mockup _seg_row).
static void drawSegRow(Canvas &canvas, int x, int y, int w, const char *(*label)(int), int n, int sel, bool focus) {
    int cw = w / n;
    for (int i = 0; i < n; ++i) {
        int bx = x + i * cw;
        bool on = i == sel;
        const char *text = label(i);
        canvas.setColor(on ? Color::Bright : Color::Low);
        canvas.drawRect(bx, y, cw - 1, 9);
        canvas.setColor(on ? (focus ? Color::Bright : Color::MediumBright) : Color::MediumLow);
        canvas.drawText(bx + (cw - 1 - canvas.textWidth(text)) / 2, y + 7, text);
    }
}

FractalSequenceEditPage::FractalSequenceEditPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void FractalSequenceEditPage::enter() {}
void FractalSequenceEditPage::exit() {}

bool FractalSequenceEditPage::isActiveForSelectedTrack() const {
    return _project.selectedTrack().trackMode() == Track::TrackMode::Fractal &&
        _engine.selectedTrackEngine().trackMode() == Track::TrackMode::Fractal;
}

void FractalSequenceEditPage::nextPage() {
    int next = (int(_currentPage) + 1) % int(Page::Last);
    _currentPage = Page(next);
}

//----------
// Draw
//----------

void FractalSequenceEditPage::draw(Canvas &canvas) {
    if (!isActiveForSelectedTrack()) return;
    switch (_currentPage) {
    case Page::Trunk:    drawTrunk(canvas); break;
    case Page::Branch:   drawBranch(canvas); break;
    case Page::Ornament: drawOrnament(canvas); break;
    case Page::Source:   drawSource(canvas); break;
    case Page::Last:     break;
    }
}

void FractalSequenceEditPage::drawTrunk(Canvas &canvas) {
    auto &track = _project.selectedTrack().fractalTrack();
    auto &seq = track.sequence(_project.selectedPatternIndex());
    const auto &eng = _engine.selectedTrackEngine().as<FractalTrackEngine>();

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "FRACTAL");
    WindowPainter::drawActiveFunction(canvas, "LOOP");

    int activeFn = _editRecordSkip ? 3 : (_bracket == Bracket::Record ? 0 : (_bracket == Bracket::Loop ? 1 : 2));
    const char *footer[] = { "REC", "LOOP", "ORN", "R.Skip", "NEXT" };
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

    // Pitch-height/gate-width tape bars: no SequencePainter primitive renders a
    // filled bar at an arbitrary pitch height with a gate-derived width, so this
    // step rendering stays custom (drawLength/drawProbability don't fit).
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

    // record extent — loop brackets below the tape
    canvas.setColor(_bracket == Bracket::Record ? Color::Bright : Color::Medium);
    SequencePainter::drawLoopStart(canvas, x0 + recFirst * stepW, tapeBot + 2, (recLast - recFirst + 1) * stepW);
    SequencePainter::drawLoopEnd(canvas, x0 + recFirst * stepW, tapeBot + 2, (recLast - recFirst + 1) * stepW);

    // loop window — loop brackets on the tape top edge
    canvas.setColor(_bracket == Bracket::Loop ? Color::Bright : Color::MediumBright);
    SequencePainter::drawLoopStart(canvas, x0 + loopFirst * stepW, tapeTop - 1, (loopLast - loopFirst + 1) * stepW);
    SequencePainter::drawLoopEnd(canvas, x0 + loopFirst * stepW, tapeTop - 1, (loopLast - loopFirst + 1) * stepW);

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

    str.reset();
    str("R.Skip %d", seq.recordSkip());
    canvas.setColor(_editRecordSkip ? Color::Bright : Color::Medium);
    canvas.drawText(2, 51, str);
}

void FractalSequenceEditPage::drawBranch(Canvas &canvas) {
    auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "FRACTAL");
    WindowPainter::drawActiveFunction(canvas, "BRANCH");

    int activeFn = int(_branchFocus);
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
    for (int r = 1; r <= N; ++r) str(">B%d", FractalTrackEngine::routeOf(seq.path(), N, r));
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
        bool sel = (_branchFocus == BranchFocus::Pool) && (k == _poolIndex);
        canvas.setColor(en ? Color::Bright : Color::Low);
        canvas.drawRect(x + 1, py, 6, 6);
        if (en) canvas.fillRect(x + 2, py + 1, 4, 4);
        if (sel) { canvas.setColor(Color::Bright); canvas.drawRect(x - 1, py - 2, 10, 10); }
        canvas.setColor(en ? Color::MediumBright : Color::Low);
        canvas.drawText(x + 10, py + 5, kPoolNames[k]);
    }
}

void FractalSequenceEditPage::drawOrnament(Canvas &canvas) {
    auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());
    const auto &eng = _engine.selectedTrackEngine().as<FractalTrackEngine>();

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "FRACTAL");
    WindowPainter::drawActiveFunction(canvas, "ORN");

    int activeFn = int(_ornamentFocus);
    const char *footer[] = { "RATE", "INT", "SCALE", "ZONE", "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), activeFn);

    canvas.setFont(Font::Tiny);
    FixedStringBuilder<24> str;

    const int rate = seq.ornamentRate();
    const int intensity = seq.ornamentIntensity();

    // Rate
    canvas.setColor(Color::Medium);
    canvas.drawText(2, 17, "Rate");
    drawBar(canvas, 40, 13, 96, 6, rate, _ornamentFocus == OrnamentFocus::Rate);
    str.reset(); str("%d%%", rate);
    canvas.setColor(Color::MediumBright);
    canvas.drawText(Width - 2 - canvas.textWidth(str), 17, str);

    // Intensity (tier label: off / 2-step / 4-step / 8-step trill)
    canvas.setColor(Color::Medium);
    canvas.drawText(2, 28, "Int");
    drawBar(canvas, 40, 24, 96, 6, intensity, _ornamentFocus == OrnamentFocus::Intensity);
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
    canvas.setColor((_ornamentFocus == OrnamentFocus::Scale || _ornamentFocus == OrnamentFocus::Root) ? Color::Bright : Color::MediumBright);
    str.reset();
    Types::printNote(str, seq.rootNote());
    if (seq.scale() < 0) str(" Default");
    else str(" %s", Scale::name(seq.scale()));
    canvas.drawText(40, 39, str);

    str.reset(); str("zone %d-%d", seq.ornFirst(), seq.ornLast());
    canvas.setColor(Color::MediumBright);
    canvas.drawText(Width - 2 - canvas.textWidth(str), 39, str);

    // Last-fired ornament.
    canvas.setColor(Color::Medium);
    canvas.drawText(2, 50, "Last");
    canvas.setColor(Color::MediumBright);
    canvas.drawText(40, 50, FractalTrackEngine::ornamentName(eng.lastOrnament()));
}

void FractalSequenceEditPage::drawSource(Canvas &canvas) {
    auto &track = _project.selectedTrack().fractalTrack();

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "FRACTAL");
    WindowPainter::drawActiveFunction(canvas, "MIX");

    int activeFn = int(_sourceFocus);
    const char *footer[] = { "SrcA", "SrcB", "GATE", "CV", "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), activeFn);

    canvas.setFont(Font::Tiny);
    FixedStringBuilder<24> str;

    // Source A / Source B refs
    canvas.setColor(Color::Medium);
    canvas.drawText(2, 16, "A");
    canvas.setColor(_sourceFocus == SourceFocus::SourceA ? Color::Bright : Color::MediumBright);
    str.reset(); printSourceRef(str, _project, track.sourceA());
    canvas.drawText(14, 16, str);

    canvas.setColor(Color::Medium);
    canvas.drawText(200, 16, "B");
    canvas.setColor(_sourceFocus == SourceFocus::SourceB ? Color::Bright : Color::MediumBright);
    str.reset(); printSourceRef(str, _project, track.sourceB());
    canvas.drawText(Width - 2 - canvas.textWidth(str), 16, str);

    // Gate logic row
    canvas.setColor(Color::Medium);
    canvas.drawText(2, 29, "Gate");
    drawSegRow(canvas, 40, 22, 214, gateLogicLabel, int(FractalTrack::GateLogic::Last), int(track.gateLogic()), _sourceFocus == SourceFocus::Gate);

    // CV logic row
    canvas.setColor(Color::Medium);
    canvas.drawText(2, 40, "CV");
    drawSegRow(canvas, 40, 33, 214, cvLogicLabel, int(FractalTrack::CvLogic::Last), int(track.cvLogic()), _sourceFocus == SourceFocus::Cv);

    // Result readout
    canvas.setColor(Color::Medium);
    str.reset();
    str("%s + %s", FractalTrack::gateLogicName(track.gateLogic()), FractalTrack::cvLogicName(track.cvLogic()));
    canvas.drawText(2, 51, str);
}

//----------
// Bracket editor (Trunk)
//----------

void FractalSequenceEditPage::editBracket(int value, bool shift) {
    auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());
    const int N = std::max(1, _project.selectedTrack().fractalTrack().bufferLength());
    const int maxCell = N - 1;

    // Read current edges.
    int recF = seq.recordFirst(), recL = seq.recordLast();
    int loopF = seq.loopFirst(), loopL = seq.loopLast();
    int ornF = seq.ornFirst(), ornL = seq.ornLast();

    // Move the targeted edge, then enforce the nesting invariant:
    //   recF <= loopF <= ornF <= ornL <= loopL <= recL
    // Inner edges clamp against their outer neighbour; moving an outer edge
    // pushes the inners it would otherwise cross.
    switch (_bracket) {
    case Bracket::Record:
        if (!shift) { recF = clamp(recF + value, 0, recL); if (loopF < recF) loopF = recF; if (ornF < loopF) ornF = loopF; }
        else        { recL = clamp(recL + value, recF, maxCell); if (loopL > recL) loopL = recL; if (ornL > loopL) ornL = loopL; }
        break;
    case Bracket::Loop:
        if (!shift) { loopF = clamp(loopF + value, recF, loopL); if (ornF < loopF) ornF = loopF; }
        else        { loopL = clamp(loopL + value, loopF, recL); if (ornL > loopL) ornL = loopL; }
        break;
    case Bracket::Ornament:
        if (!shift) ornF = clamp(ornF + value, loopF, ornL);
        else        ornL = clamp(ornL + value, ornF, loopL);
        break;
    }

    seq.setRecordFirst(recF); seq.setRecordLast(recL);
    seq.setLoopFirst(loopF);  seq.setLoopLast(loopL);
    seq.setOrnFirst(ornF);    seq.setOrnLast(ornL);
}

//----------
// Encoder
//----------

void FractalSequenceEditPage::encoder(EncoderEvent &event) {
    if (!isActiveForSelectedTrack()) return;
    switch (_currentPage) {
    case Page::Trunk:    encoderTrunk(event); break;
    case Page::Branch:   encoderBranch(event); break;
    case Page::Ornament: encoderOrnament(event); break;
    case Page::Source:   encoderSource(event); break;
    case Page::Last:     break;
    }
}

void FractalSequenceEditPage::encoderTrunk(EncoderEvent &event) {
    if (_editRecordSkip) {
        auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());
        seq.editRecordSkip(event.value(), globalKeyState()[Key::Shift]);
        return;
    }
    editBracket(event.value(), globalKeyState()[Key::Shift]);
}

void FractalSequenceEditPage::encoderBranch(EncoderEvent &event) {
    auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());
    int v = event.value();

    switch (_branchFocus) {
    case BranchFocus::Count:
        seq.setBranchCount(seq.branchCount() + v);
        break;
    case BranchFocus::Path:
        seq.setPath(seq.path() + v);
        break;
    case BranchFocus::Pool:
        _poolIndex = clamp(_poolIndex + v, 0, 7);
        break;
    case BranchFocus::Seed:
        seq.setBranchSeed(seq.branchSeed() + v);
        break;
    }
}

void FractalSequenceEditPage::encoderOrnament(EncoderEvent &event) {
    auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());
    int v = event.value();

    switch (_ornamentFocus) {
    case OrnamentFocus::Rate:      seq.setOrnamentRate(seq.ornamentRate() + v); break;
    case OrnamentFocus::Intensity: seq.setOrnamentIntensity(seq.ornamentIntensity() + v); break;
    case OrnamentFocus::Scale:     seq.setScale(seq.scale() + v); break;
    case OrnamentFocus::Root:      seq.setRootNote(seq.rootNote() + v); break;
    }
}

void FractalSequenceEditPage::encoderSource(EncoderEvent &event) {
    auto &track = _project.selectedTrack().fractalTrack();
    int v = event.value();

    switch (_sourceFocus) {
    case SourceFocus::SourceA: track.setSourceA(track.sourceA() + v); break;
    case SourceFocus::SourceB: track.setSourceB(track.sourceB() + v); break;
    case SourceFocus::Gate:    track.setGateLogic(ModelUtils::adjustedEnum(track.gateLogic(), v)); break;
    case SourceFocus::Cv:      track.setCvLogic(ModelUtils::adjustedEnum(track.cvLogic(), v)); break;
    }
}

//----------
// Key press
//----------

void FractalSequenceEditPage::keyPress(KeyPressEvent &event) {
    if (!isActiveForSelectedTrack()) { event.consume(); return; }

    const auto &key = event.key();
    if (key.shiftModifier() && key.isFunction() && key.function() == 0) {
        auto &track = _project.selectedTrack().fractalTrack();
        track.setLock(!track.lock());
        showMessage(track.lock() ? "LOCKED" : "UNLOCKED");
        event.consume();
        return;
    }
    if (key.isFunction() && key.function() == 4) {
        nextPage();
        event.consume();
        return;
    }

    switch (_currentPage) {
    case Page::Trunk:    keyPressTrunk(event); break;
    case Page::Branch:   keyPressBranch(event); break;
    case Page::Ornament: keyPressOrnament(event); break;
    case Page::Source:   keyPressSource(event); break;
    case Page::Last:     break;
    }
}

void FractalSequenceEditPage::keyPressTrunk(KeyPressEvent &event) {
    const auto &key = event.key();
    if (key.isFunction()) {
        switch (key.function()) {
        case 0: _bracket = Bracket::Record; _editRecordSkip = false; break;
        case 1: _bracket = Bracket::Loop; _editRecordSkip = false; break;
        case 2: _bracket = Bracket::Ornament; _editRecordSkip = false; break;
        case 3: _editRecordSkip = true; break;
        default: break;
        }
        event.consume();
        return;
    }
}

void FractalSequenceEditPage::keyPressBranch(KeyPressEvent &event) {
    const auto &key = event.key();
    if (key.isFunction()) {
        switch (key.function()) {
        case 0: _branchFocus = BranchFocus::Count; break;
        case 1: _branchFocus = BranchFocus::Path; break;
        case 2: {
            auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());
            if (_branchFocus == BranchFocus::Pool) {
                // toggle the selected pool bit on a second press
                seq.setBranchPool(seq.branchPool() ^ (1 << _poolIndex));
            } else {
                _branchFocus = BranchFocus::Pool;
            }
            break;
        }
        case 3: {
            // SEED: reseed to a fresh random value.
            auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());
            Random rng(seq.branchSeed() ^ 0x9e3779b9u);
            seq.setBranchSeed(rng.next() & 0xffff);
            _branchFocus = BranchFocus::Seed;
            break;
        }
        default: break;
        }
        event.consume();
        return;
    }
}

void FractalSequenceEditPage::keyPressOrnament(KeyPressEvent &event) {
    const auto &key = event.key();
    if (key.isFunction()) {
        switch (key.function()) {
        case 0: _ornamentFocus = OrnamentFocus::Rate; break;
        case 1: _ornamentFocus = OrnamentFocus::Intensity; break;
        // F3 cycles Scale <-> Root (zone is edited on the Trunk page).
        case 2: _ornamentFocus = _ornamentFocus == OrnamentFocus::Scale ? OrnamentFocus::Root : OrnamentFocus::Scale; break;
        default: break;
        }
        event.consume();
        return;
    }
}

void FractalSequenceEditPage::keyPressSource(KeyPressEvent &event) {
    const auto &key = event.key();
    if (key.isFunction()) {
        switch (key.function()) {
        case 0: _sourceFocus = SourceFocus::SourceA; break;
        case 1: _sourceFocus = SourceFocus::SourceB; break;
        case 2: _sourceFocus = SourceFocus::Gate; break;
        case 3: _sourceFocus = SourceFocus::Cv; break;
        default: break;
        }
        event.consume();
        return;
    }
}

void FractalSequenceEditPage::updateLeds(Leds &leds) {}
