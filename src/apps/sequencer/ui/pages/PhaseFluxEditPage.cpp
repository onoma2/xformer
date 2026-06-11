#include "PhaseFluxEditPage.h"

#include "Pages.h"

#include "ui/painters/WindowPainter.h"
#include "ui/MatrixMap.h"

#include "model/Curve.h"
#include "model/ModelUtils.h"
#include "model/PhaseFluxMath.h"
#include "model/Scale.h"
#include "model/StochasticTypes.h"

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

inline int repeatMultiplier(PhaseFluxSequence::RepeatType v) {
    return PhaseFluxMath::repeatMultiplier(v);
}

inline bool isInWindow(float phi, PhaseFluxSequence::WindowType v) {
    return PhaseFluxMath::isInWindow(phi, v);
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

// v0.2 temporal value tail (warp consumed upstream in evalTemporalAlloc):
// Curve -> Response -> FlipV. FlipH is a schedule reversal, applied to the
// final position at the call site (not a phase mirror).
inline float evalTemporalValue(const PhaseFluxSequence::Stage &stage, float t_local) {
    float t = Curve::eval(temporalCurveLut(stage.temporalCurve()), t_local);
    return PhaseFluxMath::evalResponseFlipV(t, stage.temporalResponse(), stage.temporalFlipV());
}

// Continuous-trace phase (v0.2): Warp -> Repeat. Caller does the window-drop
// and the FlipH left-right mirror of the screen sample.
inline float evalTemporalTracePhase(const PhaseFluxSequence::Stage &stage, float t_raw, int tempRep) {
    float t = (stage.temporalWarp() == 0) ? t_raw
        : PhaseFluxMath::powerBend(clamp01(t_raw), PhaseFluxMath::powerBendKnobToParam(stage.temporalWarp()));
    return (tempRep > 1) ? std::fmod(t * float(tempRep), 1.f) : t;
}

// v0.2 pitch pipeline — same PhaseFluxMath functions the engine uses, so the
// scope matches playback (Warp->Repeat->Window->FlipH->Curve->Response->FlipV).
inline float evalPitchFull(const PhaseFluxSequence::Stage &stage, float phi,
                           int pitchRep, PhaseFluxSequence::WindowType win) {
    float phi_input = PhaseFluxMath::evalPitchPhase(
        phi, stage.pitchWarp(), pitchRep, win, stage.pitchFlipH());
    float p_curved = Curve::eval(pitchCurveLut(stage.pitchCurve()), phi_input);
    return PhaseFluxMath::evalResponseFlipV(p_curved, stage.pitchResponse(), stage.pitchFlipV());
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

    // Header: TEMP / PTCH / PTCH.G / ACCUM.N[T] / ACCUM.P[T] / MACRO.
    const bool isPtch = (_currentSet == 1);
    const bool isAccumN = (_currentSet == 2);
    const bool isAccumP = (_currentSet == 3);
    const bool isMacro = (_currentSet == 4);
    const auto &seqForHeader = _project.selectedPhaseFluxSequence();
    const bool isGlobalPitch =
        isPtch && seqForHeader.pitchMode() == PhaseFluxSequence::PitchMode::Global;
    const char *setName = "TEMP";
    if (_currentSet == 1) {
        setName = isGlobalPitch ? "PTCH.G" : "PTCH";
    } else if (isAccumN) {
        setName = "ACCUM.N";
    } else if (isAccumP) {
        setName = "ACCUM.P";
    } else if (isMacro) {
        setName = "MACRO";
    }
    WindowPainter::drawActiveFunction(canvas, setName);

    // Footer: all topics now paged. F5 = Next. Shift bindings dropped — flips
    // are first-class on page 1 (press = toggle). TEMP / PTCH-Cell add a
    // page 2 for the §14.2 Window + Repeat transforms.
    static const char *kLabelsTempP0[5]  = { "Curve", "Warp",  "Resp", "Puls",  "Next" };
    static const char *kLabelsTempP1[5]  = { "Len",   "FlipV", "FlipH", "Mask", "Next" };
    static const char *kLabelsTempP2[5]  = { "Wind",  "Rep",   nullptr, nullptr, "Next" };
    static const char *kLabelsPtchP0[5]  = { "Curve", "Warp",  "Resp", "Note",  "Next" };
    static const char *kLabelsPtchP1[5]  = { "Span",  "Dir",   "FlipV", "FlipH", "Next" };
    static const char *kLabelsPtchP2[5]  = { "Wind",  "Rep",   "Mask",  "Tilt",  "Next" };
    static const char *kLabelsPtchGlobalP0[5] = { "Curve", "Warp", "Resp", "Rate", "Next" };
    static const char *kLabelsPtchGlobalP1[5] = { "Note",  "Span", "FlipV","FlipH","Next" };
    static const char *kLabelsAccumP0[5] = { "Ac.St", nullptr, nullptr, "Order", "Next" };
    static const char *kLabelsAccumP1[5] = { "Reset", "Polar", "Trig",  nullptr, "Next" };
    static const char *kLabelsMacroP0[5] = { "WarpN", "RespN", "PulsN", "LenN",  "Next" };
    static const char *kLabelsMacroP1[5] = { "Phase", "GWarp", "Snap",  "Zero",  "Next" };

    const char *(*primary)[5];
    if (_currentSet == 0) {
        primary = (_topicPage == 0) ? &kLabelsTempP0 :
                  (_topicPage == 1) ? &kLabelsTempP1 : &kLabelsTempP2;
    } else if (_currentSet == 1 && isGlobalPitch) {
        primary = (_topicPage == 0) ? &kLabelsPtchGlobalP0 : &kLabelsPtchGlobalP1;
    } else if (_currentSet == 1) {
        primary = (_topicPage == 0) ? &kLabelsPtchP0 :
                  (_topicPage == 1) ? &kLabelsPtchP1 : &kLabelsPtchP2;
    } else if (_currentSet == 4) {
        primary = (_topicPage == 0) ? &kLabelsMacroP0 : &kLabelsMacroP1;
    } else {
        primary = (_topicPage == 0) ? &kLabelsAccumP0 : &kLabelsAccumP1;
    }
    const char *footer[5];
    for (int i = 0; i < 5; ++i) footer[i] = (*primary)[i];
    // ACCUM P0 F2 — surface current Accum Mode (Stage/Sequence) per accumulator.
    if ((isAccumN || isAccumP) && _topicPage == 0) {
        const auto &cfg = isAccumN
            ? seqForHeader.noteAccumConfig() : seqForHeader.pulseAccumConfig();
        footer[1] = (cfg.scope() == AccumulatorConfig::Scope::Track) ? "M:Seq" : "M:Sta";
    }
    WindowPainter::drawFooter(canvas, footer, pageKeyState(), _selectedSlot);

    // ACCUM owns the full content area — 1x16 row + pancakes + glyphs.
    // TEMP/PTCH keep the 4x4 grid + scope + param list.
    if (isAccumN || isAccumP) {
        drawAccumPage(canvas);
        return;
    }

    // MACRO drops the grid + per-stage scope + param list. Full-width macro
    // scope spans x=2..253. Numeric values not displayed — the scope shape
    // is live feedback for every knob.
    if (isMacro) {
        drawMacroScope(canvas);
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
    // StepSelection handles multi-cell tracking (Immediate = held; Persist =
    // toggled in via Shift+step). PhaseFlux's 4×4 grid lives in step indices
    // 0..15 directly, so stepOffset = 0.
    _stepSelection.keyDown(event, 0);
}

void PhaseFluxEditPage::keyUp(KeyEvent &event) {
    _stepSelection.keyUp(event, 0);
}

void PhaseFluxEditPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    // Context menu — PAGE+Shift opens INIT/COPY/PASTE.
    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    // Quick-edit overlay (Page + bottom-row step S8..S15). Set-aware actions.
    if (key.isQuickEdit() && !key.shiftModifier()) {
        switch (key.quickEdit()) {
        case 6: shake(true);  break;   // Page+S15 — shake whole topic
        case 7: shake(false); break;   // Page+S16 — shake current page only
        // case 4: Init (later)
        // case 5: Even (later)
        }
        event.consume();
        return;
    }

    // Page+key combos belong to TopPage (Page+S0/S1/S2 etc.). Don't consume.
    if (key.pageModifier()) {
        return;
    }

    // StepSelection's keyPress handles Shift+Shift double-click (select-all/
    // clear) and Shift+double-click on a step (interval-select / select-equal).
    _stepSelection.keyPress(event, 0);

    if (key.isStep()) {
        // Double-click step (no shift) → toggle skip on that step.
        if (!key.shiftModifier() && event.count() == 2) {
            auto &stage = _project.selectedPhaseFluxSequence().stage(key.step());
            stage.setSkip(!stage.skip());
            event.consume();
            return;
        }
        // Single press without shift → move cursor (drives scope/grid focus).
        // Selection state is tracked separately via _stepSelection (keyDown/Up).
        if (!key.shiftModifier()) {
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
            const auto &seq = _project.selectedPhaseFluxSequence();
            const bool isGlobalPitchHere =
                _currentSet == 1 &&
                seq.pitchMode() == PhaseFluxSequence::PitchMode::Global;
            // TEMP and PTCH-Cell carry a 3rd page (§14.2 Window/Repeat).
            // PTCH-Global and ACCUM keep the 2-page cycle.
            const bool hasThreePages =
                (_currentSet == 0) || (_currentSet == 1 && !isGlobalPitchHere);
            const int pageCount = hasThreePages ? 3 : 2;
            _topicPage = (_topicPage + 1) % pageCount;
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
            } else if (_currentSet == 4) {
                // MACRO P1: Phase(0) / GWarp(1) / Snap(2)* / Zero(3)*
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

void PhaseFluxEditPage::keyboard(KeyboardEvent &event) {
    // USB keyboard baseline (matches NoteSequenceEditPage):
    //   F1..F5         → hardware F-buttons (Shift modifier preserved)
    //   Tab            → PAGE+SHIFT (= opens context menu)
    //   Left / Right   → hardware Left/Right keys
    //   Up / Down      → encoder rotation (one click per press)
    // Global shortcuts (Escape / Space) come via TopPage.
    if (handleFunctionKeys(event)) return;
    BasePage::keyboard(event);
}

void PhaseFluxEditPage::encoder(EncoderEvent &event) {
    editSlot(_selectedSlot, event.value(), event.pressed() || globalKeyState()[Key::Shift]);
    event.consume();
}

void PhaseFluxEditPage::editSlot(int slot, int value, bool shift) {
    auto &seq = _project.selectedPhaseFluxSequence();
    auto &masterStage = seq.stage(0);
    auto cycle = [](int v, int lo, int hi) { return clamp(v, lo, hi); };
    const bool isGlobalPitch =
        _currentSet == 1 &&
        _project.selectedPhaseFluxSequence().pitchMode() == PhaseFluxSequence::PitchMode::Global;
    const bool isAccumN = (_currentSet == 2);
    const bool isAccumP = (_currentSet == 3);

    // Per-cell edit target: every cell in _stepSelection if any, else just
    // _selectedCell. Multi-cell selection → encoder applies to all. C++11
    // requires explicit std::function (matches ContextMenu's pattern).
    auto forEachCell = [&](const std::function<void(PhaseFluxSequence::Stage&)> &fn) {
        if (_stepSelection.any()) {
            for (int i = 0; i < 16; ++i) {
                if (_stepSelection[i]) fn(seq.stage(i));
            }
        } else {
            fn(seq.stage(_selectedCell));
        }
    };

    if (isAccumN || isAccumP) {
        auto &cfg = isAccumN ? seq.noteAccumConfig() : seq.pulseAccumConfig();
        if (_topicPage == 0) {
            switch (slot) {
            case 0:  // Ac.St — per-cell, UI-clamped to ±7.
                if (isAccumN) {
                    forEachCell([&](PhaseFluxSequence::Stage &s) { s.setAccumulatorStep(clamp(s.accumulatorStep() + value, -7, 7)); });
                } else {
                    forEachCell([&](PhaseFluxSequence::Stage &s) { s.setPulseAccumStep(s.pulseAccumStep() + value); });
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
                // Trig is per-cell — multi-edit applies the same value (cycle ↑↓) to each.
                forEachCell([&](PhaseFluxSequence::Stage &s) {
                    int trigInt = isAccumN ? int(s.accumulatorTrigger()) : int(s.pulseAccumTrigger());
                    trigInt = cycle(trigInt + value, 0, 1);
                    auto next = PhaseFluxSequence::AccumulatorTriggerType(trigInt);
                    if (isAccumN) s.setAccumulatorTrigger(next);
                    else          s.setPulseAccumTrigger(next);
                });
                break;
            }
            case 3: break;  // Sleep reserved (§14.2)
            }
        }
        (void)shift;
        return;
    }
    if (_currentSet == 0) {
        // TEMP — P0: Curve/Warp/Resp/Puls; P1: Len/[FlipV*]/[FlipH*]/Mask;
        // P2: Window/Repeat/—/— (* = handled by togglePressSlot, not editSlot.)
        if (_topicPage == 0) {
            switch (slot) {
            case 0: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setTemporalCurve(PhaseFluxSequence::TemporalCurveType(cycle(int(s.temporalCurve()) + value, 0, 2))); }); break;
            case 1: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setTemporalWarp(ModelUtils::adjusted(s.temporalWarp(), value, -64, 64)); }); break;
            case 2: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setTemporalResponse(ModelUtils::adjusted(s.temporalResponse(), value, -64, 64)); }); break;
            case 3: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setPulseCount(ModelUtils::adjusted(s.pulseCount(), value, 0, 16)); }); break;
            }
        } else if (_topicPage == 1) {
            switch (slot) {
            case 0: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setStageLen(ModelUtils::adjusted(s.stageLen(), value, 0, 127)); }); break;
            // slots 1, 2 are FlipV / FlipH toggles (press-only)
            case 3: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setMask(PhaseFluxSequence::MaskType(cycle(int(s.mask()) + value, 0, 7))); }); break;
            }
        } else {
            // P2 — §14.2 Window + Repeat per axis.
            switch (slot) {
            case 0: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setTemporalWindow(PhaseFluxSequence::WindowType(cycle(int(s.temporalWindow()) + value, 0, 4))); }); break;
            case 1: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setTemporalRepeat(PhaseFluxSequence::RepeatType(cycle(int(s.temporalRepeat()) + value, 0, 3))); }); break;
            // slots 2, 3 reserved
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
            case 0: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setBasePitch(ModelUtils::adjusted(s.basePitch(), value, -64, 64)); }); break;
            case 1: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setPitchRange(PhaseFluxSequence::PitchRangeType(cycle(int(s.pitchRange()) + value, 0, 3))); }); break;
            // slots 2, 3 are master stage's FlipV / FlipH toggles
            }
        }
    } else if (_currentSet == 1) {
        // PTCH Cell — P0: Curve/Warp/Resp/Note; P1: Span/Dir/[FlipV*]/[FlipH*];
        // P2: Window/Repeat/—/—
        if (_topicPage == 0) {
            switch (slot) {
            case 0: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setPitchCurve(PhaseFluxSequence::PitchCurveType(cycle(int(s.pitchCurve()) + value, 0, 3))); }); break;
            case 1: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setPitchWarp(ModelUtils::adjusted(s.pitchWarp(), value, -64, 64)); }); break;
            case 2: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setPitchResponse(ModelUtils::adjusted(s.pitchResponse(), value, -64, 64)); }); break;
            case 3: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setBasePitch(ModelUtils::adjusted(s.basePitch(), value, -64, 64)); }); break;
            }
        } else if (_topicPage == 1) {
            switch (slot) {
            case 0: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setPitchRange(PhaseFluxSequence::PitchRangeType(cycle(int(s.pitchRange()) + value, 0, 3))); }); break;
            case 1: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setPitchDirection(PhaseFluxSequence::PitchDirectionType(cycle(int(s.pitchDirection()) + value, 0, 2))); }); break;
            // slots 2, 3 are active stage's FlipV / FlipH toggles
            }
        } else {
            // P2 — §14.2 Window + Repeat + §6.2.1 MaskM/TiltM for pitch axis.
            switch (slot) {
            case 0: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setPitchWindow(PhaseFluxSequence::WindowType(cycle(int(s.pitchWindow()) + value, 0, 4))); }); break;
            case 1: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setPitchRepeat(PhaseFluxSequence::RepeatType(cycle(int(s.pitchRepeat()) + value, 0, 3))); }); break;
            case 2: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setMaskMelody(s.maskMelody() + value); }); break;
            case 3: forEachCell([&](PhaseFluxSequence::Stage &s) { s.setTiltMelody(s.tiltMelody() + value); }); break;
            }
        }
    } else if (_currentSet == 4) {
        // MACRO — P0: nudges (uniform per-cell offsets); P1: cycle + utilities.
        if (_topicPage == 0) {
            switch (slot) {
            case 0: seq.editWarpNudge(value, shift); break;
            case 1: seq.editResponseNudge(value, shift); break;
            case 2: seq.editPulseNudge(value, shift); break;
            case 3: seq.editLenNudge(value, shift); break;
            }
        } else {
            switch (slot) {
            case 0: seq.editGlobalPhase(value, shift); break;
            case 1: seq.editCyclePhaseWarp(value, shift); break;
            // slots 2 (Snap) and 3 (Zero) are press-to-fire — handled by togglePressSlot
            }
        }
    }
    (void)shift;
}

// Shake — randomize current topic params. wholeTopic=true iterates all pages
// of the topic; false touches only the current page (= what's "guarded by
// Next" in the footer). TEMP and PTCH scope to the selected cell only;
// ACCUM scopes to non-skipped cells; MACRO is sequence-level.
//
// Always excluded ("chassis-feel" — stable across shakes): stageLen,
// basePitch, skip flag. MACRO P1 Snap/Zero slots are press-only, skipped.
// Randomized pulseCount never goes to 0 (silence stays intentional).
void PhaseFluxEditPage::shake(bool wholeTopic) {
    static Random rng;
    auto &seq = _project.selectedPhaseFluxSequence();

    auto pageRange = [&](int maxPage) {
        int first = wholeTopic ? 0 : _topicPage;
        int last  = wholeTopic ? maxPage : _topicPage;
        if (last > maxPage) last = maxPage;
        return std::pair<int, int>{first, last};
    };

    if (_currentSet == 0) {
        // TEMP — selected cell only, 3 pages.
        auto &s = seq.stage(_selectedCell);
        auto pr = pageRange(2); int first = pr.first, last = pr.second;
        for (int page = first; page <= last; ++page) {
            if (page == 0) {
                s.setTemporalCurve(PhaseFluxSequence::TemporalCurveType(rng.nextRange(3)));
                s.setTemporalWarp(int(rng.nextRange(129)) - 64);
                s.setTemporalResponse(int(rng.nextRange(129)) - 64);
                s.setPulseCount(1 + rng.nextRange(16));
            } else if (page == 1) {
                // Len slot skipped (chassis-feel).
                s.setTemporalFlipV(bool(rng.nextRange(2)));
                s.setTemporalFlipH(bool(rng.nextRange(2)));
                s.setMask(PhaseFluxSequence::MaskType(rng.nextRange(8)));
            } else {
                s.setTemporalWindow(PhaseFluxSequence::WindowType(rng.nextRange(5)));
                s.setTemporalRepeat(PhaseFluxSequence::RepeatType(rng.nextRange(4)));
            }
        }
    } else if (_currentSet == 1) {
        // PTCH — selected cell only; pages differ in Global vs Cell mode.
        const bool isGlobalPitch =
            seq.pitchMode() == PhaseFluxSequence::PitchMode::Global;
        auto &active = seq.stage(_selectedCell);
        auto &master = seq.stage(0);
        const int maxPage = isGlobalPitch ? 1 : 2;
        auto pr = pageRange(maxPage); int first = pr.first, last = pr.second;
        for (int page = first; page <= last; ++page) {
            if (isGlobalPitch) {
                if (page == 0) {
                    master.setPitchCurve(PhaseFluxSequence::PitchCurveType(rng.nextRange(4)));
                    master.setPitchWarp(int(rng.nextRange(129)) - 64);
                    master.setPitchResponse(int(rng.nextRange(129)) - 64);
                    seq.setPitchRate(rng.nextRange(PhaseFluxSequence::kPitchRateCount));
                } else {
                    // Note slot (basePitch) skipped (chassis-feel).
                    active.setPitchRange(PhaseFluxSequence::PitchRangeType(rng.nextRange(4)));
                    master.setPitchFlipV(bool(rng.nextRange(2)));
                    master.setPitchFlipH(bool(rng.nextRange(2)));
                }
            } else {
                if (page == 0) {
                    active.setPitchCurve(PhaseFluxSequence::PitchCurveType(rng.nextRange(4)));
                    active.setPitchWarp(int(rng.nextRange(129)) - 64);
                    active.setPitchResponse(int(rng.nextRange(129)) - 64);
                    // Note slot (basePitch) skipped.
                } else if (page == 1) {
                    active.setPitchRange(PhaseFluxSequence::PitchRangeType(rng.nextRange(4)));
                    active.setPitchDirection(PhaseFluxSequence::PitchDirectionType(rng.nextRange(3)));
                    active.setPitchFlipV(bool(rng.nextRange(2)));
                    active.setPitchFlipH(bool(rng.nextRange(2)));
                } else {
                    active.setPitchWindow(PhaseFluxSequence::WindowType(rng.nextRange(5)));
                    active.setPitchRepeat(PhaseFluxSequence::RepeatType(rng.nextRange(4)));
                    active.setMaskMelody(rng.nextRange(101));
                    active.setTiltMelody(rng.nextRange(101));
                }
            }
        }
    } else if (_currentSet == 2 || _currentSet == 3) {
        // ACCUM — non-skipped cells (per-stage step + trigger) + sequence config.
        const bool isN = (_currentSet == 2);
        auto &cfg = isN ? seq.noteAccumConfig() : seq.pulseAccumConfig();
        auto pr = pageRange(1); int first = pr.first, last = pr.second;
        for (int page = first; page <= last; ++page) {
            if (page == 0) {
                // Ac.St (per-cell step) + Order (per-sequence).
                for (auto &s : seq.stages()) {
                    if (s.skip()) continue;
                    if (isN) s.setAccumulatorStep(int(rng.nextRange(31)) - 15);
                    else     s.setPulseAccumStep(int(rng.nextRange(15)) - 7);
                }
                cfg.setOrder(AccumulatorConfig::Order(rng.nextRange(4)));
            } else {
                // Reset + Polar (per-sequence) + Trig (per-cell).
                cfg.setReset(uint8_t(rng.nextRange(16)));
                cfg.setPolarity(AccumulatorConfig::Polarity(rng.nextRange(2)));
                for (auto &s : seq.stages()) {
                    if (s.skip()) continue;
                    auto trig = PhaseFluxSequence::AccumulatorTriggerType(rng.nextRange(2));
                    if (isN) s.setAccumulatorTrigger(trig);
                    else     s.setPulseAccumTrigger(trig);
                }
            }
        }
    } else if (_currentSet == 4) {
        // MACRO — sequence-level. Snap/Zero on P1 are press-only, skipped.
        auto pr = pageRange(1); int first = pr.first, last = pr.second;
        for (int page = first; page <= last; ++page) {
            if (page == 0) {
                seq.setWarpNudge(int(rng.nextRange(129)) - 64);
                seq.setResponseNudge(int(rng.nextRange(129)) - 64);
                seq.setPulseNudge(int(rng.nextRange(31)) - 15);
                seq.setLenNudge(int(rng.nextRange(129)) - 64);
            } else {
                seq.setGlobalPhase(float(rng.nextRange(100)) / 100.f);
                seq.setCyclePhaseWarp(int(rng.nextRange(129)) - 64);
                // Snap (slot 2) + Zero (slot 3) press-only — skipped.
            }
        }
    }
}

// Context menu — top level: INIT (sub) / COPY / PASTE. INIT sub-menu opens
// when INIT is picked.
enum class PhaseFluxContextAction { Init, Copy, Paste, Dup, Last };

static const ContextMenuModel::Item kPhaseFluxTopMenu[] = {
    { "INIT" }, { "COPY" }, { "PASTE" }, { "DUP" },
};

enum class PhaseFluxInitTarget { Stage, Topic, Sequence, Track, Last };

static const ContextMenuModel::Item kPhaseFluxInitMenu[] = {
    { "STAGE" }, { "TOPIC" }, { "SEQUENCE" }, { "TRACK" },
};

void PhaseFluxEditPage::contextShow(bool doubleClick) {
    showContextMenu(ContextMenu(
        kPhaseFluxTopMenu,
        int(PhaseFluxContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); },
        doubleClick
    ));
}

void PhaseFluxEditPage::contextAction(int index) {
    switch (PhaseFluxContextAction(index)) {
    case PhaseFluxContextAction::Init:
        // Open INIT sub-menu — Stage / Topic / Sequence / Track.
        showContextMenu(ContextMenu(
            kPhaseFluxInitMenu,
            int(PhaseFluxInitTarget::Last),
            [&] (int sub) {
                switch (PhaseFluxInitTarget(sub)) {
                case PhaseFluxInitTarget::Stage:    initStage(); break;
                case PhaseFluxInitTarget::Topic:    initTopic(); break;
                case PhaseFluxInitTarget::Sequence: initSequence(); break;
                case PhaseFluxInitTarget::Track:    initTrack(); break;
                case PhaseFluxInitTarget::Last:     break;
                }
            },
            [&] (int) { return true; }
        ));
        break;
    case PhaseFluxContextAction::Copy:  copySequence(); break;
    case PhaseFluxContextAction::Paste: pasteSequence(); break;
    case PhaseFluxContextAction::Dup:   duplicateSequence(); break;
    case PhaseFluxContextAction::Last:  break;
    }
}

bool PhaseFluxEditPage::contextActionEnabled(int index) const {
    switch (PhaseFluxContextAction(index)) {
    case PhaseFluxContextAction::Paste:
        return _model.clipBoard().canPastePhaseFluxSequence();
    default:
        return true;
    }
}

void PhaseFluxEditPage::initStage() {
    auto &seq = _project.selectedPhaseFluxSequence();
    seq.stage(_selectedCell).clear();
    showMessage("STAGE CLEARED");
}

void PhaseFluxEditPage::initTopic() {
    // Reset current topic's params, scoped per the shake convention:
    // TEMP/PTCH = selected cell only; ACCUM = non-skipped cells +
    // sequence-level config; MACRO = sequence-level macros.
    auto &seq = _project.selectedPhaseFluxSequence();
    auto resetStageTemp = [](PhaseFluxSequence::Stage &s) {
        s.setTemporalCurve(PhaseFluxSequence::TemporalCurveType::Linear);
        s.setTemporalWarp(0);
        s.setTemporalResponse(0);
        s.setTemporalFlipV(false);
        s.setTemporalFlipH(false);
        s.setPulseCount(0);
        s.setMask(PhaseFluxSequence::MaskType::Off);
        s.setMaskShift(0);
        s.setPhaseShift(0);
        s.setTemporalWindow(PhaseFluxSequence::WindowType::Off);
        s.setTemporalRepeat(PhaseFluxSequence::RepeatType::x1);
        // stageLen preserved (chassis-feel, mirror of shake exclusion).
    };
    auto resetStagePitch = [](PhaseFluxSequence::Stage &s) {
        s.setPitchCurve(PhaseFluxSequence::PitchCurveType::Ramp);
        s.setPitchWarp(0);
        s.setPitchResponse(0);
        s.setPitchFlipV(false);
        s.setPitchFlipH(false);
        s.setPitchRange(PhaseFluxSequence::PitchRangeType::One);
        s.setPitchDirection(PhaseFluxSequence::PitchDirectionType::Up);
        s.setPitchWindow(PhaseFluxSequence::WindowType::Off);
        s.setPitchRepeat(PhaseFluxSequence::RepeatType::x1);
        s.setMaskMelody(100);
        s.setTiltMelody(0);
        // basePitch preserved (chassis-feel).
    };
    if (_currentSet == 0) {
        resetStageTemp(seq.stage(_selectedCell));
        showMessage("TEMP CELL CLEARED");
    } else if (_currentSet == 1) {
        resetStagePitch(seq.stage(_selectedCell));
        showMessage("PITCH CELL CLEARED");
    } else if (_currentSet == 2 || _currentSet == 3) {
        const bool isN = (_currentSet == 2);
        auto &cfg = isN ? seq.noteAccumConfig() : seq.pulseAccumConfig();
        for (auto &s : seq.stages()) {
            if (s.skip()) continue;
            if (isN) { s.setAccumulatorStep(0); s.setAccumulatorTrigger(PhaseFluxSequence::AccumulatorTriggerType::Stage); }
            else     { s.setPulseAccumStep(0);  s.setPulseAccumTrigger(PhaseFluxSequence::AccumulatorTriggerType::Stage); }
        }
        cfg = AccumulatorConfig();
        if (isN) { cfg.setPosLim(28); cfg.setNegLim(28); }
        else     { cfg.setPosLim(16); cfg.setNegLim(16); }
        showMessage(isN ? "ACCUM.N CLEARED" : "ACCUM.P CLEARED");
    } else if (_currentSet == 4) {
        seq.zeroMacros();
        seq.setGlobalPhase(0.f);
        seq.setTraversalPattern(0);
        showMessage("MACRO CLEARED");
    }
}

void PhaseFluxEditPage::initSequence() {
    _project.selectedPhaseFluxSequence().clear();
    showMessage("SEQUENCE CLEARED");
}

void PhaseFluxEditPage::initTrack() {
    auto &track = _project.selectedTrack();
    track.phaseFluxTrack().clear();
    showMessage("TRACK CLEARED");
}

void PhaseFluxEditPage::copySequence() {
    _model.clipBoard().copyPhaseFluxSequence(_project.selectedPhaseFluxSequence());
    showMessage("COPIED");
}

void PhaseFluxEditPage::pasteSequence() {
    if (!_model.clipBoard().canPastePhaseFluxSequence()) return;
    _model.clipBoard().pastePhaseFluxSequence(_project.selectedPhaseFluxSequence());
    showMessage("PASTED");
}

void PhaseFluxEditPage::duplicateSequence() {
    // PhaseFlux-only duplicate: clone the current pattern's PhaseFluxSequence
    // into the next pattern slot, then leave the user there. Other tracks at
    // the next pattern index are untouched. Mirrors PatternPage::duplicate-
    // Pattern but scoped to PhaseFlux only.
    if (_project.selectedPatternIndex() >= CONFIG_PATTERN_COUNT - 1) {
        showMessage("ALREADY LAST PATTERN");
        return;
    }
    _model.clipBoard().copyPhaseFluxSequence(_project.selectedPhaseFluxSequence());
    _project.editSelectedPatternIndex(1, false);
    _model.clipBoard().pastePhaseFluxSequence(_project.selectedPhaseFluxSequence());
    _model.clipBoard().clear();
    showMessage("DUPLICATED");
}

void PhaseFluxEditPage::togglePressSlot(int slot) {
    // Press-to-flip for binary toggles on Page 1. Topic/page selection by
    // keyPress; this function just performs the actual flip.
    auto &seq = _project.selectedPhaseFluxSequence();
    auto &masterStage = seq.stage(0);
    const bool isGlobalPitch =
        _currentSet == 1 &&
        seq.pitchMode() == PhaseFluxSequence::PitchMode::Global;

    // Multi-cell flip: toggle every selected cell when a selection is active,
    // otherwise toggle just the cursor cell. std::function form for C++11.
    auto flipEachCell = [&](const std::function<void(PhaseFluxSequence::Stage&)> &fn) {
        if (_stepSelection.any()) {
            for (int i = 0; i < 16; ++i) {
                if (_stepSelection[i]) fn(seq.stage(i));
            }
        } else {
            fn(seq.stage(_selectedCell));
        }
    };

    if (_currentSet == 0) {
        // TEMP P1: FlipV(1), FlipH(2)
        switch (slot) {
        case 1: flipEachCell([](PhaseFluxSequence::Stage &s) { s.setTemporalFlipV(!s.temporalFlipV()); }); break;
        case 2: flipEachCell([](PhaseFluxSequence::Stage &s) { s.setTemporalFlipH(!s.temporalFlipH()); }); break;
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
        // PITCH Cell P1: FlipV(2)/FlipH(3) — multi-cell when selection active.
        switch (slot) {
        case 2: flipEachCell([](PhaseFluxSequence::Stage &s) { s.setPitchFlipV(!s.pitchFlipV()); }); break;
        case 3: flipEachCell([](PhaseFluxSequence::Stage &s) { s.setPitchFlipH(!s.pitchFlipH()); }); break;
        default: break;
        }
    } else if (_currentSet == 4) {
        // MACRO P1: Snap(2) / Zero(3) — press-to-fire utilities.
        switch (slot) {
        case 2: seq.snapToGrid(int(_engine.noteDivisor())); break;
        case 3: seq.zeroMacros(); break;
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
        // Selected = cursor cell OR any cell in the multi-cell StepSelection
        // (held in Immediate mode or sticky in Persist mode).
        bool isSelected = (i == _selectedCell) || _stepSelection[i];
        drawGridCell(canvas, i, isActive, isSelected);

        const auto &stage = seq.stage(i);
        if (stage.skip()) continue;

        int row = i / 4;
        int col = i % 4;
        int x = GridX + col * (CellW + CellGap);
        int y = GridY + row * (CellH + CellGap);

        int pulses = std::max(0, std::min(16, stage.pulseCount()));
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

    // Temporal density curve trace — honors §14.2 Window + Repeat. Each
    // sub-section [k/R, (k+1)/R] of the display shows one copy of the
    // (windowed) curve mapped from local [0, 1].
    const int tempRepeat = repeatMultiplier(stage.temporalRepeat());
    const auto temporalWindowType = stage.temporalWindow();
    canvas.setColor(Color::Bright);
    int prevPy = -1;
    bool prevVisible = false;
    for (int px = 0; px < ScopeW - 2; ++px) {
        float t_raw = float(px) / float(ScopeW - 3);
        if (stage.temporalFlipH()) t_raw = 1.f - t_raw;   // v0.2: flipH mirrors the trace
        float t_local = evalTemporalTracePhase(stage, t_raw, tempRepeat);
        if (!isInWindow(t_local, temporalWindowType)) {
            prevVisible = false;
            continue;
        }
        float t_final = evalTemporalValue(stage, t_local);
        int py = y + ScopeH - 2 - int(t_final * float(ScopeH - 3));
        int cx = x + px;
        if (prevVisible) canvas.line(cx - 1, prevPy, cx, py);
        prevPy = py;
        prevVisible = true;
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

    // Match engine effective pulse count: base + pulse-accum counter, clamped 0..16.
    const auto &pulseCfg = seq.pulseAccumConfig();
    const int pulseCounterIdx = (pulseCfg.scope() == AccumulatorConfig::Scope::Local)
        ? stageIdx : 0;
    const int pulseAccOffset = te ? te->pulseAccumCounter(pulseCounterIdx) : 0;
    const int basePulses = std::max(0, std::min(16, stage.pulseCount()));
    const int effective  = std::max(0, std::min(16, stage.pulseCount() + pulseAccOffset));
    const int basesDrawn = std::min(basePulses, effective);

    // §14.2 v0.2 allocation (mirrors engine evalTemporalAlloc): warp-before-repeat.
    for (int i = 0; i < effective; ++i) {
        int subIdx;
        float t_raw_local;
        PhaseFluxMath::evalTemporalAlloc(i, effective, tempRepeat, stage.temporalWarp(), subIdx, t_raw_local);
        if (!isInWindow(t_raw_local, temporalWindowType)) continue;
        float t_final_local = evalTemporalValue(stage, t_raw_local);
        float t_final_global = (float(subIdx) + t_final_local) / float(tempRepeat);
        float t_shifted = t_final_global + float(stage.phaseShift()) * 0.125f;
        t_shifted = std::fmod(t_shifted, 1.f);
        if (t_shifted < 0.f) t_shifted += 1.f;
        if (stage.temporalFlipH()) {   // v0.2: FlipH reverses the schedule
            t_shifted = std::fmod(1.f - t_shifted, 1.f);
            if (t_shifted < 0.f) t_shifted += 1.f;
        }
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

    // §14.2 Window/Repeat for pitch — Cell mode only.
    const bool isGlobalPitchHere =
        seq.pitchMode() == PhaseFluxSequence::PitchMode::Global;
    const int pitchRep = isGlobalPitchHere ? 1 : repeatMultiplier(stage.pitchRepeat());
    const auto pitchWindowType = isGlobalPitchHere
        ? PhaseFluxSequence::WindowType::Off
        : stage.pitchWindow();

    // Curve trace — Medium so the shape reads but doesn't dominate. v0.2:
    // warp->repeat->window(held) all inside evalPitchFull, so the line is
    // continuous with flat held segments where a window clamps.
    canvas.setColor(Color::Medium);
    int prevPy = -1;
    for (int px = 0; px < traceW; ++px) {
        float phi = float(px) / float(std::max(1, traceW - 1));
        float p_final = evalPitchFull(stage, phi, pitchRep, pitchWindowType);
        int py = y + ScopeH - 2 - int(p_final * float(ScopeH - 3));
        int cx = traceX0 + px;
        if (px > 0) canvas.line(cx - 1, prevPy, cx, py);
        prevPy = py;
    }

    // Engine state for playhead-aware highlight.
    const auto *te = trackEngine();
    const bool isActiveCell = te && (te->activeCell() == stageIdx);
    const float stagePhase  = isActiveCell ? te->sequenceProgress() : -1.f;

    // Pulse-fire dots — 2x2 on the curve at each unmuted pulse's (t, pitch).
    // Honors temporal Window (drops pulses outside band) and pitch Window
    // (drops dot when curve sample is in hidden band).
    int pulses = std::max(0, std::min(16, stage.pulseCount()));
    int maskByte = kMaskTable[int(stage.mask())];
    int maskShift = stage.maskShift();
    const int tempRep = repeatMultiplier(stage.temporalRepeat());
    const auto temporalWindowType = stage.temporalWindow();
    // §14.2 v0.2 allocation (mirrors engine evalTemporalAlloc): warp-before-repeat.
    for (int i = 0; i < pulses; ++i) {
        int subIdx;
        float t_raw_local;
        PhaseFluxMath::evalTemporalAlloc(i, pulses, tempRep, stage.temporalWarp(), subIdx, t_raw_local);
        if (!isInWindow(t_raw_local, temporalWindowType)) continue;
        float t_final_local = evalTemporalValue(stage, t_raw_local);
        float t_final_global = (float(subIdx) + t_final_local) / float(tempRep);
        float t_shifted = t_final_global + float(stage.phaseShift()) * 0.125f;
        t_shifted = std::fmod(t_shifted, 1.f);
        if (t_shifted < 0.f) t_shifted += 1.f;
        if (stage.temporalFlipH()) {   // v0.2: FlipH reverses the schedule
            t_shifted = std::fmod(1.f - t_shifted, 1.f);
            if (t_shifted < 0.f) t_shifted += 1.f;
        }
        bool muted = (maskByte >> ((i + maskShift) & 7)) & 1;
        if (muted) continue;
        // Pitch axis (v0.2): window held inside evalPitchFull (no drop).
        float p_final = evalPitchFull(stage, t_shifted, pitchRep, pitchWindowType);
        int dx = traceX0 + int(t_shifted * float(std::max(1, traceW - 1)));
        int dy = y + ScopeH - 2 - int(p_final * float(ScopeH - 3));
        bool passed = (stagePhase >= 0.f) && (stagePhase >= t_shifted);
        canvas.setColor(passed ? Color::Bright : Color::Medium);
        canvas.fillRect(dx - 1, dy - 1, 2, 2);
    }

    // Reachable-degree histogram + top 4 by count.
    const Scale &scale = seq.selectedScale(_model.project().scale());
    const int rootNote = seq.selectedRootNote(_model.project().rootNote());
    const int octave = phaseFluxTrack().octave();
    const int transpose = phaseFluxTrack().transpose();
    const float rangeOctaves = pitchRangeToOctaves_local(stage.pitchRange());
    const int rangeDegrees = std::max(1, int(std::round(rangeOctaves * scale.notesPerOctave())));
    const int baseDegree = stage.basePitch() + octave * scale.notesPerOctave() + transpose;

    // §6.2.1 MaskM/TiltM filter — mirrors PhaseFluxTrackEngine.cpp. Returns
    // true if the absolute degree survives the centrality threshold.
    const int N = scale.notesPerOctave();
    const int maskMel = stage.maskMelody();
    const int tiltMel = stage.tiltMelody();
    auto passesMelodyMask = [&](int absDeg) -> bool {
        if (maskMel >= 100 || N <= 0) return true;
        int degInOct = ((absDeg % N) + N) % N;
        const uint32_t centralityMilli = std::min<uint32_t>(1000,
            uint32_t(stochasticPitchCentrality(degInOct, N)) * 1000u /
            uint32_t(kStochasticPitchCentralityMax));
        const uint32_t tiltMag = uint32_t(tiltMel);
        const uint32_t effectiveMilli =
            ((100 - tiltMag) * centralityMilli + tiltMag * (1000 - centralityMilli)) / 100;
        const uint32_t maskMilli = uint32_t(maskMel) * 10;
        return effectiveMilli >= (1000 - maskMilli);
    };

    // Histogram bucketed by signed offset (kept small for stack use).
    constexpr int kHistBias = 64;
    constexpr int kHistSize = 128;
    int counts[kHistSize] = {0};
    const int samples = 64;
    for (int s = 0; s < samples; ++s) {
        float phi = float(s) / float(samples);
        float p_final = evalPitchFull(stage, phi, pitchRep, pitchWindowType);
        float off = directionOffset(stage.pitchDirection(), p_final, rangeDegrees);
        int deg = int(std::round(off));
        if (!passesMelodyMask(baseDegree + deg)) continue;
        int bin = deg + kHistBias;
        if (bin >= 0 && bin < kHistSize) counts[bin]++;
    }

    // Currently-playing offset (sentinel = INT_MIN-ish). Hidden if filter rejects.
    constexpr int kNoCurrent = -10000;
    int currentOff = kNoCurrent;
    if (isActiveCell && stagePhase >= 0.f) {
        float p_final = evalPitchFull(stage, stagePhase, pitchRep, pitchWindowType);
        int candidate = int(std::round(
            directionOffset(stage.pitchDirection(), p_final, rangeDegrees)));
        if (passesMelodyMask(baseDegree + candidate)) currentOff = candidate;
    }

    // Bottom 3 slots = the 3 most-frequent reachable degrees (raw top-3).
    int freqDeg[3];
    int freqN = 0;
    int countsCopy[kHistSize];
    for (int i = 0; i < kHistSize; ++i) countsCopy[i] = counts[i];
    for (int slot = 0; slot < 3; ++slot) {
        int bestBin = -1;
        int bestCount = 0;
        for (int b = 0; b < kHistSize; ++b) {
            if (countsCopy[b] > bestCount) {
                bestCount = countsCopy[b];
                bestBin = b;
            }
        }
        if (bestBin < 0) break;
        freqDeg[freqN++] = bestBin - kHistBias;
        countsCopy[bestBin] = 0;
    }

    if (currentOff == kNoCurrent && freqN == 0) return;

    // 4 fixed slots: [0] = current note (highlighted; blank when idle),
    // [1..3] = the 3 most-frequent degrees. No pitch sort — fixed positions.
    canvas.setFont(Font::Tiny);
    const int innerY0 = y + 2;
    const int innerH  = ScopeH - 4;
    const int labelRight = x + PitchLabelColW - 2;
    constexpr int kSlots = 4;
    auto drawSlot = [&](int slotIdx, int deg, bool highlight) {
        const int slotCy = innerY0 + ((2 * slotIdx + 1) * innerH) / (2 * kSlots);
        const int baseline = slotCy + 2;
        FixedStringBuilder<5> noteStr;
        scale.noteName(noteStr, baseDegree + deg, rootNote, Scale::Long);
        const int tw = canvas.textWidth(noteStr);
        const int tx = labelRight - tw;
        if (highlight) {
            canvas.setColor(Color::Bright);
            canvas.fillRect(tx - 1, baseline - 5, tw + 1, 7);
            canvas.setBlendMode(BlendMode::Sub);
            canvas.drawText(tx, baseline, noteStr);
            canvas.setBlendMode(BlendMode::Set);
        } else {
            canvas.setColor(Color::Medium);
            canvas.drawText(tx, baseline, noteStr);
        }
    };

    if (currentOff != kNoCurrent) drawSlot(0, currentOff, true);   // top = live note
    for (int i = 0; i < freqN; ++i) drawSlot(1 + i, freqDeg[i], false);
}

void PhaseFluxEditPage::drawMacroScope(Canvas &canvas) {
    // Full-width scope spanning the whole content area below the header.
    // Shows all 16 stages' temporal curves stitched along X, slice widths
    // proportional to effective tick budgets (with LenN applied). Live
    // response to WarpN/RespN/PulsN/GWarp/Phase. Skipped stages have
    // zero-width slices and don't render.
    constexpr int kScopeX = 2;
    constexpr int kScopeY = 12;
    constexpr int kScopeW = 252;   // PageWidth - 4
    constexpr int kScopeH = 38;

    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(Color::Low);
    canvas.drawRect(kScopeX, kScopeY, kScopeW, kScopeH);
    canvas.hline(kScopeX + 1, kScopeY + kScopeH - 2, kScopeW - 2);

    const PhaseFluxSequence &seq = _project.selectedPhaseFluxSequence();
    const int warpNudge = seq.warpNudge();
    const int respNudge = seq.responseNudge();
    const int pulseNudge = seq.pulseNudge();
    const int lenNudge = seq.lenNudge();
    const int gwarp = seq.cyclePhaseWarp();
    const float gwarpParam = PhaseFluxMath::powerBendKnobToParam(gwarp);
    const float gwarpParamInv = PhaseFluxMath::powerBendKnobToParam(-gwarp);
    const float globalPhase = seq.globalPhase();

    // Recompute cumulative ticks UI-side with LenN applied — mirrors
    // PhaseFluxTrackEngine::rebuildCumulativeTable.
    int stageDivisorTicksArr[PhaseFluxMath::kStageCount];
    int stageLenArr[PhaseFluxMath::kStageCount];
    bool skipArr[PhaseFluxMath::kStageCount];
    for (int i = 0; i < PhaseFluxMath::kStageCount; ++i) {
        const auto &s = seq.stage(i);
        stageDivisorTicksArr[i] = PhaseFluxMath::stageDivisorTicks(s.stageDivisor());
        int effLen = s.stageLen() + lenNudge;
        if (effLen < 0)   effLen = 0;
        if (effLen > 127) effLen = 127;
        stageLenArr[i] = effLen;
        skipArr[i] = s.skip();
    }
    int cumulative[PhaseFluxMath::kStageCount + 1];
    int cycleTicks = PhaseFluxMath::computeCumulativeTicks(
        PhaseFluxMath::traversalOrder(seq.traversalPattern()),
        stageDivisorTicksArr, stageLenArr, skipArr,
        seq.divisor(), int(_engine.measureDivisor()),
        seq.clockMultiplier(), cumulative);
    if (cycleTicks <= 0) return;

    const int innerX0 = kScopeX + 1;
    const int innerW  = kScopeW - 2;
    const int innerY0 = kScopeY + 1;
    const int innerH  = kScopeH - 3;

    // Slice boundary tick marks (dotted, faint) — PhaseFlux per-stage edges.
    canvas.setColor(Color::Low);
    const auto &snake = PhaseFluxMath::traversalOrder(seq.traversalPattern());
    for (int slot = 1; slot < PhaseFluxMath::kStageCount; ++slot) {
        float rawPhase = float(cumulative[slot]) / float(cycleTicks);
        float disp = PhaseFluxMath::powerBend(rawPhase, gwarpParam);
        int x = innerX0 + int(disp * float(innerW - 1));
        for (int y = innerY0; y < innerY0 + innerH; y += 2) {
            canvas.point(x, y);
        }
    }

    // Tempo grid dents — short vertical ticks at the bottom edge, one per beat
    // (project timeSignature.noteDivisor() ticks). Bends with GWarp + Phase
    // so the displayed positions follow engine-time.
    const int beatTicks = int(_model.project().timeSignature().noteDivisor());
    if (beatTicks > 0 && cycleTicks > 0) {
        canvas.setColor(Color::Medium);
        const int dentY  = innerY0 + innerH - 5;   // 4 px above baseline
        const int dentLen = 4;
        for (int beatPos = 0; beatPos <= cycleTicks; beatPos += beatTicks) {
            float rawPhase = float(beatPos) / float(cycleTicks);
            float disp = PhaseFluxMath::powerBend(rawPhase, gwarpParam);
            disp -= globalPhase;
            disp -= std::floor(disp);
            int x = innerX0 + int(disp * float(innerW - 1));
            canvas.vline(x, dentY, dentLen);
        }
    }

    // Curve trace — one pixel per X column. Honors temporalRepeat (sub-section
    // splits within the slot) and temporalWindow (Off / Focus / Polarize bands).
    canvas.setColor(Color::Bright);
    int prevPy = -1;
    bool prevVisible = false;
    for (int px = 0; px < innerW; ++px) {
        float dispPhase = float(px) / float(innerW - 1);
        float rawPhase = PhaseFluxMath::powerBend(dispPhase, gwarpParamInv);
        rawPhase += globalPhase;
        rawPhase -= std::floor(rawPhase);
        int ticks = int(rawPhase * float(cycleTicks));

        int slot = 0;
        while (slot < PhaseFluxMath::kStageCount - 1 && ticks >= cumulative[slot + 1]) {
            ++slot;
        }
        int slotW = cumulative[slot + 1] - cumulative[slot];
        if (slotW <= 0) { prevVisible = false; continue; }
        int cell = int(snake[slot]);
        const auto &s = seq.stage(cell);
        if (s.skip()) { prevVisible = false; continue; }

        float t_slot = float(ticks - cumulative[slot]) / float(slotW);
        if (s.temporalFlipH()) t_slot = 1.f - t_slot;   // v0.2: flipH mirrors within slot

        // v0.2 pipeline (WarpN/RespN applied): Warp → Repeat → Window →
        // Curve → Response → FlipV.
        const int tempR = repeatMultiplier(s.temporalRepeat());
        const int effWarp = clamp(s.temporalWarp() + warpNudge, -64, 64);
        const int effResp = clamp(s.temporalResponse() + respNudge, -64, 64);
        float t_warped = (effWarp == 0) ? t_slot
            : PhaseFluxMath::powerBend(clamp01(t_slot), PhaseFluxMath::powerBendKnobToParam(effWarp));
        float t_local = std::fmod(t_warped * float(tempR), 1.f);
        if (!isInWindow(t_local, s.temporalWindow())) {
            prevVisible = false;
            continue;
        }
        float t = PhaseFluxMath::evalResponseFlipV(
            Curve::eval(temporalCurveLut(s.temporalCurve()), t_local), effResp, s.temporalFlipV());

        int py = innerY0 + innerH - 1 - int(t * float(innerH - 1));
        int cx = innerX0 + px;
        if (prevVisible) canvas.line(cx - 1, prevPy, cx, py);
        prevPy = py;
        prevVisible = true;
    }

    // Pulse marks — 1 px dot per fire, centered vertically. Mirrors engine's
    // sub-section algorithm (PhaseFluxTrackEngine.cpp:331..354).
    const int markY = innerY0 + innerH / 2;
    canvas.setColor(Color::Medium);
    for (int slot = 0; slot < PhaseFluxMath::kStageCount; ++slot) {
        int cell = int(snake[slot]);
        const auto &s = seq.stage(cell);
        if (s.skip()) continue;
        int slotW = cumulative[slot + 1] - cumulative[slot];
        if (slotW <= 0) continue;
        int effPulse = s.pulseCount() + pulseNudge;
        if (effPulse < 0) effPulse = 0;
        if (effPulse > 16) effPulse = 16;
        int effWarp = clamp(s.temporalWarp() + warpNudge, -64, 64);
        int effResp = clamp(s.temporalResponse() + respNudge, -64, 64);

        const int tempR = repeatMultiplier(s.temporalRepeat());
        const auto windowType = s.temporalWindow();

        for (int i = 0; i < effPulse; ++i) {
            int subIdx; float t_raw_local;
            PhaseFluxMath::evalTemporalAlloc(i, effPulse, tempR, effWarp, subIdx, t_raw_local);
            if (!isInWindow(t_raw_local, windowType)) continue;

            float t = PhaseFluxMath::evalResponseFlipV(
                Curve::eval(temporalCurveLut(s.temporalCurve()), t_raw_local), effResp, s.temporalFlipV());

            float t_slot_global = (float(subIdx) + t) / float(tempR);
            if (s.temporalFlipH()) t_slot_global = 1.f - t_slot_global;   // v0.2 schedule reversal
            float rawPhase = float(cumulative[slot] + int(t_slot_global * float(slotW))) / float(cycleTicks);
            float disp = PhaseFluxMath::powerBend(rawPhase, gwarpParam);
            disp -= globalPhase;
            disp -= std::floor(disp);
            int x = innerX0 + int(disp * float(innerW - 1));
            canvas.point(x, markY);
        }
    }

    // Playhead — vertical bright line at current cycle position.
    const auto *te = trackEngine();
    if (te && te->isActiveSequence(seq)) {
        const int slotIdx = te->slotIndex();
        const float stagePhase = te->sequenceProgress();
        if (slotIdx >= 0 && slotIdx < PhaseFluxMath::kStageCount && stagePhase >= 0.f) {
            int slotW = cumulative[slotIdx + 1] - cumulative[slotIdx];
            float rawPhase = float(cumulative[slotIdx] + int(stagePhase * float(slotW))) / float(cycleTicks);
            float disp = PhaseFluxMath::powerBend(rawPhase, gwarpParam);
            disp -= globalPhase;
            disp -= std::floor(disp);
            int x = innerX0 + int(disp * float(innerW - 1));
            canvas.setColor(Color::Bright);
            canvas.vline(x, innerY0, innerH);
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

        const int pulses = std::max(0, std::min(16, stage.pulseCount()));
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
    // ACCUM has its own drawAccumPage layout — never reaches here.
    const PhaseFluxSequence &seq = _project.selectedPhaseFluxSequence();
    const auto &activeStage = seq.stage(_selectedCell);
    const auto &masterStage = seq.stage(0);
    const bool isGlobalPitch =
        _currentSet == 1 &&
        seq.pitchMode() == PhaseFluxSequence::PitchMode::Global;
    const auto &pitchSrc = isGlobalPitch ? masterStage : activeStage;

    static const char *kTempCurve[3]  = { "Lin", "Bel", "Bnc" };
    static const char *kPitchCurve[4] = { "Rmp", "Bel", "Tri", "Bnc" };
    static const char *kPitchDir[3]   = { "Up", "Down", "Bi" };
    static const char *kPitchRange[4] = { "1/2", "1", "2", "3" };
    static const char *kWindow[5]     = { "Off", "F70", "F50", "P70", "P50" };
    static const char *kRepeat[4]     = { "x1", "x2", "x3", "x5" };
    static const char *kMask[8]       = { "Off", "1in2", "1in3", "1in4", "2in3", "2in4", "3in4", "1in8" };

    const char *labels[4] = { nullptr, nullptr, nullptr, nullptr };
    FixedStringBuilder<8> values[4];

    if (_currentSet == 0) {
        // TEMP P0: Curve/Warp/Resp/Puls — P1: Len/FlipV/FlipH/Mask — P2: Window/Repeat
        if (_topicPage == 0) {
            labels[0] = "Curve"; values[0]("%s",  kTempCurve[clamp(int(activeStage.temporalCurve()), 0, 2)]);
            labels[1] = "Warp";  values[1]("%+d", activeStage.temporalWarp());
            labels[2] = "Resp";  values[2]("%+d", activeStage.temporalResponse());
            labels[3] = "Puls";  values[3]("%d",  activeStage.pulseCount());
        } else if (_topicPage == 1) {
            labels[0] = "Len";   values[0]("%.2fx", activeStage.stageLen() / 64.0f);
            labels[1] = "FlipV"; values[1]("%s", activeStage.temporalFlipV() ? "On" : "Off");
            labels[2] = "FlipH"; values[2]("%s", activeStage.temporalFlipH() ? "On" : "Off");
            labels[3] = "Mask";  values[3]("%s", kMask[clamp(int(activeStage.mask()), 0, 7)]);
        } else {
            labels[0] = "Wind";  values[0]("%s", kWindow[clamp(int(activeStage.temporalWindow()), 0, 4)]);
            labels[1] = "Rep";   values[1]("%s", kRepeat[clamp(int(activeStage.temporalRepeat()), 0, 3)]);
        }
    } else if (isGlobalPitch) {
        // PTCH Global P0: Curve/Warp/Resp/Rate — P1: Note/Span/FlipV/FlipH (no P2)
        if (_topicPage == 0) {
            labels[0] = "Curve"; values[0]("%s",  kPitchCurve[clamp(int(pitchSrc.pitchCurve()), 0, 3)]);
            labels[1] = "Warp";  values[1]("%+d", pitchSrc.pitchWarp());
            labels[2] = "Resp";  values[2]("%+d", pitchSrc.pitchResponse());
            labels[3] = "Rate";  values[3]("%d:%d",
                PhaseFluxSequence::pitchRateNum(seq.pitchRate()),
                PhaseFluxSequence::pitchRateDen(seq.pitchRate()));
        } else {
            labels[0] = "Note";
            {
                const Scale &scale = seq.selectedScale(_model.project().scale());
                int rootNote = seq.selectedRootNote(_model.project().rootNote());
                scale.noteName(values[0], activeStage.basePitch(), rootNote, Scale::Short1);
            }
            labels[1] = "Span";  values[1]("%s", kPitchRange[clamp(int(activeStage.pitchRange()), 0, 3)]);
            labels[2] = "FlipV"; values[2]("%s", masterStage.pitchFlipV() ? "On" : "Off");
            labels[3] = "FlipH"; values[3]("%s", masterStage.pitchFlipH() ? "On" : "Off");
        }
    } else {
        // PTCH Cell P0: Curve/Warp/Resp/Note — P1: Span/Dir/FlipV/FlipH — P2: Window/Repeat
        if (_topicPage == 0) {
            labels[0] = "Curve"; values[0]("%s",  kPitchCurve[clamp(int(activeStage.pitchCurve()), 0, 3)]);
            labels[1] = "Warp";  values[1]("%+d", activeStage.pitchWarp());
            labels[2] = "Resp";  values[2]("%+d", activeStage.pitchResponse());
            labels[3] = "Note";
            {
                const Scale &scale = seq.selectedScale(_model.project().scale());
                int rootNote = seq.selectedRootNote(_model.project().rootNote());
                scale.noteName(values[3], activeStage.basePitch(), rootNote, Scale::Short1);
            }
        } else if (_topicPage == 1) {
            labels[0] = "Span";  values[0]("%s", kPitchRange[clamp(int(activeStage.pitchRange()), 0, 3)]);
            labels[1] = "Dir";   values[1]("%s", kPitchDir[clamp(int(activeStage.pitchDirection()), 0, 2)]);
            labels[2] = "FlipV"; values[2]("%s", activeStage.pitchFlipV() ? "On" : "Off");
            labels[3] = "FlipH"; values[3]("%s", activeStage.pitchFlipH() ? "On" : "Off");
        } else {
            labels[0] = "Wind";  values[0]("%s", kWindow[clamp(int(activeStage.pitchWindow()), 0, 4)]);
            labels[1] = "Rep";   values[1]("%s", kRepeat[clamp(int(activeStage.pitchRepeat()), 0, 3)]);
            labels[2] = "Mask";  values[2]("%d", activeStage.maskMelody());
            labels[3] = "Tilt";  values[3]("%d", activeStage.tiltMelody());
        }
    }
    // MACRO topic doesn't reach this function (full-width scope replaces
    // the param panel; values are encoded in the scope shape).

    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);

    // 4 rows now (was 5). Row height = ParamPanelH / 4.
    const int rowH = ParamPanelH / 4;
    const int valueColW = ParamPanelW / 3;
    const int valueColX = ParamPanelX + ParamPanelW - valueColW;

    for (int i = 0; i < 4; ++i) {
        if (labels[i] == nullptr) continue;
        int ry = ParamPanelY + i * rowH;
        int rh = rowH - 1;
        bool isSelected = (i == _selectedSlot);

        if (isSelected) {
            canvas.setColor(Color::Bright);
            canvas.drawRect(valueColX, ry, valueColW, rh);
        }
        canvas.setColor(Color::Medium);
        canvas.drawText(ParamPanelX + 2, ry + rh - 2, labels[i]);
        canvas.setColor(isSelected ? Color::Bright : Color::MediumBright);
        int vw = canvas.textWidth(values[i]);
        canvas.drawText(ParamPanelX + ParamPanelW - 2 - vw, ry + rh - 2, values[i]);
    }
}
