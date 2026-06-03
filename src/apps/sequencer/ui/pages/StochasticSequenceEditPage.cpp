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

    // --- Constellation rings -----------------------------------------------
    // Each recent event lives at a deterministic XY derived from its age
    // index and a seed mixed from rhythmSeed XOR melodySeed. Newer events
    // read brighter; older events fade out. NewR / NewM rerolls the seed
    // pair so the whole constellation reshapes instantly.
    _directWalkerTick++;

    const StochasticTrackEngine *stoEng = nullptr;
    if (_engine.selectedTrackEngine().trackMode() == Track::TrackMode::Stochastic) {
        auto &eng = _engine.selectedTrackEngine().as<StochasticTrackEngine>();
        stoEng = &eng;
    }

    if (stoEng && stoEng->directHistoryCount() > 0) {
        int count = std::min<int>(int(kDirectTrailMax), stoEng->directHistoryCount());
        for (int i = 0; i < count; ++i) {
            const auto &ev = stoEng->directHistoryEvent(i);
            uint8_t flags = (ev.rest ? 1 : 0) | (ev.children > 0 ? 2 : 0);
            _directTrail[i] = { ev.serial, flags, ev.children };
        }
        _directTrailFilled = uint8_t(count);
    } else {
        _directTrailFilled = 0;
    }

    uint32_t constSeed = seq.rhythmSeed() ^ seq.melodySeed();
    int baseR = 2 + duration / 2;             // 2..5 from DUR LUT 0..7
    int sparseThreshold = 100 - rest;          // higher Rest → fewer rings kept

    auto ringXY = [&](uint16_t serial, int &outX, int &outY) {
        uint32_t h = (uint32_t(serial) * 2654435761u) ^ (constSeed * 0x9E3779B1u);
        int rx = int(h & 0xFFFF);
        int ry = int((h >> 16) & 0xFFFF);
        int cx = (vpLeft + vpRight) / 2;
        int cy = (vpTop + vpBot) / 2;
        int hw = (vpRight - vpLeft) / 2;
        int hh = vpH / 2;
        int scale = 35 + (complexity * 65) / 100;          // 35..100
        int dx = (((rx - 0x8000) * hw) / 32768 * scale) / 100;
        int dy = (((ry - 0x8000) * hh) / 32768 * scale) / 100;
        int x = cx + dx;
        int y = cy + dy;
        // Contour skews the cloud via a serial-derived tilt — even-serial
        // events drift one way, odd the other, scaled by Contour. Keeps
        // the tilt visual without an age dependency.
        int tilt = int(serial & 0xF) - 8;       // -8..+7
        x += (contour * tilt * 6) / 1000;
        y -= (contour * tilt * 3) / 1000;
        if (x < vpLeft + 4) x = vpLeft + 4;
        if (x > vpRight - 4) x = vpRight - 4;
        if (y < vpTop + 4) y = vpTop + 4;
        if (y > vpBot - 4) y = vpBot - 4;
        outX = x;
        outY = y;
    };

    auto ageColor = [](int age) -> Color {
        if (age == 0) return Color::Bright;
        if (age <= 1) return Color::MediumBright;
        if (age <= 3) return Color::Medium;
        if (age <= 5) return Color::MediumLow;
        if (age <= 8) return Color::Low;
        return Color::None;
    };

    auto drawCircle = [&](int cx, int cy, int r) {
        int x = r, y = 0, err = 0;
        while (x >= y) {
            canvas.point(cx + x, cy + y);
            canvas.point(cx + y, cy + x);
            canvas.point(cx - y, cy + x);
            canvas.point(cx - x, cy + y);
            canvas.point(cx - x, cy - y);
            canvas.point(cx - y, cy - x);
            canvas.point(cx + y, cy - x);
            canvas.point(cx + x, cy - y);
            if (err <= 0) { y += 1; err += 2 * y + 1; }
            if (err > 0)  { x -= 1; err -= 2 * x + 1; }
        }
    };

    for (int age = _directTrailFilled - 1; age >= 0; --age) {
        Color col = ageColor(age);
        if (col == Color::None) continue;
        const auto &p = _directTrail[age];
        bool isRest = (p.flags & 1) != 0;
        bool isBurst = (p.flags & 2) != 0;
        int x, y;
        ringXY(p.serial, x, y);
        int r = baseR;
        if (variation > 0) {
            int wob = ((int(p.serial) * 11) % 5) - 2;
            r += (variation * wob) / 200;
            if (r < 2) r = 2; else if (r > 6) r = 6;
        }
        bool keep = (((uint32_t(p.serial) * 37u + constSeed) % 100u) < uint32_t(sparseThreshold));
        if (isRest || !keep) {
            canvas.setColor(col);
            canvas.point(x, y);
            continue;
        }
        if (age == 0) {
            canvas.setColor(Color::Bright);
            drawCircle(x, y, r + 1);
            if (repeats > 50) drawCircle(x, y, r - 1);
            canvas.fillRect(x - 1, y - 1, 3, 3);
            canvas.setColor(Color::MediumBright);
            canvas.point(x - r - 2, y);
            canvas.point(x + r + 2, y);
            canvas.point(x, y - r - 2);
            canvas.point(x, y + r + 2);
            if (isBurst && burst > 0 && p.children > 0) {
                int n = std::min<int>(int(p.children), burstCount - 1);
                int petalR = r + 3 + (burstRate * 3) / 100;
                static const int8_t dx8[8] = {  3,  2,  0, -2, -3, -2,  0,  2 };
                static const int8_t dy8[8] = {  0,  2,  3,  2,  0, -2, -3, -2 };
                for (int k = 0; k < n; ++k) {
                    int slot = (k * 8) / std::max(1, n);
                    int bx = x + (dx8[slot] * petalR) / 3;
                    int by = y + (dy8[slot] * petalR) / 3;
                    if (bx >= vpLeft && bx < vpRight && by >= vpTop && by <= vpBot) {
                        canvas.setColor(Color::MediumBright);
                        canvas.point(bx, by);
                    }
                }
            }
        } else {
            canvas.setColor(col);
            drawCircle(x, y, r);
            if (repeats > 50 && r > 2) drawCircle(x, y, r - 2);
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
    case 3:  str("FEEL %d", feel); break;
    case 4:  str("GATE LENGTH %d", gateLength); break;
    case 5:  str("LEGATO %d", legato); break;
    case 6:  str("BURST %d", burst); break;
    case 7:  str("BURST COUNT %d", burstCount); break;
    case 8:  str("COMPLEXITY %d", complexity); break;
    case 9:  str("CONTOUR %+d", contour); break;
    case 10: str("BIAS %d", bias); break;
    case 11: str("SPREAD %d", spread); break;
    case 12: str("RANGE %d%%", seq.range()); break;
    case 13: str("SLIDE %d", slide); break;
    case 14: str("REPEAT %d", repeats); break;
    case 15: str("BURST RATE %d", burstRate); break;
    default: labeled = false; break;
    }
    if (labeled || heldCount > 1) {
        canvas.drawText(8, 19, str);
    } else {
        canvas.setFont(Font::Tiny);
        canvas.setColor(Color::Medium);
        FixedStringBuilder<16> s;
        // 8-column grid, 32 px per column, starting at x=8. All 16 knobs.
        const int col0 = 8;
        const int colStep = 32;
        const int yTop = 18;
        const int yBot = 24;

        // Top row (slots 0..7): rhythm + articulation, burst pair on right.
        FixedStringBuilder<16> durStr; durationLabel(durStr);
        s.reset(); s("D %s",  static_cast<const char*>(durStr)); canvas.drawText(col0 + 0 * colStep, yTop, s);
        s.reset(); s("V %d",  variation);   canvas.drawText(col0 + 1 * colStep, yTop, s);
        s.reset(); s("R %d",  rest);        canvas.drawText(col0 + 2 * colStep, yTop, s);
        s.reset(); s("F %d",  feel);        canvas.drawText(col0 + 3 * colStep, yTop, s);
        s.reset(); s("G %d",  gateLength);  canvas.drawText(col0 + 4 * colStep, yTop, s);
        s.reset(); s("A %d",  legato);      canvas.drawText(col0 + 5 * colStep, yTop, s);
        s.reset(); s("B %d",  burst);       canvas.drawText(col0 + 6 * colStep, yTop, s);
        s.reset(); s("C %d",  burstCount);  canvas.drawText(col0 + 7 * colStep, yTop, s);

        // Bottom row (slots 8..15): pitch shape, repeat standalone, burst rate corner.
        s.reset(); s("X %d",  complexity);  canvas.drawText(col0 + 0 * colStep, yBot, s);
        s.reset(); s("O %+d", contour);     canvas.drawText(col0 + 1 * colStep, yBot, s);
        s.reset(); s("I %d",  bias);        canvas.drawText(col0 + 2 * colStep, yBot, s);
        s.reset(); s("S %d",  spread);      canvas.drawText(col0 + 3 * colStep, yBot, s);
        s.reset(); s("N %d",  seq.range()); canvas.drawText(col0 + 4 * colStep, yBot, s);
        s.reset(); s("L %d",  slide);       canvas.drawText(col0 + 5 * colStep, yBot, s);
        s.reset(); s("E %d",  repeats);     canvas.drawText(col0 + 6 * colStep, yBot, s);
        s.reset(); s("T %d",  burstRate);   canvas.drawText(col0 + 7 * colStep, yBot, s);
    }

    // Footer mirrors LOOP page. Shift swaps NewR / NewM labels to Undo.
    const char *fnR = (seq.rhythmMode() == StochasticSourceMode::Loop) ? "LoopR" : "LiveR";
    const char *fnM = (seq.melodyMode() == StochasticSourceMode::Loop) ? "LoopM" : "LiveM";
    const bool shift = globalKeyState()[Key::Shift];
    const char *footer[] = { fnR, fnM, shift ? "UndoR" : "NewR", shift ? "UndoM" : "NewM", "NEXT" };
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

    // Side bars — MaskR / TiltR in the left margin (R domain),
    // MaskM / TiltM in the right margin (M domain). 2 px wide bars
    // climbing tape_h from the bottom; faint at default 50, brighter
    // when knob deviates from default.
    auto drawSideBar = [&](int x, int value) {
        canvas.setColor(Color::Low);
        canvas.drawRect(x, tapeTop, 2, tapeRowH);
        int fillH = (value * (tapeRowH - 2)) / 100;
        if (fillH > 0) {
            canvas.setColor(value == 50 ? Color::Low : Color::Medium);
            canvas.fillRect(x, tapeBot - fillH, 2, fillH);
        }
    };
    drawSideBar(2,           seq.maskRhythm());
    drawSideBar(6,           seq.tiltRhythm());
    drawSideBar(Width - 8,   seq.tiltMelody());
    drawSideBar(Width - 4,   seq.maskMelody());

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
        else if (heldStep == 2)  str("MUTATE %d", seq.mutate());
        else if (heldStep == 3)  str("JUMP %d", seq.jump());
        else if (heldStep == 4)  str("SLEEP %d", seq.sleep());
        else if (heldStep == 8)  str("FIRST %d", seq.first());
        else if (heldStep == 9)  str("SIZE %d", seq.size());
        else if (heldStep == 10) str("ROTATE %+d", seq.rotate());
        else if (heldStep == 6)  str("MASK MELODY %d", seq.maskMelody());
        else if (heldStep == 7)  str("TILT MELODY %d", seq.tiltMelody());
        else if (heldStep == 14) str("MASK RHYTHM %d", seq.maskRhythm());
        else if (heldStep == 15) str("TILT RHYTHM %d", seq.tiltRhythm());
        canvas.setFont(Font::Small);
        canvas.setColor(Color::Bright);
        canvas.drawText(8, 18, str);
    } else {
        canvas.setFont(Font::Tiny);
        canvas.setColor(Color::Medium);
        FixedStringBuilder<8> chips[5];
        chips[0]("PR%d", seq.patienceRhythm());
        chips[1]("PM%d", seq.patienceMelody());
        chips[2]("MU%d", seq.mutate());
        chips[3]("JU%d", seq.jump());
        chips[4]("SL%d", seq.sleep());
        const int gap = 6;
        int total = 0;
        int widths[5];
        for (int i = 0; i < 5; ++i) {
            widths[i] = canvas.textWidth(chips[i]);
            total += widths[i];
        }
        total += gap * 4;
        int x = margin + (availW - total) / 2;
        for (int i = 0; i < 5; ++i) {
            canvas.drawText(x, 16, chips[i]);
            x += widths[i] + gap;
        }
    }

    const char *fnR = (seq.rhythmMode() == StochasticSourceMode::Loop) ? "LoopR" : "LiveR";
    const char *fnM = (seq.melodyMode() == StochasticSourceMode::Loop) ? "LoopM" : "LiveM";
    const bool shift = globalKeyState()[Key::Shift];
    const char *footer[] = { fnR, fnM, shift ? "UndoR" : "NewR", shift ? "UndoM" : "NewM", "NEXT" };
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), -1);
}

//----------
// Hero step edits — encoder writes the held step's param
//----------

void StochasticSequenceEditPage::editLiveStep(int step, int value, bool shift) {
    auto &seq = _project.selectedTrack().stochasticTrack().sequence(_project.selectedPatternIndex());
    int v = shift ? value * 10 : value;
    // LIVE layout (semantic grouping):
    //   Top row    0=DURA  1=VARI  2=REST  3=FEEL  4=GATE  5=LEGA   (rhythm + articulation)
    //              6=BURS  7=BCNT                                   (burst — top-right)
    //   Bottom row 8=CMPX  9=CONT  10=BIAS 11=SPRE 12=RANG 13=SLID  (pitch shape)
    //              14=REPT                                          (repeat — standalone)
    //              15=BRAT                                          (burst — bottom-right)
    switch (step) {
    case 0:  seq.setNoteDuration(seq.noteDuration() + value);        notifyStochasticShapingEdit(); break;
    case 1:  seq.setVariation(seq.variation() + v);                  notifyStochasticShapingEdit(); break;
    case 2:  seq.setRest(seq.rest() + v); break;
    case 3:  seq.setFeel(seq.feel() + v);                            notifyStochasticShapingEdit(); break;
    case 4:  seq.setGateLength(seq.gateLength() + v);                notifyStochasticShapingEdit(); break;
    case 5:  seq.setLegatoProb(seq.legatoProb() + v);                notifyStochasticShapingEdit(); break;
    case 6:  seq.setBurst(seq.burst() + v);                          notifyStochasticShapingEdit(); break;
    case 7:  seq.setBurstCount(seq.burstCount() + v);                notifyStochasticShapingEdit(); break;
    case 8:  seq.setComplexity(seq.complexity() + v);                notifyStochasticShapingEdit(); break;
    case 9:  seq.setContour(seq.contour() + v);                      notifyStochasticShapingEdit(); break;
    case 10: seq.setMarblesBias(seq.marblesBias() + v);              notifyStochasticShapingEdit(); break;
    case 11: seq.setMarblesSpread(seq.marblesSpread() + v);          notifyStochasticShapingEdit(); break;
    case 12: seq.setRange(seq.range() + (shift ? value * 10 : value)); notifyStochasticShapingEdit(); break;
    case 13: seq.setSlide(seq.slide() + v);                          notifyStochasticShapingEdit(); break;
    case 14: seq.setRepeatProb(seq.repeatProb() + v); break;
    case 15: seq.setBurstRate(seq.burstRate() + v);                  notifyStochasticShapingEdit(); break;
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
    case 9:  seq.setSize(seq.size() + value);     notifyStochasticWindowEdit(); break;
    case 10: seq.setRotate(seq.rotate() + value); break;   // rotation doesn't move window bounds
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
        // Plain mode toggle. No capture side effect.
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
        // NewR mode-aware: Live mode = capture shadow (single press),
        // Loop mode = renew (double-press guard). Shift = undo last renew.
        if (shift) {
            auto *eng = stoEng();
            if (eng && eng->undoRenewRhythm()) showMessage("Undo NewR");
            else                                showMessage("Nothing to undo");
            return true;
        }
        if (seq.rhythmMode() == StochasticSourceMode::Live) {
            if (auto *eng = stoEng()) eng->captureLiveAsLoopRhythm();
            showMessage("Live rhythm captured");
        } else if (pressCount == 2) {
            if (auto *eng = stoEng()) { eng->captureUndoRhythm(); eng->renewRhythm(); showMessage("Rhythm renewed"); }
        } else {
            showMessage("Press again to renew rhythm");
        }
        return true;
    case 3:
        if (shift) {
            auto *eng = stoEng();
            if (eng && eng->undoRenewMelody()) showMessage("Undo NewM");
            else                                showMessage("Nothing to undo");
            return true;
        }
        if (seq.melodyMode() == StochasticSourceMode::Live) {
            if (auto *eng = stoEng()) eng->captureLiveAsLoopMelody();
            showMessage("Live melody captured");
        } else if (pressCount == 2) {
            if (auto *eng = stoEng()) { eng->captureUndoMelody(); eng->renewMelody(); showMessage("Melody renewed"); }
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
    case 0: { // LoopR / LiveR — plain mode toggle.
        auto newMode = (seq.rhythmMode() == StochasticSourceMode::Loop)
            ? StochasticSourceMode::Live : StochasticSourceMode::Loop;
        seq.setRhythmMode(newMode);
        return true;
    }
    case 1: { // LoopM / LiveM — plain mode toggle.
        auto newMode = (seq.melodyMode() == StochasticSourceMode::Loop)
            ? StochasticSourceMode::Live : StochasticSourceMode::Loop;
        seq.setMelodyMode(newMode);
        return true;
    }
    case 2: // NewR — mode-aware (mirrors Live page logic).
        if (shift) {
            auto *eng = stoEng();
            if (eng && eng->undoRenewRhythm()) showMessage("Undo NewR");
            else                                showMessage("Nothing to undo");
            return true;
        }
        if (seq.rhythmMode() == StochasticSourceMode::Live) {
            if (auto *eng = stoEng()) eng->captureLiveAsLoopRhythm();
            showMessage("Live rhythm captured");
        } else if (pressCount == 2) {
            if (auto *eng = stoEng()) { eng->captureUndoRhythm(); eng->renewRhythm(); showMessage("Rhythm renewed"); }
        } else {
            showMessage("Press again to renew rhythm");
        }
        return true;
    case 3: // NewM — mode-aware.
        if (shift) {
            auto *eng = stoEng();
            if (eng && eng->undoRenewMelody()) showMessage("Undo NewM");
            else                                showMessage("Nothing to undo");
            return true;
        }
        if (seq.melodyMode() == StochasticSourceMode::Live) {
            if (auto *eng = stoEng()) eng->captureLiveAsLoopMelody();
            showMessage("Live melody captured");
        } else if (pressCount == 2) {
            if (auto *eng = stoEng()) { eng->captureUndoMelody(); eng->renewMelody(); showMessage("Melody renewed"); }
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

    // Quick-edit LED hint on the Live page only: visual step 15 (Tuesday-
    // style Random slot) glows green while PAGE is held.
    auto drawLiveQuickEditHint = [&]() {
        if (!globalKeyState()[Key::Page] || globalKeyState()[Key::Shift]) return;
        const int randIdx = MatrixMap::fromStep(14);   // PAGE+step15 (visual) = Random
        leds.unmask(randIdx); leds.set(randIdx, false, true); leds.mask(randIdx);
    };

    switch (_currentPage) {
    case Page::Live: {
        // Slot palette:
        //   red   = rhythm + articulation (DURA/VARI/REST/FEEL/GATE/LEGA + REPT)
        //   orange (red+green) = burst (BURS/BCNT/BRAT)
        //   green = pitch shape (CMPX/CONT/BIAS/SPRE/RANG/SLID)
        auto paintRed   = [&](int i) { leds.set(MatrixMap::fromStep(i), /*red*/true, /*green*/heldStep(i)); };
        auto paintGreen = [&](int i) { leds.set(MatrixMap::fromStep(i), /*red*/heldStep(i), /*green*/true); };
        auto paintOrange = [&](int i) {
            const bool held = heldStep(i);
            leds.set(MatrixMap::fromStep(i), /*red*/true, /*green*/!held);
        };
        // Top row: 0..5 rhythm/articulation (red), 6..7 burst (orange).
        for (int i = 0; i <= 5; ++i) paintRed(i);
        for (int i = 6; i <= 7; ++i) paintOrange(i);
        // Bottom row: 8..13 pitch shape (green), 14 repeat (red), 15 burst rate (orange).
        for (int i = 8; i <= 13; ++i) paintGreen(i);
        paintRed(14);
        paintOrange(15);
        drawLiveQuickEditHint();
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
        // Bottom row left (8..10): green — loop window (first, size, rotate)
        for (int i = 8; i <= 10; ++i) {
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
            // Hint Init/Even/Random slots (visual steps 13/14/15).
            for (int s = 12; s <= 14; ++s) {
                int index = MatrixMap::fromStep(s);
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
            leds.set(MatrixMap::fromStep(i), active || (i == _selectedDurEntry), true);
        }
        if (globalKeyState()[Key::Page] && !globalKeyState()[Key::Shift]) {
            for (int s = 12; s <= 14; ++s) {
                int index = MatrixMap::fromStep(s);
                leds.unmask(index);
                leds.set(index, false, true);
                leds.mask(index);
            }
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
        // Quick-edit: PAGE + visual step 15 → randomize Live-row knobs.
        // Tuesday-style slot (quickEdit() == 6 = step() == 14).
        if (key.isQuickEdit() && !key.shiftModifier()) {
            if (_currentPage == Page::Live && key.quickEdit() == 6) {
                contextAction(int(ContextAction::Random));
                event.consume();
                return;
            }
        }
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
        // PAGE + visual step 13/14/15 = Init / Even / Random. Random slot
        // matches Live page's quick-edit Random for cross-page consistency.
        switch (key.quickEdit()) {
        case 4: contextAction(int(ContextAction::Init)); break;
        case 5: contextAction(int(ContextAction::Even)); break;
        case 6: contextAction(int(ContextAction::Random)); break;
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
        case 1: // F2: NewM — mode-aware. Live = capture (single press),
                // Loop = renew (double-press guard). Shift = undo last renew.
            if (event.key().shiftModifier()) {
                if (auto *eng = stoEng()) {
                    if (eng->undoRenewMelody()) showMessage("Undo NewM");
                    else                        showMessage("Nothing to undo");
                }
            } else if (sequence.melodyMode() == StochasticSourceMode::Live) {
                if (auto *eng = stoEng()) eng->captureLiveAsLoopMelody();
                showMessage("Live melody captured");
            } else if (event.count() == 2) {
                if (auto *eng = stoEng()) { eng->captureUndoMelody(); eng->renewMelody(); showMessage("Melody renewed"); }
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
        // Same Init/Even/Random slot layout as Pitch page (steps 13/14/15).
        switch (key.quickEdit()) {
        case 4: contextAction(int(ContextAction::Init)); break;
        case 5: contextAction(int(ContextAction::Even)); break;
        case 6: contextAction(int(ContextAction::Random)); break;
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
        case 1: // F2: NewR — mode-aware (matches Pitch page NewM logic).
            if (event.key().shiftModifier()) {
                if (auto *eng = stoEng()) {
                    if (eng->undoRenewRhythm()) showMessage("Undo NewR");
                    else                        showMessage("Nothing to undo");
                }
            } else if (sequence.rhythmMode() == StochasticSourceMode::Live) {
                if (auto *eng = stoEng()) eng->captureLiveAsLoopRhythm();
                showMessage("Live rhythm captured");
            } else if (event.count() == 2) {
                if (auto *eng = stoEng()) { eng->captureUndoRhythm(); eng->renewRhythm(); showMessage("Rhythm renewed"); }
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
        // BurstHold cycle through the four combined pitch×timing modes:
        // HoldOver → RollOver → HoldFit → RollFit → back. Pitch axis: Hold
        // shares anchor pitch, Roll re-rolls per cell. Timing axis: Fit packs
        // cluster into one prev_dur with BurstRate curve, Over uses the
        // existing denom-pick uniform math (can overflow).
        if (ContextAction(index) == ContextAction::BurstHold) {
            auto cur = sequence.burstHold();
            StochasticBurstHold next;
            switch (cur) {
            case StochasticBurstHold::HoldOver: next = StochasticBurstHold::RollOver; break;
            case StochasticBurstHold::RollOver: next = StochasticBurstHold::HoldFit;  break;
            case StochasticBurstHold::HoldFit:  next = StochasticBurstHold::RollFit;  break;
            case StochasticBurstHold::RollFit:  next = StochasticBurstHold::HoldOver; break;
            default:                            next = StochasticBurstHold::HoldOver; break;
            }
            sequence.setBurstHold(next);
            notifyStochasticShapingEdit();
            const char *label = "BURST HOLD";
            switch (next) {
            case StochasticBurstHold::HoldOver: label = "HOLD / OVER"; break;
            case StochasticBurstHold::RollOver: label = "ROLL / OVER"; break;
            case StochasticBurstHold::HoldFit:  label = "HOLD / FIT";  break;
            case StochasticBurstHold::RollFit:  label = "ROLL / FIT";  break;
            default: break;
            }
            showMessage(label);
            break;
        }
        if (ContextAction(index) == ContextAction::Random) {
            // Tuesday-style page-owned Random. Selection-aware: empty mask
            // means all 16; held mask scopes the roll to picked slots.
            uint32_t mask = _heroSelectionMask;
            if (mask == 0) mask = 0xFFFFu;
            auto wants = [&](int i) { return (mask & (1U << i)) != 0; };
            static Random rng;
            // Slots match the LIVE layout (see editLiveStep comment).
            if (wants(0))  sequence.setNoteDuration(rng.nextRange(8));
            if (wants(1))  sequence.setVariation(rng.nextRange(101));
            if (wants(2))  sequence.setRest(rng.nextRange(50));
            if (wants(3))  sequence.setFeel(40 + rng.nextRange(21));
            if (wants(4))  sequence.setGateLength(rng.nextRange(101));
            if (wants(5))  sequence.setLegatoProb(rng.nextRange(60));
            if (wants(6))  sequence.setBurst(rng.nextRange(60));
            if (wants(7))  sequence.setBurstCount(rng.nextRange(101));
            if (wants(8))  sequence.setComplexity(rng.nextRange(101));
            if (wants(9))  sequence.setContour(int(rng.nextRange(101)) - 50);
            if (wants(10)) sequence.setMarblesBias(rng.nextRange(101));
            if (wants(11)) sequence.setMarblesSpread(40 + rng.nextRange(61));
            if (wants(12)) sequence.setRange(20 + rng.nextRange(61));
            if (wants(13)) sequence.setSlide(rng.nextRange(60));
            if (wants(14)) sequence.setRepeatProb(rng.nextRange(50));
            if (wants(15)) sequence.setBurstRate(rng.nextRange(101));
            notifyStochasticShapingEdit();
            showMessage(_heroSelectionMask ? "RAND SELECTED" : "RAND LIVE");
            break;
        }
        // INIT resets the LIVE-page params bound to step buttons back to
        // their model defaults. Selection-aware: empty mask hits all 16.
        // EVEN is not meaningful for the mixed-type slots — short reminder.
        if (ContextAction(index) != ContextAction::Init) {
            showMessage("INIT ONLY ON LIVE");
            break;
        }
        uint32_t mask = _heroSelectionMask;
        if (mask == 0) mask = 0xFFFFu;
        auto wants = [&](int i) { return (mask & (1U << i)) != 0; };
        if (wants(0))  sequence.setNoteDuration(5);
        if (wants(1))  sequence.setVariation(16);
        if (wants(2))  sequence.setRest(0);
        if (wants(3))  sequence.setFeel(50);
        if (wants(4))  sequence.setGateLength(0);
        if (wants(5))  sequence.setLegatoProb(0);
        if (wants(6))  sequence.setBurst(0);
        if (wants(7))  sequence.setBurstCount(0);
        if (wants(8))  sequence.setComplexity(50);
        if (wants(9))  sequence.setContour(0);
        if (wants(10)) sequence.setMarblesBias(50);
        if (wants(11)) sequence.setMarblesSpread(50);
        if (wants(12)) sequence.setRange(50);
        if (wants(13)) sequence.setSlide(0);
        if (wants(14)) sequence.setRepeatProb(0);
        if (wants(15)) sequence.setBurstRate(50);
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
        if (mask == 0) mask = 0xC1CFu;  // bits 0-3, 6-7, 8, 14-15 (same defaults as before — Size/Rotate/Sleep stay opt-in)
        auto wants = [&](int i) { return (mask & (1U << i)) != 0; };
        if (wants(0))  sequence.setPatienceRhythm(100);
        if (wants(1))  sequence.setPatienceMelody(100);
        if (wants(2))  sequence.setMutate(0);
        if (wants(3))  sequence.setJump(0);
        if (wants(4))  sequence.setSleep(0);
        bool windowEdited = false;
        if (wants(8))  { sequence.setFirst(0);  windowEdited = true; }
        if (wants(9))  { sequence.setSize(16);  windowEdited = true; }
        if (wants(10)) sequence.setRotate(0);   // rotation doesn't move bounds
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
