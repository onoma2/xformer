#include "PhaseFluxEditPage.h"

#include "Pages.h"

#include "ui/painters/WindowPainter.h"
#include "ui/MatrixMap.h"

#include "model/Curve.h"
#include "model/ModelUtils.h"
#include "model/PhaseFluxMath.h"
#include "model/Scale.h"

#include "core/math/Math.h"
#include "core/utils/Random.h"
#include "core/utils/StringBuilder.h"

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

// Right pane: single scope (left half) + 5-row param list (right half).
static constexpr int ScopeTempX   = 50;
static constexpr int ScopeW       = 100;
static constexpr int ScopeH       = 38;
static constexpr int ScopeY       = 12;
static constexpr int DividerX2    = 152;
static constexpr int ParamPanelX  = 156;
static constexpr int ParamPanelY  = 12;
static constexpr int ParamPanelW  = 256 - ParamPanelX - 2;
static constexpr int ParamPanelH  = 40;
static constexpr int ParamRowH    = ParamPanelH / 5;

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
    case PhaseFluxSequence::PitchCurveType::Bounce:   return Curve::ExpDown3x;
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

    // Header: TEMP / PTCH / PTCH.G / ACCUM.N[T] / ACCUM.P[T].
    const bool isPtch = (_currentSet == 1);
    const bool isAccumN = (_currentSet == 2);
    const bool isAccumP = (_currentSet == 3);
    const auto &seqForHeader = _project.selectedPhaseFluxSequence();
    const bool isGlobalPitch =
        isPtch && seqForHeader.pitchMode() == PhaseFluxSequence::PitchMode::Global;
    const char *setName = "TEMP";
    if (_currentSet == 1) {
        setName = isGlobalPitch ? "PTCH.G" : "PTCH";
    } else if (isAccumN) {
        setName = (seqForHeader.noteAccumConfig().scope() == AccumulatorConfig::Scope::Track)
            ? "ACCUM.NT" : "ACCUM.N";
    } else if (isAccumP) {
        setName = (seqForHeader.pulseAccumConfig().scope() == AccumulatorConfig::Scope::Track)
            ? "ACCUM.PT" : "ACCUM.P";
    }
    WindowPainter::drawActiveFunction(canvas, setName);

    // Footer: 5 labels, swap to shift variants when Shift is held.
    const bool shift = globalKeyState()[Key::Shift];
    static const char *kLabelsTemp[5]      = { "Curve", "Warp",  "Resp", "Len",  "Puls" };
    static const char *kLabelsTempShift[5] = { "FlipV", "FlipH", nullptr, nullptr, nullptr };
    static const char *kLabelsPtch[5]        = { "Curve", "Warp",  "Resp", "Base", "Span" };
    static const char *kLabelsPtchShift[5]   = { "FlipV", "FlipH", nullptr, nullptr, nullptr };
    static const char *kLabelsPtchGlobal[5]      = { "Curve", "Warp",  "Resp", "Rate", "Note" };
    static const char *kLabelsPtchGlobalShift[5] = { "FlipV", "FlipH", nullptr, nullptr, "Span" };
    static const char *kLabelsAccum[5]       = { "Amount", "+Lim", "-Lim", "Order", "Reset" };
    static const char *kLabelsAccumShift[5]  = { "Trig",   nullptr, nullptr, "Polar", "Scope" };

    const char *(*primary)[5];
    const char *(*altShift)[5];
    if (_currentSet == 0) {
        primary = &kLabelsTemp;
        altShift = &kLabelsTempShift;
    } else if (_currentSet == 1 && isGlobalPitch) {
        primary = &kLabelsPtchGlobal;
        altShift = &kLabelsPtchGlobalShift;
    } else if (_currentSet == 1) {
        primary = &kLabelsPtch;
        altShift = &kLabelsPtchShift;
    } else {
        primary = &kLabelsAccum;
        altShift = &kLabelsAccumShift;
    }
    const char *footer[5];
    for (int i = 0; i < 5; ++i) {
        footer[i] = (shift && (*altShift)[i]) ? (*altShift)[i] : (*primary)[i];
    }
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), _selectedSlot);

    drawGrid(canvas);

    // Vertical dividers — safe area y=11..52
    canvas.setColor(Color::Low);
    canvas.vline(DividerX,  13, 39);
    canvas.vline(DividerX2, 13, 39);

    // Middle pane dispatch: TEMP→scope, ACCUM→dual strip, else→pitch scope.
    if (_currentSet == 0) {
        drawTemporalScope(canvas, _selectedCell, ScopeTempX);
    } else if (isAccumN || isAccumP) {
        drawAccumDualStrip(canvas, ScopeTempX, _currentSet);
    } else {
        drawPitchScope(canvas, isGlobalPitch ? 0 : _selectedCell, ScopeTempX);
    }
    drawStageBadge(canvas, ScopeTempX);
    drawParamList(canvas);
}

void PhaseFluxEditPage::updateLeds(Leds &leds) {
    // Step matrix LEDs — port of NoteSequenceEditPage::updateLeds pattern.
    //   red   = playhead (current cell) OR edit cursor (selected cell)
    //   green = cell will fire (not skipped AND stageLen > 0)
    // Overlap → yellow (R+G both on).
    const auto *te = trackEngine();
    int currentCell = te ? te->activeCell() : -1;
    const auto &seq = _project.selectedPhaseFluxSequence();
    for (int i = 0; i < 16; ++i) {
        bool red   = (i == currentCell) || (i == _selectedCell);
        bool green = !seq.stage(i).skip() && seq.stage(i).stageLen() > 0;
        leds.set(MatrixMap::fromStep(i), red, green);
    }

    // Empty quick-edit overlay — when Page modifier is held, the bottom row
    // (S8..S15) is reserved for future quick-edit slots. No slots wired yet,
    // so mask the row dark to signal "ready but empty".
    if (globalKeyState()[Key::Page] && !globalKeyState()[Key::Shift]) {
        for (int i = 0; i < 8; ++i) {
            int index = MatrixMap::fromStep(i + 8);
            leds.unmask(index);
            leds.set(index, false, false);
            leds.mask(index);
        }
    }
}

void PhaseFluxEditPage::keyDown(KeyEvent &event) {
    (void)event;
}

void PhaseFluxEditPage::keyUp(KeyEvent &event) {
    (void)event;
}

void PhaseFluxEditPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    // Quick-edit overlay (Page + bottom-row step S8..S15). Set-aware actions.
    if (key.isQuickEdit() && !key.shiftModifier()) {
        switch (key.quickEdit()) {
        case 6: randomizeCurrentSet(); break;   // Page+S15 — randomize current set
        // case 4: Init (later)
        // case 5: Even (later)
        // case 7: spare
        }
        event.consume();
        return;
    }

    // Page+key combos belong to TopPage (Page+S0/S1/S2 etc.). Don't consume.
    if (key.pageModifier()) {
        return;
    }

    if (key.isStep()) {
        if (key.shiftModifier()) {
            auto &stage = _project.selectedPhaseFluxSequence().stage(key.step());
            stage.setSkip(!stage.skip());
        } else {
            _selectedCell = key.step();
        }
        event.consume();
        return;
    }

    // Left/Right pages between sets.
    if (key.isLeft()) {
        _currentSet = (_currentSet + kSetCount - 1) % kSetCount;
        event.consume();
        return;
    }
    if (key.isRight()) {
        _currentSet = (_currentSet + 1) % kSetCount;
        event.consume();
        return;
    }

    // F1..F5 select slot. Shift+F toggles the binary at that slot if any.
    if (key.isFunction()) {
        int slot = key.function();
        if (globalKeyState()[Key::Shift]) {
            toggleShiftAt(slot);
        } else {
            _selectedSlot = slot;
        }
        event.consume();
        return;
    }
}

void PhaseFluxEditPage::encoder(EncoderEvent &event) {
    editSlot(_selectedSlot, event.value(), event.pressed() || globalKeyState()[Key::Shift]);
    event.consume();
}

void PhaseFluxEditPage::editSlot(int slot, int value, bool shift) {
    auto &seq = _project.selectedPhaseFluxSequence();
    auto &activeStage = seq.stage(_selectedCell);
    auto &masterStage = seq.stage(0);
    auto cycle = [](int v, int lo, int hi) { return clamp(v, lo, hi); };
    const bool isGlobalPitch =
        _currentSet == 1 &&
        _project.selectedPhaseFluxSequence().pitchMode() == PhaseFluxSequence::PitchMode::Global;
    const bool isAccumN = (_currentSet == 2);
    const bool isAccumP = (_currentSet == 3);
    if (isAccumN || isAccumP) {
        auto &cfg = isAccumN ? seq.noteAccumConfig() : seq.pulseAccumConfig();
        const int limMax = isAccumN ? 28 : 8;
        switch (slot) {
        case 0:
            if (isAccumN) {
                activeStage.setAccumulatorStep(activeStage.accumulatorStep() + value);
            } else {
                activeStage.setPulseAccumStep(activeStage.pulseAccumStep() + value);
            }
            break;
        case 1: cfg.setPosLim(clamp(int(cfg.posLim()) + value, 1, limMax)); break;
        case 2: cfg.setNegLim(clamp(int(cfg.negLim()) + value, 1, limMax)); break;
        case 3: cfg.setOrder(AccumulatorConfig::Order(cycle(int(cfg.order()) + value, 0, 3))); break;
        case 4: cfg.setReset(uint8_t(cycle(int(cfg.reset()) + value, 0, 15))); break;
        }
        (void)shift;
        return;
    }
    if (_currentSet == 0) {
        // TEMP
        switch (slot) {
        case 0: activeStage.setTemporalCurve(PhaseFluxSequence::TemporalCurveType(cycle(int(activeStage.temporalCurve()) + value, 0, 2))); break;
        case 1: activeStage.setTemporalWarp(ModelUtils::adjusted(activeStage.temporalWarp(), value, -64, 64)); break;
        case 2: activeStage.setTemporalResponse(ModelUtils::adjusted(activeStage.temporalResponse(), value, -64, 64)); break;
        case 3: activeStage.setStageLen(ModelUtils::adjusted(activeStage.stageLen(), value, 0, 127)); break;
        case 4: activeStage.setPulseCount(ModelUtils::adjusted(activeStage.pulseCount(), value, 1, 8)); break;
        }
    } else if (isGlobalPitch) {
        // Global PTCH — F1-F3 edit stage[0], F4 edits sequence rate, F5
        // edits active stage's basePitch (per-cell anchor stays per-cell).
        switch (slot) {
        case 0: masterStage.setPitchCurve(PhaseFluxSequence::PitchCurveType(cycle(int(masterStage.pitchCurve()) + value, 0, 3))); break;
        case 1: masterStage.setPitchWarp(ModelUtils::adjusted(masterStage.pitchWarp(), value, -64, 64)); break;
        case 2: masterStage.setPitchResponse(ModelUtils::adjusted(masterStage.pitchResponse(), value, -64, 64)); break;
        case 3: seq.editPitchRate(value, shift); break;
        case 4: activeStage.setBasePitch(ModelUtils::adjusted(activeStage.basePitch(), value, -64, 64)); break;
        }
    } else {
        // Cell PTCH
        switch (slot) {
        case 0: activeStage.setPitchCurve(PhaseFluxSequence::PitchCurveType(cycle(int(activeStage.pitchCurve()) + value, 0, 3))); break;
        case 1: activeStage.setPitchWarp(ModelUtils::adjusted(activeStage.pitchWarp(), value, -64, 64)); break;
        case 2: activeStage.setPitchResponse(ModelUtils::adjusted(activeStage.pitchResponse(), value, -64, 64)); break;
        case 3: activeStage.setBasePitch(ModelUtils::adjusted(activeStage.basePitch(), value, -64, 64)); break;
        case 4: activeStage.setPitchRange(PhaseFluxSequence::PitchRangeType(cycle(int(activeStage.pitchRange()) + value, 0, 3))); break;
        }
    }
    (void)shift;
}

void PhaseFluxEditPage::randomizeCurrentSet() {
    // Set-aware: TEMP randomizes temporal shape params, PTCH randomizes pitch
    // shape params. stageLen and basePitch deliberately excluded — they're
    // chassis-feel knobs the user wants stable across "shake" gestures.
    static Random rng;
    auto &seq = _project.selectedPhaseFluxSequence();
    if (_currentSet != 0 && _currentSet != 1) return;
    for (int i = 0; i < 16; ++i) {
        auto &s = seq.stage(i);
        if (_currentSet == 0) {
            s.setTemporalCurve(PhaseFluxSequence::TemporalCurveType(rng.nextRange(3)));
            s.setTemporalWarp(int(rng.nextRange(128)) - 64);
            s.setTemporalResponse(int(rng.nextRange(128)) - 64);
            s.setPulseCount(1 + rng.nextRange(8));
        } else {
            s.setPitchCurve(PhaseFluxSequence::PitchCurveType(rng.nextRange(4)));
            s.setPitchWarp(int(rng.nextRange(128)) - 64);
            s.setPitchResponse(int(rng.nextRange(128)) - 64);
            s.setPitchRange(PhaseFluxSequence::PitchRangeType(rng.nextRange(4)));
        }
    }
}

void PhaseFluxEditPage::toggleShiftAt(int slot) {
    auto &seq = _project.selectedPhaseFluxSequence();
    auto &activeStage = seq.stage(_selectedCell);
    auto &masterStage = seq.stage(0);
    const bool isGlobalPitch =
        _currentSet == 1 &&
        _project.selectedPhaseFluxSequence().pitchMode() == PhaseFluxSequence::PitchMode::Global;
    const bool isAccumN = (_currentSet == 2);
    const bool isAccumP = (_currentSet == 3);
    if (isAccumN || isAccumP) {
        auto &cfg = isAccumN ? seq.noteAccumConfig() : seq.pulseAccumConfig();
        switch (slot) {
        case 0: {
            auto trig = isAccumN ? activeStage.accumulatorTrigger() : activeStage.pulseAccumTrigger();
            auto next = (trig == PhaseFluxSequence::AccumulatorTriggerType::Stage)
                ? PhaseFluxSequence::AccumulatorTriggerType::Pulse
                : PhaseFluxSequence::AccumulatorTriggerType::Stage;
            if (isAccumN) activeStage.setAccumulatorTrigger(next);
            else          activeStage.setPulseAccumTrigger(next);
            break;
        }
        case 3:
            cfg.setPolarity(cfg.polarity() == AccumulatorConfig::Polarity::Uni
                ? AccumulatorConfig::Polarity::Bi
                : AccumulatorConfig::Polarity::Uni);
            break;
        case 4:
            cfg.setScope(cfg.scope() == AccumulatorConfig::Scope::Local
                ? AccumulatorConfig::Scope::Track
                : AccumulatorConfig::Scope::Local);
            break;
        default: break;
        }
        return;
    }
    if (_currentSet == 0) {
        switch (slot) {
        case 0: activeStage.setTemporalFlipV(!activeStage.temporalFlipV()); break;
        case 1: activeStage.setTemporalFlipH(!activeStage.temporalFlipH()); break;
        default: break;
        }
    } else if (isGlobalPitch) {
        // F1/F2 flips toggle stage[0] (the master pitch curve). Shift+F5
        // cycles the active stage's range — demoted from F5 encoder edit.
        switch (slot) {
        case 0: masterStage.setPitchFlipV(!masterStage.pitchFlipV()); break;
        case 1: masterStage.setPitchFlipH(!masterStage.pitchFlipH()); break;
        case 4: {
            int next = (int(activeStage.pitchRange()) + 1) & 3;
            activeStage.setPitchRange(PhaseFluxSequence::PitchRangeType(next));
            break;
        }
        default: break;
        }
    } else {
        switch (slot) {
        case 0: activeStage.setPitchFlipV(!activeStage.pitchFlipV()); break;
        case 1: activeStage.setPitchFlipH(!activeStage.pitchFlipH()); break;
        default: break;
        }
    }
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

void PhaseFluxEditPage::drawTemporalScope(Canvas &canvas, int stageIdx, int scopeX) {
    const PhaseFluxSequence &seq = _project.selectedPhaseFluxSequence();
    const auto &stage = seq.stage(stageIdx);
    if (stage.skip()) return;

    int x = scopeX;
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

void PhaseFluxEditPage::drawPitchScope(Canvas &canvas, int stageIdx, int scopeX) {
    const PhaseFluxSequence &seq = _project.selectedPhaseFluxSequence();
    const auto &stage = seq.stage(stageIdx);
    if (stage.skip()) return;

    int x = scopeX;
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

void PhaseFluxEditPage::drawAccumDualStrip(Canvas &canvas, int scopeX, int activeSetIdx) {
    const PhaseFluxSequence &seq = _project.selectedPhaseFluxSequence();
    const auto *te = trackEngine();
    const int activeCell = te ? te->activeCell() : -1;
    const bool nActive = (activeSetIdx == 2);

    const int x0 = scopeX;
    const int y0 = ScopeY;
    const int h  = ScopeH;

    const int badgeW       = 12;
    const int gapOuter     = 2;
    const int stripH       = (h - gapOuter) / 2;
    const int topMidline   = y0 + stripH / 2;
    const int botMidline   = y0 + stripH + gapOuter + stripH / 2;

    const int barW         = 3;
    const int gapWithin    = 1;
    const int interGroup   = 5;

    int positions[16];
    int cursor = x0 + badgeW;
    for (int group = 0; group < 4; ++group) {
        for (int c = 0; c < 4; ++c) {
            positions[group * 4 + c] = cursor;
            cursor += barW + gapWithin;
        }
        cursor -= gapWithin;
        if (group < 3) cursor += interGroup;
    }

    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(Color::Low);
    for (int group = 0; group < 3; ++group) {
        int endOfGroupX = positions[group * 4 + 3] + barW;
        int gridX = endOfGroupX + interGroup / 2;
        for (int y = y0; y < y0 + h; y += 2) {
            canvas.point(gridX, y);
        }
    }

    const int halfH = stripH / 2 - 1;

    auto drawStrip = [&](int midlineY, int maxAmount, bool isNoteStrip, bool bright) {
        for (int i = 0; i < 16; ++i) {
            const auto &stage = seq.stage(i);
            int amount = isNoteStrip ? stage.accumulatorStep() : stage.pulseAccumStep();
            if (amount == 0) continue;

            int absAmt = amount < 0 ? -amount : amount;
            int barH = (absAmt * halfH) / maxAmount;
            if (barH == 0) barH = 1;

            int barY = (amount > 0) ? (midlineY - barH) : (midlineY + 1);

            Color color;
            if (bright) {
                if (i == activeCell)            color = Color::Bright;
                else if (i == _selectedCell)    color = Color::MediumBright;
                else                            color = Color::Medium;
            } else {
                color = (i == activeCell) ? Color::Medium : Color::Low;
            }
            canvas.setColor(color);
            canvas.fillRect(positions[i], barY, barW, barH);
        }
    };

    drawStrip(topMidline, PhaseFluxSequence::AccumulatorStep::Max, true,  nActive);
    drawStrip(botMidline, PhaseFluxSequence::PulseAccumStep::Max,  false, !nActive);
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

void PhaseFluxEditPage::drawStageBadge(Canvas &canvas, int scopeX) {
    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(Color::MediumBright);
    const auto &seq = _project.selectedPhaseFluxSequence();
    const bool isPtchGlobal =
        _currentSet == 1 &&
        seq.pitchMode() == PhaseFluxSequence::PitchMode::Global;
    const bool isAccumN = (_currentSet == 2);
    const bool isAccumP = (_currentSet == 3);
    if (isPtchGlobal) {
        canvas.drawText(scopeX + 2, ScopeY + 6, "G");
    } else if (isAccumN || isAccumP) {
        const auto &cfg = isAccumN ? seq.noteAccumConfig() : seq.pulseAccumConfig();
        if (cfg.scope() == AccumulatorConfig::Scope::Track) {
            canvas.drawText(scopeX + 2, ScopeY + 6, "T");
        } else {
            FixedStringBuilder<4> s("%d", _selectedCell + 1);
            canvas.drawText(scopeX + 2, ScopeY + 6, s);
        }
    } else {
        FixedStringBuilder<4> s("%d", _selectedCell + 1);
        canvas.drawText(scopeX + 2, ScopeY + 6, s);
    }
}

void PhaseFluxEditPage::drawParamList(Canvas &canvas) {
    const PhaseFluxSequence &seq = _project.selectedPhaseFluxSequence();
    const auto &activeStage = seq.stage(_selectedCell);
    const auto &masterStage = seq.stage(0);
    const bool shift = globalKeyState()[Key::Shift];
    const bool isGlobalPitch =
        _currentSet == 1 &&
        _project.selectedPhaseFluxSequence().pitchMode() == PhaseFluxSequence::PitchMode::Global;
    const bool isAccumN = (_currentSet == 2);
    const bool isAccumP = (_currentSet == 3);
    const bool isAccum = isAccumN || isAccumP;
    // In Global PTCH mode, F1-F3 read from stage[0] (the master). Cell mode
    // and TEMP set read from the currently selected cell.
    const auto &pitchSrc = isGlobalPitch ? masterStage : activeStage;

    static const char *kTempCurve[3]  = { "Lin", "Bel", "Bnc" };
    static const char *kPitchCurve[4] = { "Rmp", "Bel", "Tri", "Bnc" };

    static const char *kNamesTemp[5]       = { "Curve", "Warp", "Resp", "Len",  "Puls" };
    static const char *kNamesPtch[5]       = { "Curve", "Warp", "Resp", "Base", "Span" };
    static const char *kNamesPtchGlobal[5] = { "Curve", "Warp", "Resp", "Rate", "Note" };
    static const char *kNamesAccum[5]      = { "Amount", "+Lim", "-Lim", "Order", "Reset" };
    static const char *kNamesAccumShift[5] = { "Trig",   "+Lim", "-Lim", "Polar", "Scope" };

    FixedStringBuilder<8> values[5];
    if (isAccum) {
        const auto &cfg = isAccumN ? seq.noteAccumConfig() : seq.pulseAccumConfig();
        const int amount = isAccumN ? activeStage.accumulatorStep() : activeStage.pulseAccumStep();
        const auto trig  = isAccumN ? activeStage.accumulatorTrigger() : activeStage.pulseAccumTrigger();
        static const char *kOrder[4] = { "Wrap", "Pend", "Hold", "RTZ" };
        const char *trigStr = (trig == PhaseFluxSequence::AccumulatorTriggerType::Pulse) ? "Pulse" : "Stage";
        const char *polarStr = (cfg.polarity() == AccumulatorConfig::Polarity::Bi) ? "Bi" : "Uni";
        const char *scopeStr = (cfg.scope() == AccumulatorConfig::Scope::Track) ? "Track" : "Local";
        if (shift) values[0]("%s", trigStr);
        else       values[0]("%+d", amount);
        values[1]("%d", cfg.posLim());
        values[2]("%d", cfg.negLim());
        if (shift) values[3]("%s", polarStr);
        else       values[3]("%s", kOrder[clamp(int(cfg.order()), 0, 3)]);
        if (shift) values[4]("%s", scopeStr);
        else if (cfg.reset() == 0) values[4]("Manual");
        else values[4]("%dcyc", int(cfg.reset()));
    } else if (_currentSet == 0) {
        values[0]("%s",  kTempCurve[clamp(int(activeStage.temporalCurve()), 0, 2)]);
        values[1]("%+d", activeStage.temporalWarp());
        values[2]("%+d", activeStage.temporalResponse());
        values[3]("%.2fx", activeStage.stageLen() / 64.0f);
        values[4]("%d",  activeStage.pulseCount());
    } else if (isGlobalPitch) {
        values[0]("%s",  kPitchCurve[clamp(int(pitchSrc.pitchCurve()), 0, 3)]);
        values[1]("%+d", pitchSrc.pitchWarp());
        values[2]("%+d", pitchSrc.pitchResponse());
        values[3]("%d:%d",
            PhaseFluxSequence::pitchRateNum(seq.pitchRate()),
            PhaseFluxSequence::pitchRateDen(seq.pitchRate()));
        // values[4] populated specially below (note name + range dots stacked).
    } else {
        values[0]("%s",  kPitchCurve[clamp(int(activeStage.pitchCurve()), 0, 3)]);
        values[1]("%+d", activeStage.pitchWarp());
        values[2]("%+d", activeStage.pitchResponse());
        values[3]("%+d", activeStage.basePitch());
        static const char *kPitchRange[4] = { "1/2", "1", "2", "3" };
        values[4]("%s",  kPitchRange[clamp(int(activeStage.pitchRange()), 0, 3)]);
    }
    const char *(*names)[5];
    if (_currentSet == 0) {
        names = &kNamesTemp;
    } else if (isAccum) {
        names = shift ? &kNamesAccumShift : &kNamesAccum;
    } else if (isGlobalPitch) {
        // In Global PTCH, the F5 panel label swaps NOTE → SPAN while Shift is
        // held, since Shift+F5 cycles the per-cell range (range demoted).
        static const char *kNamesPtchGlobalShift[5] = { "Curve", "Warp", "Resp", "Rate", "Span" };
        names = shift ? &kNamesPtchGlobalShift : &kNamesPtchGlobal;
    } else {
        names = &kNamesPtch;
    }

    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);

    // Highlight box wraps only the value column (~1/3 of panel width), right-
    // aligned, so the label stays static and only the live value pops.
    const int valueColW = ParamPanelW / 3;
    const int valueColX = ParamPanelX + ParamPanelW - valueColW;

    for (int i = 0; i < 5; ++i) {
        int ry = ParamPanelY + i * ParamRowH;
        int rh = ParamRowH - 1;
        bool isSelected = (i == _selectedSlot);

        if (isSelected) {
            canvas.setColor(Color::Bright);
            canvas.drawRect(valueColX, ry, valueColW, rh);
        }
        canvas.setColor(Color::Medium);
        canvas.drawText(ParamPanelX + 2, ry + rh - 2, (*names)[i]);
        canvas.setColor(isSelected ? Color::Bright : Color::MediumBright);

        // F5 in Global PTCH renders the active stage's note name on the row's
        // upper portion and range dots beneath. All other rows print a single
        // right-flush value.
        if (isGlobalPitch && i == 4) {
            const Scale &scale = seq.selectedScale(_model.project().scale());
            int rootNote = seq.selectedRootNote(_model.project().rootNote());
            FixedStringBuilder<8> noteStr;
            scale.noteName(noteStr, activeStage.basePitch(), rootNote, Scale::Short1);
            int vw = canvas.textWidth(noteStr);
            canvas.drawText(ParamPanelX + ParamPanelW - 2 - vw, ry + rh - 2, noteStr);
            // Range dots beneath note name, flush right.
            int dots = clamp(int(activeStage.pitchRange()), 0, 3) + 1;
            int dotY = ry + rh + 1;
            int dotX = ParamPanelX + ParamPanelW - 4;
            canvas.setColor(isSelected ? Color::Bright : Color::MediumBright);
            for (int k = 0; k < dots; ++k) {
                canvas.point(dotX, dotY);
                dotX -= 3;
            }
            continue;
        }

        int vw = canvas.textWidth(values[i]);
        canvas.drawText(ParamPanelX + ParamPanelW - 2 - vw, ry + rh - 2, values[i]);
    }
}
