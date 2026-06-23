#pragma once

#include "Config.h"
#include "Types.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Scale.h"
#include "Routing.h"
#include "Bitfield.h"
#include "RouteParamKey.h"

#include "core/math/Math.h"
#include "core/utils/StringBuilder.h"

class TuesdaySequence {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    enum CvUpdateMode {
        Free = 0,   // CV updates every step (current behavior)
        Gated = 1   // CV only updates when gate fires (original Tuesday behavior)
    };

    //----------------------------------------
    // Properties
    //----------------------------------------

    // algorithm

    int algorithm() const { return Routing::routedValueInt(ParamKey::Algorithm, _trackIndex, _algorithm, 0, 15); }
    void setAlgorithm(int algorithm) {
        _algorithm = clamp(algorithm, 0, 15);
    }

    void editAlgorithm(int value, bool shift) {
        // Cycle only through valid algorithms: 0-15 (consecutive)
        static const int VALID_ALGORITHMS[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        const int VALID_COUNT = 15;

        // Find current algorithm position in valid array
        int currentIdx = -1;
        for (int i = 0; i < VALID_COUNT; i++) {
            if (VALID_ALGORITHMS[i] == _algorithm) {
                currentIdx = i;
                break;
            }
        }

        // If current algorithm is not valid, start at beginning
        if (currentIdx == -1) {
            currentIdx = 0;
        }

        // Cycle to new position (with wrapping)
        int newIdx = (currentIdx + value) % VALID_COUNT;
        if (newIdx < 0) {
            newIdx = (newIdx + VALID_COUNT) % VALID_COUNT; // Handle negative wrap
        }

        setAlgorithm(VALID_ALGORITHMS[newIdx]);
    }

    void printAlgorithm(StringBuilder &str) const;

    // flow

    int flow() const { return Routing::routedValueInt(ParamKey::Flow, _trackIndex, _flow, 0, 16); }
    void setFlow(int flow) {
        _flow = clamp(flow, 0, 16);
    }

    void editFlow(int value, bool shift) {
        setFlow(_flow + value);
    }

    void printFlow(StringBuilder &str) const {
        printRouted(str, Routing::Target::Flow);
        str("%d", flow());
    }

    // ornament

    int ornament() const { return Routing::routedValueInt(ParamKey::Ornament, _trackIndex, _ornament, 0, 16); }
    void setOrnament(int ornament) {
        _ornament = clamp(ornament, 0, 16);
    }

    void editOrnament(int value, bool shift) {
        setOrnament(_ornament + value);
    }

    void printOrnament(StringBuilder &str) const {
        printRouted(str, Routing::Target::Ornament);
        str("%d", ornament());
    }

    // power

    int power() const { return Routing::routedValueInt(ParamKey::Power, _trackIndex, _power, 0, 16); }
    void setPower(int power) {
        _power = clamp(power, 0, 16);
    }

    void editPower(int value, bool shift) {
        setPower(_power + value);
    }

    void printPower(StringBuilder &str) const {
        printRouted(str, Routing::Target::Power);
        str("%d", power());
    }

    // start (0-16)

    int start() const { return _start; }
    void setStart(int start) { _start = clamp(start, 0, 16); }

    void editStart(int value, bool shift) {
        setStart(this->start() + value);
    }

    void printStart(StringBuilder &str) const {
        str("%d", start());
    }

    // loopLength

    int loopLength() const { return _loopLength; }
    void setLoopLength(int loopLength) {
        _loopLength = clamp(loopLength, 0, 29);
        // Re-clamp rotate to new loop length
        setRotate(_rotate);
    }

    void editLoopLength(int value, bool shift) {
        setLoopLength(this->loopLength() + value);
    }

    void printLoopLength(StringBuilder &str) const;

    // Get actual loop length value (for engine use)
    int actualLoopLength() const;

    // glide (slide probability 0-100%)

    int glide() const { return Routing::routedValueInt(ParamKey::Glide, _trackIndex, _glide, 0, 100); }
    void setGlide(int glide) {
        _glide = clamp(glide, 0, 100);
    }

    void editGlide(int value, bool shift) {
        setGlide(_glide + value * (shift ? 10 : 1));
    }

    void printGlide(StringBuilder &str) const {
        printRouted(str, Routing::Target::Glide);
        str("%d%%", glide());
    }

    // trill (re-trigger probability 0-100%)
    int trill() const { return Routing::routedValueInt(ParamKey::Trill, _trackIndex, _trill, 0, 100); }
    void setTrill(int trill) {
        _trill = clamp(trill, 0, 100);
    }

    void editTrill(int value, bool shift) {
        setTrill(_trill + value * (shift ? 10 : 1));
    }

    void printTrill(StringBuilder &str) const {
        printRouted(str, Routing::Target::Trill);
        str("%d%%", trill());
    }

    // stepTrill (intra-step subdivision count 0-100%)
    int stepTrill() const { return Routing::routedValueInt(ParamKey::StepTrill, _trackIndex, _stepTrill, 0, 100); }
    void setStepTrill(int stepTrill) {
        _stepTrill = clamp(stepTrill, 0, 100);
    }

    void editStepTrill(int value, bool shift) {
        setStepTrill(_stepTrill + value * (shift ? 10 : 1));
    }

    void printStepTrill(StringBuilder &str) const {
        printRouted(str, Routing::Target::StepTrill);
        str("%d%%", stepTrill());
    }

    // skew (density curve across loop, -8 to +8)

    int skew() const { return _skew; }
    void setSkew(int skew) {
        _skew = clamp(skew, -8, 8);
    }

    void editSkew(int value, bool shift) {
        setSkew(this->skew() + value);
    }

    void printSkew(StringBuilder &str) const {
        str("%+d", skew());
    }

    // cvUpdateMode (controls when CV changes: every step or only with gates)

    CvUpdateMode cvUpdateMode() const { return static_cast<CvUpdateMode>(_cvUpdateMode); }
    void setCvUpdateMode(CvUpdateMode mode) {
        _cvUpdateMode = static_cast<uint8_t>(mode);
    }

    void editCvUpdateMode(int value, bool shift) {
        if (value != 0) {
            setCvUpdateMode(_cvUpdateMode == Free ? Gated : Free);
        }
    }

    void printCvUpdateMode(StringBuilder &str) const {
        str(_cvUpdateMode == Free ? "Free" : "Gated");
    }

    // octave (-10 to +10)

    int octave() const { return Routing::routedValueInt(ParamKey::Octave, _trackIndex, _octave, -10, 10); }
    void setOctave(int octave) {
        _octave = clamp(octave, -10, 10);
    }

    void editOctave(int value, bool shift) {
        setOctave(_octave + value);
    }

    void printOctave(StringBuilder &str) const {
        printRouted(str, Routing::Target::Octave);
        str("%+d", octave());
    }

    // transpose (-11 to +11)

    int transpose() const { return Routing::routedValueInt(ParamKey::Transpose, _trackIndex, _transpose, -11, 11); }
    void setTranspose(int transpose) {
        _transpose = clamp(transpose, -11, 11);
    }

    void editTranspose(int value, bool shift) {
        setTranspose(_transpose + value);
    }

    void printTranspose(StringBuilder &str) const {
        printRouted(str, Routing::Target::Transpose);
        str("%+d", transpose());
    }

    // divisor

    int divisor() const { return Routing::routedValueInt(ParamKey::Divisor, _trackIndex, _divisor, 1, 768); }
    int divisorBase() const { return _divisor; }
    void setDivisor(int divisor) {
        _divisor = ModelUtils::clampDivisor(divisor);
    }

    int indexedDivisor() const { return ModelUtils::divisorToIndex(divisor()); }
    void setIndexedDivisor(int index) {
        int div = ModelUtils::indexToDivisor(index);
        if (div > 0) {
            setDivisor(div);
        }
    }

    void editDivisor(int value, bool shift) {
        setDivisor(ModelUtils::adjustedByDivisor(_divisor, value, shift));
    }

    void printDivisor(StringBuilder &str) const {
        printRouted(str, Routing::Target::Divisor);
        ModelUtils::printDivisor(str, divisor());
    }

    // clockMultiplier

    int clockMultiplier() const { return Routing::routedValueInt(ParamKey::ClockMultiplier, _trackIndex, _clockMultiplier, 50, 150); }
    void setClockMultiplier(int clockMultiplier) {
        _clockMultiplier = clamp(clockMultiplier, 50, 150);
    }

    void editClockMultiplier(int value, bool shift) {
        setClockMultiplier(_clockMultiplier + value * (shift ? 10 : 1));
    }

    void printClockMultiplier(StringBuilder &str) const {
        printRouted(str, Routing::Target::ClockMult);
        str("%.2fx", clockMultiplier() * 0.01f);
    }

    // resetMeasure (0-128)

    int resetMeasure() const { return _resetMeasure; }
    void setResetMeasure(int resetMeasure) {
        _resetMeasure = clamp(resetMeasure, 0, 128);
    }

    void editResetMeasure(int value, bool shift) {
        setResetMeasure(ModelUtils::adjustedByPowerOfTwo(resetMeasure(), value, shift));
    }

    void printResetMeasure(StringBuilder &str) const {
        if (resetMeasure() == 0) {
            str("off");
        } else {
            str("%d %s", resetMeasure(), resetMeasure() > 1 ? "bars" : "bar");
        }
    }

    // scale (-1 = Project scale, 0 = Chromatic/Semitones, 1+ = specific scale)
    // Note: Scale 0 is "Semitones" (chromatic) - quantizes to all 12 semitones
    // This controls OUTPUT quantization, not algorithm behavior

    int rawScale() const { return int(_scaleGroup.scale) - 1; }
    int scale() const { return Routing::routedValueInt(ParamKey::Scale, _trackIndex, rawScale(), 0, 23); }
    void setScale(int scale) {
        _scaleGroup.scale = clamp(scale, -1, Scale::Count - 1) + 1;
    }

    void editScale(int value, bool shift) {
        setScale(rawScale() + value);
    }

    void printScale(StringBuilder &str) const {
        if (scale() == -1) {
            str("Default");
        } else {
            str(Scale::name(scale()));
        }
    }

    // rootNote (-1 = Default, 0-11 = C to B)

    int rawRootNote() const { return int(_scaleGroup.rootNote) - 1; }
    int rootNote() const { return Routing::routedValueInt(ParamKey::RootNote, _trackIndex, rawRootNote(), 0, 11); }
    void setRootNote(int rootNote) {
        _scaleGroup.rootNote = clamp(rootNote, -1, 11) + 1;
    }

    void editRootNote(int value, bool shift) {
        setRootNote(rawRootNote() + value);
    }

    // scaleRotate

    int scaleRotate() const { return int(_scaleGroup.scaleRotate) - 1; }
    void setScaleRotate(int scaleRotate) {
        _scaleGroup.scaleRotate = clamp(scaleRotate, -1, 31) + 1;
    }

    void printRootNote(StringBuilder &str) const {
        if (rootNote() == -1) {
            str("Default");
        } else {
            Types::printNote(str, rootNote());
        }
    }

    // gateLength (0-100% scaling for gate duration)
    int gateLength() const { return Routing::routedValueInt(ParamKey::GateLength, _trackIndex, _gateLength, 0, 100); }
    void setGateLength(int gateLength) {
        _gateLength = clamp(gateLength, 0, 100);
    }

    void editGateLength(int value, bool shift) {
        setGateLength(_gateLength + value * (shift ? 10 : 1));
    }

    void printGateLength(StringBuilder &str) const {
        printRouted(str, Routing::Target::GateLength);
        str("%d%%", gateLength());
    }

    // gateOffset (0-100% user override for algorithmic gate timing)
    int gateOffset() const { return Routing::routedValueInt(ParamKey::GateOffset, _trackIndex, _gateOffset, 0, 100); }
    void setGateOffset(int gateOffset) {
        _gateOffset = clamp(gateOffset, 0, 100);
    }

    void editGateOffset(int value, bool shift) {
        setGateOffset(_gateOffset + value * (shift ? 10 : 1));
    }

    void printGateOffset(StringBuilder &str) const {
        printRouted(str, Routing::Target::GateOffset);
        str("%d%%", gateOffset());
    }


    // maskParameter (0=ALL, 1-14=mask values, 15=NONE)
    int maskParameter() const { return _maskParameter; }
    void setMaskParameter(int param) {
        _maskParameter = clamp(param, 0, 15);
    }

    void editMaskParameter(int value, bool shift) {
        setMaskParameter(_maskParameter + value);
    }

    void printMaskParameter(StringBuilder &str) const {
        // Map to mask values with special values for 0 and 15
        static const int MASK_VALUES[] = {2, 3, 5, 11, 19, 31, 43, 61, 89, 131, 197, 277, 409, 599};
        const int MASK_COUNT = sizeof(MASK_VALUES) / sizeof(MASK_VALUES[0]);

        int currentParam = maskParameter();

        if (currentParam == 0) {
            str("ALL");  // Allow all
        } else if (currentParam == 15) {
            str("NONE"); // Mask all
        } else {
            // Parameter 1-14 map to mask values (0-indexed in array)
            int maskIndex = currentParam - 1; // Adjust to 0-indexed
            if (maskIndex < MASK_COUNT) {
                str("%d", MASK_VALUES[maskIndex]);
            } else {
                // If param exceeds available masks, wrap around
                str("%d", MASK_VALUES[maskIndex % MASK_COUNT]);
            }
        }
    }

    // timeMode (0=FREE, 1=QRT, 2=1.5Q, 3=3QRT)
    int timeMode() const { return _timeMode; }
    void setTimeMode(int mode) {
        _timeMode = clamp(mode, 0, 3);
    }

    void editTimeMode(int value, bool shift) {
        if (value != 0) {
            int current = _timeMode;
            current = (current + 1) % 4;  // Cycle through 0, 1, 2, 3
            _timeMode = current;
        }
    }

    void printTimeMode(StringBuilder &str) const {
        switch (_timeMode) {
            case 0: str("FREE"); break;
            case 1: str("QRT"); break;
            case 2: str("1.5Q"); break;
            case 3: str("3QRT"); break;
            default: str("TM-%d", _timeMode); break;
        }
    }

    // maskProgression (0=NO PROGRESSION, 1=PROGRESS BY 1, 2=BY 5, 3=BY 7)
    int maskProgression() const { return _maskProgression; }
    void setMaskProgression(int progression) {
        _maskProgression = clamp(progression, 0, 3);
    }

    void editMaskProgression(int value, bool shift) {
        if (value != 0) {
            int current = _maskProgression;
            current = (current + 1) % 4;  // Cycle through 0, 1, 2, 3
            _maskProgression = current;
        }
    }

    void printMaskProgression(StringBuilder &str) const {
        switch (_maskProgression) {
            case 0: str("NO PROG"); break;
            case 1: str("PROG+1"); break;
            case 2: str("PROG+5"); break;
            case 3: str("PROG+7"); break;
            default: str("PROG-%d", _maskProgression); break;
        }
    }


    // rotate (bipolar shift for finite loops, limited by loop length)

    int rotate() const {
        int v = Routing::routedValueInt(ParamKey::Rotate, _trackIndex, _rotate, -64, 64);
        int len = actualLoopLength();
        if (len > 0) {
            int maxRot = len - 1;
            return clamp(v, -maxRot, maxRot);
        }
        return 0;
    }
    void setRotate(int rotate) {
        int len = actualLoopLength();
        if (len > 0) {
            // Limit to loop length for easier use
            int maxRot = len - 1;
            _rotate = clamp(rotate, -maxRot, maxRot);
        } else {
            // Infinite loop: no rotation
            _rotate = 0;
        }
    }

    void editRotate(int value, bool shift) {
        setRotate(_rotate + value);
    }

    void printRotate(StringBuilder &str) const {
        printRouted(str, Routing::Target::Rotate);
        str("%+d", rotate());
    }

    //----------------------------------------
    // Routing
    //----------------------------------------

    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    inline void printRouted(StringBuilder &str, Routing::Target target) const { Routing::printRouted(str, target, _trackIndex); }

    //----------------------------------------
    // Methods
    //----------------------------------------

    TuesdaySequence() {
        clear();
    }

    void clear();

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    void setTrackIndex(int trackIndex) {
        _trackIndex = trackIndex;
    }

    int8_t _trackIndex = -1;
    uint8_t _algorithm;
    uint8_t _flow;
    uint8_t _ornament;
    uint8_t _power;
    uint8_t _start = 0;  // Default: 0 (start from beginning)
    uint8_t _loopLength = 0;  // Default: infinite (evolving patterns)
    uint8_t _glide;  // Default 0% (no slides)
    uint8_t _trill;  // Default 0% (no trills/re-triggers)
    uint8_t _stepTrill;  // Default 0% (no intra-step subdivision)
    int8_t _skew = 0;  // Default: 0 (even distribution)
    uint8_t _cvUpdateMode = Free;  // Default: Free (CV updates every step)

    // Sequence parameters
    int8_t _octave;  // Default: 0 (no transposition)
    int8_t _transpose;  // Default: 0 (no transposition)
    uint16_t _divisor;  // Default: 1/16 note
    uint8_t _clockMultiplier;
    uint8_t _resetMeasure = 8;  // Default: 8 bars
    union {
        uint16_t raw;
        BitField<uint16_t, 0, 5> scale;       // scale + 1   (-1..23 -> 0..24)
        BitField<uint16_t, 5, 4> rootNote;    // rootNote + 1 (-1..11 -> 0..12)
        BitField<uint16_t, 9, 6> scaleRotate; // scaleRotate + 1 (-1..31 -> 0..32)
    } _scaleGroup = { 0 };  // Default: -1/-1/-1 (Project scale, project root, inherit)
    int8_t _rotate;  // Default: 0 (no rotation)
    uint8_t _gateLength; // Default: 50% (Standard)
    uint8_t _gateOffset;  // Default: 0% (no gate timing offset)
    uint8_t _maskParameter = 0;  // Default: 0 (ALL = no skipping)
    uint8_t _timeMode = 0;  // Default: 0 (FREE mode)
    uint8_t _maskProgression = 0;  // Default: 0 (no progression)

    friend class TuesdayTrack;
};
