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
    { "BPITCH" },   // context-menu label: toggle BurstHold Hold/Roll (slot 7 owned by Feel)
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
    _heroSelectionMask = 0;
}

// Lowest-bit-set helper: returns step index 0..15 of the only/first held step,
// or -1 if no step is held. Used by single-step overlay paths.
static int heroFirstHeld(uint32_t mask) {
    if (mask == 0) return -1;
    for (int i = 0; i < 16; ++i) if (mask & (1U << i)) return i;
    return -1;
}

static int heroPopcount(uint32_t mask) {
    int n = 0;
    for (int i = 0; i < 16; ++i) if (mask & (1U << i)) ++n;
    return n;
}

//----------
// Draw
//----------

bool StochasticSequenceEditPage::isActiveForSelectedTrack() const {
    return _project.selectedTrack().trackMode() == Track::TrackMode::Stochastic &&
        _engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic;
}

void StochasticSequenceEditPage::draw(Canvas &canvas) {
    if (!isActiveForSelectedTrack()) return;
    switch (_currentPage) {
    case Page::Live:     drawLivePage(canvas); break;
    case Page::Loop:     drawLoopPage(canvas); break;
    case Page::Pitch:    drawPitchPage(canvas); break;
    case Page::Duration: drawDurationPage(canvas); break;
    case Page::Count:    break;
    }
}

//----------
// Hero page draws
//----------

void StochasticSequenceEditPage::drawLivePage(Canvas &canvas) {
    auto &seq = _project.selectedTrack().stochasticTrack().sequence(_project.selectedPatternIndex());

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "LIVE");

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
    int range      = seq.range();
    int burst      = seq.burst();
    int burstCount = seq.burstCount();
    int burstRate  = seq.burstRate();
    int gateLength = seq.gateLength();
    int slide      = seq.slide();
    int legato     = seq.legatoProb();
    int feel       = seq.feel();

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
        int dist2 = (dy * dy * 160) / (spreadY * spreadY);
        int env = 256 - dist2;
        if (env < 48) continue;       // skip dim tails
        Color col = (env >= 210) ? Color::MediumLow : Color::Low;
        canvas.setColor(col);
        int step = (env >= 170) ? 3 : (env >= 96 ? 4 : 5);
        int offset = (y * 5 + spread) % step;
        for (int x = vpLeft + offset; x < vpRight; x += step) {
            if (((x + y * 3) % 11) < 2) continue;
            canvas.point(x, y);
        }
    }

    // --- Event-driven walker ----------------------------------------------
    // The trail is an event-time ring buffer. On every gate rising edge the
    // engine emits a new event; we shift the ring left and write a fresh
    // particle at slot 0 (the right edge of the viewport) with vertical
    // position derived from the live CV (= pitch). Horizontal motion = rhythm
    // (events arriving), vertical = pitch.
    _directWalkerTick++;

    const StochasticTrackEngine *stoEng = nullptr;
    if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
        auto &eng = _engine.selectedTrackEngine().as<StochasticTrackEngine>();
        stoEng = &eng;
    }

    // Map liveCv (≈ -5..+5 V typical) to a vertical offset around biasY.
    // Range opens the vertical travel, while Bias/Spread stay the visible pitch
    // field. This keeps Bias/Spread semantics intact and makes Range legible.
    auto cvToOffset = [&] (float cv) {
        int offset = -int(cv * (2.f + float(range)));   // 1V ≈ 3..6 px
        int cvOffsetMax = 4 + range * 3;
        int hardMax = vpH / 2 - 2;
        if (cvOffsetMax > hardMax) cvOffsetMax = hardMax;
        if (offset > cvOffsetMax) offset = cvOffsetMax;
        else if (offset < -cvOffsetMax) offset = -cvOffsetMax;
        return offset;
    };

    // Runtime history supplies event truth: recent CV positions, actual rests,
    // and actual child count when the engine produced a parent burst. Knobs
    // still shape the metaphor below.
    if (stoEng && stoEng->directHistoryCount() > 0) {
        int count = std::min<int>(kDirectTrailMax, stoEng->directHistoryCount());
        for (int i = 0; i < count; ++i) {
            const auto &ev = stoEng->directHistoryEvent(i);
            uint8_t flags = (ev.rest ? 1 : 0) | (ev.children > 0 ? 2 : 0);
            _directTrail[i] = { int16_t(cvToOffset(ev.cv)), flags, ev.children };
        }
        _directTrailFilled = uint8_t(count);
    } else {
        _directTrailFilled = 0;
    }

    // Horizontal stride per step. DURA controls the base footstep distance;
    // VARI perturbs each older particle deterministically. The whole walker set
    // is midpoint-anchored: it grows from center and collapses back to center,
    // never from the right wall. Clamp the visible span so it neither hits the
    // walls nor squeezes below a readable 60 px footprint.
    int baseStride = 7 + (7 - duration) * 4;
    if (baseStride < 5) baseStride = 5;
    int vpCx = (vpLeft + vpRight) / 2;
    int filled = _directTrailFilled > 0 ? _directTrailFilled : 1;
    const int kMinDirectSpan = 60;
    int maxSpan = (vpRight - vpLeft) - 18;
    int rawSpan = (filled - 1) * baseStride;
    int span = rawSpan;
    if (filled > 1 && span < kMinDirectSpan) span = kMinDirectSpan;
    if (span > maxSpan) span = maxSpan;
    int walkerX = vpCx + span / 2;
    int oldestX = vpCx - span / 2;
    int effectiveStride = filled > 1 ? span / (filled - 1) : 0;

    // Pitch guide rails: RANGE opens the vertical travel without disturbing the
    // Bias/Spread haze. Low contrast so it reads as motion affordance, not grid.
    canvas.setColor(Color::Low);
    int rails = 1 + range * 2;
    for (int r = 0; r < rails; ++r) {
        int y = vpCy;
        if (rails > 1) y = vpCy - ((rails - 1) * 3) / 2 + r * 3;
        if (y >= vpTop + 1 && y <= vpBot - 1) canvas.hline(oldestX, y, span + 1);
    }

    // Particle slope encodes CONT direction (positive = up-right tilt). Contour
    // also adds an age-based vertical drift so the whole phrase visibly leans.
    int slopeDy = -(contour * 3) / 200;             // -1..+1 typically
    int contourStep = (contour * 4) / 100;          // -4..+4 px over age
    int burstSpacing = 7 - (burstRate * 5) / 100;   // high rate = tighter child pips
    if (burstSpacing < 2) burstSpacing = 2;
    bool burstPitchScatter = (seq.burstHold() == StochasticBurstHold::Roll);

    // Slide and Legato turn the point cloud into a connected phrase. Slide draws
    // a pitch ribbon; Legato draws a low gate-continuity rail between events.
    if (_directTrailFilled > 1 && (slide > 0 || legato > 0)) {
        int prevX = -1;
        int prevY = -1;
        for (int i = _directTrailFilled - 1; i >= 0; --i) {
            const DirectParticle &p = _directTrail[i];
            bool isRest = (p.flags & 1) != 0;
            int jitter = (variation * int((uint32_t(_directWalkerTick + i * 29) % 9u) - 4)) / 120;
            int px = walkerX - i * effectiveStride + jitter;
            int py = biasY + p.yOffset + contourStep * i;
            if (py < vpTop + 2) py = vpTop + 2;
            if (py > vpBot - 2) py = vpBot - 2;
            if (!isRest && prevX >= 0) {
                if (slide > 0) {
                    canvas.setColor(slide > 65 ? Color::Medium : Color::MediumLow);
                    canvas.line(prevX, prevY, px, py);
                }
                if (legato > 45) {
                    canvas.setColor(Color::Low);
                    int railY = (prevY > py ? prevY : py) + 2;
                    if (railY <= vpBot - 1) canvas.hline(prevX, railY, px - prevX + 1);
                }
            }
            if (!isRest) {
                prevX = px;
                prevY = py;
            }
        }
    }

    // Draw oldest → newest so newer events overdraw older fade.
    for (int i = _directTrailFilled - 1; i >= 0; --i) {
        const DirectParticle &p = _directTrail[i];
        int jitter = (variation * int((uint32_t(_directWalkerTick + i * 29) % 9u) - 4)) / 120;
        int px = walkerX - i * effectiveStride + jitter;
        if (px < vpLeft) break;
        if (px > vpRight) continue;
        int py = biasY + p.yOffset + contourStep * i;
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
        int radius = gateLength >= 75 ? 2 : (gateLength >= 35 ? 1 : 0);
        if (i == 0) {
            int pulseRadius = radius + 1;
            canvas.hline(px - pulseRadius, py, pulseRadius * 2 + 1);
            for (int yy = 1; yy <= pulseRadius; ++yy) {
                int w = pulseRadius * 2 + 1 - yy * 2;
                if (w < 1) w = 1;
                canvas.hline(px - w / 2, py - yy, w);
                canvas.hline(px - w / 2, py + yy, w);
            }
            canvas.setColor(Color::MediumBright);
            canvas.point(px - pulseRadius - 2, py);
            canvas.point(px + pulseRadius + 2, py);
            canvas.point(px, py - pulseRadius - 2);
            canvas.point(px, py + pulseRadius + 2);
        } else {
            canvas.line(px - 1, py + slopeDy, px + 1, py - slopeDy);
            canvas.point(px, py);
            if (radius > 1) canvas.point(px, py - 1);
        }
        // Burst children — small dots offset rightward from the parent slot.
        if (isBurst && p.children > 0) {
            for (int c = 0; c < p.children; ++c) {
                int cxp = px + (c + 1) * burstSpacing;
                if (cxp >= vpRight - 1) break;
                int cy = py;
                if (burstPitchScatter) {
                    int scatter = 1 + spread / 35;
                    cy += (int((_directWalkerTick + i * 17 + c * 5) % uint32_t(scatter * 2 + 1)) - scatter);
                }
                if (cy < vpTop + 1) cy = vpTop + 1;
                if (cy > vpBot - 1) cy = vpBot - 1;
                canvas.setColor(Color::MediumBright);
                canvas.point(cxp, cy);
                canvas.point(cxp, cy + 1 <= vpBot ? cy + 1 : cy);
            }
        }
    }

    // --- Info rows ---------------------------------------------------------
    // Tiny readout: per-step 1-letter label + value, two rows of 8 columns
    // aligned to a 32-px grid. Small overlay: full param name + value when a
    // step is held.

    // Helper: Duration step shows the actual fraction (e.g. "1/16", "1/8T")
    // from the divisor-aware printDurationEntry table — not the raw step index.
    auto durationLabel = [&](FixedStringBuilder<16> &s) {
        s.reset();
        seq.printDurationEntry(s, duration);
    };
    FixedStringBuilder<32> str;
    bool labeled = true;
    canvas.setFont(Font::Small);
    canvas.setColor(Color::Bright);
    int heldCount = heroPopcount(_heroSelectionMask);
    int heldStep  = (heldCount == 1) ? heroFirstHeld(_heroSelectionMask) : -1;
    if (heldCount > 1) {
        str("MANY SELECTED");
    } else switch (heldStep) {
    case 0:  { FixedStringBuilder<16> d; durationLabel(d); str("DURATION %s", static_cast<const char*>(d)); break; }
    case 1:  str("VARIATION %d", variation); break;
    case 2:  str("REST %d", rest); break;
    case 3:  str("RANGE %d%%", seq.range()); break;
    case 4:  str("BURST %d", burst); break;
    case 5:  str("BURST COUNT %d", burstCount); break;
    case 6:  str("BURST RATE %d", burstRate); break;
    case 7:  str("FEEL %d", feel); break;
    case 8:  str("COMPLEXITY %d", complexity); break;
    case 9:  str("CONTOUR %+d", contour); break;
    case 10: str("BIAS %d", bias); break;
    case 11: str("SPREAD %d", spread); break;
    case 12: str("REPEAT %d", repeats); break;
    case 13: str("GATE LENGTH %d", gateLength); break;
    case 14: str("SLIDE %d", slide); break;
    case 15: str("LEGATO %d", legato); break;
    default: labeled = false; break;
    }
    if (labeled || heldCount > 1) {
        canvas.drawText(8, 19, str);
    } else {
        canvas.setFont(Font::Tiny);
        canvas.setColor(Color::Medium);
        FixedStringBuilder<16> s;
        // 8-column grid, 32 px per column, starting at x=8.
        const int col0 = 8;
        const int colStep = 32;
        const int yTop = 18;
        const int yBot = 24;

        // Top row (slots 0..7): rhythm + burst sub-set.
        FixedStringBuilder<16> durStr; durationLabel(durStr);
        s.reset(); s("D %s",  static_cast<const char*>(durStr)); canvas.drawText(col0 + 0 * colStep, yTop, s);
        s.reset(); s("V %d",  variation);   canvas.drawText(col0 + 1 * colStep, yTop, s);
        s.reset(); s("R %d",  rest);        canvas.drawText(col0 + 2 * colStep, yTop, s);
        s.reset(); s("R%d",   seq.range()); canvas.drawText(col0 + 3 * colStep, yTop, s);
        s.reset(); s("B %d",  burst);       canvas.drawText(col0 + 4 * colStep, yTop, s);
        s.reset(); s("C %d",  burstCount);  canvas.drawText(col0 + 5 * colStep, yTop, s);
        s.reset(); s("T %d",  burstRate);   canvas.drawText(col0 + 6 * colStep, yTop, s);
        s.reset(); s("F %d",  feel);        canvas.drawText(col0 + 7 * colStep, yTop, s);

        // Bottom row (slots 8..15): pitch shape + per-event gate behavior.
        s.reset(); s("X %d",  complexity);  canvas.drawText(col0 + 0 * colStep, yBot, s);
        s.reset(); s("O %+d", contour);     canvas.drawText(col0 + 1 * colStep, yBot, s);
        s.reset(); s("I %d",  bias);        canvas.drawText(col0 + 2 * colStep, yBot, s);
        s.reset(); s("S %d",  spread);      canvas.drawText(col0 + 3 * colStep, yBot, s);
        s.reset(); s("E %d",  repeats);     canvas.drawText(col0 + 4 * colStep, yBot, s);
        s.reset(); s("G %d",  gateLength);  canvas.drawText(col0 + 5 * colStep, yBot, s);
        s.reset(); s("L %d",  slide);       canvas.drawText(col0 + 6 * colStep, yBot, s);
        s.reset(); s("A %d",  legato);      canvas.drawText(col0 + 7 * colStep, yBot, s);
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
        // currentStep() returns _currentStep, which is the NEXT step to fire
        // (engine increments inside triggerStep after reading the index).
        // Step back one with wrap to track what's audible right now.
        //
        // Bug fix 2026-05-22: previous math wrapped across the full sequence
        // size, not the playback window. At the cycle boundary (engine
        // wraps from `last` to `first`), `_currentStep` becomes `first` and
        // the just-played audible step is `last`. Old math gave `first - 1`
        // (sometimes outside the window). Walk the window, not the sequence.
        int next = eng.currentStep();
        int winFirst = std::max(0, std::min(lf, seqSize - 1));
        int winLast  = std::max(winFirst, std::min(ll, seqSize - 1));
        int winSize  = winLast - winFirst + 1;
        if (winSize <= 0) winSize = 1;
        // Clamp next into the window before stepping back (defensive: snap
        // clause may have repositioned _currentStep on a window edit).
        int relNext = ((next - winFirst) % winSize + winSize) % winSize;
        int relAudible = (relNext - 1 + winSize) % winSize;
        currentStep = winFirst + relAudible;
    }

    // Adaptive event timeline: ALL steps 0..size-1 always visible. Brackets
    // snap to the [first, last] window — moving first/last shifts only the
    // brackets, not the step count. Rotate shifts which step plays at each
    // window step (same logic the engine uses for read-index resolution).
    const int minStepW = 3;

    auto resolveEventIdx = [&] (int displayIdx) -> int {
        if (displayIdx >= lf && displayIdx <= ll && windowSize > 0 && rot != 0) {
            int offset = (displayIdx - lf + rot) % windowSize;
            if (offset < 0) offset += windowSize;
            return lf + offset;
        }
        return displayIdx;
    };

    // The engine writes back live-generated events to sequence.steps()[] each
    // tick (Live mode), so the stored buffer is always the most recently-played
    // content. Tape reads stored data uniformly — Loop mode is a snapshot, Live
    // mode updates progressively as each step plays.
    auto durIndexForSlot = [&] (const StochasticStepContent &ev) {
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
        const auto &ev = seq.steps()[eIdx];
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
        const auto &ev = seq.steps()[eIdx];
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

    // Bottom-row chip state — MaskR + TiltR under the bars (normal color).
    canvas.setFont(Font::Tiny);
    canvas.setColor(Color::Medium);
    FixedStringBuilder<16> mt;
    mt("%d", seq.maskRhythm());
    canvas.drawText(margin, tapeBot + 14, mt);
    mt.reset();
    mt("%+d", seq.tiltRhythm());
    canvas.drawText(margin + availW - canvas.textWidth(mt), tapeBot + 14, mt);

    // Top-row chip state — MaskM + TiltM above the tape (inverse color).
    canvas.setColor(Color::Bright);
    mt.reset();
    mt("%d", seq.maskMelody());
    canvas.drawText(margin, tapeTop - 4, mt);
    mt.reset();
    mt("%d", seq.tiltMelody());
    canvas.drawText(margin + availW - canvas.textWidth(mt), tapeTop - 4, mt);

    // Top-area readout. Multi-select aware: popcount>1 → "MANY SELECTED",
    // popcount==1 → that step's value in Small font, popcount==0 → tiny macros.
    int heldCount = heroPopcount(_heroSelectionMask);
    int heldStep  = (heldCount == 1) ? heroFirstHeld(_heroSelectionMask) : -1;
    if (heldCount > 1) {
        canvas.setFont(Font::Small);
        canvas.setColor(Color::Bright);
        canvas.drawText(8, 18, "MANY SELECTED");
    } else if (heldStep >= 0) {
        FixedStringBuilder<32> str;
        if (heldStep == 0)       str("PATIENCE R %d", seq.patienceRhythm());
        else if (heldStep == 1)  str("PATIENCE M %d", seq.patienceMelody());
        else if (heldStep == 2)  str("MUTATE %+d", seq.mutate());
        else if (heldStep == 3)  str("JUMP %d", seq.jump());
        else if (heldStep == 4)  str("SLEEP %d", seq.sleep());
        else if (heldStep == 8)  str("FIRST %d", seq.first());
        else if (heldStep == 9)  str("LAST %d", seq.last());
        else if (heldStep == 10) str("SIZE %d", seq.size());
        else if (heldStep == 11) str("ROTATE %+d", seq.rotate());
        else if (heldStep == 6)  str("MASKM %d", seq.maskMelody());
        else if (heldStep == 7)  str("TILTM %d", seq.tiltMelody());
        else if (heldStep == 14) str("MASKR %d", seq.maskRhythm());
        else if (heldStep == 15) str("TILTR %d", seq.tiltRhythm());
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

void StochasticSequenceEditPage::editLiveStep(int step, int value, bool shift) {
    auto &seq = _project.selectedTrack().stochasticTrack().sequence(_project.selectedPatternIndex());
    int v = shift ? value * 10 : value;
    // DIRECT layout:
    //   Top row    0=DURA  1=VARI  2=REST  3=RANG       (red)
    //              4=BURS  5=BCNT  6=BRAT  7=BPIT       (orange — burst sub-set)
    //   Bottom row 8=CMPX  9=CONT  10=BIAS 11=SPRE 12=REPT  (green — pitch shape)
    //              13=GATE 14=SLID 15=LEGA              (red — per-event gate behavior)
    switch (step) {
    // Phase 16 P10 (2026-05-23): every knob whose value is now consumed by the
    // cache walk (rebuildStepCache) must invalidate the cache on
    // edit. Knobs that only roll at trigger time (Rest, Repeat, Legato, Slide
    // probability) stay no-op refresh-wise. Feel is read per-trigger by the
    // engine so notify is redundant for it — kept for consistency.
    case 0:  seq.setNoteDuration(seq.noteDuration() + value);        notifyStochasticShapingEdit(); break;
    case 1:  seq.setVariation(seq.variation() + v);                  notifyStochasticShapingEdit(); break;
    case 2:  seq.setRest(seq.rest() + v); break;
    case 3:  seq.setRange(seq.range() + (shift ? value * 10 : value)); notifyStochasticShapingEdit(); break;
    case 4:  seq.setBurst(seq.burst() + v);                          notifyStochasticShapingEdit(); break;
    case 5:  seq.setBurstCount(seq.burstCount() + v);                notifyStochasticShapingEdit(); break;
    case 6:  seq.setBurstRate(seq.burstRate() + v);                  notifyStochasticShapingEdit(); break;
    case 7:  seq.setFeel(seq.feel() + v);                            notifyStochasticShapingEdit(); break;
    case 8:  seq.setComplexity(seq.complexity() + v);                notifyStochasticShapingEdit(); break;
    case 9:  seq.setContour(seq.contour() + v);                      notifyStochasticShapingEdit(); break;
    case 10: seq.setMarblesBias(seq.marblesBias() + v);              notifyStochasticShapingEdit(); break;
    case 11: seq.setMarblesSpread(seq.marblesSpread() + v);          notifyStochasticShapingEdit(); break;
    case 12: seq.setRepeatProb(seq.repeatProb() + v); break;
    case 13: seq.setGateLength(seq.gateLength() + v);                notifyStochasticShapingEdit(); break;
    case 14: seq.setSlide(seq.slide() + v); break;
    case 15: seq.setLegatoProb(seq.legatoProb() + v); break;
    }
}

void StochasticSequenceEditPage::notifyStochasticShapingEdit() {
    int trackIdx = _project.selectedTrackIndex();
    auto &engine = _engine.trackEngine(trackIdx);
    if (engine.trackMode() != Track::TrackMode::Stochastic) return;
    if (engine.pattern() != _project.selectedPatternIndex()) return;
    static_cast<StochasticTrackEngine &>(engine).refreshStepCacheNow();
}

void StochasticSequenceEditPage::notifyStochasticWindowEdit() {
    // Resolve the selected track's engine and ask it to sync. Three guards:
    //   1. Selected track must currently be running as Stochastic.
    //   2. The just-edited pattern must be the one the engine is currently
    //      playing — editing pattern 3 while engine plays pattern 0 is a
    //      model-only change that doesn't need engine sync (and calling sync
    //      would clear pattern 0's audio queues for no reason).
    //   3. The engine-array index matches the project's track index;
    //      engine/model are kept consistent via Engine::updateTrackSetups.
    int trackIdx = _project.selectedTrackIndex();
    auto &engine = _engine.trackEngine(trackIdx);
    if (engine.trackMode() != Track::TrackMode::Stochastic) return;
    if (engine.pattern() != _project.selectedPatternIndex()) return;
    static_cast<StochasticTrackEngine &>(engine).syncWindowEdit();
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
    case 8:  seq.setFirst(seq.first() + value);   notifyStochasticWindowEdit(); break;
    case 9:  /* Last slot stubbed dead 2026-05-24 — collapsed into Size */ break;
    case 10: seq.setSize(seq.size() + value);     notifyStochasticWindowEdit(); break;
    case 11: seq.setRotate(seq.rotate() + value); break;   // rotation doesn't move window bounds
    // Top row last two — MaskM + TiltM (pitch-centrality filter, LoopM-only)
    case 6: seq.setMaskMelody(seq.maskMelody() + v); break;
    case 7: seq.setTiltMelody(seq.tiltMelody() + v); break;
    // Bottom row last two — MaskR + TiltR (rhythm density filter, LoopR-only)
    case 14: seq.setMaskRhythm(seq.maskRhythm() + v); break;
    case 15: seq.setTiltRhythm(seq.tiltRhythm() + v); break;
    }
}

//----------
// Hero Fn handlers
//----------

bool StochasticSequenceEditPage::handleLiveFunction(int fn, bool shift, int pressCount) {
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
            if (auto *eng = stoEng()) { eng->renewRhythm(); showMessage("Rhythm renewed"); }
        } else {
            showMessage("Press again to renew rhythm");
        }
        return true;
    case 3:
        if (pressCount == 2) {
            if (auto *eng = stoEng()) { eng->renewMelody(); showMessage("Melody renewed"); }
        } else {
            showMessage("Press again to renew melody");
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
            if (auto *eng = stoEng()) { eng->renewRhythm(); showMessage("Rhythm renewed"); }
        } else {
            showMessage("Press again to renew rhythm");
        }
        return true;
    case 3: // NewM — double-click guard.
        if (pressCount == 2) {
            if (auto *eng = stoEng()) { eng->renewMelody(); showMessage("Melody renewed"); }
        } else {
            showMessage("Press again to renew melody");
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

    // Bar geometry: top at y = baseY - barMaxH = 50 - 22 = 28. Small-font readout
    // at y=18 (glyphs span ~y=12..19) leaves ~9 px clearance below to the bar
    // tops at y=28 — no overlap with the topmost ticket values.
    int baseY = 50;
    int barMaxH = 22;
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
    canvas.setFont(Font::Tiny);
    canvas.setColor(Color::Medium);
    canvas.drawText(Width - rw - 8, 18, str);

    // Footer: F1 LoopM/LiveM, F2 NewM, F3 DROT, F4 MROT, F5 NEXT.
    // EditFocus::Ticket is the implicit default — encoder edits the held
    // degree's ticket value when neither DROT nor MROT is active.
    const char *fnM = (sequence.melodyMode() == StochasticSourceMode::Loop) ? "LoopM" : "LiveM";
    const char *footer[] = { fnM, "NewM", "DROT", "MROT", "NEXT" };
    int activeFn = -1;
    if (_editFocus == EditFocus::DegreeRotation) activeFn = 2;
    else if (_editFocus == EditFocus::MaskRotation) activeFn = 3;
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), activeFn);
}

void StochasticSequenceEditPage::drawDurationPage(Canvas &canvas) {
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &sequence = track.sequence(_project.selectedPatternIndex());

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "DURATION TICKETS");

    const int numSlots = 8;
    // Bar geometry: baseY=46 with barMaxH=22 puts bar tops at y=24, leaving
    // ~5 px clearance below the Small-font readout at y=18 (glyphs y=12..19).
    // Slot label text at baseY+8=54 stays well above the footer band (~y=58+).
    const int baseY = 46;
    const int barMaxH = 22;
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
        } else if (i == _selectedDurEntry) {
            canvas.setColor(Color::MediumBright);
            canvas.hline(x - 1, baseY + 2, barW + 2);
        }

        canvas.setFont(Font::Tiny);
        canvas.setColor(Color::Low);
        FixedStringBuilder<8> entryLabel;
        sequence.printDurationEntry(entryLabel, i);
        int labelW = canvas.textWidth(entryLabel);
        canvas.drawText(x + (barW - labelW) / 2, baseY + 8, entryLabel);
    }

    // Left info: Small font primary value depending on focus.
    FixedStringBuilder<32> str;
    if (_durFocus == DurFocus::Rest) {
        str("REST %d%%", sequence.rest());
    } else {
        FixedStringBuilder<8> entryLabel;
        sequence.printDurationEntry(entryLabel, _selectedDurEntry);
        str("%s: %d", (const char *)entryLabel, sequence.durationTicket(_selectedDurEntry));
    }
    canvas.setFont(Font::Small);
    canvas.setColor(Color::Bright);
    canvas.drawText(8, 18, str);

    // Tiny rest readout always visible (top-right). When focus is on Rest, the
    // big Small-font version on the left takes the spotlight; this tiny line
    // is a constant reminder of the current value either way.
    FixedStringBuilder<16> restTiny;
    restTiny("REST %d%%", sequence.rest());
    canvas.setFont(Font::Tiny);
    canvas.setColor(_durFocus == DurFocus::Rest ? Color::Medium : Color::Bright);
    canvas.drawText(Width - canvas.textWidth(restTiny) - 8, 18, restTiny);

    // Footer: F1 LoopR/LiveR, F2 NewR, F3 RST, F4 unused, F5 NEXT.
    // DurFocus::DurTicket is the implicit default (encoder edits held slot's
    // ticket weight).
    const char *fnR = (sequence.rhythmMode() == StochasticSourceMode::Loop) ? "LoopR" : "LiveR";
    const char *footer[] = { fnR, "NewR", "RST", nullptr, "NEXT" };
    int activeFn = (_durFocus == DurFocus::Rest) ? 2 : -1;
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), activeFn);
}

//----------
// LEDs
//----------

void StochasticSequenceEditPage::updateLeds(Leds &leds) {
    if (!isActiveForSelectedTrack()) return;
    auto &track = _project.selectedTrack().stochasticTrack();
    auto &sequence = track.sequence(_project.selectedPatternIndex());

    auto heldStep = [&] (int i) { return (_heroSelectionMask & (1U << i)) != 0; };

    switch (_currentPage) {
    case Page::Live: {
        // Top row 0..3 red — DURA/VARI/REST/RANG.
        for (int i = 0; i <= 3; ++i) {
            bool held = heldStep(i);
            leds.set(MatrixMap::fromStep(i), /*red*/true, /*green*/held);
        }
        // Top row 4..7 orange (red+green) — burst sub-set (BURS/BCNT/BRAT/BPIT).
        for (int i = 4; i <= 7; ++i) {
            bool held = heldStep(i);
            leds.set(MatrixMap::fromStep(i), /*red*/!held, /*green*/!held ? true : true);
            if (held) {
                leds.set(MatrixMap::fromStep(i), /*red*/true, /*green*/false);
            }
        }
        // Bottom row 8..12 green — pitch shape (CMPX/CONT/BIAS/SPRE/REPT).
        for (int i = 8; i <= 12; ++i) {
            bool held = heldStep(i);
            leds.set(MatrixMap::fromStep(i), /*red*/held, /*green*/true);
        }
        // Bottom row 13..15 red — per-event gate behavior (GATE/SLID/LEGA).
        for (int i = 13; i <= 15; ++i) {
            bool held = heldStep(i);
            leds.set(MatrixMap::fromStep(i), /*red*/true, /*green*/held);
        }
        break;
    }
    case Page::Loop:
        // Top row (0..4): red — patience R, patience M, mutate, jump, sleep
        for (int i = 0; i <= 4; ++i) {
            bool held = heldStep(i);
            leds.set(MatrixMap::fromStep(i), /*red*/true, /*green*/held);
        }
        // Top row right (6..7): green — MaskM + TiltM (pitch-centrality, LoopM)
        for (int i = 6; i <= 7; ++i) {
            bool held = heldStep(i);
            leds.set(MatrixMap::fromStep(i), /*red*/held, /*green*/true);
        }
        // Bottom row left (8..11): green — loop window (first, last, size, rotate)
        for (int i = 8; i <= 11; ++i) {
            bool held = heldStep(i);
            leds.set(MatrixMap::fromStep(i), /*red*/held, /*green*/true);
        }
        // Bottom row right (14..15): green — MaskR + TiltR (rhythm density, LoopR)
        for (int i = 14; i <= 15; ++i) {
            bool held = heldStep(i);
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
            leds.set(MatrixMap::fromStep(i), active || (i == _selectedDurEntry), true);
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
    if (!isActiveForSelectedTrack()) return;
    const auto &key = event.key();
    bool isHero = (_currentPage == Page::Live || _currentPage == Page::Loop);
    if (isHero) {
        if (key.isStep() && !key.pageModifier()) {
            if (!_persistMode && key.shiftModifier()) {
                _heroSelectionMask = 0;
                _persistMode = true;
            }
            if (_persistMode && !key.shiftModifier()) {
                _heroSelectionMask = 0;
                _persistMode = false;
            }
            int step = key.step();
            if (step >= 0 && step < 16) {
                _heroSelectionMask ^= (1U << step);
            }
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
            _selectedDurEntry = step;
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
    if (!isActiveForSelectedTrack()) return;
    const auto &key = event.key();
    bool isHero = (_currentPage == Page::Live || _currentPage == Page::Loop);
    if (isHero) {
        if (key.isStep() && !_persistMode) {
            int step = key.step();
            if (step >= 0 && step < 16) {
                _heroSelectionMask &= ~(1U << step);
            }
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
    if (!isActiveForSelectedTrack()) return;
    const auto &key = event.key();
    bool isHero = (_currentPage == Page::Live || _currentPage == Page::Loop);
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
            case Page::Live:  handleLiveFunction(fn, shift, event.count()); break;
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
        auto &track = _project.selectedTrack().stochasticTrack();
        auto &sequence = track.sequence(_project.selectedPatternIndex());
        auto stoEng = [&] () -> StochasticTrackEngine* {
            if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
                return &_engine.selectedTrackEngine().as<StochasticTrackEngine>();
            }
            return nullptr;
        };
        switch (key.function()) {
        case 0: { // F1: LoopM / LiveM — instant toggle.
            auto newMode = (sequence.melodyMode() == StochasticSourceMode::Loop)
                ? StochasticSourceMode::Live : StochasticSourceMode::Loop;
            sequence.setMelodyMode(newMode);
            break;
        }
        case 1: // F2: NewM — double-click guard.
            if (event.count() == 2) {
                if (auto *eng = stoEng()) { eng->renewMelody(); showMessage("Melody renewed"); }
            } else {
                showMessage("Press again to renew melody");
            }
            break;
        case 2: // F3: DROT toggle (Ticket ↔ DegreeRotation).
            _editFocus = (_editFocus == EditFocus::DegreeRotation) ? EditFocus::Ticket : EditFocus::DegreeRotation;
            break;
        case 3: // F4: MROT toggle (Ticket ↔ MaskRotation).
            _editFocus = (_editFocus == EditFocus::MaskRotation) ? EditFocus::Ticket : EditFocus::MaskRotation;
            break;
        case 4: // F5: NEXT.
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
        auto &track = _project.selectedTrack().stochasticTrack();
        auto &sequence = track.sequence(_project.selectedPatternIndex());
        auto stoEng = [&] () -> StochasticTrackEngine* {
            if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
                return &_engine.selectedTrackEngine().as<StochasticTrackEngine>();
            }
            return nullptr;
        };
        switch (key.function()) {
        case 0: { // F1: LoopR / LiveR — instant toggle.
            auto newMode = (sequence.rhythmMode() == StochasticSourceMode::Loop)
                ? StochasticSourceMode::Live : StochasticSourceMode::Loop;
            sequence.setRhythmMode(newMode);
            break;
        }
        case 1: // F2: NewR — double-click guard.
            if (event.count() == 2) {
                if (auto *eng = stoEng()) { eng->renewRhythm(); showMessage("Rhythm renewed"); }
            } else {
                showMessage("Press again to renew rhythm");
            }
            break;
        case 2: // F3: RST toggle (DurTicket ↔ Rest).
            _durFocus = (_durFocus == DurFocus::Rest) ? DurFocus::DurTicket : DurFocus::Rest;
            break;
        case 4: // F5: NEXT.
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
    if (!isActiveForSelectedTrack()) return;
    bool shift = event.pressed();
    if (_currentPage == Page::Live || _currentPage == Page::Loop) {
        // Multi-select: apply the encoder delta to every step set in the mask.
        // Fallback (mask == 0) keeps the page-default behavior of writing to
        // step 0 so a bare encoder turn still touches something.
        uint32_t mask = _heroSelectionMask;
        if (mask == 0) mask = 1U;   // implicit step 0
        for (int i = 0; i < 16; ++i) {
            if (!(mask & (1U << i))) continue;
            if (_currentPage == Page::Live) editLiveStep(i, event.value(), shift);
            else                            editLoopStep(i, event.value(), shift);
        }
        event.consume();
        return;
    }
    switch (_currentPage) {
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
        if (_durSelectionMask == 0) mask = (1U << _selectedDurEntry);
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
    case Page::Live: {
        // BurstHold toggle — demoted off slot 7 (slot 7 is Feel)
        // because Feel took its place. Hold = cluster cells share previous
        // pitch (ratchet); Roll = cluster cells each pick own pitch.
        if (ContextAction(index) == ContextAction::BurstHold) {
            auto cur = sequence.burstHold();
            sequence.setBurstHold(cur == StochasticBurstHold::Hold
                                  ? StochasticBurstHold::Roll
                                  : StochasticBurstHold::Hold);
            notifyStochasticShapingEdit();
            showMessage(sequence.burstHold() == StochasticBurstHold::Hold
                        ? "BURST HOLD"
                        : "BURST ROLL");
            break;
        }
        // INIT resets the LIVE-page params bound to step buttons back to their
        // model defaults (matches `StochasticSequence::clear()`). When the
        // selection mask is non-empty, only the held steps' params are reset
        // so the user can scope the init to a subset (mirrors ticket-page
        // selection-aware INIT). EVEN/RAND are not meaningful for these
        // mixed-type slots — surface a short reminder instead.
        if (ContextAction(index) != ContextAction::Init) {
            showMessage("INIT ONLY ON LIVE");
            break;
        }
        uint32_t mask = _heroSelectionMask;
        if (mask == 0) mask = 0xFFFFu;  // no selection → init all 16 slots
        auto wants = [&](int i) { return (mask & (1U << i)) != 0; };
        if (wants(0))  sequence.setNoteDuration(5);
        if (wants(1))  sequence.setVariation(16);
        if (wants(2))  sequence.setRest(0);
        if (wants(3))  sequence.setRange(50);
        if (wants(4))  sequence.setBurst(0);
        if (wants(5))  sequence.setBurstCount(0);
        if (wants(6))  sequence.setBurstRate(50);
        if (wants(7))  sequence.setFeel(50);   // slot 7 is Feel; BurstHold lives on the context menu
        if (wants(8))  sequence.setComplexity(50);
        if (wants(9))  sequence.setContour(0);
        if (wants(10)) sequence.setMarblesBias(50);
        if (wants(11)) sequence.setMarblesSpread(50);
        if (wants(12)) sequence.setRepeatProb(0);
        if (wants(13)) sequence.setGateLength(0);
        if (wants(14)) sequence.setSlide(0);
        if (wants(15)) sequence.setLegatoProb(0);
        // INIT touches many cache-affecting knobs; always refresh.
        notifyStochasticShapingEdit();
        showMessage(_heroSelectionMask ? "INIT SELECTED" : "INIT LIVE");
        break;
    }
    case Page::Loop: {
        if (ContextAction(index) != ContextAction::Init) {
            showMessage("INIT ONLY ON LOOP");
            break;
        }
        uint32_t mask = _heroSelectionMask;
        if (mask == 0) mask = 0xC3CFu;  // bits 0-4, 6-7, 8-11, 14-15 (the bound LOOP slots)
        auto wants = [&](int i) { return (mask & (1U << i)) != 0; };
        if (wants(0))  sequence.setPatienceRhythm(100);
        if (wants(1))  sequence.setPatienceMelody(100);
        if (wants(2))  sequence.setMutate(0);
        if (wants(3))  sequence.setJump(0);
        if (wants(4))  sequence.setSleep(0);
        bool windowEdited = false;
        if (wants(8))  { sequence.setFirst(0);  windowEdited = true; }
        // wants(9) → Last slot is a dead knob (2026-05-24); equivalent default
        // is Size=16 covered by wants(10).
        if (wants(10)) { sequence.setSize(16);  windowEdited = true; }
        if (wants(11)) sequence.setRotate(0);   // rotation doesn't move bounds
        if (windowEdited) notifyStochasticWindowEdit();
        if (wants(6))  sequence.setMaskMelody(100);
        if (wants(7))  sequence.setTiltMelody(0);
        if (wants(14)) sequence.setMaskRhythm(100);
        if (wants(15)) sequence.setTiltRhythm(0);
        showMessage(_heroSelectionMask ? "INIT SELECTED" : "INIT LOOP");
        break;
    }
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
