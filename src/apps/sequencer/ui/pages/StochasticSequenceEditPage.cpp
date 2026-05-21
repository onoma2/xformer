#include "StochasticSequenceEditPage.h"

#include "ui/painters/WindowPainter.h"
#include "ui/MatrixMap.h"
#include "model/StochasticTrack.h"
#include "model/StochasticTypes.h"
#include "core/utils/Random.h"

#include "engine/StochasticTrackEngine.h"

#include <cmath>
#include <cstdint>

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
    { "EVEN" },
    { "RAND" },
};

static const ContextMenuModel::Item durContextMenuItems[] = {
    { "INIT" },
    { "EVEN" },
    { "RAND" },
};

StochasticSequenceEditPage::StochasticSequenceEditPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void StochasticSequenceEditPage::enter() {
}

void StochasticSequenceEditPage::exit() {
}

void StochasticSequenceEditPage::nextPage() {
    auto &seq = _project.selectedTrack().stochasticTrack().sequence(_project.selectedPatternIndex());
    int next = (int(_currentPage) + 1) % int(Page::Count);
    // Skip Marbles page entirely when marblesMode is Off.
    if (Page(next) == Page::Marbles && seq.marblesMode() == MarblesMode::Off) {
        next = (next + 1) % int(Page::Count);
    }
    _currentPage = Page(next);
    _heroHeldStep = -1;
}

//----------
// Draw
//----------

void StochasticSequenceEditPage::draw(Canvas &canvas) {
    switch (_currentPage) {
    case Page::Core:     drawCorePage(canvas); break;
    case Page::Marbles:  drawMarblesPage(canvas); break;
    case Page::Direct:   drawDirectPage(canvas); break;
    case Page::Loop:     drawLoopPage(canvas); break;
    case Page::Pitch:    drawPitchPage(canvas); break;
    case Page::Duration: drawDurationPage(canvas); break;
    case Page::Count:    break;
    }
}

//----------
// Hero page draws
//----------

void StochasticSequenceEditPage::drawCorePage(Canvas &canvas) {
    auto &seq = _project.selectedTrack().stochasticTrack().sequence(_project.selectedPatternIndex());

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "CORE");

    int density = seq.density();
    int complexity = seq.complexity();
    bool marblesOn = (seq.marblesMode() == MarblesMode::On);

    int dropCount = 12 + (density * 88) / 100;          // ~12..100
    int jitter = 2 + (complexity * 14) / 100;           // 2..16
    int wind = marblesOn ? 2 : 0;

    Random rng(11);
    canvas.setBlendMode(BlendMode::Set);
    for (int i = 0; i < dropCount; ++i) {
        int x = 2 + int(rng.nextRange(Width - 4));
        int y = 13 + int(rng.nextRange(36));
        int dropLen = 2 + int(rng.nextRange(uint32_t(jitter / 2 + 1)));
        int b = int(rng.nextRange(15));
        Color col = (b >= 12) ? Color::Bright :
                    (b >= 8)  ? Color::MediumBright :
                    (b >= 4)  ? Color::Medium :
                                Color::Low;
        canvas.setColor(col);
        for (int j = 0; j < dropLen; ++j) {
            int px = x + (wind * j) / std::max(1, dropLen);
            int py = y + j;
            if (py >= 12 && py <= 51) canvas.point(px, py);
        }
    }

    FixedStringBuilder<32> str;
    if (_heroHeldStep == 0) str("DENS %d", density);
    else if (_heroHeldStep == 1) str("CMPX %d", complexity);
    else str("D%d  C%d", density, complexity);
    canvas.setFont(Font::Small);
    canvas.setColor(Color::Bright);
    canvas.drawText(8, 18, str);

    canvas.setColor(marblesOn ? Color::Bright : Color::Medium);
    const char *shapeLabel = marblesOn ? "SHAPE ON" : "SHAPE OFF";
    canvas.drawText(Width - canvas.textWidth(shapeLabel) - 8, 18, shapeLabel);

    // Dynamic footer labels — show actual current state per DiscreteMap convention.
    auto &track = _project.selectedTrack().stochasticTrack();
    const char *modeLabel = "LIVE";
    if (seq.rhythmMode() == seq.melodyMode()) {
        modeLabel = (seq.rhythmMode() == StochasticSourceMode::Loop) ? "LOOP" : "LIVE";
    } else {
        modeLabel = "SPLIT";
    }
    const char *lockLabel = track.lock() ? "LOCKED" : "FREE";
    const char *footer[] = { marblesOn ? "MARBLE" : "SHAPE", modeLabel, "RENEW", lockLabel, "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), -1);
}

void StochasticSequenceEditPage::drawMarblesPage(Canvas &canvas) {
    auto &seq = _project.selectedTrack().stochasticTrack().sequence(_project.selectedPatternIndex());

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "MARBLES");

    int bias = seq.marblesBias();       // 0..100, 50=center
    int spreadVal = seq.marblesSpread(); // 0..100
    int stepsVal = seq.marblesSteps();   // 1..100
    bool on = (seq.marblesMode() == MarblesMode::On);

    int biasOffset = ((bias - 50) * 4) / 5;            // -40..+40
    int cx = Width / 2 + biasOffset;
    int spread = std::max(8, (spreadVal * 4) / 5);
    int stepsCount = std::max(2, stepsVal);
    int baseline = 50;
    int peakY = 14;

    canvas.setBlendMode(BlendMode::Set);
    if (!on) {
        canvas.setColor(Color::Low);
        canvas.hline(8, baseline, Width - 16);
    } else {
        // Quantization ticks
        canvas.setColor(Color::Low);
        int bandW = std::max(2, (spread * 4) / stepsCount);
        for (int k = -stepsCount; k <= stepsCount; ++k) {
            int x = cx + k * bandW;
            if (x >= 0 && x < Width) canvas.vline(x, baseline - 1, 3);
        }
        // Bell curve fill — gaussian-ish, gradient brighter toward peak
        for (int x = 0; x < Width; ++x) {
            float d = float(x - cx) / float(spread);
            float env = expf(-d * d * 1.4f);
            int h = int(env * (baseline - peakY));
            if (h <= 0) continue;
            int top = baseline - h;
            for (int y = top; y <= baseline; ++y) {
                int v = ((baseline - y) * 85) / std::max(1, h * 100 / 100);
                v = (baseline - y) * 100 / std::max(1, h);
                Color col = (v >= 85) ? Color::Bright :
                            (v >= 60) ? Color::MediumBright :
                            (v >= 35) ? Color::Medium :
                            (v >= 12) ? Color::MediumLow :
                                        Color::Low;
                canvas.setColor(col);
                canvas.point(x, y);
            }
        }
        // Peak marker
        canvas.setColor(Color::Bright);
        canvas.point(cx, peakY - 1);
        canvas.point(cx - 1, peakY);
        canvas.point(cx + 1, peakY);
    }

    FixedStringBuilder<32> str;
    if (_heroHeldStep == 0) str("BIAS %d", bias);
    else if (_heroHeldStep == 1) str("SPRD %d", spreadVal);
    else if (_heroHeldStep == 2) str("STEP %d", stepsVal);
    else str("B%d  S%d  N%d", bias, spreadVal, stepsVal);
    canvas.setFont(Font::Small);
    canvas.setColor(Color::Bright);
    canvas.drawText(8, 18, str);

    const char *footer[] = { "SHAPE", nullptr, nullptr, nullptr, "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), -1);
}

void StochasticSequenceEditPage::drawDirectPage(Canvas &canvas) {
    auto &seq = _project.selectedTrack().stochasticTrack().sequence(_project.selectedPatternIndex());

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "DIRECT");

    // Live CV / gate from engine
    float liveCv = 0.f;
    bool liveGate = false;
    if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
        auto &eng = _engine.selectedTrackEngine().as<StochasticTrackEngine>();
        liveCv = eng.cvOutput(0);
        liveGate = eng.gateOutput(0);
    }

    canvas.setBlendMode(BlendMode::Set);

    // Pond floor — dim dotted line at y=31
    canvas.setColor(Color::Low);
    for (int x = 0; x < Width; x += 4) canvas.point(x, 31);

    // Live point at right side, y by CV
    int px = (Width * 72) / 100;
    int py = 31 - int(liveCv * 16.f);
    py = clamp(py, 13, 49);
    Color ringCols[] = { Color::Bright, Color::MediumBright, Color::Medium, Color::Low };
    int ringRs[] = { 2, 5, 9, 14 };
    for (int i = 0; i < 4; ++i) {
        canvas.setColor(ringCols[i]);
        int r = ringRs[i];
        // Sample 32 points around circle
        for (int k = 0; k < 32; ++k) {
            float a = float(k) * 6.2832f / 32.f;
            int x = px + int(r * cosf(a));
            int y = py + int(r * sinf(a));
            if (x >= 0 && x < Width && y >= 12 && y <= 51) canvas.point(x, y);
        }
    }
    if (liveGate) {
        canvas.setColor(Color::Bright);
        canvas.fillRect(px - 1, py - 1, 3, 3);
    }

    // Param labels
    const char *names[] = { "VAR", "TILT", "CONT", "RATE", "LIN", "REST", "SLIDE", "JUMP" };
    int values[] = {
        seq.variation(), seq.tilt(), seq.contour(), seq.rate(),
        seq.linearity(), seq.rest(), seq.slide(), seq.jump()
    };
    FixedStringBuilder<32> str;
    if (_heroHeldStep >= 0 && _heroHeldStep < 8) {
        str("%s %d", names[_heroHeldStep], values[_heroHeldStep]);
    } else {
        str("DIRECT");
    }
    canvas.setFont(Font::Small);
    canvas.setColor(Color::Bright);
    canvas.drawText(8, 18, str);

    const char *footer[] = { nullptr, nullptr, nullptr, nullptr, "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), -1);
}

void StochasticSequenceEditPage::drawLoopPage(Canvas &canvas) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &seq = track.sequence(_project.selectedPatternIndex());

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "LOOP");

    int margin = 10;
    int tapeTop = 22;
    int tapeBot = 40;
    int availW = Width - 2 * margin;

    canvas.setBlendMode(BlendMode::Set);

    int tapeRowH = tapeBot - tapeTop + 1;
    int lf = seq.first();
    int ll = seq.last();
    int seqSize = std::max(1, seq.size());
    int windowSize = ll - lf + 1;
    int rot = seq.rotate();

    bool haveLoopContent = seq.rhythmValid() || seq.melodyValid();

    int currentStep = -1;
    if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
        auto &eng = _engine.selectedTrackEngine().as<StochasticTrackEngine>();
        currentStep = eng.currentStep();
    }

    // Adaptive event timeline: ALL steps 0..size-1 always visible. Brackets
    // snap to the [first, last] window — moving first/last shifts only the
    // brackets, not the slot count. Rotate shifts which event plays at each
    // window slot (same logic the engine uses for read-index resolution).
    const int minStepW = 3;

    auto resolveEventIdx = [&] (int displayIdx) -> int {
        if (displayIdx >= lf && displayIdx <= ll && windowSize > 0 && rot != 0) {
            int offset = (displayIdx - lf + rot) % windowSize;
            if (offset < 0) offset += windowSize;
            return lf + offset;
        }
        return displayIdx;
    };

    // Total ticks + pitch range across all slots (adaptive per-frame normalization).
    int totalTicks = 0;
    int activeCount = 0;
    int pitchMin = INT32_MAX;
    int pitchMax = INT32_MIN;
    for (int i = 0; i < seqSize; ++i) {
        int eIdx = resolveEventIdx(i);
        const auto &ev = seq.events()[eIdx];
        int durIdx = ev.durationIndex();
        int ticks = int(StochasticTrackEngine::getDurationMultiplier(durIdx));
        totalTicks += ticks;
        if (ticks > 0) activeCount++;
        if (!ev.rest()) {
            int pitch = int(ev.degree()) + int(ev.octave()) * 12;
            if (pitch < pitchMin) pitchMin = pitch;
            if (pitch > pitchMax) pitchMax = pitch;
        }
    }
    if (totalTicks <= 0) totalTicks = 1;
    if (activeCount <= 0) activeCount = 1;
    int pitchRange = std::max(1, pitchMax - pitchMin);
    if (pitchMin > pitchMax) { pitchMin = 0; pitchMax = 0; }  // no gates → no range

    int extraPixels = availW - minStepW * activeCount;
    if (extraPixels < 0) extraPixels = 0;
    int error = 0;
    int currentX = margin;
    int bxFirst = margin;        // pixel x of left bracket
    int bxLast = margin + availW; // pixel x of right bracket

    for (int i = 0; i < seqSize; ++i) {
        int eIdx = resolveEventIdx(i);
        const auto &ev = seq.events()[eIdx];
        int durIdx = ev.durationIndex();
        int ticks = int(StochasticTrackEngine::getDurationMultiplier(durIdx));
        int stepW = 0;
        if (ticks > 0) {
            int scaled = extraPixels * ticks + error;
            int extraW = scaled / totalTicks;
            error = scaled % totalTicks;
            stepW = minStepW + extraW;
        }
        if (stepW <= 0) stepW = 1;

        // Snap bracket positions to slot boundaries.
        if (i == lf) bxFirst = currentX;
        if (i == ll) bxLast = currentX + stepW - 1;

        bool inWindow = (i >= lf && i <= ll);
        bool isRest = ev.rest();
        bool isActive = (i == currentStep);

        if (!haveLoopContent) {
            // No captured loop content yet — just a flat dim slot.
            canvas.setColor(inWindow ? Color::MediumLow : Color::Low);
            canvas.fillRect(currentX, tapeTop, stepW, tapeRowH);
        } else {
            Color frameCol;
            if (isActive)        frameCol = Color::Bright;
            else if (!inWindow)  frameCol = Color::Low;
            else if (isRest)     frameCol = Color::Medium;
            else                 frameCol = Color::MediumBright;
            canvas.setColor(frameCol);
            canvas.drawRect(currentX, tapeTop, stepW, tapeRowH);

            // Pitch-mapped fill bar: bar climbs from slot bottom; bar height encodes
            // (degree + octave*12) normalized to the loop's observed pitch range.
            if (!isRest && stepW > 2) {
                int innerW = stepW - 2;
                int innerH = tapeRowH - 2;
                int pitch = int(ev.degree()) + int(ev.octave()) * 12;
                int barH;
                if (pitchRange > 0) {
                    int norm = clamp(pitch - pitchMin, 0, pitchRange);
                    barH = 1 + (norm * (innerH - 1)) / pitchRange;
                } else {
                    barH = innerH;  // single-pitch loop — fill fully.
                }
                Color fillCol = isActive ? Color::Bright :
                                (inWindow ? Color::Medium : Color::Low);
                canvas.setColor(fillCol);
                canvas.fillRect(currentX + 1, tapeTop + 1 + (innerH - barH), innerW, barH);
            }
        }

        currentX += stepW;
    }

    // Mutation scratches — across the loop window only.
    int mutateVal = seq.mutate();
    int nScratches = (mutateVal * 10) / 100;
    int scratchSpan = std::max(2, bxLast - bxFirst - 2);
    Random rng(7);
    for (int i = 0; i < nScratches; ++i) {
        int sx = bxFirst + 1 + int(rng.nextRange(uint32_t(scratchSpan)));
        int sy = tapeTop + 1 + int(rng.nextRange(uint32_t(tapeRowH - 2)));
        canvas.setColor(Color::None);
        canvas.fillRect(sx, sy, 2, 2);
    }

    // Window brackets — snapped to slot boundaries computed during the loop.
    canvas.setColor(Color::Bright);
    canvas.vline(bxFirst - 1, tapeTop - 3, tapeBot - tapeTop + 7);
    canvas.hline(bxFirst - 1, tapeTop - 3, 4);
    canvas.hline(bxFirst - 1, tapeBot + 3, 4);
    canvas.vline(bxLast + 1, tapeTop - 3, tapeBot - tapeTop + 7);
    canvas.hline(bxLast - 2, tapeTop - 3, 4);
    canvas.hline(bxLast - 2, tapeBot + 3, 4);

    // Boredom counter bar — engine internal _loopCycleCount progress to patience threshold.
    // Read from engine if available.
    int boredomFill = 0;
    int patience = seq.patience();
    if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
        auto &eng = _engine.selectedTrackEngine().as<StochasticTrackEngine>();
        int loops = eng.loopCycleCount();
        // patience 0..100 → bucket 0..7, loopsBeforeRefresh = 1<<bucket
        int bucket = clamp(patience / 14, 0, 7);
        uint32_t threshold = (patience == 100) ? UINT32_MAX : (uint32_t(1) << bucket);
        if (threshold > 0) {
            boredomFill = int((uint64_t(loops) * 100) / std::max(uint32_t(1), threshold));
            boredomFill = clamp(boredomFill, 0, 100);
        }
    }
    canvas.setColor(Color::Low);
    canvas.drawRect(margin, tapeBot + 7, availW, 3);
    canvas.setColor(Color::MediumBright);
    canvas.fillRect(margin, tapeBot + 7, (boredomFill * availW) / 100, 3);

    // Param labels
    FixedStringBuilder<32> str;
    if (_heroHeldStep == 0)       str("PAT %d", patience);
    else if (_heroHeldStep == 1)  str("MUT %d", seq.mutate());
    else if (_heroHeldStep == 2)  str("JMP %d", seq.jump());
    else if (_heroHeldStep == 3)  str("SLP %d", seq.sleep());
    else if (_heroHeldStep == 8)  str("FRST %d", seq.first());
    else if (_heroHeldStep == 9)  str("LAST %d", seq.last());
    else if (_heroHeldStep == 10) str("SIZE %d", seq.size());
    else if (_heroHeldStep == 11) str("ROT %+d", seq.rotate());
    else                           str("LOOP");
    canvas.setFont(Font::Small);
    canvas.setColor(Color::Bright);
    canvas.drawText(8, 18, str);

    canvas.setColor(track.lock() ? Color::Bright : Color::Medium);
    const char *lockState = track.lock() ? "LOCKED" : "LIVE";
    canvas.drawText(Width - canvas.textWidth(lockState) - 8, 18, lockState);

    const char *modeLabel = "LIVE";
    if (seq.rhythmMode() == seq.melodyMode()) {
        modeLabel = (seq.rhythmMode() == StochasticSourceMode::Loop) ? "LOOP" : "LIVE";
    } else {
        modeLabel = "SPLIT";
    }
    const char *lockFooter = track.lock() ? "LOCKED" : "FREE";
    const char *footer[] = { modeLabel, "RENEW", lockFooter, nullptr, "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), -1);
}

//----------
// Hero step edits — encoder writes the held step's param
//----------

void StochasticSequenceEditPage::editCoreStep(int step, int value, bool shift) {
    auto &seq = _project.selectedTrack().stochasticTrack().sequence(_project.selectedPatternIndex());
    int v = shift ? value * 10 : value;
    switch (step) {
    case 0: seq.setDensity(seq.density() + v); break;
    case 1: seq.setComplexity(seq.complexity() + v); break;
    }
}

void StochasticSequenceEditPage::editMarblesStep(int step, int value, bool shift) {
    auto &seq = _project.selectedTrack().stochasticTrack().sequence(_project.selectedPatternIndex());
    int v = shift ? value * 10 : value;
    switch (step) {
    case 0: seq.setMarblesBias(seq.marblesBias() + v); break;
    case 1: seq.setMarblesSpread(seq.marblesSpread() + v); break;
    case 2: seq.setMarblesSteps(seq.marblesSteps() + v); break;
    }
}

void StochasticSequenceEditPage::editDirectStep(int step, int value, bool shift) {
    auto &seq = _project.selectedTrack().stochasticTrack().sequence(_project.selectedPatternIndex());
    int v = shift ? value * 10 : value;
    switch (step) {
    case 0: seq.setVariation(seq.variation() + v); break;
    case 1: seq.setTilt(seq.tilt() + v); break;
    case 2: seq.setContour(seq.contour() + v); break;
    case 3: seq.setRate(seq.rate() + v); break;
    case 4: seq.setLinearity(seq.linearity() + v); break;
    case 5: seq.setRest(seq.rest() + v); break;
    case 6: seq.setSlide(seq.slide() + v); break;
    case 7: seq.setJump(seq.jump() + v); break;
    }
}

void StochasticSequenceEditPage::editLoopStep(int step, int value, bool shift) {
    auto &seq = _project.selectedTrack().stochasticTrack().sequence(_project.selectedPatternIndex());
    int v = shift ? value * 10 : value;
    switch (step) {
    // Top row — destructive / timing params (red LEDs)
    case 0: seq.setPatience(seq.patience() + v); break;
    case 1: seq.setMutate(seq.mutate() + v); break;
    case 2: seq.setJump(seq.jump() + v); break;
    case 3: seq.setSleep(seq.sleep() + v); break;
    // Bottom row — loop window / shape params (green LEDs)
    case 8:  seq.setFirst(seq.first() + value); break;
    case 9:  seq.setLast(seq.last() + value); break;
    case 10: seq.setSize(seq.size() + value); break;
    case 11: seq.setRotate(seq.rotate() + value); break;
    }
}

//----------
// Hero Fn handlers
//----------

bool StochasticSequenceEditPage::handleCoreFunction(int fn, bool shift) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &seq = track.sequence(_project.selectedPatternIndex());
    switch (fn) {
    case 0: // SHAPE — toggle marblesMode and jump to Marbles page when turning On
        seq.setMarblesMode(seq.marblesMode() == MarblesMode::Off ? MarblesMode::On : MarblesMode::Off);
        if (seq.marblesMode() == MarblesMode::On) {
            _currentPage = Page::Marbles;
            _heroHeldStep = -1;
        }
        return true;
    case 1: { // MODE — toggle coupled rhythm/melody between Loop and Live
        auto newMode = (seq.rhythmMode() == StochasticSourceMode::Loop)
            ? StochasticSourceMode::Live : StochasticSourceMode::Loop;
        seq.setRhythmMode(newMode);
        seq.setMelodyMode(newMode);
        return true;
    }
    case 2: // RENEW — refresh loop sources
        if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
            auto &eng = _engine.selectedTrackEngine().as<StochasticTrackEngine>();
            eng.renewLoopSources();
            showMessage("RENEW");
        }
        return true;
    case 3: // LOCK toggle
        track.setLock(!track.lock());
        return true;
    }
    return false;
}

bool StochasticSequenceEditPage::handleMarblesFunction(int fn, bool shift) {
    auto &seq = _project.selectedTrack().stochasticTrack().sequence(_project.selectedPatternIndex());
    switch (fn) {
    case 0: // SHAPE — back to Core (marbles off)
        seq.setMarblesMode(MarblesMode::Off);
        _currentPage = Page::Core;
        return true;
    }
    return false;
}

bool StochasticSequenceEditPage::handleDirectFunction(int /*fn*/, bool /*shift*/) {
    return false;
}

bool StochasticSequenceEditPage::handleLoopFunction(int fn, bool shift) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &seq = track.sequence(_project.selectedPatternIndex());
    switch (fn) {
    case 0: { // MODE — toggle coupled rhythm/melody Loop/Live
        auto newMode = (seq.rhythmMode() == StochasticSourceMode::Loop)
            ? StochasticSourceMode::Live : StochasticSourceMode::Loop;
        seq.setRhythmMode(newMode);
        seq.setMelodyMode(newMode);
        return true;
    }
    case 1: // RENEW
        if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
            auto &eng = _engine.selectedTrackEngine().as<StochasticTrackEngine>();
            eng.renewLoopSources();
            showMessage("RENEW");
        }
        return true;
    case 2: // LOCK toggle
        track.setLock(!track.lock());
        return true;
    }
    return false;
}

void StochasticSequenceEditPage::drawPitchPage(Canvas &canvas) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &sequence = track.sequence(_project.selectedPatternIndex());
    auto &scale = sequence.selectedScale(_project.scale());
    int scaleSize = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);

    if (_selectedDegree >= scaleSize) {
        _selectedDegree = scaleSize - 1;
    }

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "SCALE TICKETS");

    int baseY = 46;
    int barMaxH = 26;
    int barW = std::max(3, (Width - 16) / scaleSize - 2);
    int gap = 2;
    if (scaleSize > 20) { gap = 1; barW = std::max(2, (Width - 16) / scaleSize - 1); }

    int totalW = scaleSize * (barW + gap) - gap;
    int xOffset = std::max(8, (Width - totalW) / 2);

    int maxTicket = 0, totalTickets = 0, activeSlotCount = 0;
    for (int i = 0; i < scaleSize; ++i) {
        int t = sequence.effectiveDegreeTicket(i, scaleSize);
        if (t >= 0) {
            if (t > maxTicket) maxTicket = t;
            totalTickets += t;
            ++activeSlotCount;
        }
    }
    bool uniformFallback = (totalTickets == 0 && activeSlotCount > 0);
    int denom = std::max(1, maxTicket);

    if (totalTickets > 0 && activeSlotCount > 0) {
        int avgH = (totalTickets * barMaxH) / (activeSlotCount * denom);
        if (avgH > 0 && avgH < barMaxH) {
            canvas.setBlendMode(BlendMode::Set);
            canvas.setColor(Color::Low);
            canvas.hline(xOffset - 2, baseY - avgH, totalW + 4);
        }
    }

    for (int i = 0; i < scaleSize; ++i) {
        int degreeIndex = i;
        int x = xOffset + i * (barW + gap);
        int tickets = sequence.effectiveDegreeTicket(degreeIndex, scaleSize);

        bool active = false;
        int activeDegree = -1;
        if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
            auto &stoEng = _engine.selectedTrackEngine().as<StochasticTrackEngine>();
            activeDegree = stoEng.lastDegree() % scaleSize;
            active = (degreeIndex == activeDegree);
        }
        bool selected = (_pitchSelectionMask & (1U << degreeIndex)) != 0;

        if (tickets < 0) {
            canvas.setBlendMode(BlendMode::Set);
            canvas.setColor(active ? Color::Bright : (selected ? Color::Medium : Color::Low));
            canvas.line(x, baseY - 5, x + barW - 1, baseY);
            canvas.line(x, baseY, x + barW - 1, baseY - 5);
        } else if (tickets == 0 && !uniformFallback) {
            canvas.setBlendMode(BlendMode::Set);
            canvas.setColor(active ? Color::Bright : (selected ? Color::Medium : Color::Low));
            canvas.drawRect(x, baseY - 2, barW - 1, 2);
        } else {
            int h = uniformFallback ? barMaxH : (tickets * barMaxH) / denom;
            h = std::max(1, h);
            canvas.setBlendMode(BlendMode::Set);
            if (active) canvas.setColor(Color::Bright);
            else if (selected) canvas.setColor(Color::MediumBright);
            else canvas.setColor(uniformFallback ? Color::Low : Color::Medium);
            canvas.fillRect(x, baseY - h, barW, h);
        }

        if (active) {
            canvas.setColor(Color::Bright);
            canvas.hline(x - 1, baseY + 2, barW + 2);
        } else if (degreeIndex == _selectedDegree) {
            canvas.setColor(Color::MediumBright);
            canvas.hline(x - 1, baseY + 2, barW + 2);
        }
    }

    FixedStringBuilder<32> str;
    str("DEG %d: ", _selectedDegree + 1);
    int t = sequence.degreeTicket(_selectedDegree);
    if (t < 0) str("EXCL");
    else str("%d", t);
    canvas.setFont(Font::Small);
    canvas.setColor(Color::Bright);
    canvas.drawText(8, 18, str);

    if (scaleSize > 16) {
        str.reset();
        str("%d total  [%d-%d]", scaleSize, _bank * 16 + 1, std::min((_bank + 1) * 16, scaleSize));
        canvas.setFont(Font::Tiny);
        canvas.setColor(Color::Low);
        canvas.drawText(8, 28, str);
    }

    str.reset();
    str("DROT:%+d MROT:%+d", sequence.degreeRotation(), sequence.maskRotation());
    int rw = canvas.textWidth(str);

    // Ticket active badge
    canvas.setFont(Font::Tiny);
    canvas.setColor(sequence.pitchTicketsActive() ? Color::Bright : Color::Medium);
    FixedStringBuilder<8> badge;
    badge(sequence.pitchTicketsActive() ? "ON" : "OFF");
    int bw = canvas.textWidth(badge);
    canvas.drawText(Width - rw - bw - 16, 18, badge);

    // Rotation info
    canvas.setColor(Color::Medium);
    canvas.drawText(Width - rw - 12, 18, str);

    const char *footer[] = { "TIX", "DROT", "MROT", nullptr, "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), int(_editFocus));
}

void StochasticSequenceEditPage::drawDurationPage(Canvas &canvas) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &sequence = track.sequence(_project.selectedPatternIndex());

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "DURATION TICKETS");

    const int numSlots = 8;
    const int baseY = 42;
    const int barMaxH = 26;
    const int barW = 12;
    const int gap = barW;

    int totalW = numSlots * (barW + gap) - gap;
    int xOffset = std::max(8, (Width - totalW) / 2);

    int maxWeight = 0, totalWeight = 0;
    for (int i = 0; i < numSlots; ++i) {
        int w = sequence.durationTicket(i);
        if (w > maxWeight) maxWeight = w;
        totalWeight += w;
    }
    bool uniformFallback = (totalWeight == 0);
    int denom = std::max(1, maxWeight);

    if (totalWeight > 0) {
        int avgH = (totalWeight * barMaxH) / (numSlots * denom);
        if (avgH > 0 && avgH < barMaxH) {
            canvas.setBlendMode(BlendMode::Set);
            canvas.setColor(Color::Low);
            canvas.hline(xOffset - 2, baseY - avgH, totalW + 4);
        }
    }

    for (int i = 0; i < numSlots; ++i) {
        int x = xOffset + i * (barW + gap);
        int weight = sequence.durationTicket(i);

        int activeIdx = -1;
        if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
            auto &stoEng = _engine.selectedTrackEngine().as<StochasticTrackEngine>();
            activeIdx = stoEng.lastDurationIndex();
        }
        bool active = (i == activeIdx);
        bool selected = (_durSelectionMask & (1U << i)) != 0;

        if (weight == 0 && !uniformFallback) {
            canvas.setBlendMode(BlendMode::Set);
            canvas.setColor(active ? Color::Bright : (selected ? Color::Medium : Color::Low));
            canvas.drawRect(x, baseY - 2, barW - 1, 2);
        } else {
            int h = uniformFallback ? barMaxH : (weight * barMaxH) / denom;
            h = std::max(1, h);
            canvas.setBlendMode(BlendMode::Set);
            if (active) canvas.setColor(Color::Bright);
            else if (selected) canvas.setColor(Color::MediumBright);
            else canvas.setColor(uniformFallback ? Color::Low : Color::Medium);
            canvas.fillRect(x, baseY - h, barW, h);
        }

        if (active) {
            canvas.setColor(Color::Bright);
            canvas.hline(x - 1, baseY + 2, barW + 2);
        } else if (i == _selectedDurSlot) {
            canvas.setColor(Color::MediumBright);
            canvas.hline(x - 1, baseY + 2, barW + 2);
        }

        canvas.setFont(Font::Tiny);
        canvas.setColor(Color::Low);
        int labelW = canvas.textWidth(durationTicketLabels[i]);
        canvas.drawText(x + (barW - labelW) / 2, baseY + 8, durationTicketLabels[i]);
    }

    // Left info text
    FixedStringBuilder<32> str;
    if (_durFocus == DurFocus::Rest) {
        str("REST: %d%%", sequence.rest());
        canvas.setFont(Font::Small);
        canvas.setColor(Color::Bright);
        canvas.drawText(8, 18, str);
    } else {
        str("%s: %d", durationTicketLabels[_selectedDurSlot], sequence.durationTicket(_selectedDurSlot));
        canvas.setFont(Font::Small);
        canvas.setColor(Color::Bright);
        canvas.drawText(8, 18, str);
    }

    // Duration tickets on/off
    str.reset();
    str(sequence.durationTicketsActive() ? "ON" : "OFF");
    canvas.setFont(Font::Small);
    canvas.setColor(sequence.durationTicketsActive() ? Color::Bright : Color::Medium);
    canvas.drawText(Width - canvas.textWidth(str) - 8, 18, str);

    const char *footer[] = { "DUR", "RST", nullptr, nullptr, "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), _durFocus == DurFocus::DurTicket ? 0 : (_durFocus == DurFocus::Rest ? 1 : -1));
}

//----------
// LEDs
//----------

void StochasticSequenceEditPage::updateLeds(Leds &leds) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &sequence = track.sequence(_project.selectedPatternIndex());

    auto heroLeds = [&] (int paramCount, bool useGreen, bool useRed) {
        for (int i = 0; i < paramCount; ++i) {
            bool held = (i == _heroHeldStep);
            leds.set(MatrixMap::fromStep(i), useRed && held, useGreen);
        }
    };

    switch (_currentPage) {
    case Page::Core:
        // Orange = green + red. Active param brighter.
        heroLeds(2, /*green*/true, /*red*/true);
        break;
    case Page::Marbles:
        heroLeds(3, /*green*/true, /*red*/false);
        break;
    case Page::Direct:
        heroLeds(8, /*green*/true, /*red*/false);
        break;
    case Page::Loop:
        // Top row (0..3): red — destructive / timing params (patience, mutate, jump, sleep)
        for (int i = 0; i < 4; ++i) {
            bool held = (i == _heroHeldStep);
            leds.set(MatrixMap::fromStep(i), /*red*/true, /*green*/held);
        }
        // Bottom row (8..11): green — loop window / shape params (first, last, size, rotate)
        for (int i = 8; i <= 11; ++i) {
            bool held = (i == _heroHeldStep);
            leds.set(MatrixMap::fromStep(i), /*red*/held, /*green*/true);
        }
        break;
    case Page::Pitch: {
        auto &scale = sequence.selectedScale(_project.scale());
        int scaleSize = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);

        int activeDegree = -1;
        if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
            auto &stoEng = _engine.selectedTrackEngine().as<StochasticTrackEngine>();
            activeDegree = stoEng.lastDegree() % scaleSize;
        }

        for (int i = 0; i < 16; ++i) {
            int degreeIndex = _bank * 16 + i;
            bool bit = (degreeIndex < scaleSize);
            bool selected = (_pitchSelectionMask & (1U << degreeIndex)) != 0;
            bool active = (degreeIndex == activeDegree);
            leds.set(MatrixMap::fromStep(i), active || (bit && selected), bit);
        }

        if (globalKeyState()[Key::Page] && !globalKeyState()[Key::Shift]) {
            for (int i = 8; i < 11; ++i) {
                int index = MatrixMap::fromStep(i + 1);
                leds.unmask(index);
                leds.set(index, false, true);
                leds.mask(index);
            }
        }
        break;
    }
    case Page::Duration: {
        int activeIdx = -1;
        if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
            auto &stoEng = _engine.selectedTrackEngine().as<StochasticTrackEngine>();
            activeIdx = stoEng.lastDurationIndex();
        }
        for (int i = 0; i < 8; ++i) {
            bool active = (i == activeIdx);
            bool selected = (_durSelectionMask & (1U << i)) != 0;
            leds.set(MatrixMap::fromStep(i), active || (i == _selectedDurSlot), true);
        }
        break;
    }
    case Page::Count:
        break;
    }
}

//----------
// Key Input
//----------

void StochasticSequenceEditPage::keyDown(KeyEvent &event) {
    const auto &key = event.key();
    bool isHero = (_currentPage == Page::Core || _currentPage == Page::Marbles ||
                   _currentPage == Page::Direct || _currentPage == Page::Loop);
    if (isHero) {
        if (key.isStep() && !key.pageModifier()) {
            _heroHeldStep = key.step();
            event.consume();
            return;
        }
        return;
    }
    switch (_currentPage) {
    case Page::Pitch:    handlePitchKeyDown(event); break;
    case Page::Duration: handleDurationKeyDown(event); break;
    default: break;
    }
}

void StochasticSequenceEditPage::handlePitchKeyDown(KeyEvent &event) {
    const auto &key = event.key();
    if (key.isStep() && !key.pageModifier()) {
        auto &track = _project.selectedTrack().stochasticTrack();
        auto &sequence = track.sequence(_project.selectedPatternIndex());
        auto &scale = sequence.selectedScale(_project.scale());
        int scaleSize = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);
        int degreeIndex = _bank * 16 + key.step();
        if (degreeIndex < scaleSize) {
            _selectedDegree = degreeIndex;
            if (_persistMode) {
                _pitchSelectionMask ^= (1U << degreeIndex);
            } else {
                _pitchSelectionMask |= (1U << degreeIndex);
            }
        }
        event.consume();
        return;
    }

    if (key.isShift()) {
        _persistMode = !_persistMode;
        if (!_persistMode) _pitchSelectionMask = 0;
        event.consume();
        return;
    }

    {
        auto &track = _project.selectedTrack().stochasticTrack();
        auto &sequence = track.sequence(_project.selectedPatternIndex());
        auto &scale = sequence.selectedScale(_project.scale());
        int scaleSize = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);
        if (key.isLeft() && scaleSize > 16) {
            _bank = std::max(0, _bank - 1);
            event.consume();
            return;
        }
        if (key.isRight() && scaleSize > 16) {
            int maxBank = (scaleSize - 1) / 16;
            _bank = std::min(maxBank, _bank + 1);
            event.consume();
            return;
        }
    }

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    BasePage::keyDown(event);
}

void StochasticSequenceEditPage::handleDurationKeyDown(KeyEvent &event) {
    const auto &key = event.key();
    if (key.isStep() && !key.pageModifier()) {
        int step = key.step();
        if (step >= 0 && step < 8) {
            _selectedDurSlot = step;
            if (_persistMode) {
                _durSelectionMask ^= (1U << step);
            } else {
                _durSelectionMask |= (1U << step);
            }
        }
        event.consume();
        return;
    }

    if (key.isShift()) {
        _persistMode = !_persistMode;
        if (!_persistMode) _durSelectionMask = 0;
        event.consume();
        return;
    }

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    BasePage::keyDown(event);
}

void StochasticSequenceEditPage::keyUp(KeyEvent &event) {
    const auto &key = event.key();
    bool isHero = (_currentPage == Page::Core || _currentPage == Page::Marbles ||
                   _currentPage == Page::Direct || _currentPage == Page::Loop);
    if (isHero) {
        if (key.isStep() && _heroHeldStep >= 0 && key.step() == _heroHeldStep) {
            _heroHeldStep = -1;
            event.consume();
            return;
        }
        return;
    }

    if (key.isStep() && !_persistMode) {
        if (_currentPage == Page::Pitch) {
            int degreeIndex = _bank * 16 + key.step();
            _pitchSelectionMask &= ~(1U << degreeIndex);
        } else if (_currentPage == Page::Duration) {
            int step = key.step();
            if (step >= 0 && step < 8) {
                _durSelectionMask &= ~(1U << step);
            }
        }
        event.consume();
        return;
    }

    if (key.isShift()) {
        event.consume();
        return;
    }

    BasePage::keyUp(event);
}

void StochasticSequenceEditPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();
    bool isHero = (_currentPage == Page::Core || _currentPage == Page::Marbles ||
                   _currentPage == Page::Direct || _currentPage == Page::Loop);
    if (isHero) {
        // F5 NEXT first (universal).
        if (key.isFunction() && key.function() == 4) {
            nextPage();
            event.consume();
            return;
        }
        // Per-page Fn dispatch.
        if (key.isFunction()) {
            int fn = key.function();
            bool shift = key.shiftModifier();
            switch (_currentPage) {
            case Page::Core:    handleCoreFunction(fn, shift); break;
            case Page::Marbles: handleMarblesFunction(fn, shift); break;
            case Page::Direct:  handleDirectFunction(fn, shift); break;
            case Page::Loop:    handleLoopFunction(fn, shift); break;
            default: break;
            }
            event.consume();
            return;
        }
        if (key.isContextMenu()) {
            contextShow();
            event.consume();
            return;
        }
        // Let unmatched events (Play, Stop, Tempo, Pattern, Performer, etc.) reach TopPage.
        return;
    }
    switch (_currentPage) {
    case Page::Pitch:    handlePitchKeyPress(event); break;
    case Page::Duration: handleDurationKeyPress(event); break;
    default: break;
    }
}

void StochasticSequenceEditPage::handlePitchKeyPress(KeyPressEvent &event) {
    const auto &key = event.key();
    if (key.isStep() && !key.pageModifier()) {
        event.consume();
        return;
    }

    if (key.isQuickEdit() && !key.shiftModifier()) {
        switch (key.quickEdit()) {
        case 1: contextAction(int(ContextAction::Init)); break;
        case 2: contextAction(int(ContextAction::Even)); break;
        case 3: contextAction(int(ContextAction::Random)); break;
        }
        event.consume();
        return;
    }

    if (key.isFunction()) {
        switch (key.function()) {
        case 0: _editFocus = EditFocus::Ticket; break;
        case 1: _editFocus = EditFocus::DegreeRotation; break;
        case 2: _editFocus = EditFocus::MaskRotation; break;
        case 4: // F5 = NEXT
            nextPage();
            event.consume();
            return;
        }
        event.consume();
        return;
    }

    if (key.isLeft()) {
        auto &track = _project.selectedTrack().stochasticTrack();
        auto &sequence = track.sequence(_project.selectedPatternIndex());
        auto &scale = sequence.selectedScale(_project.scale());
        int scaleSize = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);
        if (scaleSize > 16) {
            _bank = std::max(0, _bank - 1);
        }
        event.consume();
    }
    if (key.isRight()) {
        auto &track = _project.selectedTrack().stochasticTrack();
        auto &sequence = track.sequence(_project.selectedPatternIndex());
        auto &scale = sequence.selectedScale(_project.scale());
        int scaleSize = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);
        if (scaleSize > 16) {
            _bank = std::min(1, _bank + 1);
        }
        event.consume();
    }

    BasePage::keyPress(event);
}

void StochasticSequenceEditPage::handleDurationKeyPress(KeyPressEvent &event) {
    const auto &key = event.key();
    if (key.isStep() && !key.pageModifier()) {
        event.consume();
        return;
    }

    if (key.isQuickEdit() && !key.shiftModifier()) {
        switch (key.quickEdit()) {
        case 1: contextAction(int(ContextAction::Init)); break;
        case 2: contextAction(int(ContextAction::Even)); break;
        case 3: contextAction(int(ContextAction::Random)); break;
        }
        event.consume();
        return;
    }

    if (key.isFunction()) {
        switch (key.function()) {
        case 0: _durFocus = DurFocus::DurTicket; break;
        case 1: _durFocus = DurFocus::Rest; break;
        case 4:
            nextPage();
            event.consume();
            return;
        }
        event.consume();
        return;
    }

    BasePage::keyPress(event);
}

void StochasticSequenceEditPage::encoder(EncoderEvent &event) {
    int step = _heroHeldStep >= 0 ? _heroHeldStep : 0;
    bool shift = event.pressed();
    switch (_currentPage) {
    case Page::Core:    editCoreStep(step, event.value(), shift); event.consume(); return;
    case Page::Marbles: editMarblesStep(step, event.value(), shift); event.consume(); return;
    case Page::Direct:  editDirectStep(step, event.value(), shift); event.consume(); return;
    case Page::Loop:    editLoopStep(step, event.value(), shift); event.consume(); return;
    case Page::Pitch:    handlePitchEncoder(event); break;
    case Page::Duration: handleDurationEncoder(event); break;
    default: break;
    }
}

void StochasticSequenceEditPage::handlePitchEncoder(EncoderEvent &event) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &sequence = track.sequence(_project.selectedPatternIndex());
    auto &scale = sequence.selectedScale(_project.scale());
    int scaleSize = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);

    switch (_editFocus) {
    case EditFocus::Ticket: {
        uint32_t mask = _pitchSelectionMask;
        if (_pitchSelectionMask == 0) mask = (1U << _selectedDegree);
        for (int i = 0; i < scaleSize; ++i) {
            if (mask & (1U << i)) {
                sequence.setDegreeTicket(i, sequence.degreeTicket(i) + event.value());
            }
        }
        break;
    }
    case EditFocus::DegreeRotation:
        sequence.setDegreeRotation(sequence.degreeRotation() + event.value());
        break;
    case EditFocus::MaskRotation:
        sequence.setMaskRotation(sequence.maskRotation() + event.value());
        break;
    default:
        break;
    }

    event.consume();
}

void StochasticSequenceEditPage::handleDurationEncoder(EncoderEvent &event) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &sequence = track.sequence(_project.selectedPatternIndex());

    switch (_durFocus) {
    case DurFocus::DurTicket: {
        uint32_t mask = _durSelectionMask;
        if (_durSelectionMask == 0) mask = (1U << _selectedDurSlot);
        for (int i = 0; i < 8; ++i) {
            if (mask & (1U << i)) {
                sequence.setDurationTicket(i, sequence.durationTicket(i) + event.value());
            }
        }
        break;
    }
    case DurFocus::Rest:
        sequence.setRest(sequence.rest() + event.value());
        break;
    default:
        break;
    }

    event.consume();
}

void StochasticSequenceEditPage::contextShow(bool doubleClick) {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return true; },
        doubleClick
    ));
}

void StochasticSequenceEditPage::contextAction(int index) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &sequence = track.sequence(_project.selectedPatternIndex());

    switch (_currentPage) {
    case Page::Core:
    case Page::Marbles:
    case Page::Direct:
    case Page::Loop:
        // Hero pages don't yet have INIT/EVEN/RAND semantics — no-op for now.
        break;
    case Page::Pitch: {
        switch (ContextAction(index)) {
        case ContextAction::Init:
            for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) {
                sequence.setDegreeTicket(i, 100);
            }
            sequence.setDegreeRotation(0);
            sequence.setMaskRotation(0);
            showMessage("INIT TICKETS");
            break;
        case ContextAction::Even: {
            int val = sequence.degreeTicket(_selectedDegree);
            if (val < 0) val = 50;
            for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) {
                sequence.setDegreeTicket(i, val);
            }
            showMessage("EVEN TICKETS");
            break;
        }
        case ContextAction::Random: {
            static Random rng;
            for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) {
                sequence.setDegreeTicket(i, rng.nextRange(101));
            }
            showMessage("RANDOM TICKETS");
            break;
        }
        case ContextAction::Last:
            break;
        }
        break;
    }
    case Page::Duration: {
        switch (ContextAction(index)) {
        case ContextAction::Init:
            for (int i = 0; i < 8; ++i) {
                sequence.setDurationTicket(i, 50);
            }
            sequence.setRest(0);
            showMessage("INIT DUR");
            break;
        case ContextAction::Even:
            for (int i = 0; i < 8; ++i) {
                sequence.setDurationTicket(i, 50);
            }
            sequence.setRest(0);
            showMessage("EVEN DUR");
            break;
        case ContextAction::Random: {
            static Random rng;
            for (int i = 0; i < 8; ++i) {
                sequence.setDurationTicket(i, rng.nextRange(101));
            }
            showMessage("RAND DUR");
            break;
        }
        case ContextAction::Last:
            break;
        }
        break;
    }
    case Page::Count:
        break;
    }
}
