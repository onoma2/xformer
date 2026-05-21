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
    int next = (int(_currentPage) + 1) % int(Page::Count);
    // Phase 12: marblesMode toggle is dead (always-on transparent defaults).
    // Marbles page is always visible in the cycle.
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

    int rest = seq.rest();
    int burst = seq.burst();
    int complexity = seq.complexity();

    // dropCount = mood proxy: rest sparsens, burst + complexity busy it up.
    // Range ~12..100. Density slot is engine-reserved and not visualized.
    int dropCount = 12 + ((100 - rest) * 40) / 100 + (burst * 30) / 100 + (complexity * 18) / 100;
    int jitter = 2 + (complexity * 14) / 100;           // 2..16
    // Phase 12: marblesMode toggle is dead — pitch shape always applies.
    // Wind is purely cosmetic, driven by complexity instead.
    int wind = 1 + complexity / 50;

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
    if (_heroHeldStep == 0) str("REST %d", rest);
    else if (_heroHeldStep == 1) str("CMPX %d", complexity);
    else str("R%d  C%d", rest, complexity);
    canvas.setFont(Font::Small);
    canvas.setColor(Color::Bright);
    canvas.drawText(8, 18, str);

    // Phase 12: removed the SHAPE ON/OFF on-screen affordance — engine no
    // longer honors `marblesMode`, so the label was a lie.

    // Dynamic footer labels — show actual current state per DiscreteMap convention.
    auto &track = _project.selectedTrack().stochasticTrack();
    const char *modeLabel = "LIVE";
    if (seq.rhythmMode() == seq.melodyMode()) {
        modeLabel = (seq.rhythmMode() == StochasticSourceMode::Loop) ? "LOOP" : "LIVE";
    } else {
        modeLabel = "SPLIT";
    }
    const char *lockLabel = track.lock() ? "LOCKED" : "FREE";
    // F1 slot used to toggle marblesMode; that's dead now, so the slot is empty.
    const char *footer[] = { nullptr, modeLabel, "RENEW", lockLabel, "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), -1);
}

void StochasticSequenceEditPage::drawMarblesPage(Canvas &canvas) {
    auto &seq = _project.selectedTrack().stochasticTrack().sequence(_project.selectedPatternIndex());

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "MARBLES");

    int bias = seq.marblesBias();       // 0..100, 50=center
    int spreadVal = seq.marblesSpread(); // 0..100
    int stepsVal = seq.marblesSteps();   // 0..100 (sieve cutoff)
    // Phase 12 musicality fix: Steps is a coarse rank cutoff (K = ceil(N×%/100)).
    // Show K/N alongside the raw % so the user sees the actual sieve count and
    // understands the staircase, not a smooth continuous knob.
    auto &scaleForSteps = seq.selectedScale(_project.scale());
    int scaleSizeForSteps = clamp(scaleForSteps.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);
    int stepsK = (scaleSizeForSteps * stepsVal + 99) / 100;
    if (stepsK < 1) stepsK = 1;
    if (stepsK > scaleSizeForSteps) stepsK = scaleSizeForSteps;

    int biasOffset = ((bias - 50) * 4) / 5;            // -40..+40
    int cx = Width / 2 + biasOffset;
    int spread = std::max(8, (spreadVal * 4) / 5);
    int stepsCount = std::max(2, stepsVal);
    int baseline = 50;
    int peakY = 14;

    canvas.setBlendMode(BlendMode::Set);
    // Phase 12: pitch shape is always live — no "off" branch. Always draw bell.
    {
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
    else if (_heroHeldStep == 2) str("STEP %d  (%d/%d)", stepsVal, stepsK, scaleSizeForSteps);
    else str("B%d  S%d  N%d/%d", bias, spreadVal, stepsK, scaleSizeForSteps);
    canvas.setFont(Font::Small);
    canvas.setColor(Color::Bright);
    canvas.drawText(8, 18, str);

    // Phase 12: F1 SHAPE toggle removed (was a lie — engine always honors
    // Bias/Spread/Steps regardless of marblesMode).
    const char *footer[] = { nullptr, nullptr, nullptr, nullptr, "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), -1);
}

void StochasticSequenceEditPage::drawDirectPage(Canvas &canvas) {
    auto &seq = _project.selectedTrack().stochasticTrack().sequence(_project.selectedPatternIndex());

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "DIRECT");

    canvas.setBlendMode(BlendMode::Set);

    // Knob snapshot (4-letter labels in held/macro readout).
    int duration   = seq.noteDuration();
    int variation  = seq.variation();
    int rest       = seq.rest();
    int complexity = seq.complexity();
    int contour    = seq.contour();
    int bias       = seq.marblesBias();
    int spread     = seq.marblesSpread();
    int repeats    = seq.repeatProb();

    // Viewport — leaves room for the two info rows at top and footer at bottom.
    const int vpLeft = 8;
    const int vpRight = Width - 8;
    const int vpTop = 25;
    const int vpBot = 53;
    const int vpH = vpBot - vpTop;
    const int vpCy = (vpTop + vpBot) / 2;

    // --- Marbles bell haze (BIAS + SPRE) -----------------------------------
    int biasY = vpTop + ((100 - bias) * vpH) / 100;
    int spreadY = (spread * vpH) / 200;
    if (spreadY < 2) spreadY = 2;
    for (int y = vpTop; y <= vpBot; ++y) {
        int dy = y - biasY;
        // Gaussian-ish envelope: env ≈ exp(-(dy/spreadY)² × 1.4). Approximated
        // with fixed-point math: scale = max(0, 256 - (dy*dy*180)/(spreadY*spreadY))
        int dist2 = (dy * dy * 180) / (spreadY * spreadY);
        int env = 256 - dist2;
        if (env < 64) continue;       // skip dim tails
        Color col = (env >= 218) ? Color::MediumLow : Color::Low;
        canvas.setColor(col);
        int step = (env >= 154) ? 2 : 3;
        int offset = (y * 7) % step;
        for (int x = vpLeft + offset; x < vpRight; x += step) canvas.point(x, y);
    }

    // --- Event-driven walker ----------------------------------------------
    // The trail is an event-time ring buffer. On every gate rising edge the
    // engine emits a new event; we shift the ring left and write a fresh
    // particle at slot 0 (the right edge of the viewport) with vertical
    // position derived from the live CV (= pitch). Horizontal motion = rhythm
    // (events arriving), vertical = pitch.
    _directWalkerTick++;

    float liveCv = 0.f;
    bool liveGate = false;
    if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
        auto &eng = _engine.selectedTrackEngine().as<StochasticTrackEngine>();
        liveCv = eng.cvOutput(0);
        liveGate = eng.gateOutput(0);
    }

    // Map liveCv (≈ -5..+5 V typical) to a vertical offset around biasY.
    // The bias-anchored mapping keeps the trail centered when CV is near 0.
    int cvOffset = -int(liveCv * 4.f);   // 1V ≈ 4 px
    int cvOffsetMax = vpH / 2 - 2;
    if (cvOffset > cvOffsetMax) cvOffset = cvOffsetMax;
    else if (cvOffset < -cvOffsetMax) cvOffset = -cvOffsetMax;

    // Rising edge → shift ring + insert.
    if (liveGate && !_directLastGate) {
        for (int i = kDirectTrailMax - 1; i > 0; --i) {
            _directTrail[i] = _directTrail[i - 1];
        }
        bool isRest = (rest > 0) && (int((_directWalkerTick * 1103515245u) % 100u) < rest);
        bool isBurst = !isRest && (seq.burst() > 0)
            && (int((_directWalkerTick * 22695477u) % 100u) < seq.burst());
        uint8_t flags = (isRest ? 1 : 0) | (isBurst ? 2 : 0);
        uint8_t children = isBurst ? uint8_t(2 + std::min(3, (seq.burstCount() * 4) / 100)) : 0;
        _directTrail[0] = { int16_t(cvOffset), flags, children };
        if (_directTrailFilled < kDirectTrailMax) _directTrailFilled++;
        _directLastEventEpoch = _directWalkerTick;
    }
    _directLastGate = liveGate;

    // Horizontal stride per slot, modulated by DUR so the trail visibly tightens
    // at short durations and spreads at long ones.
    int baseStride = 6 + (7 - duration) * 5;
    if (baseStride < 4) baseStride = 4;

    // Center the (walker ↔ oldest particle) midpoint on the viewport center.
    // Walker = slot 0, oldest = slot (filled-1). With stride S and N filled
    // slots: span = (N-1)*S; anchor walker at vpCx + span/2 so midpoint = vpCx.
    int vpCx = (vpLeft + vpRight) / 2;
    int filled = _directTrailFilled > 0 ? _directTrailFilled : 1;
    int span = (filled - 1) * baseStride;
    int walkerX = vpCx + span / 2;
    if (walkerX > vpRight - 2) walkerX = vpRight - 2;

    // Particle slope encodes CONT direction (positive = up-right tilt).
    int slopeDy = -(contour * 3) / 200;             // -1..+1 typically
    int burstSpacing = 2 + (seq.burstRate() * 6) / 100;
    bool burstPitchScatter = (seq.burstPitch() == StochasticBurstPitch::Generate);

    // Draw oldest → newest so newer events overdraw older fade.
    for (int i = _directTrailFilled - 1; i >= 0; --i) {
        const DirectParticle &p = _directTrail[i];
        int px = walkerX - i * baseStride;
        if (px < vpLeft) break;
        int py = biasY + p.yOffset;
        if (py < vpTop + 2) py = vpTop + 2;
        if (py > vpBot - 2) py = vpBot - 2;

        bool isRest = (p.flags & 1) != 0;
        bool isBurst = (p.flags & 2) != 0;

        if (isRest) {
            canvas.setColor(Color::Medium);
            canvas.hline(px - 1, vpCy, 3);
            continue;
        }

        // Brighter trail: clamp floor up so even the oldest slot reads as a
        // visible dot rather than disappearing into the dim haze.
        int fade = 100 - i * 8;
        if (fade < 30) fade = 30;
        int trailStrength = (fade + repeats) / 2;
        Color col = (trailStrength >= 70) ? Color::Bright
                   : (trailStrength >= 50) ? Color::MediumBright
                   : (trailStrength >= 30) ? Color::Medium
                                           : Color::MediumLow;
        canvas.setColor(col);
        if (i == 0) {
            canvas.fillRect(px - 1, py - 1, 3, 3);
        } else {
            canvas.line(px - 1, py + slopeDy, px + 1, py - slopeDy);
            canvas.point(px, py);
        }
        // Burst children — small dots offset rightward from the parent slot.
        if (isBurst && p.children > 0) {
            for (int c = 0; c < p.children; ++c) {
                int cxp = px + (c + 1) * burstSpacing;
                if (cxp >= vpRight - 1) break;
                int cy = py;
                if (burstPitchScatter) {
                    cy += int((_directWalkerTick + i * 17 + c * 5) % 5) - 2;
                }
                if (cy < vpTop + 1) cy = vpTop + 1;
                if (cy > vpBot - 1) cy = vpBot - 1;
                canvas.point(cxp, cy);
            }
        }
    }

    // "Now" cross marker — sits at the walker head (centered span).
    int nowX = walkerX;
    int nowY = biasY + cvOffset;
    if (nowY < vpTop + 2) nowY = vpTop + 2;
    if (nowY > vpBot - 2) nowY = vpBot - 2;
    canvas.setColor(Color::Bright);
    canvas.fillRect(nowX - 2, nowY - 2, 5, 5);
    canvas.setColor(Color::MediumBright);
    canvas.point(nowX + 3, nowY);
    canvas.point(nowX - 3, nowY);
    canvas.point(nowX, nowY + 3);
    canvas.point(nowX, nowY - 3);

    // --- Info rows ---------------------------------------------------------
    canvas.setFont(Font::Small);
    canvas.setColor(Color::Bright);
    FixedStringBuilder<32> str;
    bool labeled = true;
    switch (_heroHeldStep) {
    case 0:  str("DURA %d", duration); break;
    case 1:  str("VARI %d", variation); break;
    case 2:  str("REST %d", rest); break;
    case 3:  str("RANG %d oct", seq.range()); break;
    case 4:  str("BURS %d", seq.burst()); break;
    case 5:  str("BCNT %d", seq.burstCount()); break;
    case 6:  str("BRAT %d", seq.burstRate()); break;
    case 7:  str("BPIT %s", seq.burstPitch() == StochasticBurstPitch::Parent ? "PAR" : "GEN"); break;
    case 8:  str("CMPX %d", complexity); break;
    case 9:  str("CONT %+d", contour); break;
    case 10: str("BIAS %d", bias); break;
    case 11: str("SPRE %d", spread); break;
    case 12: str("REPT %d", repeats); break;
    case 13: str("GATE %d", seq.gateLength()); break;
    case 14: str("SLID %d", seq.slide()); break;
    case 15: str("LEGA %d", seq.legatoProb()); break;
    default: labeled = false; break;
    }
    if (labeled) {
        canvas.drawText(8, 19, str);
    } else {
        canvas.setFont(Font::Tiny);
        canvas.setColor(Color::Medium);
        FixedStringBuilder<16> s;
        // Top row: rhythm + burst sub-set
        s.reset(); s("DUR%d", duration);          canvas.drawText(8,   18, s);
        s.reset(); s("VAR%d", variation);         canvas.drawText(40,  18, s);
        s.reset(); s("RES%d", rest);              canvas.drawText(72,  18, s);
        s.reset(); s("BUR%d", seq.burst());       canvas.drawText(104, 18, s);
        s.reset(); s("BCN%d", seq.burstCount());  canvas.drawText(136, 18, s);
        s.reset(); s("BRT%d", seq.burstRate());   canvas.drawText(168, 18, s);
        s.reset(); s("BPI%s", seq.burstPitch() == StochasticBurstPitch::Parent ? "P" : "G");
                                                  canvas.drawText(200, 18, s);
        // Bottom row: pitch shaping
        s.reset(); s("CMP%d",  complexity);   canvas.drawText(8,   24, s);
        s.reset(); s("CON%+d", contour);      canvas.drawText(56,  24, s);
        s.reset(); s("BIA%d",  bias);         canvas.drawText(104, 24, s);
        s.reset(); s("SPR%d",  spread);       canvas.drawText(152, 24, s);
        s.reset(); s("REP%d",  repeats);      canvas.drawText(200, 24, s);
    }

    // Footer mirrors LOOP page.
    const char *fnR = (seq.rhythmMode() == StochasticSourceMode::Loop) ? "LoopR" : "LiveR";
    const char *fnM = (seq.melodyMode() == StochasticSourceMode::Loop) ? "LoopM" : "LiveM";
    const char *footer[] = { fnR, fnM, "NewR", "NewM", "NEXT" };
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
        // currentStep() returns _patternIndex, which is the NEXT step to fire
        // (engine increments inside triggerStep after reading the index).
        // Step back one with wrap so the highlight tracks what's audible right
        // now, not what's about to play.
        int next = eng.currentStep();
        int sz = std::max(1, seq.size());
        currentStep = ((next - 1) % sz + sz) % sz;
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

    // The engine writes back live-generated events to sequence.events()[] each
    // tick (Live mode), so the stored buffer is always the most recently-played
    // content. Tape reads stored data uniformly — Loop mode is a snapshot, Live
    // mode updates progressively as each step plays.
    auto durIndexForSlot = [&] (const StochasticSourceEvent &ev) {
        return int(ev.durationIndex());
    };

    // Divisor-aware-and-adjusted width:
    //   rawFill   = actual loop ticks / max possible (size × largest LUT × divisor)
    //   adjFill   = max(0.4, rawFill)  — short loops still get usable tape
    //   totalDraw = adjFill × availW  — how many pixels the loop occupies
    // Each event's slot is then a proportional slice of totalDraw, so per-event
    // variation stays visible even at dense settings, and longer-duration loops
    // visibly fill more of the tape (up to 100%).
    auto ticksForSlot = [&] (int durIdx) {
        auto f = StochasticTrackEngine::getDurationFraction(durIdx);
        return (int(seq.divisor()) * f.num) / f.den;
    };

    int referenceTotal = int(seq.divisor()) * 8 * seqSize;
    if (referenceTotal <= 0) referenceTotal = 1;

    int totalEventTicks = 0;
    int pitchMin = INT32_MAX;
    int pitchMax = INT32_MIN;
    for (int i = 0; i < seqSize; ++i) {
        int eIdx = resolveEventIdx(i);
        const auto &ev = seq.events()[eIdx];
        totalEventTicks += ticksForSlot(durIndexForSlot(ev));
        if (!ev.rest()) {
            int pitch = int(ev.degree()) + int(ev.octave()) * 12;
            if (pitch < pitchMin) pitchMin = pitch;
            if (pitch > pitchMax) pitchMax = pitch;
        }
    }
    if (totalEventTicks <= 0) totalEventTicks = 1;
    int pitchRange = std::max(1, pitchMax - pitchMin);
    if (pitchMin > pitchMax) { pitchMin = 0; pitchMax = 0; }

    // rawFill ∈ [0..1], adjusted to keep at least 40% tape for very short loops.
    int rawFillNum = totalEventTicks;
    int rawFillDen = referenceTotal;
    if (rawFillNum > rawFillDen) rawFillNum = rawFillDen;   // cap at 100%
    int totalDrawW = (rawFillNum * availW) / rawFillDen;
    int minDrawW = (availW * 40) / 100;
    if (totalDrawW < minDrawW) totalDrawW = minDrawW;

    // Empty-tape outline so unused span is visible.
    canvas.setColor(Color::Low);
    canvas.drawRect(margin, tapeTop, availW, tapeRowH);

    // Center the loop horizontally inside the tape — small loops sit in the
    // middle, large loops fill it. Brackets/slots all start from this offset.
    int loopOriginX = margin + (availW - totalDrawW) / 2;
    int ticksAccum = 0;
    int currentX = loopOriginX;
    int bxFirst = loopOriginX;
    int bxLast = loopOriginX + totalDrawW;

    for (int i = 0; i < seqSize; ++i) {
        int eIdx = resolveEventIdx(i);
        const auto &ev = seq.events()[eIdx];
        int durIdx = durIndexForSlot(ev);
        int eventTicks = ticksForSlot(durIdx);
        ticksAccum += eventTicks;
        int xEnd = loopOriginX + (ticksAccum * totalDrawW) / totalEventTicks;
        int stepW = xEnd - currentX;
        if (stepW < 1) stepW = 1;
        if (currentX + stepW > loopOriginX + totalDrawW) {
            stepW = std::max(1, loopOriginX + totalDrawW - currentX);
        }

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
            // In Live-melody mode the stored pitches are stale, so use a neutral
            // midline height for all slots instead.
            if (!isRest && stepW > 2) {
                int innerW = stepW - 2;
                int innerH = tapeRowH - 2;
                int barH;
                if (pitchRange > 0) {
                    int pitch = int(ev.degree()) + int(ev.octave()) * 12;
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

    // Boredom counter bar — Phase 12 split. Left half = rhythm progress
    // (fills from the centre outward leftward). Right half = melody progress
    // (fills from the centre outward rightward). 2px gap between halves.
    int fillR = 0, fillM = 0;
    if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
        auto &eng = _engine.selectedTrackEngine().as<StochasticTrackEngine>();
        // Linear meter scaled to expected loops (λ) — bar reaches 100% at the
        // typical regen point, then sits at 100% until the Poisson roll fires.
        // Replaces CDF math which mathematically rises but rarely hits high
        // values before a regen resets the counter.
        float pR = StochasticTrackEngine::patienceMeter(eng.loopCycleCount(),       seq.patienceRhythm());
        float pM = StochasticTrackEngine::patienceMeter(eng.loopCycleCountMelody(), seq.patienceMelody());
        fillR = clamp(int(pR * 100.0f), 0, 100);
        fillM = clamp(int(pM * 100.0f), 0, 100);
    }
    const int gap = 2;
    const int halfW = (availW - gap) / 2;
    const int leftX  = margin;
    const int rightX = margin + halfW + gap;
    canvas.setColor(Color::Low);
    canvas.drawRect(leftX,  tapeBot + 7, halfW, 3);
    canvas.drawRect(rightX, tapeBot + 7, halfW, 3);
    canvas.setColor(Color::MediumBright);
    // Both halves fill rightward from their respective left edge.
    canvas.fillRect(leftX,  tapeBot + 7, (fillR * halfW) / 100, 3);
    canvas.fillRect(rightX, tapeBot + 7, (fillM * halfW) / 100, 3);

    // Truncated bottom-row state — Mask + Tilt under the bars, tiny font.
    canvas.setFont(Font::Tiny);
    canvas.setColor(Color::Medium);
    FixedStringBuilder<16> mt;
    mt("MK%d", seq.mask());
    canvas.drawText(margin, tapeBot + 14, mt);
    mt.reset();
    mt("TL%+d", seq.tilt());
    canvas.drawText(margin + availW - canvas.textWidth(mt), tapeBot + 14, mt);

    // Top-area readout. When a step is held, show that step's value in Small
    // font. Otherwise show all four destructive macros at once in Tiny font.
    if (_heroHeldStep >= 0) {
        FixedStringBuilder<32> str;
        if (_heroHeldStep == 0)       str("PR %d", seq.patienceRhythm());
        else if (_heroHeldStep == 1)  str("PM %d", seq.patienceMelody());
        else if (_heroHeldStep == 2)  str("M %+d", seq.mutate());
        else if (_heroHeldStep == 3)  str("J %d", seq.jump());
        else if (_heroHeldStep == 4)  str("S %d", seq.sleep());
        else if (_heroHeldStep == 8)  str("FRST %d", seq.first());
        else if (_heroHeldStep == 9)  str("LAST %d", seq.last());
        else if (_heroHeldStep == 10) str("SIZE %d", seq.size());
        else if (_heroHeldStep == 11) str("ROT %+d", seq.rotate());
        else if (_heroHeldStep == 14) str("MASK %d", seq.mask());
        else if (_heroHeldStep == 15) str("TILT %+d", seq.tilt());
        canvas.setFont(Font::Small);
        canvas.setColor(Color::Bright);
        canvas.drawText(8, 18, str);
    } else {
        canvas.setFont(Font::Tiny);
        canvas.setColor(Color::Medium);
        FixedStringBuilder<16> s;
        s("PR%d",  seq.patienceRhythm()); canvas.drawText(4,   16, s);
        s.reset(); s("PM%d",  seq.patienceMelody()); canvas.drawText(34,  16, s);
        s.reset(); s("M%+d", seq.mutate());          canvas.drawText(66,  16, s);
        s.reset(); s("J%d",  seq.jump());            canvas.drawText(94,  16, s);
        s.reset(); s("S%d",  seq.sleep());           canvas.drawText(120, 16, s);
    }

    const char *fnR = (seq.rhythmMode() == StochasticSourceMode::Loop) ? "LoopR" : "LiveR";
    const char *fnM = (seq.melodyMode() == StochasticSourceMode::Loop) ? "LoopM" : "LiveM";
    const char *footer[] = { fnR, fnM, "NewR", "NewM", "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), -1);
}

//----------
// Hero step edits — encoder writes the held step's param
//----------

void StochasticSequenceEditPage::editCoreStep(int step, int value, bool shift) {
    auto &seq = _project.selectedTrack().stochasticTrack().sequence(_project.selectedPatternIndex());
    int v = shift ? value * 10 : value;
    switch (step) {
    case 0: seq.setRest(seq.rest() + v); break;     // S0 now drives the live rest gate
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
    // DIRECT layout:
    //   Top row    0=DURA  1=VARI  2=REST  3=RANG       (red)
    //              4=BURS  5=BCNT  6=BRAT  7=BPIT       (orange — burst sub-set)
    //   Bottom row 8=CMPX  9=CONT  10=BIAS 11=SPRE 12=REPT  (green — pitch shape)
    //              13=GATE 14=SLID 15=LEGA              (red — per-event gate behavior)
    switch (step) {
    case 0:  seq.setNoteDuration(seq.noteDuration() + value); break;
    case 1:  seq.setVariation(seq.variation() + v); break;
    case 2:  seq.setRest(seq.rest() + v); break;
    case 3:  seq.setRange(seq.range() + value); break;
    case 4:  seq.setBurst(seq.burst() + v); break;
    case 5:  seq.setBurstCount(seq.burstCount() + v); break;
    case 6:  seq.setBurstRate(seq.burstRate() + v); break;
    case 7:  seq.setBurstPitch(StochasticBurstPitch(
                ((int(seq.burstPitch()) + (value > 0 ? 1 : value < 0 ? -1 : 0)) % 2 + 2) % 2)); break;
    case 8:  seq.setComplexity(seq.complexity() + v); break;
    case 9:  seq.setContour(seq.contour() + v); break;
    case 10: seq.setMarblesBias(seq.marblesBias() + v); break;
    case 11: seq.setMarblesSpread(seq.marblesSpread() + v); break;
    case 12: seq.setRepeatProb(seq.repeatProb() + v); break;
    case 13: seq.setGateLength(seq.gateLength() + v); break;
    case 14: seq.setSlide(seq.slide() + v); break;
    case 15: seq.setLegatoProb(seq.legatoProb() + v); break;
    }
}

void StochasticSequenceEditPage::editLoopStep(int step, int value, bool shift) {
    auto &seq = _project.selectedTrack().stochasticTrack().sequence(_project.selectedPatternIndex());
    int v = shift ? value * 10 : value;
    switch (step) {
    // Top row — patience (split per domain), mutation, walk, sleep
    case 0: seq.setPatienceRhythm(seq.patienceRhythm() + v); break;
    case 1: seq.setPatienceMelody(seq.patienceMelody() + v); break;
    case 2: seq.setMutate(seq.mutate() + v); break;
    case 3: seq.setJump(seq.jump() + v); break;
    case 4: seq.setSleep(seq.sleep() + v); break;
    // Bottom row — loop window / shape params (green LEDs)
    case 8:  seq.setFirst(seq.first() + value); break;
    case 9:  seq.setLast(seq.last() + value); break;
    case 10: seq.setSize(seq.size() + value); break;
    case 11: seq.setRotate(seq.rotate() + value); break;
    // Last two — Mask + Tilt performance pair (filter + duration-tilt resonance)
    case 14: seq.setMask(seq.mask() + v); break;
    case 15: seq.setTilt(seq.tilt() + v); break;
    }
}

//----------
// Hero Fn handlers
//----------

bool StochasticSequenceEditPage::handleCoreFunction(int fn, bool shift) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &seq = track.sequence(_project.selectedPatternIndex());
    switch (fn) {
    case 0: // F1 unused — was SHAPE toggle, dead since Phase 12 (engine always
            // honors Bias/Spread/Steps regardless of marblesMode).
        return false;
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

bool StochasticSequenceEditPage::handleDirectFunction(int fn, bool shift, int pressCount) {
    // DIRECT page Fn keys mirror the LOOP page: toggle source modes per domain
    // and renew loop content (double-click guarded for the destructive renews).
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &seq = track.sequence(_project.selectedPatternIndex());
    auto stoEng = [&] () -> StochasticTrackEngine* {
        if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
            return &_engine.selectedTrackEngine().as<StochasticTrackEngine>();
        }
        return nullptr;
    };
    switch (fn) {
    case 0: {
        auto newMode = (seq.rhythmMode() == StochasticSourceMode::Loop)
            ? StochasticSourceMode::Live : StochasticSourceMode::Loop;
        seq.setRhythmMode(newMode);
        return true;
    }
    case 1: {
        auto newMode = (seq.melodyMode() == StochasticSourceMode::Loop)
            ? StochasticSourceMode::Live : StochasticSourceMode::Loop;
        seq.setMelodyMode(newMode);
        return true;
    }
    case 2:
        if (pressCount == 2) {
            if (auto *eng = stoEng()) { eng->renewRhythm(); showMessage("NEW R"); }
        } else {
            showMessage("Press again - renew R");
        }
        return true;
    case 3:
        if (pressCount == 2) {
            if (auto *eng = stoEng()) { eng->renewMelody(); showMessage("NEW M"); }
        } else {
            showMessage("Press again - renew M");
        }
        return true;
    }
    return false;
}

bool StochasticSequenceEditPage::handleLoopFunction(int fn, bool shift, int pressCount) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &seq = track.sequence(_project.selectedPatternIndex());
    auto stoEng = [&] () -> StochasticTrackEngine* {
        if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
            return &_engine.selectedTrackEngine().as<StochasticTrackEngine>();
        }
        return nullptr;
    };
    switch (fn) {
    case 0: { // LoopR / LiveR — toggle rhythm source mode
        auto newMode = (seq.rhythmMode() == StochasticSourceMode::Loop)
            ? StochasticSourceMode::Live : StochasticSourceMode::Loop;
        seq.setRhythmMode(newMode);
        return true;
    }
    case 1: { // LoopM / LiveM — toggle melody source mode
        auto newMode = (seq.melodyMode() == StochasticSourceMode::Loop)
            ? StochasticSourceMode::Live : StochasticSourceMode::Loop;
        seq.setMelodyMode(newMode);
        return true;
    }
    case 2: // NewR — double-click guard. First press warns, second fires.
        if (pressCount == 2) {
            if (auto *eng = stoEng()) { eng->renewRhythm(); showMessage("NEW R"); }
        } else {
            showMessage("Press again - renew R");
        }
        return true;
    case 3: // NewM — double-click guard.
        if (pressCount == 2) {
            if (auto *eng = stoEng()) { eng->renewMelody(); showMessage("NEW M"); }
        } else {
            showMessage("Press again - renew M");
        }
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
        FixedStringBuilder<8> slotLabel;
        sequence.printSlotDuration(slotLabel, i);
        int labelW = canvas.textWidth(slotLabel);
        canvas.drawText(x + (barW - labelW) / 2, baseY + 8, slotLabel);
    }

    // Left info text
    FixedStringBuilder<32> str;
    if (_durFocus == DurFocus::Rest) {
        str("REST: %d%%", sequence.rest());
        canvas.setFont(Font::Small);
        canvas.setColor(Color::Bright);
        canvas.drawText(8, 18, str);
    } else {
        FixedStringBuilder<8> slotLabel;
        sequence.printSlotDuration(slotLabel, _selectedDurSlot);
        str("%s: %d", (const char *)slotLabel, sequence.durationTicket(_selectedDurSlot));
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
    case Page::Direct: {
        // Top row 0..3 red — DURA/VARI/REST/RANG.
        for (int i = 0; i <= 3; ++i) {
            bool held = (i == _heroHeldStep);
            leds.set(MatrixMap::fromStep(i), /*red*/true, /*green*/held);
        }
        // Top row 4..7 orange (red+green) — burst sub-set (BURS/BCNT/BRAT/BPIT).
        for (int i = 4; i <= 7; ++i) {
            bool held = (i == _heroHeldStep);
            leds.set(MatrixMap::fromStep(i), /*red*/!held, /*green*/!held ? true : true);
            if (held) {
                leds.set(MatrixMap::fromStep(i), /*red*/true, /*green*/false);
            }
        }
        // Bottom row 8..12 green — pitch shape (CMPX/CONT/BIAS/SPRE/REPT).
        for (int i = 8; i <= 12; ++i) {
            bool held = (i == _heroHeldStep);
            leds.set(MatrixMap::fromStep(i), /*red*/held, /*green*/true);
        }
        // Bottom row 13..15 red — per-event gate behavior (GATE/SLID/LEGA).
        for (int i = 13; i <= 15; ++i) {
            bool held = (i == _heroHeldStep);
            leds.set(MatrixMap::fromStep(i), /*red*/true, /*green*/held);
        }
        break;
    }
    case Page::Loop:
        // Top row (0..4): red — patience R, patience M, mutate, jump, sleep
        for (int i = 0; i <= 4; ++i) {
            bool held = (i == _heroHeldStep);
            leds.set(MatrixMap::fromStep(i), /*red*/true, /*green*/held);
        }
        // Bottom row left (8..11): green — loop window (first, last, size, rotate)
        for (int i = 8; i <= 11; ++i) {
            bool held = (i == _heroHeldStep);
            leds.set(MatrixMap::fromStep(i), /*red*/held, /*green*/true);
        }
        // Bottom row right (14..15): green — Mask + Tilt performance pair
        for (int i = 14; i <= 15; ++i) {
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
            case Page::Direct:  handleDirectFunction(fn, shift, event.count()); break;
            case Page::Loop:    handleLoopFunction(fn, shift, event.count()); break;
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
