#include "OverviewPage.h"

#include "model/NoteTrack.h"
#include "model/TuesdayTrack.h"
#include "model/TuesdaySequence.h"
#include "engine/TuesdayTrackEngine.h"
#include "model/IndexedTrack.h"
#include "engine/IndexedTrackEngine.h"
#include "model/DiscreteMapTrack.h"
#include "engine/DiscreteMapTrackEngine.h"

#include "ui/painters/WindowPainter.h"

static void drawNoteTrack(Canvas &canvas, int trackIndex, const NoteTrackEngine &trackEngine, const NoteSequence &sequence) {
    canvas.setBlendMode(BlendMode::Set);

    int stepOffset = (std::max(0, trackEngine.currentStep()) / 16) * 16;
    int y = trackIndex * 8;

    for (int i = 0; i < 16; ++i) {
        int stepIndex = stepOffset + i;
        const auto &step = sequence.step(stepIndex);

        int x = 64 + i * 8;

        if (trackEngine.currentStep() == stepIndex) {
            canvas.setColor(step.gate() ? Color::Bright : Color::MediumBright);
            canvas.fillRect(x + 1, y + 1, 6, 6);
        } else {
            canvas.setColor(step.gate() ? Color::Medium : Color::Low);
            canvas.fillRect(x + 1, y + 1, 6, 6);
        }

        // if (trackEngine.currentStep() == stepIndex) {
        //     canvas.setColor(Color::Bright);
        //     canvas.drawRect(x + 1, y + 1, 6, 6);
        // }
    }
}

static void drawCurve(Canvas &canvas, int x, int y, int w, int h, float &lastY, const Curve::Function function, float min, float max) {
    const int Step = 1;

    auto eval = [=] (float x) {
        return (1.f - (function(x) * (max - min) + min)) * h;
    };

    float fy0 = y + eval(0.f);

    if (lastY >= 0.f && lastY != fy0) {
        canvas.line(x, lastY, x, fy0);
    }

    for (int i = 0; i < w; i += Step) {
        float fy1 = y + eval((float(i) + Step) / w);
        canvas.line(x + i, fy0, x + i + Step, fy1);
        fy0 = fy1;
    }

    lastY = fy0;
}

static void drawCurveTrack(Canvas &canvas, int trackIndex, const CurveTrackEngine &trackEngine, const CurveTrack &curveTrack, const CurveSequence &sequence) {
    canvas.setBlendMode(BlendMode::Add);
    canvas.setColor(Color::MediumBright);

    int stepOffset = (std::max(0, trackEngine.currentStep()) / 16) * 16;
    int y = trackIndex * 8;

    float lastY = -1.f;

    for (int i = 0; i < 16; ++i) {
        int stepIndex = stepOffset + i;
        const auto &step = sequence.step(stepIndex);
        float min = step.minNormalized();
        float max = step.maxNormalized();
        const auto function = Curve::function(Curve::Type(std::min(Curve::Last - 1, step.shape())));

        int x = 64 + i * 8;

        drawCurve(canvas, x, y + 1, 8, 6, lastY, function, min, max);
    }

    if (curveTrack.globalPhase() > 0.f) {
        // draw only phased cursor
        if (trackEngine.phasedStep() >= 0) {
            int x = 64 + ((trackEngine.phasedStep() - stepOffset) + trackEngine.phasedStepFraction()) * 8;
            canvas.setBlendMode(BlendMode::Set);
            canvas.setColor(Color::Medium);
            canvas.vline(x, y + 1, 7);
        }
    } else {
        // draw normal cursor
        if (trackEngine.currentStep() >= 0) {
            int x = 64 + ((trackEngine.currentStep() - stepOffset) + trackEngine.currentStepFraction()) * 8;
            canvas.setBlendMode(BlendMode::Set);
            canvas.setColor(Color::Bright);
            canvas.vline(x, y + 1, 7);
        }
    }
}

static void drawTuesdayTrack(Canvas &canvas, int trackIndex, const TuesdayTrackEngine &trackEngine, const TuesdaySequence &tuesdaySequence) {
    canvas.setBlendMode(BlendMode::Set);

    int y = trackIndex * 8;
    int currentStep = trackEngine.currentStep();
    int loopLength = tuesdaySequence.actualLoopLength();
    int currentStepForDisplay = currentStep;
    // Wrap current step to actual loop length if needed
    if (loopLength > 0) {
        currentStepForDisplay = currentStep % loopLength;
    }

    // Gate indicator square (blinking when active)
    bool gateActive = trackEngine.gateOutput(0);
    canvas.setColor(gateActive ? Color::Bright : Color::Low);
    canvas.fillRect(64 + 1, y + 1, 6, 6);

    // Step counter
    canvas.setColor(Color::Medium);
    if (currentStep >= 0) {
        if (loopLength > 0) {
            // Show step/loop format for finite loops
            canvas.drawText(64 + 12, y + 5, FixedStringBuilder<16>("%d/%d", currentStepForDisplay + 1, loopLength));
        } else {
            // Just show step for infinite loops
            canvas.drawText(64 + 12, y + 5, FixedStringBuilder<16>("%d", currentStep + 1));
        }
    }

    // Algorithm name (right-aligned)
    const char *algoName = "";
    switch (tuesdaySequence.algorithm()) {
    case 0:  algoName = "Test"; break;
    case 1:  algoName = "TriTr"; break;
    case 2:  algoName = "Stomp"; break;
    case 3:  algoName = "Marko"; break;
    case 4:  algoName = "Chip1"; break;
    case 5:  algoName = "Chip2"; break;
    case 6:  algoName = "Wobbl"; break;
    case 7:  algoName = "SclWk"; break;
    case 8:  algoName = "Wndow"; break;
    case 9:  algoName = "Minml"; break;
    case 10: algoName = "Ganz"; break;
    case 11: algoName = "Blake"; break;
    case 12: algoName = "Aphex"; break;
    case 13: algoName = "Autec"; break;
    case 14: algoName = "StpWv"; break;
    }

    if (algoName[0] != '\0') {
        canvas.setColor(Color::Medium);
        int textWidth = canvas.textWidth(algoName);
        canvas.drawText(192 - textWidth, y + 5, algoName);
    }
}

static void drawIndexedTrack(Canvas &canvas, int trackIndex, const IndexedTrackEngine &trackEngine, const IndexedSequence &sequence) {
    // Adaptation from IndexedSequenceEditPage timeline bar (lines 66-121)
    canvas.setBlendMode(BlendMode::Set);

    const int y = trackIndex * 8;
    const int barX = 64;
    const int barW = 128;
    const int barH = 7;
    const int minStepW = 3;

    int activeLength = sequence.activeLength();

    // Calculate total ticks in active sequence
    int totalTicks = 0;
    int nonzeroSteps = 0;
    for (int i = 0; i < activeLength; ++i) {
        int duration = sequence.step(i).duration();
        totalTicks += duration;
        if (duration > 0) {
            nonzeroSteps++;
        }
    }

    if (totalTicks > 0 && nonzeroSteps > 0) {
        int currentX = barX;
        int extraPixels = barW - minStepW * nonzeroSteps;
        if (extraPixels < 0) {
            extraPixels = 0;
        }
        int error = 0;

        for (int i = 0; i < activeLength; ++i) {
            const auto &step = sequence.step(i);
            int stepW = 0;
            if (step.duration() > 0) {
                int scaled = extraPixels * step.duration() + error;
                int extraW = scaled / totalTicks;
                error = scaled % totalTicks;
                stepW = minStepW + extraW;
            }

            bool active = (trackEngine.currentStep() == i);

            // Draw step rectangle
            canvas.setColor(active ? Color::Bright : Color::Medium);
            canvas.drawRect(currentX, y, stepW, barH);

            // Draw gate length as filled portion from bottom
            int gateW = 0;
            if (step.gateLength() == IndexedSequence::GateLengthTrigger) {
                gateW = std::min(1, stepW - 2);
            } else {
                gateW = (int)(stepW * (step.gateLength() / 100.0f));
            }
            if (gateW > 0 && stepW > 2) {
                canvas.setColor(active ? Color::MediumBright : Color::Low);
                canvas.fillRect(currentX + 1, y + 1, gateW - 1, barH - 2);
            }

            currentX += stepW;
            if (currentX >= barX + barW) break;
        }
    }
}

static void drawDiscreteMapTrack(Canvas &canvas, int trackIndex, const DiscreteMapTrackEngine &trackEngine, const DiscreteMapSequence &sequence) {
    // Adaptation from DiscreteMapSequencePage threshold bar (lines 96-137)
    canvas.setBlendMode(BlendMode::Set);

    const int y = trackIndex * 8;
    const int barX = 64;
    const int barW = 128;
    const int barLineY = y + 6; // Baseline position (near bottom)

    // Get range from sequence for normalization
    float rangeMin = sequence.rangeLow();
    float rangeMax = sequence.rangeHigh();
    if (std::abs(rangeMax - rangeMin) < 0.001f) {
        rangeMin = -5.0f;
        rangeMax = 5.0f;
    }

    // Draw baseline (2px thick)
    canvas.setColor(Color::Low);
    canvas.hline(barX, barLineY, barW);

    // Draw threshold markers for all 32 stages
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        const auto &stage = sequence.stage(i);
        if (stage.direction() == DiscreteMapSequence::Stage::TriggerDir::Off) {
            continue;
        }

        // Normalize threshold (-100 to +100) to 0-1
        float norm = (stage.threshold() + 100.0f) / 200.0f;
        norm = clamp(norm, 0.f, 1.f);
        int x = barX + int(norm * barW);

        bool active = trackEngine.activeStage() == i;

        // Marker height: active=5px, normal=3px
        int markerHeight = active ? 5 : 3;

        canvas.setColor(active ? Color::Bright : Color::Medium);
        canvas.vline(x, barLineY - markerHeight, markerHeight); // Grow upward from baseline
    }

    // Draw input cursor
    float inputNorm = (trackEngine.currentInput() - rangeMin) / (rangeMax - rangeMin);
    inputNorm = clamp(inputNorm, 0.f, 1.f);
    int cursorX = barX + int(inputNorm * barW);

    if (cursorX >= barX && cursorX < barX + barW) {
        canvas.setColor(Color::Bright);
        canvas.vline(cursorX, y, 7); // Full height cursor
    }
}


OverviewPage::OverviewPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void OverviewPage::enter() {
}

void OverviewPage::exit() {
}

void OverviewPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);

    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(Color::Medium);

    canvas.vline(64 - 3, 0, 64);
    canvas.vline(64 - 2, 0, 64);
    canvas.vline(192 + 1, 0, 64);
    canvas.vline(192 + 2, 0, 64);

    for (int trackIndex = 0; trackIndex < 8; trackIndex++) {
        const auto &track = _project.track(trackIndex);
        const auto &trackState = _project.playState().trackState(trackIndex);
        const auto &trackEngine = _engine.trackEngine(trackIndex);

        canvas.setBlendMode(BlendMode::Set);
        canvas.setColor(Color::Medium);

        int y = 5 + trackIndex * 8;

        // track number / pattern number
        canvas.setColor(trackState.mute() ? Color::Medium : Color::Bright);
        canvas.drawText(2, y, FixedStringBuilder<8>("T%d", trackIndex + 1));
        canvas.drawText(18, y, FixedStringBuilder<8>("P%d", trackState.pattern() + 1));

        // gate output
        bool gate = _engine.gateOutput() & (1 << trackIndex);
        canvas.setColor(gate ? Color::Bright : Color::Medium);
        canvas.fillRect(256 - 48 + 1, trackIndex * 8 + 1, 6, 6);

        // cv output
        canvas.setColor(Color::Bright);
        canvas.drawText(256 - 32, y, FixedStringBuilder<8>("%.2fV", _engine.cvOutput().channel(trackIndex)));

        switch (track.trackMode()) {
        case Track::TrackMode::Note:
            drawNoteTrack(canvas, trackIndex, trackEngine.as<NoteTrackEngine>(), track.noteTrack().sequence(trackState.pattern()));
            break;
        case Track::TrackMode::Curve:
            drawCurveTrack(canvas, trackIndex, trackEngine.as<CurveTrackEngine>(), track.curveTrack(), track.curveTrack().sequence(trackState.pattern()));
            break;
        case Track::TrackMode::MidiCv:
            break;
        case Track::TrackMode::Tuesday:
            drawTuesdayTrack(canvas, trackIndex, trackEngine.as<TuesdayTrackEngine>(), track.tuesdayTrack().sequence(trackState.pattern()));
            break;
        case Track::TrackMode::DiscreteMap:
            drawDiscreteMapTrack(canvas, trackIndex, trackEngine.as<DiscreteMapTrackEngine>(), track.discreteMapTrack().sequence(trackState.pattern()));
            break;
        case Track::TrackMode::Indexed:
            drawIndexedTrack(canvas, trackIndex, trackEngine.as<IndexedTrackEngine>(), track.indexedTrack().sequence(trackState.pattern()));
            break;
        case Track::TrackMode::Last:
            break;
        }
    }
}

void OverviewPage::updateLeds(Leds &leds) {
}

void OverviewPage::keyDown(KeyEvent &event) {
    const auto &key = event.key();

    if (key.isGlobal()) {
        return;
    }

    // event.consume();
}

void OverviewPage::keyUp(KeyEvent &event) {
    const auto &key = event.key();

    if (key.isGlobal()) {
        return;
    }

    // event.consume();
}

void OverviewPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isGlobal()) {
        return;
    }

    // event.consume();
}

void OverviewPage::encoder(EncoderEvent &event) {
    // event.consume();
}
