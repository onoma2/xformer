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
static constexpr int GridX     = 6;
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

// ACCUM 1x16 row layout (mirrors ui-preview/pages_phaseflux_accum_redesign.py)
static constexpr int AccumRowXStart      = 17;
static constexpr int AccumRowGap         = 6;
static constexpr int AccumRowY           = 28;
static constexpr int AccumPancakeW       = 4;
static constexpr int AccumPancakeXOff    = 5;       // right half of the 9 px cell
static constexpr int AccumGlyphX         = 4;       // 5 px wide column in left margin
static constexpr int AccumGlyphOrderY    = 18;
static constexpr int AccumGlyphPolarY    = 28;
static constexpr int AccumGlyphResetBL   = 43;      // tiny5x5 baseline

// 5x5 hand-drawn glyphs — each row holds 5 LSB-first pixel bits.
constexpr uint8_t kGlyphOrderWrap[5]  = { 0x04, 0x08, 0x1F, 0x08, 0x04 };
constexpr uint8_t kGlyphOrderPend[5]  = { 0x00, 0x0A, 0x1F, 0x0A, 0x00 };
constexpr uint8_t kGlyphOrderHold[5]  = { 0x00, 0x00, 0x1F, 0x00, 0x00 };
constexpr uint8_t kGlyphOrderRTZ[5]   = { 0x01, 0x01, 0x0F, 0x08, 0x04 };
constexpr uint8_t kGlyphPolarUni[5]   = { 0x04, 0x0E, 0x15, 0x04, 0x04 };
constexpr uint8_t kGlyphPolarBi[5]    = { 0x04, 0x0E, 0x04, 0x0E, 0x04 };

inline void paintGlyph5x5(Canvas &canvas, int x, int y, const uint8_t pattern[5]) {
    for (int row = 0; row < 5; ++row) {
        uint8_t b = pattern[row];
        for (int col = 0; col < 5; ++col) {
            if ((b >> col) & 1) canvas.point(x + col, y + row);
        }
    }
}

// PITCH scope — label column (left) + trace zone (right).
static constexpr int PitchLabelColW = 22;

inline float pitchRangeToOctaves_local(PhaseFluxSequence::PitchRangeType v) {
    switch (v) {
    case PhaseFluxSequence::PitchRangeType::Half:  return 0.5f;
    case PhaseFluxSequence::PitchRangeType::One:   return 1.0f;
    case PhaseFluxSequence::PitchRangeType::Two:   return 2.0f;
    case PhaseFluxSequence::PitchRangeType::Three: return 3.0f;
    }
    return 1.0f;
}

inline float directionOffset(PhaseFluxSequence::PitchDirectionType d,
                              float p_final, int rangeDegrees) {
    switch (d) {
    case PhaseFluxSequence::PitchDirectionType::Up:
        return p_final * float(rangeDegrees);
    case PhaseFluxSequence::PitchDirectionType::Down:
        return -p_final * float(rangeDegrees);
    case PhaseFluxSequence::PitchDirectionType::Bipolar:
        return (p_final - 0.5f) * float(rangeDegrees);
    }
    return 0.f;
}

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

    // Footer: all topics now paged. F5 = Next. Shift bindings dropped — flips
    // are first-class on page 1 (press = toggle).
    static const char *kLabelsTempP0[5]  = { "Curve", "Warp",  "Resp", "Puls",  "Next" };
    static const char *kLabelsTempP1[5]  = { "Len",   "FlipV", "FlipH", "Mask", "Next" };
    static const char *kLabelsPtchP0[5]  = { "Curve", "Warp",  "Resp", "Note",  "Next" };
    static const char *kLabelsPtchP1[5]  = { "Span",  "Dir",   "FlipV", "FlipH", "Next" };
    static const char *kLabelsPtchGlobalP0[5] = { "Curve", "Warp", "Resp", "Rate", "Next" };
    static const char *kLabelsPtchGlobalP1[5] = { "Note",  "Span", "FlipV","FlipH","Next" };
    static const char *kLabelsAccumP0[5] = { "Ac.St", nullptr, nullptr, "Order", "Next" };
    static const char *kLabelsAccumP1[5] = { "Reset", "Polar", "Trig",  nullptr, "Next" };

    const char *(*primary)[5];
    if (_currentSet == 0) {
        primary = (_topicPage == 0) ? &kLabelsTempP0 : &kLabelsTempP1;
    } else if (_currentSet == 1 && isGlobalPitch) {
        primary = (_topicPage == 0) ? &kLabelsPtchGlobalP0 : &kLabelsPtchGlobalP1;
    } else if (_currentSet == 1) {
        primary = (_topicPage == 0) ? &kLabelsPtchP0 : &kLabelsPtchP1;
    } else {
        primary = (_topicPage == 0) ? &kLabelsAccumP0 : &kLabelsAccumP1;
    }
    const char *footer[5];
    for (int i = 0; i < 5; ++i) footer[i] = (*primary)[i];
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), _selectedSlot);

    // ACCUM owns the full content area — 1x16 row + pancakes + glyphs.
    // TEMP/PTCH keep the 4x4 grid + scope + param list.
    if (isAccumN || isAccumP) {
        drawAccumPage(canvas);
        return;
    }

    drawGrid(canvas);

    // Vertical dividers — safe area y=11..52
    canvas.setColor(Color::Low);
    canvas.vline(DividerX,  13, 39);
    canvas.vline(DividerX2, 13, 39);

    if (_currentSet == 0) {
        drawTemporalScope(canvas, _selectedCell, ScopeTempX);
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

    // Left/Right cycles topics. Reset page + selection so each topic lands
    // on its page 0 / F1 with no carry-over from the previous topic.
    if (key.isLeft()) {
        _currentSet = (_currentSet + kSetCount - 1) % kSetCount;
        _topicPage = 0;
        _selectedSlot = 0;
        event.consume();
        return;
    }
    if (key.isRight()) {
        _currentSet = (_currentSet + 1) % kSetCount;
        _topicPage = 0;
        _selectedSlot = 0;
        event.consume();
        return;
    }

    // F1..F5 — F5 always = Next (cycle topic page). F1..F4 either flip a
    // binary slot on press or select for encoder editing.
    if (key.isFunction()) {
        int slot = key.function();
        if (slot == 4) {
            _topicPage = 1 - _topicPage;
            _selectedSlot = 0;
            event.consume();
            return;
        }
        // Binary toggles are first-class — press flips, no slot selection.
        const auto &seq = _project.selectedPhaseFluxSequence();
        const bool isGlobalPitch =
            _currentSet == 1 &&
            seq.pitchMode() == PhaseFluxSequence::PitchMode::Global;
        bool isToggle = false;
        if (_topicPage == 1) {
            if (_currentSet == 0) {
                // TEMP P1: Len(0) / FlipV(1) / FlipH(2) / Mask(3)
                isToggle = (slot == 1 || slot == 2);
            } else if (_currentSet == 1 && isGlobalPitch) {
                // PITCH Global P1: Note(0) / Span(1) / FlipV(2) / FlipH(3)
                isToggle = (slot == 2 || slot == 3);
            } else if (_currentSet == 1) {
                // PITCH Cell P1: Span(0) / Dir(1) / FlipV(2) / FlipH(3)
                isToggle = (slot == 2 || slot == 3);
            }
        }
        if (isToggle) {
            togglePressSlot(slot);
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
        if (_topicPage == 0) {
            switch (slot) {
            case 0:  // Ac.St — per-cell, UI-clamped to ±7.
                if (isAccumN) {
                    activeStage.setAccumulatorStep(clamp(activeStage.accumulatorStep() + value, -7, 7));
                } else {
                    activeStage.setPulseAccumStep(activeStage.pulseAccumStep() + value);
                }
                break;
            case 1: break;  // reserved (+Lim → list page)
            case 2: break;  // reserved (-Lim → list page)
            case 3: cfg.setOrder(AccumulatorConfig::Order(cycle(int(cfg.order()) + value, 0, 3))); break;
            }
        } else {
            switch (slot) {
            case 0: cfg.setReset(uint8_t(cycle(int(cfg.reset()) + value, 0, 15))); break;
            case 1: cfg.setPolarity(AccumulatorConfig::Polarity(cycle(int(cfg.polarity()) + value, 0, 1))); break;
            case 2: {
                int trigInt = isAccumN
                    ? int(activeStage.accumulatorTrigger())
                    : int(activeStage.pulseAccumTrigger());
                trigInt = cycle(trigInt + value, 0, 1);
                auto next = PhaseFluxSequence::AccumulatorTriggerType(trigInt);
                if (isAccumN) activeStage.setAccumulatorTrigger(next);
                else          activeStage.setPulseAccumTrigger(next);
                break;
            }
            case 3: break;  // Sleep reserved (§14.2)
            }
        }
        (void)shift;
        return;
    }
    if (_currentSet == 0) {
        // TEMP — P0: Curve/Warp/Resp/Puls; P1: Len/[FlipV*]/[FlipH*]/Mask
        // (* = handled by togglePressSlot, not editSlot.)
        if (_topicPage == 0) {
            switch (slot) {
            case 0: activeStage.setTemporalCurve(PhaseFluxSequence::TemporalCurveType(cycle(int(activeStage.temporalCurve()) + value, 0, 2))); break;
            case 1: activeStage.setTemporalWarp(ModelUtils::adjusted(activeStage.temporalWarp(), value, -64, 64)); break;
            case 2: activeStage.setTemporalResponse(ModelUtils::adjusted(activeStage.temporalResponse(), value, -64, 64)); break;
            case 3: activeStage.setPulseCount(ModelUtils::adjusted(activeStage.pulseCount(), value, 1, 8)); break;
            }
        } else {
            switch (slot) {
            case 0: activeStage.setStageLen(ModelUtils::adjusted(activeStage.stageLen(), value, 0, 127)); break;
            // slots 1, 2 are FlipV / FlipH toggles (press-only)
            case 3: activeStage.setMask(PhaseFluxSequence::MaskType(cycle(int(activeStage.mask()) + value, 0, 7))); break;
            }
        }
    } else if (isGlobalPitch) {
        // PTCH Global — P0: Curve/Warp/Resp/Rate (master stage); P1: Note(active)/Span(active)/[FlipV*]/[FlipH*]
        if (_topicPage == 0) {
            switch (slot) {
            case 0: masterStage.setPitchCurve(PhaseFluxSequence::PitchCurveType(cycle(int(masterStage.pitchCurve()) + value, 0, 3))); break;
            case 1: masterStage.setPitchWarp(ModelUtils::adjusted(masterStage.pitchWarp(), value, -64, 64)); break;
            case 2: masterStage.setPitchResponse(ModelUtils::adjusted(masterStage.pitchResponse(), value, -64, 64)); break;
            case 3: seq.editPitchRate(value, shift); break;
            }
        } else {
            switch (slot) {
            case 0: activeStage.setBasePitch(ModelUtils::adjusted(activeStage.basePitch(), value, -64, 64)); break;
            case 1: activeStage.setPitchRange(PhaseFluxSequence::PitchRangeType(cycle(int(activeStage.pitchRange()) + value, 0, 3))); break;
            // slots 2, 3 are master stage's FlipV / FlipH toggles
            }
        }
    } else {
        // PTCH Cell — P0: Curve/Warp/Resp/Note; P1: Span/Dir/[FlipV*]/[FlipH*]
        if (_topicPage == 0) {
            switch (slot) {
            case 0: activeStage.setPitchCurve(PhaseFluxSequence::PitchCurveType(cycle(int(activeStage.pitchCurve()) + value, 0, 3))); break;
            case 1: activeStage.setPitchWarp(ModelUtils::adjusted(activeStage.pitchWarp(), value, -64, 64)); break;
            case 2: activeStage.setPitchResponse(ModelUtils::adjusted(activeStage.pitchResponse(), value, -64, 64)); break;
            case 3: activeStage.setBasePitch(ModelUtils::adjusted(activeStage.basePitch(), value, -64, 64)); break;
            }
        } else {
            switch (slot) {
            case 0: activeStage.setPitchRange(PhaseFluxSequence::PitchRangeType(cycle(int(activeStage.pitchRange()) + value, 0, 3))); break;
            case 1: activeStage.setPitchDirection(PhaseFluxSequence::PitchDirectionType(cycle(int(activeStage.pitchDirection()) + value, 0, 2))); break;
            // slots 2, 3 are active stage's FlipV / FlipH toggles
            }
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

void PhaseFluxEditPage::togglePressSlot(int slot) {
    // Press-to-flip for binary toggles on Page 1. Topic/page selection by
    // keyPress; this function just performs the actual flip.
    auto &seq = _project.selectedPhaseFluxSequence();
    auto &activeStage = seq.stage(_selectedCell);
    auto &masterStage = seq.stage(0);
    const bool isGlobalPitch =
        _currentSet == 1 &&
        seq.pitchMode() == PhaseFluxSequence::PitchMode::Global;

    if (_currentSet == 0) {
        // TEMP P1: FlipV(1), FlipH(2)
        switch (slot) {
        case 1: activeStage.setTemporalFlipV(!activeStage.temporalFlipV()); break;
        case 2: activeStage.setTemporalFlipH(!activeStage.temporalFlipH()); break;
        default: break;
        }
    } else if (isGlobalPitch) {
        // PITCH Global P1: FlipV(2)/FlipH(3) on the master pitch curve (stage 0).
        switch (slot) {
        case 2: masterStage.setPitchFlipV(!masterStage.pitchFlipV()); break;
        case 3: masterStage.setPitchFlipH(!masterStage.pitchFlipH()); break;
        default: break;
        }
    } else if (_currentSet == 1) {
        // PITCH Cell P1: FlipV(2)/FlipH(3) on the active stage.
        switch (slot) {
        case 2: activeStage.setPitchFlipV(!activeStage.pitchFlipV()); break;
        case 3: activeStage.setPitchFlipH(!activeStage.pitchFlipH()); break;
        default: break;
        }
    }
    // ACCUM has no toggle slots on either page — F-press always selects.
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

    // Pulse marks — 3x3 hollow ring per base pulse, 3x2 U-shape per
    // accumulator-added extra. Centred on the trigger time, vertically
    // centred in the scope. 2 px inset from each border keeps the mark 1 px
    // clear of the outline at both extremes. Dim by default; brighten as the
    // playhead passes each pulse in the active cell.
    int maskByte = kMaskTable[int(stage.mask())];
    int maskShift = stage.maskShift();
    const int markCenterY = y + ScopeH / 2;
    const int innerSpan   = ScopeW - 5;   // 95 — px ranges 52..147
    const auto *te = trackEngine();
    const bool isActiveCell = te && (te->activeCell() == stageIdx);
    const float stagePhase  = isActiveCell ? te->sequenceProgress() : -1.f;

    // Match engine effective pulse count: base + pulse-accum counter, clamped 1..8.
    const auto &pulseCfg = seq.pulseAccumConfig();
    const int pulseCounterIdx = (pulseCfg.scope() == AccumulatorConfig::Scope::Local)
        ? stageIdx : 0;
    const int pulseAccOffset = te ? te->pulseAccumCounter(pulseCounterIdx) : 0;
    const int basePulses = std::max(1, std::min(8, stage.pulseCount()));
    const int effective  = std::max(1, std::min(8, stage.pulseCount() + pulseAccOffset));
    const int basesDrawn = std::min(basePulses, effective);

    for (int i = 0; i < effective; ++i) {
        // §6.1 — matches engine: first pulse at slot start, last at (N-1)/N.
        float t_raw = float(i) / float(effective);
        float t_final = evalTemporal(stage, t_raw);
        float t_shifted = t_final + float(stage.phaseShift()) * 0.125f;
        t_shifted = std::fmod(t_shifted, 1.f);
        if (t_shifted < 0.f) t_shifted += 1.f;
        bool muted = (maskByte >> ((i + maskShift) & 7)) & 1;
        bool passed = (stagePhase >= 0.f) && (stagePhase >= t_shifted);
        int px = x + 2 + int(t_shifted * float(innerSpan));
        if (muted) {
            canvas.setColor(Color::Low);
            canvas.point(px, markCenterY);
            continue;
        }
        canvas.setColor(passed ? Color::Bright : Color::Medium);
        if (i < basesDrawn) {
            // Base pulse — 3x3 hollow ring.
            canvas.drawRect(px - 1, markCenterY - 1, 3, 3);
        } else {
            // Accumulator-extra — 3 wide × 2 tall U (open top, closed bottom).
            canvas.point(px - 1, markCenterY);
            canvas.point(px + 1, markCenterY);
            canvas.hline(px - 1, markCenterY + 1, 3);
        }
    }
}

void PhaseFluxEditPage::drawPitchScope(Canvas &canvas, int stageIdx, int scopeX) {
    const PhaseFluxSequence &seq = _project.selectedPhaseFluxSequence();
    const auto &stage = seq.stage(stageIdx);
    if (stage.skip()) return;

    const int x = scopeX;
    const int y = ScopeY;
    const int traceX0 = x + PitchLabelColW + 1;
    const int traceW  = ScopeW - PitchLabelColW - 2;

    canvas.setBlendMode(BlendMode::Set);

    // Outline + label/trace divider.
    canvas.setColor(Color::Low);
    canvas.drawRect(x, y, ScopeW, ScopeH);
    canvas.vline(x + PitchLabelColW, y + 1, ScopeH - 2);

    // Curve trace — Medium so the shape reads but doesn't dominate.
    canvas.setColor(Color::Medium);
    int prevPy = -1;
    for (int px = 0; px < traceW; ++px) {
        float phi = float(px) / float(std::max(1, traceW - 1));
        float p_final = evalPitch(stage, phi);
        int py = y + ScopeH - 2 - int(p_final * float(ScopeH - 3));
        int cx = traceX0 + px;
        if (prevPy >= 0) canvas.line(cx - 1, prevPy, cx, py);
        prevPy = py;
    }

    // Engine state for playhead-aware highlight.
    const auto *te = trackEngine();
    const bool isActiveCell = te && (te->activeCell() == stageIdx);
    const float stagePhase  = isActiveCell ? te->sequenceProgress() : -1.f;

    // Pulse-fire dots — 2x2, on the curve at each unmuted pulse's (t, pitch).
    // Spacing uses i/N (engine §6.1). Dim by default, bright as the
    // playhead crosses each one in the active cell.
    int pulses = std::max(1, std::min(8, stage.pulseCount()));
    int maskByte = kMaskTable[int(stage.mask())];
    int maskShift = stage.maskShift();
    for (int i = 0; i < pulses; ++i) {
        float t_raw = float(i) / float(pulses);
        float t_final = evalTemporal(stage, t_raw);
        float t_shifted = t_final + float(stage.phaseShift()) * 0.125f;
        t_shifted = std::fmod(t_shifted, 1.f);
        if (t_shifted < 0.f) t_shifted += 1.f;
        bool muted = (maskByte >> ((i + maskShift) & 7)) & 1;
        if (muted) continue;
        float p_final = evalPitch(stage, t_shifted);
        int dx = traceX0 + int(t_shifted * float(std::max(1, traceW - 1)));
        int dy = y + ScopeH - 2 - int(p_final * float(ScopeH - 3));
        bool passed = (stagePhase >= 0.f) && (stagePhase >= t_shifted);
        canvas.setColor(passed ? Color::Bright : Color::Medium);
        canvas.fillRect(dx - 1, dy - 1, 2, 2);
    }

    // Reachable-degree histogram + top 4 by count.
    const Scale &scale = seq.selectedScale(_model.project().scale());
    const int rootNote = seq.selectedRootNote(_model.project().rootNote());
    const int octave = _phaseFluxTrack.octave();
    const int transpose = _phaseFluxTrack.transpose();
    const float rangeOctaves = pitchRangeToOctaves_local(stage.pitchRange());
    const int rangeDegrees = std::max(1, int(std::round(rangeOctaves * scale.notesPerOctave())));
    const int baseDegree = stage.basePitch() + octave * scale.notesPerOctave() + transpose;

    // Histogram bucketed by signed offset (kept small for stack use).
    constexpr int kHistBias = 64;
    constexpr int kHistSize = 128;
    int counts[kHistSize] = {0};
    const int samples = 64;
    for (int s = 0; s < samples; ++s) {
        float phi = float(s) / float(samples);
        float p_final = evalPitch(stage, phi);
        float off = directionOffset(stage.pitchDirection(), p_final, rangeDegrees);
        int deg = int(std::round(off));
        int bin = deg + kHistBias;
        if (bin >= 0 && bin < kHistSize) counts[bin]++;
    }

    // Currently-playing offset (sentinel = INT_MIN-ish).
    constexpr int kNoCurrent = -10000;
    int currentOff = kNoCurrent;
    if (isActiveCell && stagePhase >= 0.f) {
        float p_final = evalPitch(stage, stagePhase);
        currentOff = int(std::round(
            directionOffset(stage.pitchDirection(), p_final, rangeDegrees)));
    }

    // Find top 4 by count (linear repeated max — N=4, fine for small N).
    int topDeg[5];
    int topN = 0;
    int countsCopy[kHistSize];
    for (int i = 0; i < kHistSize; ++i) countsCopy[i] = counts[i];
    for (int slot = 0; slot < 4; ++slot) {
        int bestBin = -1;
        int bestCount = 0;
        for (int b = 0; b < kHistSize; ++b) {
            if (countsCopy[b] > bestCount) {
                bestCount = countsCopy[b];
                bestBin = b;
            }
        }
        if (bestBin < 0) break;
        topDeg[topN++] = bestBin - kHistBias;
        countsCopy[bestBin] = 0;
    }

    // Ensure current is in the visible set (push out lowest-count one).
    if (currentOff != kNoCurrent) {
        bool already = false;
        for (int i = 0; i < topN; ++i) if (topDeg[i] == currentOff) { already = true; break; }
        // Only include if reachable (its bucket non-zero in original histogram).
        const int currentBin = currentOff + kHistBias;
        const bool reachable = (currentBin >= 0 && currentBin < kHistSize && counts[currentBin] > 0);
        if (!already && reachable) {
            if (topN >= 4) topN = 3;
            topDeg[topN++] = currentOff;
        }
    }

    if (topN == 0) return;

    // Sort by pitch — high at top, low at bottom (descending).
    for (int i = 0; i < topN; ++i) {
        for (int j = i + 1; j < topN; ++j) {
            if (topDeg[i] < topDeg[j]) std::swap(topDeg[i], topDeg[j]);
        }
    }

    // Draw labels: evenly distribute slots in the label column.
    canvas.setFont(Font::Tiny);
    const int innerY0 = y + 2;
    const int innerH  = ScopeH - 4;
    const int labelRight = x + PitchLabelColW - 2;
    for (int i = 0; i < topN; ++i) {
        const int slotCy  = innerY0 + ((2 * i + 1) * innerH) / (2 * topN);
        const int baseline = slotCy + 2;
        FixedStringBuilder<5> noteStr;
        scale.noteName(noteStr, baseDegree + topDeg[i], rootNote, Scale::Long);
        const int tw = canvas.textWidth(noteStr);
        const int tx = labelRight - tw;
        if (topDeg[i] == currentOff) {
            canvas.setColor(Color::Bright);
            canvas.fillRect(tx - 1, baseline - 5, tw + 1, 7);
            canvas.setBlendMode(BlendMode::Sub);
            canvas.drawText(tx, baseline, noteStr);
            canvas.setBlendMode(BlendMode::Set);
        } else {
            canvas.setColor(Color::Medium);
            canvas.drawText(tx, baseline, noteStr);
        }
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

void PhaseFluxEditPage::drawAccumPage(Canvas &canvas) {
    const auto &seq = _project.selectedPhaseFluxSequence();
    const auto *te  = trackEngine();
    const int activeCell = te ? te->activeCell() : -1;
    const bool isAccumN = (_currentSet == 2);
    const auto &cfg = isAccumN ? seq.noteAccumConfig() : seq.pulseAccumConfig();

    canvas.setBlendMode(BlendMode::Set);

    // 1) 16-cell row — same per-cell visual rules as the existing 4x4 grid.
    for (int i = 0; i < 16; ++i) {
        const auto &stage = seq.stage(i);
        const bool isActive   = (i == activeCell);
        const bool isSelected = (i == _selectedCell);
        const int x = AccumRowXStart + i * (CellW + AccumRowGap);
        const int y = AccumRowY;

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

        if (stage.skip()) continue;

        const int pulses = std::max(1, std::min(8, stage.pulseCount()));
        const Color barColor = isActive
            ? Color::None
            : (isSelected ? Color::Bright : Color::MediumBright);
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

    // 2) Per-cell BIPOLAR Ac.St pancakes — positive grows up from square top,
    //    negative grows down from square bottom. Magnitude UI-clamped to 7
    //    (matches pulse range; note storage stays ±15).
    constexpr int kAccumPancakeMaxMag = 7;
    for (int i = 0; i < 16; ++i) {
        const auto &stage = seq.stage(i);
        if (stage.skip()) continue;
        const int amt = isAccumN ? stage.accumulatorStep() : stage.pulseAccumStep();
        if (amt == 0) continue;
        const int mag = std::min(amt < 0 ? -amt : amt, kAccumPancakeMaxMag);
        const int x = AccumRowXStart + i * (CellW + AccumRowGap) + AccumPancakeXOff;
        canvas.setColor((i == _selectedCell) ? Color::Bright : Color::Medium);
        if (amt > 0) {
            for (int n = 1; n <= mag; ++n) {
                canvas.hline(x, AccumRowY - 2 * n, AccumPancakeW);
            }
        } else {
            for (int n = 1; n <= mag; ++n) {
                canvas.hline(x, AccumRowY + CellH + 2 * n - 1, AccumPancakeW);
            }
        }
    }

    // 3) Per-cell S/P trig chip below square, left half of the cell column.
    canvas.setFont(Font::Tiny);
    for (int i = 0; i < 16; ++i) {
        const auto &stage = seq.stage(i);
        if (stage.skip()) continue;
        const int x = AccumRowXStart + i * (CellW + AccumRowGap);
        const auto trig = isAccumN ? stage.accumulatorTrigger() : stage.pulseAccumTrigger();
        const char *trigStr =
            (trig == PhaseFluxSequence::AccumulatorTriggerType::Stage) ? "S" : "P";
        canvas.setColor(Color::Medium);
        canvas.drawText(x - 2, AccumRowY + CellH + 6, trigStr);
    }

    // 4) Sequence-wide glyphs in the left margin: Order, Polar, Reset.
    canvas.setColor(Color::MediumBright);
    const uint8_t *orderGlyph;
    switch (cfg.order()) {
    case AccumulatorConfig::Order::Wrap:     orderGlyph = kGlyphOrderWrap; break;
    case AccumulatorConfig::Order::Pendulum: orderGlyph = kGlyphOrderPend; break;
    case AccumulatorConfig::Order::Hold:     orderGlyph = kGlyphOrderHold; break;
    case AccumulatorConfig::Order::RTZ:      orderGlyph = kGlyphOrderRTZ;  break;
    default:                                 orderGlyph = kGlyphOrderWrap; break;
    }
    paintGlyph5x5(canvas, AccumGlyphX, AccumGlyphOrderY, orderGlyph);
    paintGlyph5x5(canvas, AccumGlyphX, AccumGlyphPolarY,
                  (cfg.polarity() == AccumulatorConfig::Polarity::Uni)
                      ? kGlyphPolarUni : kGlyphPolarBi);

    const int reset = int(cfg.reset());
    FixedStringBuilder<3> resetBuf;
    if (reset <= 0)       resetBuf("M");
    else if (reset <= 9)  resetBuf("%d", reset);
    else                  resetBuf("+");
    const int rw = canvas.textWidth(resetBuf);
    canvas.drawText(AccumGlyphX + (5 - rw) / 2, AccumGlyphResetBL, resetBuf);
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
