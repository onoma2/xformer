#pragma once

#include "Config.h"
#include "Bitfield.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Types.h"
#include "Scale.h"
#include "Routing.h"
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
    using PulseCount = UnsignedValue<3>;
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
    using AccumulatorLength = UnsignedValue<4>;
    using GateLength = UnsignedValue<7>;
    using StageDivisor = UnsignedValue<3>;
    using StageLen = UnsignedValue<7>;   // 0..127 mapped to 0..~2× factor via /64

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

    // 3-bit slot index. Maps to Performer divisor primitive ticks via
    // stageDivisorTicks(). Default slot 2 = 1/16 (12 ticks at PPQN 48).
    enum class StageDivisorSlot : uint8_t {
        Div1_32 = 0,
        Div1_16T = 1,
        Div1_16 = 2,
        Div1_8T = 3,
        Div1_8 = 4,
        Div1_4T = 5,
        Div1_4 = 6,
        Div1_2 = 7,
    };

    class Stage {
    public:
        //----------------------------------------
        // Properties
        //----------------------------------------

        // pulseCount — stored 0..7, exposed as 1..8
        int pulseCount() const { return int(_data0.pulseCount) + 1; }
        void setPulseCount(int v) { _data0.pulseCount = PulseCount::clamp(v - 1); }

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

        // pulseCount lives in _data2 — see layout below
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

        // accumulatorLength — stored 0..15, exposed as 1..16
        int accumulatorLength() const { return int(_data2.accumulatorLength) + 1; }
        void setAccumulatorLength(int v) { _data2.accumulatorLength = AccumulatorLength::clamp(v - 1); }

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
        // Layout: 100 bits across 4 × uint32_t = 128 bits envelope, 3 spare in
        // _data2 (bits 29..31) and 25 spare in _data3 (bits 7..31). Do NOT
        // reorder without bumping ProjectVersion.

        // word 0 — pitch shape + pulseCount (32 used, 0 spare)
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
            BitField<uint32_t, 29, PulseCount::Bits>      pulseCount;      // 29..31
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

        // word 2 — mask / accum / gate / divisor / skip (29 used, 3 spare bits 29..31)
        union {
            uint32_t raw;
            BitField<uint32_t,  0, PhaseShift::Bits>         phaseShift;        //  0..2
            BitField<uint32_t,  3, Mask::Bits>               mask;              //  3..5
            BitField<uint32_t,  6, MaskShift::Bits>          maskShift;         //  6..8
            BitField<uint32_t,  9, AccumulatorStep::Bits>    accumulatorStep;   //  9..13
            BitField<uint32_t, 14, AccumulatorLength::Bits>  accumulatorLength; // 14..17
            BitField<uint32_t, 18, GateLength::Bits>         gateLength;        // 18..24
            BitField<uint32_t, 25, StageDivisor::Bits>       stageDivisor;      // 25..27
            BitField<uint32_t, 28, 1>                        skip;              // 28
            // bits 29..31 spare
        } _data2;

        // word 3 — stageLen + 25 spare bits (future fields)
        union {
            uint32_t raw;
            BitField<uint32_t, 0, StageLen::Bits> stageLen;   // 0..6
            // bits 7..31 spare
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
    int scale() const { return _scale; }
    void setScale(int v) { _scale = clamp(v, -1, Scale::Count - 1); }
    const Scale &selectedScale(int defaultScale) const {
        return Scale::get(_scale != -1 ? _scale : defaultScale);
    }
    void editScale(int value, bool) { if (!isRouted(Routing::Target::Scale)) setScale(scale() + value); }
    void printScale(StringBuilder &str) const {
        printRouted(str, Routing::Target::Scale);
        str(scale() < 0 ? "Default" : Scale::name(scale()));
    }

    // rootNote — −1 = inherit
    int rootNote() const { return _rootNote; }
    void setRootNote(int v) { _rootNote = clamp(v, -1, 11); }
    int selectedRootNote(int defaultRootNote) const {
        return _rootNote != -1 ? _rootNote : defaultRootNote;
    }
    void editRootNote(int value, bool) { if (!isRouted(Routing::Target::RootNote)) setRootNote(rootNote() + value); }
    void printRootNote(StringBuilder &str) const {
        printRouted(str, Routing::Target::RootNote);
        if (rootNote() < 0) { str("Default"); } else { Types::printNote(str, rootNote()); }
    }

    // divisor — Routable<uint16_t>, matches NoteSequence convention
    int divisor() const { return _divisor.get(isRouted(Routing::Target::Divisor)); }
    void setDivisor(int v, bool routed = false) {
        _divisor.set(ModelUtils::clampDivisor(v), routed);
    }
    void editDivisor(int value, bool shift) {
        if (!isRouted(Routing::Target::Divisor)) setDivisor(ModelUtils::adjustedByDivisor(divisor(), value, shift));
    }
    void printDivisor(StringBuilder &str) const {
        printRouted(str, Routing::Target::Divisor);
        ModelUtils::printDivisor(str, divisor());
    }

    // clockMultiplier — Routable<uint8_t>, 50..150
    int clockMultiplier() const { return _clockMultiplier.get(isRouted(Routing::Target::ClockMult)); }
    void setClockMultiplier(int v, bool routed = false) {
        _clockMultiplier.set(clamp(v, 50, 150), routed);
    }
    void editClockMultiplier(int value, bool shift) {
        if (!isRouted(Routing::Target::ClockMult)) setClockMultiplier(clockMultiplier() + value * (shift ? 10 : 1));
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
    float globalPhase() const { return _globalPhase; }
    void setGlobalPhase(float v) { _globalPhase = clamp(v, 0.f, 1.f); }
    void editGlobalPhase(int value, bool shift) {
        setGlobalPhase(globalPhase() + value * (shift ? 0.1f : 0.01f));
    }
    void printGlobalPhase(StringBuilder &str) const {
        str("%.2f", globalPhase());
    }

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

    void writeRouted(Routing::Target target, int intValue, float floatValue) {
        switch (target) {
        case Routing::Target::Divisor:    setDivisor(intValue, true); break;
        case Routing::Target::ClockMult:  setClockMultiplier(intValue, true); break;
        case Routing::Target::Scale:      setScale(intValue); break;
        case Routing::Target::RootNote:   setRootNote(intValue); break;
        default: break;
        }
    }

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
    float _globalPhase = 0.f;
    Routable<uint16_t> _divisor;
    Routable<uint8_t> _clockMultiplier;

    StageArray _stages;

    friend class PhaseFluxTrack;
};
