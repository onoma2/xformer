#pragma once

#include "AccumulatorConfig.h"
#include "Config.h"
#include "Bitfield.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Types.h"
#include "Scale.h"
#include "Routing.h"
#include "RouteParamKey.h"
#include "ProjectVersion.h"

#include "core/math/Math.h"
#include "core/utils/StringBuilder.h"

#include <array>
#include <cstdint>

class PhaseFluxSequence {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    static constexpr int StageCount = 16;

    // Width typedefs feed Bitfield + range checks. Keep names verbose so
    // call sites read like the spec table (§5).
    using PulseCount = UnsignedValue<5>;  // 0..31 storage; UI/engine clamp to 0..16. 0 = silent stage.
    using BasePitch = SignedValue<7>;
    using PitchRange = UnsignedValue<2>;
    using PitchDirection = UnsignedValue<2>;
    using TemporalCurve = UnsignedValue<2>;
    using TemporalWarp = SignedValue<7>;
    using TemporalResponse = SignedValue<7>;
    using PitchCurve = UnsignedValue<2>;
    using PitchWarp = SignedValue<7>;
    using PitchResponse = SignedValue<7>;
    using MaskMelody = UnsignedValue<7>;
    using TiltMelody = UnsignedValue<7>;
    using PhaseShift = UnsignedValue<3>;
    using Mask = UnsignedValue<3>;
    using MaskShift = UnsignedValue<3>;
    using AccumulatorStep = SignedValue<5>;
    using PulseAccumStep = SignedValue<4>;
    using GateLength = UnsignedValue<7>;
    using StageDivisor = UnsignedValue<3>;
    using StageLen = UnsignedValue<7>;   // 0..127 mapped to 0..~2× factor via /64
    using Repeat = UnsignedValue<2>;     // x1 / x2 / x3 / x5 (RepeatType enum)
    using Window = UnsignedValue<3>;     // Off / F70 / F50 / P70 / P50 (WindowType, 3 spare)

    enum class PitchRangeType : uint8_t {
        Half = 0,    // 0.5 octave
        One = 1,     // 1 octave (default)
        Two = 2,     // 2 octaves
        Three = 3,   // 3 octaves
    };

    enum class PitchDirectionType : uint8_t {
        Up = 0,
        Down = 1,
        Bipolar = 2,
        // 3 reserved
    };

    enum class TemporalCurveType : uint8_t {
        Linear = 0,
        Bell = 1,
        Bounce = 2,
        // 3 reserved
    };

    // Pitch base shape. 4 slots filled (2-bit field full).
    // Bounce = ExpDown3x (3 cascading exp drops) — multi-peak/weird shape
    // matching the temporal Bounce slot but applied to pitch CV instead of
    // trigger timing.
    enum class PitchCurveType : uint8_t {
        Ramp = 0,
        Bell = 1,
        Triangle = 2,
        Bounce = 3,    // ExpDown3x — 3 cascading exp drops, mirrors temporal Bounce
    };

    enum class MaskType : uint8_t {
        Off = 0,
        OneInTwo = 1,
        OneInThree = 2,
        OneInFour = 3,
        TwoInThree = 4,
        TwoInFour = 5,
        ThreeInFour = 6,
        OneInEight = 7,
    };

    enum class AccumulatorTriggerType : uint8_t {
        Stage = 0,
        Pulse = 1,
    };

    // §14.2 — per-stage Repeat enum. 4 slots filled (2-bit field full).
    enum class RepeatType : uint8_t {
        x1 = 0,    // no repeat (default)
        x2 = 1,
        x3 = 2,
        x5 = 3,
    };

    // §14.2 — per-stage Window enum. 5 slots filled, 3 reserved.
    enum class WindowType : uint8_t {
        Off        = 0,    // full curve visible (default)
        Focus70    = 1,    // center 70% kept, outer 30% hidden
        Focus50    = 2,    // center 50% kept, outer 50% hidden
        Polarize70 = 3,    // outer 70% kept (35% each side), middle 30% hidden
        Polarize50 = 4,    // outer 50% kept (25% each side), middle 50% hidden
        // 5..7 reserved
    };

    // 3-bit slot index. Stochastic-style multiplier against sequence divisor —
    // labels follow seqDivisor live (printStageDivisor uses ModelUtils::
    // printDivisorShort against the effective divisor). Default slot 6 (Bar)
    // gives one-bar cells at the NoteTrack-default seqDivisor=12.
    enum class StageDivisorSlot : uint8_t {
        Sixteenth = 0,    // ×1     = 1/16 at seqDiv=12
        EighthT   = 1,    // ×4/3   = 1/8T
        Eighth    = 2,    // ×2     = 1/8
        QuarterT  = 3,    // ×8/3   = 1/4T
        Quarter   = 4,    // ×4     = 1/4
        Half      = 5,    // ×8     = 1/2
        Bar       = 6,    // ×16    = 1 bar  ← DEFAULT
        TwoBar    = 7,    // ×32    = 2 bars
    };

    class Stage {
    public:
        //----------------------------------------
        // Properties
        //----------------------------------------

        // pulseCount — 0..16. 0 = silent stage (duration consumed, no events).
        int pulseCount() const { return int(_data3.pulseCount); }
        void setPulseCount(int v) { _data3.pulseCount = PulseCount::clamp(clamp(v, 0, 16)); }

        // basePitch — ±63 scale degrees
        int basePitch() const { return BasePitch::Min + int(_data0.basePitch); }
        void setBasePitch(int v) { _data0.basePitch = BasePitch::clamp(v) - BasePitch::Min; }

        // pitchRange — enum slot
        PitchRangeType pitchRange() const { return PitchRangeType(int(_data0.pitchRange)); }
        void setPitchRange(PitchRangeType v) { _data0.pitchRange = PitchRange::clamp(int(v)); }

        // pitchDirection — enum slot (1 reserved slot at idx 3)
        PitchDirectionType pitchDirection() const { return PitchDirectionType(int(_data0.pitchDirection)); }
        void setPitchDirection(PitchDirectionType v) { _data0.pitchDirection = PitchDirection::clamp(int(v)); }

        // pitchCurve — enum slot (1 reserved slot at idx 3)
        PitchCurveType pitchCurve() const { return PitchCurveType(int(_data0.pitchCurve)); }
        void setPitchCurve(PitchCurveType v) { _data0.pitchCurve = PitchCurve::clamp(int(v)); }

        // pitchFlipV / pitchFlipH
        bool pitchFlipV() const { return bool(_data0.pitchFlipV); }
        void setPitchFlipV(bool v) { _data0.pitchFlipV = v; }
        bool pitchFlipH() const { return bool(_data0.pitchFlipH); }
        void setPitchFlipH(bool v) { _data0.pitchFlipH = v; }

        // pitchWarp / pitchResponse — ±63 raw; PowerBend mapping is engine-side
        int pitchWarp() const { return PitchWarp::Min + int(_data0.pitchWarp); }
        void setPitchWarp(int v) { _data0.pitchWarp = PitchWarp::clamp(v) - PitchWarp::Min; }
        int pitchResponse() const { return PitchResponse::Min + int(_data0.pitchResponse); }
        void setPitchResponse(int v) { _data0.pitchResponse = PitchResponse::clamp(v) - PitchResponse::Min; }

        // temporalCurve — enum slot (1 reserved slot at idx 3)
        TemporalCurveType temporalCurve() const { return TemporalCurveType(int(_data1.temporalCurve)); }
        void setTemporalCurve(TemporalCurveType v) { _data1.temporalCurve = TemporalCurve::clamp(int(v)); }

        // temporalFlipV / temporalFlipH
        bool temporalFlipV() const { return bool(_data1.temporalFlipV); }
        void setTemporalFlipV(bool v) { _data1.temporalFlipV = v; }
        bool temporalFlipH() const { return bool(_data1.temporalFlipH); }
        void setTemporalFlipH(bool v) { _data1.temporalFlipH = v; }

        // temporalWarp / temporalResponse — ±63 raw
        int temporalWarp() const { return TemporalWarp::Min + int(_data1.temporalWarp); }
        void setTemporalWarp(int v) { _data1.temporalWarp = TemporalWarp::clamp(v) - TemporalWarp::Min; }
        int temporalResponse() const { return TemporalResponse::Min + int(_data1.temporalResponse); }
        void setTemporalResponse(int v) { _data1.temporalResponse = TemporalResponse::clamp(v) - TemporalResponse::Min; }

        // maskMelody / tiltMelody — 0..100 (UnsignedValue<7> clamp covers 0..127)
        int maskMelody() const { return int(_data1.maskMelody); }
        void setMaskMelody(int v) { _data1.maskMelody = MaskMelody::clamp(clamp(v, 0, 100)); }
        int tiltMelody() const { return int(_data1.tiltMelody); }
        void setTiltMelody(int v) { _data1.tiltMelody = TiltMelody::clamp(clamp(v, 0, 100)); }

        // phaseShift 0..7
        int phaseShift() const { return int(_data2.phaseShift); }
        void setPhaseShift(int v) { _data2.phaseShift = PhaseShift::clamp(v); }

        // mask enum
        MaskType mask() const { return MaskType(int(_data2.mask)); }
        void setMask(MaskType v) { _data2.mask = Mask::clamp(int(v)); }

        // maskShift 0..7
        int maskShift() const { return int(_data2.maskShift); }
        void setMaskShift(int v) { _data2.maskShift = MaskShift::clamp(v); }

        // accumulatorStep — ±15 scale degrees
        int accumulatorStep() const { return AccumulatorStep::Min + int(_data2.accumulatorStep); }
        void setAccumulatorStep(int v) { _data2.accumulatorStep = AccumulatorStep::clamp(v) - AccumulatorStep::Min; }

        // pulseAccumStep — ±7 pulses per trigger (signed 4-bit)
        int pulseAccumStep() const { return PulseAccumStep::Min + int(_data2.pulseAccumStep); }
        void setPulseAccumStep(int v) { _data2.pulseAccumStep = PulseAccumStep::clamp(v) - PulseAccumStep::Min; }

        // accumulatorTrigger — Stage or Pulse (§13.4)
        AccumulatorTriggerType accumulatorTrigger() const { return bool(_data2.accumulatorTrigger) ? AccumulatorTriggerType::Pulse : AccumulatorTriggerType::Stage; }
        void setAccumulatorTrigger(AccumulatorTriggerType v) { _data2.accumulatorTrigger = bool(uint8_t(v) & 1); }

        // pulseAccumTrigger — Stage or Pulse (§13.4)
        AccumulatorTriggerType pulseAccumTrigger() const { return bool(_data2.pulseAccumTrigger) ? AccumulatorTriggerType::Pulse : AccumulatorTriggerType::Stage; }
        void setPulseAccumTrigger(AccumulatorTriggerType v) { _data2.pulseAccumTrigger = bool(uint8_t(v) & 1); }

        // gateLength 0..100
        int gateLength() const { return int(_data2.gateLength); }
        void setGateLength(int v) { _data2.gateLength = GateLength::clamp(clamp(v, 0, 100)); }

        // stageDivisor — slot index 0..7
        StageDivisorSlot stageDivisor() const { return StageDivisorSlot(int(_data2.stageDivisor)); }
        void setStageDivisor(StageDivisorSlot v) { _data2.stageDivisor = StageDivisor::clamp(int(v)); }

        // skip
        bool skip() const { return bool(_data2.skip); }
        void setSkip(bool v) { _data2.skip = v; }

        // stageLen — ±100 multiplier (encoded width clamps to SignedValue<7>)
        // stageLen — 0..127 unipolar. Stored value × enumTicks / 64 gives the
        // effective stage duration (Phaseque STEP_LEN pattern). Default = 64
        // (= 1×, transparent). 0 = silent stage. 127 ≈ 2× stretch.
        int stageLen() const { return int(_data3.stageLen); }
        void setStageLen(int v) { _data3.stageLen = StageLen::clamp(clamp(v, 0, 127)); }

        // temporalRepeat / pitchRepeat — per-axis enum, x1/x2/x3/x5.
        RepeatType temporalRepeat() const { return RepeatType(int(_data3.temporalRepeat)); }
        void setTemporalRepeat(RepeatType v) { _data3.temporalRepeat = Repeat::clamp(int(v)); }
        RepeatType pitchRepeat() const { return RepeatType(int(_data3.pitchRepeat)); }
        void setPitchRepeat(RepeatType v) { _data3.pitchRepeat = Repeat::clamp(int(v)); }

        // temporalWindow / pitchWindow — per-axis 5-value enum.
        WindowType temporalWindow() const { return WindowType(int(_data3.temporalWindow)); }
        void setTemporalWindow(WindowType v) { _data3.temporalWindow = Window::clamp(int(v)); }
        WindowType pitchWindow() const { return WindowType(int(_data3.pitchWindow)); }
        void setPitchWindow(WindowType v) { _data3.pitchWindow = Window::clamp(int(v)); }

        //----------------------------------------
        // Methods
        //----------------------------------------

        Stage() { clear(); }

        void clear();

        void write(VersionedSerializedWriter &writer) const;
        void read(VersionedSerializedReader &reader);

        bool operator==(const Stage &other) const {
            return _data0.raw == other._data0.raw &&
                   _data1.raw == other._data1.raw &&
                   _data2.raw == other._data2.raw &&
                   _data3.raw == other._data3.raw;
        }
        bool operator!=(const Stage &other) const { return !(*this == other); }

    private:
        // Layout: 4 × uint32_t = 128 bits envelope.

        // word 0 — pitch shape (29 used, 3 spare bits 29..31)
        union {
            uint32_t raw;
            BitField<uint32_t,  0, BasePitch::Bits>       basePitch;       //  0..6
            BitField<uint32_t,  7, PitchRange::Bits>      pitchRange;      //  7..8
            BitField<uint32_t,  9, PitchDirection::Bits>  pitchDirection;  //  9..10
            BitField<uint32_t, 11, PitchCurve::Bits>      pitchCurve;      // 11..12
            BitField<uint32_t, 13, 1>                     pitchFlipV;      // 13
            BitField<uint32_t, 14, 1>                     pitchFlipH;      // 14
            BitField<uint32_t, 15, PitchWarp::Bits>       pitchWarp;       // 15..21
            BitField<uint32_t, 22, PitchResponse::Bits>   pitchResponse;   // 22..28
            // bits 29..31 spare (3)
        } _data0;

        // word 1 — temporal shape + melody mask (32 used, 0 spare)
        union {
            uint32_t raw;
            BitField<uint32_t,  0, TemporalCurve::Bits>    temporalCurve;    //  0..1
            BitField<uint32_t,  2, 1>                      temporalFlipV;    //  2
            BitField<uint32_t,  3, 1>                      temporalFlipH;    //  3
            BitField<uint32_t,  4, TemporalWarp::Bits>     temporalWarp;     //  4..10
            BitField<uint32_t, 11, TemporalResponse::Bits> temporalResponse; // 11..17
            BitField<uint32_t, 18, MaskMelody::Bits>       maskMelody;       // 18..24
            BitField<uint32_t, 25, TiltMelody::Bits>       tiltMelody;       // 25..31
        } _data1;

        // word 2 — mask / accum / gate / divisor / skip / triggers (31 used, 1 spare bit 31)
        union {
            uint32_t raw;
            BitField<uint32_t,  0, PhaseShift::Bits>         phaseShift;          //  0..2
            BitField<uint32_t,  3, Mask::Bits>               mask;                //  3..5
            BitField<uint32_t,  6, MaskShift::Bits>          maskShift;           //  6..8
            BitField<uint32_t,  9, AccumulatorStep::Bits>    accumulatorStep;     //  9..13
            BitField<uint32_t, 14, PulseAccumStep::Bits>     pulseAccumStep;      // 14..17
            BitField<uint32_t, 18, GateLength::Bits>         gateLength;          // 18..24
            BitField<uint32_t, 25, StageDivisor::Bits>       stageDivisor;        // 25..27
            BitField<uint32_t, 28, 1>                        skip;                // 28
            BitField<uint32_t, 29, 1>                        accumulatorTrigger;  // 29
            BitField<uint32_t, 30, 1>                        pulseAccumTrigger;   // 30
            // bit 31 spare
        } _data2;

        // word 3 — stageLen + Repeat × 2 + Window × 2 + pulseCount (22 used, 10 spare)
        union {
            uint32_t raw;
            BitField<uint32_t,  0, StageLen::Bits>   stageLen;        //  0..6
            BitField<uint32_t,  7, Repeat::Bits>     pitchRepeat;     //  7..8
            BitField<uint32_t,  9, Repeat::Bits>     temporalRepeat;  //  9..10
            BitField<uint32_t, 11, Window::Bits>     pitchWindow;     // 11..13
            BitField<uint32_t, 14, Window::Bits>     temporalWindow;  // 14..16
            BitField<uint32_t, 17, PulseCount::Bits> pulseCount;      // 17..21
            // bits 22..31 spare (10)
        } _data3;
    };

    using StageArray = std::array<Stage, StageCount>;

    //----------------------------------------
    // Properties
    //----------------------------------------

    // trackIndex — back-reference set by parent track
    int trackIndex() const { return _trackIndex; }
    void setTrackIndex(int v) { _trackIndex = v; }

    // scale — −1 = inherit
    int scale() const { return Routing::routedValueInt(ParamKey::Scale, _trackIndex, _scale, 0, 23); }
    void setScale(int v) { _scale = clamp(v, -1, Scale::Count - 1); }
    const Scale &selectedScale(int defaultScale) const {
        return Scale::get(scale() != -1 ? scale() : defaultScale);
    }
    void editScale(int value, bool) { setScale(_scale + value); }
    void printScale(StringBuilder &str) const {
        printRouted(str, Routing::Target::Scale);
        str(scale() < 0 ? "Default" : Scale::name(scale()));
    }

    // rootNote — −1 = inherit
    int rootNote() const { return Routing::routedValueInt(ParamKey::RootNote, _trackIndex, _rootNote, 0, 11); }
    void setRootNote(int v) { _rootNote = clamp(v, -1, 11); }
    int selectedRootNote(int defaultRootNote) const {
        return rootNote() != -1 ? rootNote() : defaultRootNote;
    }
    void editRootNote(int value, bool) { setRootNote(_rootNote + value); }
    void printRootNote(StringBuilder &str) const {
        printRouted(str, Routing::Target::RootNote);
        if (rootNote() < 0) { str("Default"); } else { Types::printNote(str, rootNote()); }
    }

    // divisor — Routable<uint16_t>, matches NoteSequence convention
    int divisor() const { return Routing::routedValueInt(ParamKey::Divisor, _trackIndex, _divisor, 1, 768); }
    void setDivisor(int v) {
        _divisor = ModelUtils::clampDivisor(v);
    }
    void editDivisor(int value, bool shift) {
        setDivisor(ModelUtils::adjustedByDivisor(_divisor, value, shift));
    }
    void printDivisor(StringBuilder &str) const {
        printRouted(str, Routing::Target::Divisor);
        ModelUtils::printDivisor(str, divisor());
    }

    // clockMultiplier — Routable<uint8_t>, 50..150
    int clockMultiplier() const { return Routing::routedValueInt(ParamKey::ClockMultiplier, _trackIndex, _clockMultiplier, 50, 150); }
    void setClockMultiplier(int v) {
        _clockMultiplier = clamp(v, 50, 150);
    }
    void editClockMultiplier(int value, bool shift) {
        setClockMultiplier(_clockMultiplier + value * (shift ? 10 : 1));
    }
    void printClockMultiplier(StringBuilder &str) const {
        printRouted(str, Routing::Target::ClockMult);
        str("%.2fx", clockMultiplier() * 0.01f);
    }

    // resetMeasure 0..128
    int resetMeasure() const { return _resetMeasure; }
    void setResetMeasure(int v) { _resetMeasure = clamp(v, 0, 128); }
    void editResetMeasure(int value, bool shift) {
        setResetMeasure(ModelUtils::adjustedByPowerOfTwo(resetMeasure(), value, shift));
    }
    void printResetMeasure(StringBuilder &str) const {
        if (resetMeasure() == 0) { str("off"); }
        else { str("%d %s", resetMeasure(), resetMeasure() > 1 ? "bars" : "bar"); }
    }

    // globalPhase — fraction [0, 1] of cycle to shift the read phase by.
    // CurveTrack precedent (model/CurveTrack.h:201). Stored as plain float.
    float globalPhase() const { return Routing::routedValue(ParamKey::Phase, _trackIndex, _globalPhase, 0.f, 1.f); }
    void setGlobalPhase(float v) { _globalPhase = clamp(v, 0.f, 1.f); }
    void editGlobalPhase(int value, bool shift) {
        setGlobalPhase(_globalPhase + value * (shift ? 0.1f : 0.01f));
    }
    void printGlobalPhase(StringBuilder &str) const {
        printRouted(str, Routing::Target::Phase);
        str("%.2f", globalPhase());
    }

    // MACRO nudges — per-sequence offsets added to every stage's per-cell value
    // live. §14.2 design sketches. Plain int8_t (not Routable for v1).
    int warpNudge() const { return _warpNudge; }
    void setWarpNudge(int v) { _warpNudge = clamp(v, -64, 64); }
    void editWarpNudge(int value, bool shift) {
        setWarpNudge(warpNudge() + value * (shift ? 4 : 1));
    }
    void printWarpNudge(StringBuilder &str) const { str("%+d", warpNudge()); }

    int responseNudge() const { return _responseNudge; }
    void setResponseNudge(int v) { _responseNudge = clamp(v, -64, 64); }
    void editResponseNudge(int value, bool shift) {
        setResponseNudge(responseNudge() + value * (shift ? 4 : 1));
    }
    void printResponseNudge(StringBuilder &str) const { str("%+d", responseNudge()); }

    int pulseNudge() const { return _pulseNudge; }
    void setPulseNudge(int v) { _pulseNudge = clamp(v, -15, 15); }
    void editPulseNudge(int value, bool) { setPulseNudge(pulseNudge() + value); }
    void printPulseNudge(StringBuilder &str) const { str("%+d", pulseNudge()); }

    int lenNudge() const { return _lenNudge; }
    void setLenNudge(int v) { _lenNudge = clamp(v, -64, 64); }
    void editLenNudge(int value, bool shift) {
        setLenNudge(lenNudge() + value * (shift ? 4 : 1));
    }
    void printLenNudge(StringBuilder &str) const { str("%+d", lenNudge()); }

    int cyclePhaseWarp() const { return _cyclePhaseWarp; }
    void setCyclePhaseWarp(int v) { _cyclePhaseWarp = clamp(v, -64, 64); }
    void editCyclePhaseWarp(int value, bool shift) {
        setCyclePhaseWarp(cyclePhaseWarp() + value * (shift ? 4 : 1));
    }
    void printCyclePhaseWarp(StringBuilder &str) const { str("%+d", cyclePhaseWarp()); }

    // traversalPattern — picks which of the 8 grid-walk patterns the engine
    // uses to map slotIdx → cellIdx. Index 0 = PhaseFlux original snake
    // (default). 1..7 = Make Noise René mk2 Access Patterns 1..7.
    int traversalPattern() const { return _traversalPattern; }
    void setTraversalPattern(int v) { _traversalPattern = clamp(v, 0, 7); }
    void editTraversalPattern(int value, bool) { setTraversalPattern(traversalPattern() + value); }
    void printTraversalPattern(StringBuilder &str) const;

    // Snap to grid — press-to-fire from MACRO P1. Two passes:
    //  1. globalPhase → nearest 1/16 of cycle.
    //  2. Each non-skipped stage's stageLen → nearest whole project beat
    //     (TimeSignature.noteDivisor — 1/4 in /4 signatures, 1/8 in /8).
    //     Floor at 1 beat — never snap a cell to 0 length. Silent cells
    //     (stageLen=0) are left alone, they're intentional.
    // beatTicks in master-PPQN-192 ticks; caller passes engine noteDivisor.
    void snapToGrid(int beatTicks);

    // Reset all 5 magnitude macros (nudges + cyclePhaseWarp) to 0.
    // globalPhase deliberately untouched — different category.
    void zeroMacros() {
        _warpNudge = 0;
        _responseNudge = 0;
        _pulseNudge = 0;
        _lenNudge = 0;
        _cyclePhaseWarp = 0;
    }

    // pitchRate — P:T ratio table index for Global pitch mode. 1:1 = locked.
    static constexpr int kPitchRateCount = 17;
    static int pitchRateNum(int index);
    static int pitchRateDen(int index);
    static int defaultPitchRateIndex();  // index of 1:1

    int pitchRate() const { return _pitchRate; }
    void setPitchRate(int v) { _pitchRate = clamp(v, 0, kPitchRateCount - 1); }
    void editPitchRate(int value, bool) {
        setPitchRate(pitchRate() + value);
    }
    void printPitchRate(StringBuilder &str) const {
        str("%d:%d", pitchRateNum(_pitchRate), pitchRateDen(_pitchRate));
    }

    // pitchMode — Cell = per-stage curves (today). Global = stage[0] curve
    // driven by free-running pitchPhase. Sequence-owned so different sequences
    // on the same track can use different modes.
    enum class PitchMode : uint8_t {
        Cell,
        Global,
        Last
    };

    static const char *pitchModeName(PitchMode mode) {
        switch (mode) {
        case PitchMode::Cell:   return "Cell";
        case PitchMode::Global: return "Global";
        case PitchMode::Last:   break;
        }
        return nullptr;
    }

    PitchMode pitchMode() const { return _pitchMode; }
    void setPitchMode(PitchMode mode) { _pitchMode = ModelUtils::clampedEnum(mode); }
    void editPitchMode(int value, bool) { setPitchMode(ModelUtils::adjustedEnum(pitchMode(), value)); }
    void printPitchMode(StringBuilder &str) const { str(pitchModeName(pitchMode())); }

    // cycleLength — Adaptive = skipped stages drop out of cycle (current).
    // Fixed = skipped stages stay in cycle but emit silence (Note-Track-like).
    enum class CycleLengthMode : uint8_t {
        Adaptive,
        Fixed,
        Last
    };

    static const char *cycleLengthModeName(CycleLengthMode mode) {
        switch (mode) {
        case CycleLengthMode::Adaptive: return "Adaptive";
        case CycleLengthMode::Fixed:    return "Fixed";
        case CycleLengthMode::Last:     break;
        }
        return nullptr;
    }

    CycleLengthMode cycleLength() const { return _cycleLength; }
    void setCycleLength(CycleLengthMode mode) { _cycleLength = ModelUtils::clampedEnum(mode); }
    void editCycleLength(int value, bool) { setCycleLength(ModelUtils::adjustedEnum(cycleLength(), value)); }
    void printCycleLength(StringBuilder &str) const { str(cycleLengthModeName(cycleLength())); }

    // accumulator configs — per-sequence note/pulse counter behavior (spec §13.3)
    const AccumulatorConfig &noteAccumConfig() const { return _noteAccumConfig; }
          AccumulatorConfig &noteAccumConfig()       { return _noteAccumConfig; }
    const AccumulatorConfig &pulseAccumConfig() const { return _pulseAccumConfig; }
          AccumulatorConfig &pulseAccumConfig()       { return _pulseAccumConfig; }

    // accumulator mode — Stage = per-cell counter (each stage drifts alone),
    // Sequence = shared counter (whole pattern transposes together).
    static const char *accumModeName(AccumulatorConfig::Scope scope) {
        return scope == AccumulatorConfig::Scope::Track ? "Sequence" : "Stage";
    }
    void editNoteAccumMode(int value, bool) {
        int v = (int(_noteAccumConfig.scope()) + value) & 1;
        _noteAccumConfig.setScope(AccumulatorConfig::Scope(v));
    }
    void printNoteAccumMode(StringBuilder &str) const { str(accumModeName(_noteAccumConfig.scope())); }
    void editPulseAccumMode(int value, bool) {
        int v = (int(_pulseAccumConfig.scope()) + value) & 1;
        _pulseAccumConfig.setScope(AccumulatorConfig::Scope(v));
    }
    void printPulseAccumMode(StringBuilder &str) const { str(accumModeName(_pulseAccumConfig.scope())); }

    // edited — dirty bit for UI
    int edited() const { return _edited; }
    void setEdited(int v) { _edited = v; }

    // stages
    const StageArray &stages() const { return _stages; }
          StageArray &stages()       { return _stages; }
    const Stage &stage(int index) const { return _stages[index]; }
          Stage &stage(int index)       { return _stages[index]; }

    //----------------------------------------
    // Routing
    //----------------------------------------

    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    inline void printRouted(StringBuilder &str, Routing::Target target) const { Routing::printRouted(str, target, _trackIndex); }

    //----------------------------------------
    // Methods
    //----------------------------------------

    PhaseFluxSequence() { clear(); }

    void clear();

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    int8_t _trackIndex = -1;
    int8_t _scale = -1;
    int8_t _rootNote = -1;
    uint8_t _resetMeasure = 0;
    uint8_t _edited = 0;
    uint8_t _pitchRate = 0;  // set to defaultPitchRateIndex() in clear()
    PitchMode _pitchMode = PitchMode::Cell;
    CycleLengthMode _cycleLength = CycleLengthMode::Fixed;
    float _globalPhase = 0.f;
    int8_t _warpNudge = 0;
    int8_t _responseNudge = 0;
    int8_t _pulseNudge = 0;
    int8_t _lenNudge = 0;
    int8_t _cyclePhaseWarp = 0;
    uint8_t _traversalPattern = 0;
    uint16_t _divisor;
    uint8_t _clockMultiplier;

    AccumulatorConfig _noteAccumConfig;
    AccumulatorConfig _pulseAccumConfig;

    StageArray _stages;

    friend class PhaseFluxTrack;
};
