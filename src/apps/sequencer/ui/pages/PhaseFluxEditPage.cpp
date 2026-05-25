#include "PhaseFluxEditPage.h"

#include "Pages.h"

#include "ui/painters/WindowPainter.h"

#include "model/Curve.h"
#include "model/PhaseFluxMath.h"

#include "core/math/Math.h"

#include <algorithm>
#include <cmath>

namespace {

// Grid layout — matches Python preview spec
static constexpr int GridX     = 2;
static constexpr int GridY     = 12;
static constexpr int CellW     = 9;
static constexpr int CellH     = 9;
static constexpr int CellGap   = 1;
static constexpr int DividerX  = 46;

// Right pane scopes
static constexpr int ScopeTempX  = 50;
static constexpr int ScopePitchX = 154;
static constexpr int ScopeW      = 100;
static constexpr int ScopeH      = 38;
static constexpr int ScopeY      = 12;
static constexpr int DividerX2   = 152;

// Mask table — bit set = muted (LSB = pulse 0)
constexpr uint8_t kMaskTable[8] = { 0x00, 0xAA, 0x49, 0x11, 0xB6, 0x55, 0x77, 0x01 };

inline Curve::Type temporalCurveLut(PhaseFluxSequence::TemporalCurveType v) {
    switch (v) {
    case PhaseFluxSequence::TemporalCurveType::Linear: return Curve::RampUp;
    case PhaseFluxSequence::TemporalCurveType::Bell:   return Curve::Bell;
    case PhaseFluxSequence::TemporalCurveType::Bounce: return Curve::ExpDown3x;
    }
    return Curve::RampUp;
}

inline Curve::Type pitchCurveLut(PhaseFluxSequence::PitchCurveType v) {
    switch (v) {
    case PhaseFluxSequence::PitchCurveType::Ramp:     return Curve::RampUp;
    case PhaseFluxSequence::PitchCurveType::Bell:     return Curve::Bell;
    case PhaseFluxSequence::PitchCurveType::Triangle: return Curve::Triangle;
    }
    return Curve::RampUp;
}

inline float clamp01(float x) { return std::max(0.f, std::min(1.f, x)); }

inline float evalTemporal(const PhaseFluxSequence::Stage &stage, float t_raw) {
    float t = PhaseFluxMath::powerBend(clamp01(t_raw), PhaseFluxMath::powerBendKnobToParam(stage.temporalWarp()));
    if (stage.temporalFlipH()) t = 1.f - t;
    t = Curve::eval(temporalCurveLut(stage.temporalCurve()), t);
    if (stage.temporalFlipV()) t = 1.f - t;
    return PhaseFluxMath::powerBend(clamp01(t), PhaseFluxMath::powerBendKnobToParam(stage.temporalResponse()));
}

inline float evalPitch(const PhaseFluxSequence::Stage &stage, float phi) {
    float p = PhaseFluxMath::powerBend(clamp01(phi), PhaseFluxMath::powerBendKnobToParam(stage.pitchWarp()));
    if (stage.pitchFlipH()) p = 1.f - p;
    p = Curve::eval(pitchCurveLut(stage.pitchCurve()), p);
    if (stage.pitchFlipV()) p = 1.f - p;
    return PhaseFluxMath::powerBend(clamp01(p), PhaseFluxMath::powerBendKnobToParam(stage.pitchResponse()));
}

} // namespace

PhaseFluxEditPage::PhaseFluxEditPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void PhaseFluxEditPage::enter() {
    _selectedCell = 0;
}

void PhaseFluxEditPage::exit() {
}

void PhaseFluxEditPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "PHFLX");
    WindowPainter::drawActiveFunction(canvas, "GRID");
    WindowPainter::drawFooter(canvas);

    drawGrid(canvas);

    // Vertical dividers — safe area y=11..52
    canvas.setColor(Color::Low);
    canvas.vline(DividerX,  13, 39);
    canvas.vline(DividerX2, 13, 39);

    drawScopes(canvas);
}

void PhaseFluxEditPage::updateLeds(Leds &leds) {
    (void)leds;
}

void PhaseFluxEditPage::keyDown(KeyEvent &event) {
    event.consume();
}

void PhaseFluxEditPage::keyUp(KeyEvent &event) {
    event.consume();
}

void PhaseFluxEditPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isStep()) {
        _selectedCell = key.step();
        event.consume();
        return;
    }

    event.consume();
}

void PhaseFluxEditPage::encoder(EncoderEvent &event) {
    event.consume();
}

void PhaseFluxEditPage::drawGrid(Canvas &canvas) {
    const auto *te = trackEngine();
    int activeCell = te ? te->activeCell() : -1;

    const PhaseFluxSequence &seq = _project.selectedPhaseFluxSequence();

    for (int i = 0; i < 16; ++i) {
        bool isActive   = (i == activeCell);
        bool isSelected = (i == _selectedCell);
        drawGridCell(canvas, i, isActive, isSelected);

        const auto &stage = seq.stage(i);
        if (stage.skip()) continue;

        int row = i / 4;
        int col = i % 4;
        int x = GridX + col * (CellW + CellGap);
        int y = GridY + row * (CellH + CellGap);

        int pulses = std::max(1, std::min(8, stage.pulseCount()));
        Color barColor = isActive ? Color::None : (isSelected ? Color::Bright : Color::MediumBright);
        canvas.setColor(barColor);
        canvas.hline(x + 1, y + CellH - 2, std::min(CellW - 2, pulses));

        if (!isActive) {
            if (stage.phaseShift() != 0) {
                canvas.setColor(barColor);
                canvas.point(x + 1, y + 1);
            }
            if (int(stage.mask()) != 0) {
                canvas.setColor(barColor);
                canvas.point(x + CellW - 2, y + 1);
            }
        }
    }
}

void PhaseFluxEditPage::drawGridCell(Canvas &canvas, int idx, bool isActive, bool isSelected) {
    const PhaseFluxSequence &seq = _project.selectedPhaseFluxSequence();
    const auto &stage = seq.stage(idx);

    int row = idx / 4;
    int col = idx % 4;
    int x = GridX + col * (CellW + CellGap);
    int y = GridY + row * (CellH + CellGap);

    if (isActive) {
        canvas.setColor(Color::Bright);
        canvas.fillRect(x, y, CellW, CellH);
    } else if (stage.skip()) {
        canvas.setColor(Color::Low);
        canvas.drawRect(x, y, CellW, CellH);
        canvas.line(x + 2, y + 2, x + CellW - 3, y + CellH - 3);
        canvas.line(x + CellW - 3, y + 2, x + 2, y + CellH - 3);
    } else {
        canvas.setColor(isSelected ? Color::Bright : Color::MediumLow);
        canvas.drawRect(x, y, CellW, CellH);
    }
}

void PhaseFluxEditPage::drawScopes(Canvas &canvas) {
    drawTemporalScope(canvas, _selectedCell);
    drawPitchScope(canvas, _selectedCell);
}

void PhaseFluxEditPage::drawTemporalScope(Canvas &canvas, int stageIdx) {
    const PhaseFluxSequence &seq = _project.selectedPhaseFluxSequence();
    const auto &stage = seq.stage(stageIdx);
    if (stage.skip()) return;

    int x = ScopeTempX;
    int y = ScopeY;
    canvas.setColor(Color::Low);
    canvas.drawRect(x, y, ScopeW, ScopeH);
    canvas.hline(x + 1, y + ScopeH - 2, ScopeW - 2);

    // Temporal density curve trace
    canvas.setColor(Color::Bright);
    int prevPy = -1;
    for (int px = 0; px < ScopeW - 2; ++px) {
        float t_raw = float(px) / float(ScopeW - 3);
        float t_final = evalTemporal(stage, t_raw);
        int py = y + ScopeH - 2 - int(t_final * float(ScopeH - 3));
        if (prevPy >= 0) canvas.line(x + px, prevPy, x + px + 1, py);
        prevPy = py;
    }

    // Pulse tick marks
    int pulses = std::max(1, std::min(8, stage.pulseCount()));
    int maskByte = kMaskTable[int(stage.mask())];
    int maskShift = stage.maskShift();
    for (int i = 0; i < pulses; ++i) {
        float t_raw = (pulses == 1) ? 0.5f : (float(i) / float(pulses - 1));
        float t_final = evalTemporal(stage, t_raw);
        float t_shifted = t_final + float(stage.phaseShift()) * 0.125f;
        t_shifted = std::fmod(t_shifted, 1.f);
        if (t_shifted < 0.f) t_shifted += 1.f;
        bool muted = (maskByte >> ((i + maskShift) & 7)) & 1;
        int px = x + 1 + int(t_shifted * float(ScopeW - 3));
        if (muted) {
            canvas.setColor(Color::Low);
            canvas.point(px, y + ScopeH - 1);
        } else {
            canvas.setColor(Color::Bright);
            canvas.vline(px, y + ScopeH - 4, 3);
        }
    }
}

void PhaseFluxEditPage::drawPitchScope(Canvas &canvas, int stageIdx) {
    const PhaseFluxSequence &seq = _project.selectedPhaseFluxSequence();
    const auto &stage = seq.stage(stageIdx);
    if (stage.skip()) return;

    int x = ScopePitchX;
    int y = ScopeY;
    canvas.setColor(Color::Low);
    canvas.drawRect(x, y, ScopeW, ScopeH);
    canvas.hline(x + 1, y + ScopeH - 2, ScopeW - 2);

    // Pitch curve trace
    canvas.setColor(Color::Bright);
    int prevPy = -1;
    for (int px = 0; px < ScopeW - 2; ++px) {
        float phi = float(px) / float(ScopeW - 3);
        float p_final = evalPitch(stage, phi);
        int py = y + ScopeH - 2 - int(p_final * float(ScopeH - 3));
        if (prevPy >= 0) canvas.line(x + px, prevPy, x + px + 1, py);
        prevPy = py;
    }

    // Pulse dots at fire times
    int pulses = std::max(1, std::min(8, stage.pulseCount()));
    int maskByte = kMaskTable[int(stage.mask())];
    int maskShift = stage.maskShift();
    for (int i = 0; i < pulses; ++i) {
        float t_raw = (pulses == 1) ? 0.5f : (float(i) / float(pulses - 1));
        float t_final = evalTemporal(stage, t_raw);
        float t_shifted = t_final + float(stage.phaseShift()) * 0.125f;
        t_shifted = std::fmod(t_shifted, 1.f);
        if (t_shifted < 0.f) t_shifted += 1.f;
        bool muted = (maskByte >> ((i + maskShift) & 7)) & 1;
        if (muted) continue;
        float p_final = evalPitch(stage, t_shifted);
        int px = x + 1 + int(t_shifted * float(ScopeW - 3));
        int py = y + ScopeH - 2 - int(p_final * float(ScopeH - 3));
        canvas.setColor(Color::Bright);
        canvas.fillRect(px - 1, py - 1, 2, 2);
    }
}

const PhaseFluxTrackEngine *PhaseFluxEditPage::trackEngine() const {
    const auto &te = _engine.trackEngine(_project.selectedTrackIndex());
    if (te.trackMode() == Track::TrackMode::PhaseFlux) {
        return static_cast<const PhaseFluxTrackEngine *>(&te);
    }
    return nullptr;
}

const PhaseFluxTrack &PhaseFluxEditPage::phaseFluxTrack() const {
    return _project.selectedTrack().phaseFluxTrack();
}
