#include "FractalSequenceEditPage.h"

#include "ui/painters/WindowPainter.h"
#include "ui/painters/SequencePainter.h"
#include "ui/LedPainter.h"
#include "ui/MatrixMap.h"
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

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
    { "CLEAR BUF" },
    { "COPY" },
    { "PASTE" },
};

// 8-slot transform pool (matches branchPool bitmask order, KD-12).
static const char *kPoolNames[8] = { "Tra", "Rev", "Inv", "RtI", "Rot", "Cmp", "Exp", "Thn" };

static const FractalSequenceListModel::Item quickEditItems[8] = {
    FractalSequenceListModel::Item::RecordFirst,
    FractalSequenceListModel::Item::RecordLast,
    FractalSequenceListModel::Item::LoopFirst,
    FractalSequenceListModel::Item::LoopLast,
    FractalSequenceListModel::Item::OrnFirst,
    FractalSequenceListModel::Item::OrnLast,
    FractalSequenceListModel::Item::OrnamentRate,
    FractalSequenceListModel::Item::OrnamentIntensity,
};

// Branch-block label: pool abbrev + resolved param (KD-12). Paramless ops
// (Reverse/Inverse/RetInverse/GateThin) yield abbrev only. fx100 → compact
// ASCII factor (50→.5, 67→.67, 150→1.5, 200→2).
static void branchBlockLabel(StringBuilder &str, int kind, int param) {
    const char *ab = kPoolNames[kind];
    switch (kind) {
    case 0: str("%s%+d", ab, param); break;                 // Transpose
    case 4: str("%s%d", ab, param); break;                  // Rotate
    case 5: case 6: {                                        // Compress / Expand
        int whole = param / 100, frac = param % 100;
        if (frac == 0) str("%s%d", ab, whole);
        else if (frac % 10 == 0) { if (whole) str("%s%d.%d", ab, whole, frac / 10); else str("%s.%d", ab, frac / 10); }
        else { if (whole) str("%s%d.%d", ab, whole, frac); else str("%s.%d", ab, frac); }
        break;
    }
    default: str("%s", ab); break;                          // paramless
    }
}

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

// Compact source-slot ref: "-" for none, "<mode-letter> T<n>" for a parent track,
// or a compact channel name (CvIn2 / CvOut3 / Bus1 / Gate4 / Mod5) for a channel.
static void printSourceRef(StringBuilder &str, const Project &project, int index) {
    using S = Routing::Source;
    switch (FractalTrack::sourceKind(index)) {
    case FractalTrack::SourceKind::None:
        str("-");
        return;
    case FractalTrack::SourceKind::Track:
        str("%c T%d", Track::trackModeLetter(project.track(index).trackMode()), index + 1);
        return;
    case FractalTrack::SourceKind::Channel: {
        S ch = FractalTrack::sourceChannelOf(index);
        if (ch >= S::CvIn1 && ch <= S::CvIn4) str("CvIn%d", int(ch) - int(S::CvIn1) + 1);
        else if (ch >= S::CvOut1 && ch <= S::CvOut8) str("CvOut%d", int(ch) - int(S::CvOut1) + 1);
        else if (ch >= S::BusCv1 && ch <= S::BusCv4) str("Bus%d", int(ch) - int(S::BusCv1) + 1);
        else if (ch >= S::GateOut1 && ch <= S::GateOut8) str("Gate%d", int(ch) - int(S::GateOut1) + 1);
        else str("Mod%d", int(ch) - int(S::Mod1) + 1);
        return;
    }
    }
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

    const int tapeTop = 25, tapeBot = 44;
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

    // Nested zone lines above the tape — rec ⊇ loop ⊇ orn, top to bottom. Same
    // thin-line idiom for all three; the focused bracket is bright. The decreasing
    // width + indent reads the nesting invariant at a glance.
    canvas.setColor(_bracket == Bracket::Record ? Color::Bright : Color::Medium);
    canvas.hline(x0 + recFirst * stepW, tapeTop - 7, (recLast - recFirst + 1) * stepW);
    canvas.setColor(_bracket == Bracket::Loop ? Color::Bright : Color::MediumBright);
    canvas.hline(x0 + loopFirst * stepW, tapeTop - 5, (loopLast - loopFirst + 1) * stepW);
    canvas.setColor(_bracket == Bracket::Ornament ? Color::Bright : Color::MediumBright);
    canvas.hline(x0 + ornFirst * stepW, tapeTop - 3, (ornLast - ornFirst + 1) * stepW);

    // playhead
    canvas.setColor(Color::Bright);
    canvas.vline(x0 + cur * stepW + stepW / 2, tapeTop, tapeH);

    FixedStringBuilder<16> str;
    canvas.setFont(Font::Tiny);
    str("rec %d-%d", recFirst + 1, recLast + 1);
    canvas.setColor(_bracket == Bracket::Record ? Color::Bright : Color::Medium);
    canvas.drawText(2, 16, str);

    str.reset();
    str("orn %d-%d", ornFirst + 1, ornLast + 1);
    canvas.drawText(Width - 2 - canvas.textWidth(str), 16, str);

    str.reset();
    str("loop %d-%d", loopFirst + 1, loopLast + 1);
    canvas.drawText(90, 51, str);

    str.reset();
    str("R.Skip %d", seq.recordSkip());
    canvas.setColor(_editRecordSkip ? Color::Bright : Color::Medium);
    canvas.drawText(2, 51, str);

    str.reset();
    str("%dc", N);
    canvas.setColor(Color::Medium);
    canvas.drawText(Width - 2 - canvas.textWidth(str), 51, str);
}

void FractalSequenceEditPage::drawBranch(Canvas &canvas) {
    auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());
    const auto &eng = _engine.selectedTrackEngine().as<FractalTrackEngine>();

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
    const int playing = eng.currentSegment();
    const int queued = eng.queuedSegment();
    for (int j = 0; j < cols; ++j) {
        int x = j * bw;
        bool isTrunk = j == 0;
        bool isPlaying = j == playing;
        bool isQueued = queued >= 0 && j == queued;
        canvas.setColor(isPlaying ? Color::Bright : Color::Medium);   // bright border = playhead
        canvas.drawRect(x + 1, y, bw - 2, 10);
        str.reset();
        if (isTrunk) {
            str("T");
        } else {
            int kind = eng.branchKind(j), param = eng.branchParam(j);
            branchBlockLabel(str, kind, param);
            if (canvas.textWidth(str) > bw - 3) { str.reset(); str("%s", kPoolNames[kind]); }
        }
        canvas.setColor(isPlaying ? Color::Bright : Color::MediumBright);
        canvas.drawText(x + 1 + (bw - 2 - canvas.textWidth(str)) / 2, y + 6, str);
        // queued jump target: "Q" tag above the block, distinct from the
        // bright-border playhead.
        if (isQueued) {
            canvas.setColor(Color::MediumBright);
            canvas.drawText(x + 2, y - 1, "Q");
        }
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

    // LOCK footer highlights while the track is locked (persistent state); else
    // the active F-slot follows the encoder focus.
    int activeFn = _project.selectedTrack().fractalTrack().lock() ? 3 : int(_ornamentFocus);
    const char *footer[] = { "RATE", "INT", "SCALE", "LOCK", "NEXT" };
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
    canvas.drawText(2, 28, "Intens");
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
    canvas.setColor(_ornamentFocus == OrnamentFocus::Scale ? Color::Bright : Color::MediumBright);
    str.reset();
    if (seq.scale() < 0) str("Default");
    else str("%s", Scale::name(seq.scale()));
    canvas.drawText(40, 39, str);

    str.reset(); str("zone %d-%d", seq.ornFirst() + 1, seq.ornLast() + 1);
    canvas.setColor(Color::MediumBright);
    canvas.drawText(Width - 2 - canvas.textWidth(str), 39, str);

    // Last-fired ornament.
    canvas.setColor(Color::Medium);
    canvas.drawText(2, 50, "Last");
    canvas.setColor(Color::MediumBright);
    canvas.drawText(40, 50, FractalTrackEngine::ornamentName(eng.lastOrnament()));

    // Pending punch-in ornament, bright + right-aligned on the Last row so it
    // reads distinct from the last-fired readout.
    if (eng.queuedOrnament() >= 0) {
        str.reset();
        str("Q %s", FractalTrackEngine::ornamentName(eng.queuedOrnament()));
        canvas.setColor(Color::Bright);
        canvas.drawText(Width - 2 - canvas.textWidth(str), 50, str);
    }
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

    // Source A / Source B refs. A single-channel slot suffixes its label with the
    // slot's tailored role (A → CV, B → Gate); a track/none slot keeps plain A/B.
    using SK = FractalTrack::SourceKind;
    canvas.setColor(Color::Medium);
    const char *labelA = FractalTrack::sourceKind(track.sourceA()) == SK::Channel ? "A-CV" : "A";
    canvas.drawText(2, 16, labelA);
    canvas.setColor(_sourceFocus == SourceFocus::SourceA ? Color::Bright : Color::MediumBright);
    str.reset(); printSourceRef(str, _project, track.sourceA());
    canvas.drawText(28, 16, str);

    canvas.setColor(Color::Medium);
    const char *labelB = FractalTrack::sourceKind(track.sourceB()) == SK::Channel ? "B-Gt" : "B";
    canvas.drawText(196, 16, labelB);
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
    editEdge(_bracket, shift, value);
}

void FractalSequenceEditPage::editEdge(Bracket bracket, bool shift, int value) {
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
    switch (bracket) {
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
    }
}

void FractalSequenceEditPage::encoderSource(EncoderEvent &event) {
    auto &track = _project.selectedTrack().fractalTrack();
    int v = event.value();

    switch (_sourceFocus) {
    case SourceFocus::SourceA: track.editSourceA(v, false); break;
    case SourceFocus::SourceB: track.editSourceB(v, false); break;
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
    if (key.isQuickEdit()) {
        quickEdit(key.quickEdit());
        event.consume();
        return;
    }
    if (key.pageModifier()) return;   // Page held → let global nav switch away (exit)
    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }
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
    // Tuesday-style keypad: columns = rec/loop/orn edges (±8, nesting-clamped),
    // R.Skip (±1), Divisor (±1). Top row (S1-8) adds, bottom row (S9-16) subtracts.
    if (key.isStep()) {
        auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());
        int s = key.step();
        int col = s % 8;
        bool top = s < 8;
        int d8 = top ? 8 : -8;
        int d1 = top ? 1 : -1;
        switch (col) {
        case 0: editEdge(Bracket::Record, false, d8); break;   // Rec First
        case 1: editEdge(Bracket::Record, true, d8); break;    // Rec Last
        case 2: editEdge(Bracket::Loop, false, d8); break;     // Loop First
        case 3: editEdge(Bracket::Loop, true, d8); break;      // Loop Last
        case 4: editEdge(Bracket::Ornament, false, d8); break; // Orn First
        case 5: editEdge(Bracket::Ornament, true, d8); break;  // Orn Last
        case 6: seq.editRecordSkip(d1, false); break;          // R.Skip
        case 7: seq.editDivisor(-d1, false); break;            // Divisor: up = faster (lower divisor)
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
        case 2: _branchFocus = BranchFocus::Pool; break;   // select layer; encoder click toggles the bit
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
    if (key.isEncoder() && _branchFocus == BranchFocus::Pool) {
        auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());
        seq.setBranchPool(seq.branchPool() ^ (1 << _poolIndex));
        event.consume();
        return;
    }
    if (key.isStep()) {
        auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());
        const int s = key.step();
        if (s < 8) {
            // Top row S1..S(N+1): queue a beat-synced jump to that segment.
            if (s <= seq.branchCount())
                _engine.selectedTrackEngine().as<FractalTrackEngine>().queueSegment(s);
        } else {
            // Bottom row S9..S16: toggle the matching transform-pool bit.
            seq.setBranchPool(seq.branchPool() ^ (1 << (s - 8)));
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
        case 2: _ornamentFocus = OrnamentFocus::Scale; break;
        case 3: {
            // LOCK: toggle on press (freezes the ornament roll to the last-fired).
            auto &track = _project.selectedTrack().fractalTrack();
            track.setLock(!track.lock());
            showMessage(track.lock() ? "LOCKED" : "UNLOCKED");
            break;
        }
        default: break;
        }
        event.consume();
        return;
    }
    if (key.isStep()) {
        // S1..S15: queue ornament id 0..14 as a forced punch-in; S16 inert.
        if (key.step() < 15)
            _engine.selectedTrackEngine().as<FractalTrackEngine>().queueOrnament(key.step());
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
    if (key.isStep()) {
        auto &track = _project.selectedTrack().fractalTrack();
        const int s = key.step();
        if (s < int(FractalTrack::GateLogic::Last)) {
            // Top row S1..S6: select gate-logic mode.
            track.setGateLogic(FractalTrack::GateLogic(s));
        } else if (s == 15) {
            // S16: roll both logics at random.
            _engine.selectedTrackEngine().as<FractalTrackEngine>().randomizeLogic();
            showMessage("Random Logic");
        } else if (s >= 8 && (s - 8) < int(FractalTrack::CvLogic::Last)) {
            // Bottom row S9..S15: select cv-logic mode.
            track.setCvLogic(FractalTrack::CvLogic(s - 8));
        }
        event.consume();
        return;
    }
}

//----------
// Context menu
//----------

void FractalSequenceEditPage::contextShow(bool doubleClick) {
    if (!isActiveForSelectedTrack()) return;
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); },
        doubleClick
    ));
}

void FractalSequenceEditPage::contextAction(int index) {
    if (!isActiveForSelectedTrack()) return;
    auto &seq = _project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex());
    switch (ContextAction(index)) {
    case ContextAction::Init:
        seq.clear();
        showMessage("INIT");
        break;
    case ContextAction::ClearBuffer:
        _engine.selectedTrackEngine().as<FractalTrackEngine>().clearTrunk();
        showMessage("BUFFER CLEARED");
        break;
    case ContextAction::Copy:
        _model.clipBoard().copyFractalSequence(seq);
        showMessage("COPIED");
        break;
    case ContextAction::Paste:
        _model.clipBoard().pasteFractalSequence(seq);
        showMessage("PASTED");
        break;
    case ContextAction::Last:
        break;
    }
}

bool FractalSequenceEditPage::contextActionEnabled(int index) const {
    if (!isActiveForSelectedTrack()) return false;
    switch (ContextAction(index)) {
    case ContextAction::Paste:
        return _model.clipBoard().canPasteFractalSequence();
    default:
        return true;
    }
}

void FractalSequenceEditPage::quickEdit(int index) {
    if (!isActiveForSelectedTrack()) return;
    _quickEditModel.setSequence(&_project.selectedTrack().fractalTrack().sequence(_project.selectedPatternIndex()));
    int row = _quickEditModel.rowForItem(quickEditItems[index]);
    if (row < 0) return;
    _manager.pages().quickEdit.show(_quickEditModel, row);
}

void FractalSequenceEditPage::updateLeds(Leds &leds) {
    if (!isActiveForSelectedTrack()) return;

    // show quick edit keys
    if (globalKeyState()[Key::Page] && !globalKeyState()[Key::Shift]) {
        for (int i = 0; i < 8; ++i) {
            int index = MatrixMap::fromStep(i + 8);
            leds.unmask(index);
            leds.set(index, false, true);
            leds.mask(index);
        }
        return;
    }

    // Page not held: paint the per-page step grid (mirrors Stochastic's idiom).
    auto &track = _project.selectedTrack().fractalTrack();
    auto &seq = track.sequence(_project.selectedPatternIndex());
    const auto &eng = _engine.selectedTrackEngine().as<FractalTrackEngine>();
    // On/off only (like every other page); amber = selection, green = active, red = inactive.
    auto paint = [&](int i, bool red, bool green) {
        int idx = MatrixMap::fromStep(i);
        leds.unmask(idx); leds.set(idx, red, green); leds.mask(idx);
    };

    switch (_currentPage) {
    case Page::Branch: {
        const int N = seq.branchCount();
        const int playing = eng.currentSegment();
        const int queued = eng.queuedSegment();
        // Top row S1..S(N+1): present = green, playing = amber, queued = red.
        for (int i = 0; i <= N && i < 8; ++i) {
            if (queued >= 0 && i == queued) paint(i, true, false);
            else if (i == playing)          paint(i, true, true);
            else                            paint(i, false, true);
        }
        // Bottom row S9..S16: transform-pool bits — green when set.
        const int pool = seq.branchPool();
        for (int k = 0; k < 8; ++k) paint(8 + k, false, (pool >> k) & 1);
        break;
    }
    case Page::Ornament: {
        const int queued = eng.queuedOrnament();
        const int last = eng.lastOrnament();
        // S1..S15: slot = green, last-fired = red, queued = amber.
        for (int i = 0; i < 15; ++i) {
            if (i == queued)    paint(i, true, true);
            else if (i == last) paint(i, true, false);
            else                paint(i, false, true);
        }
        break;
    }
    case Page::Source: {
        // Selected mode = amber, other modes in the row = green.
        const int gsel = int(track.gateLogic());
        for (int i = 0; i < int(FractalTrack::GateLogic::Last); ++i) paint(i, i == gsel, true);
        const int csel = int(track.cvLogic());
        for (int i = 0; i < int(FractalTrack::CvLogic::Last); ++i) paint(8 + i, i == csel, true);
        paint(15, true, true);   // randomize = amber
        break;
    }
    case Page::Trunk:
        // Keypad: top row + (green), bottom row − (red).
        for (int i = 0; i < 8; ++i) paint(i, false, true);
        for (int i = 8; i < 16; ++i) paint(i, true, false);
        break;
    case Page::Last:
        break;
    }
}
