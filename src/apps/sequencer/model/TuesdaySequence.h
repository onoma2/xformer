#pragma once

#include "Config.h"
#include "Types.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Scale.h"
#include "Routing.h"

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

    int algorithm() const { return _algorithm.get(isRouted(Routing::Target::Algorithm)); }
    void setAlgorithm(int algorithm, bool routed = false) {
        _algorithm.set(clamp(algorithm, 0, 14), routed);
    }

    void editAlgorithm(int value, bool shift) {
        if (!isRouted(Routing::Target::Algorithm)) {
            // Cycle only through valid algorithms: 0-14 (consecutive)
            static const int VALID_ALGORITHMS[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
            const int VALID_COUNT = 15;

            // Find current algorithm position in valid array
            int currentIdx = -1;
            for (int i = 0; i < VALID_COUNT; i++) {
                if (VALID_ALGORITHMS[i] == algorithm()) {
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
    }

    void printAlgorithm(StringBuilder &str) const;

    // flow

    int flow() const { return _flow.get(isRouted(Routing::Target::Flow)); }
    void setFlow(int flow, bool routed = false) {
        _flow.set(clamp(flow, 0, 16), routed);
    }

    void editFlow(int value, bool shift) {
        if (!isRouted(Routing::Target::Flow)) {
            setFlow(this->flow() + value);
        }
    }

    void printFlow(StringBuilder &str) const {
        printRouted(str, Routing::Target::Flow);
        str("%d", flow());
    }

    // ornament

    int ornament() const { return _ornament.get(isRouted(Routing::Target::Ornament)); }
    void setOrnament(int ornament, bool routed = false) {
        _ornament.set(clamp(ornament, 0, 16), routed);
    }

    void editOrnament(int value, bool shift) {
        if (!isRouted(Routing::Target::Ornament)) {
            setOrnament(this->ornament() + value);
        }
    }

    void printOrnament(StringBuilder &str) const {
        printRouted(str, Routing::Target::Ornament);
        str("%d", ornament());
    }

    // power

    int power() const { return _power.get(isRouted(Routing::Target::Power)); }
    void setPower(int power, bool routed = false) {
        _power.set(clamp(power, 0, 16), routed);
    }

    void editPower(int value, bool shift) {
        if (!isRouted(Routing::Target::Power)) {
            setPower(this->power() + value);
        }
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
        setRotate(_rotate.base);
    }

    void editLoopLength(int value, bool shift) {
        setLoopLength(this->loopLength() + value);
    }

    void printLoopLength(StringBuilder &str) const;

    // Get actual loop length value (for engine use)
    int actualLoopLength() const;

    // glide (slide probability 0-100%)

    int glide() const { return _glide.get(isRouted(Routing::Target::Glide)); }
    void setGlide(int glide, bool routed = false) {
        _glide.set(clamp(glide, 0, 100), routed);
    }

    void editGlide(int value, bool shift) {
        if (!isRouted(Routing::Target::Glide)) {
            setGlide(this->glide() + value * (shift ? 10 : 1));
        }
    }

    void printGlide(StringBuilder &str) const {
        printRouted(str, Routing::Target::Glide);
        str("%d%%", glide());
    }

    // trill (re-trigger probability 0-100%)
    int trill() const { return _trill.get(isRouted(Routing::Target::Trill)); }
    void setTrill(int trill, bool routed = false) {
        _trill.set(clamp(trill, 0, 100), routed);
    }

    void editTrill(int value, bool shift) {
        if (!isRouted(Routing::Target::Trill)) {
            setTrill(this->trill() + value * (shift ? 10 : 1));
        }
    }

    void printTrill(StringBuilder &str) const {
        printRouted(str, Routing::Target::Trill);
        str("%d%%", trill());
    }

    // stepTrill (intra-step subdivision count 0-100%)
    int stepTrill() const { return _stepTrill.get(isRouted(Routing::Target::StepTrill)); }
    void setStepTrill(int stepTrill, bool routed = false) {
        _stepTrill.set(clamp(stepTrill, 0, 100), routed);
    }

    void editStepTrill(int value, bool shift) {
        if (!isRouted(Routing::Target::StepTrill)) {
            setStepTrill(this->stepTrill() + value * (shift ? 10 : 1));
        }
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

    int octave() const { return _octave.get(isRouted(Routing::Target::Octave)); }
    void setOctave(int octave, bool routed = false) {
        _octave.set(clamp(octave, -10, 10), routed);
    }

    void editOctave(int value, bool shift) {
        if (!isRouted(Routing::Target::Octave)) {
            setOctave(this->octave() + value);
        }
    }

    void printOctave(StringBuilder &str) const {
        printRouted(str, Routing::Target::Octave);
        str("%+d", octave());
    }

    // transpose (-11 to +11)

    int transpose() const { return _transpose.get(isRouted(Routing::Target::Transpose)); }
    void setTranspose(int transpose, bool routed = false) {
        _transpose.set(clamp(transpose, -11, 11), routed);
    }

    void editTranspose(int value, bool shift) {
        if (!isRouted(Routing::Target::Transpose)) {
            setTranspose(this->transpose() + value);
        }
    }

    void printTranspose(StringBuilder &str) const {
        printRouted(str, Routing::Target::Transpose);
        str("%+d", transpose());
    }

    // divisor

    int divisor() const { return _divisor.get(isRouted(Routing::Target::Divisor)); }
    void setDivisor(int divisor, bool routed = false) {
        _divisor.set(ModelUtils::clampDivisor(divisor), routed);
    }

    int indexedDivisor() const { return ModelUtils::divisorToIndex(divisor()); }
    void setIndexedDivisor(int index) {
        int div = ModelUtils::indexToDivisor(index);
        if (div > 0) {
            setDivisor(div);
        }
    }

    void editDivisor(int value, bool shift) {
        if (!isRouted(Routing::Target::Divisor)) {
            setDivisor(ModelUtils::adjustedByDivisor(divisor(), value, shift));
        }
    }

    void printDivisor(StringBuilder &str) const {
        printRouted(str, Routing::Target::Divisor);
        ModelUtils::printDivisor(str, divisor());
    }

    // clockMultiplier

    int clockMultiplier() const { return _clockMultiplier.get(isRouted(Routing::Target::ClockMult)); }
    void setClockMultiplier(int clockMultiplier, bool routed = false) {
        _clockMultiplier.set(clamp(clockMultiplier, 50, 150), routed);
    }

    void editClockMultiplier(int value, bool shift) {
        if (!isRouted(Routing::Target::ClockMult)) {
            setClockMultiplier(clockMultiplier() + value * (shift ? 10 : 1));
        }
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

    int scale() const { return _scale; }
    void setScale(int scale) {
        _scale = clamp(scale, -1, Scale::Count - 1);
    }

    void editScale(int value, bool shift) {
        setScale(this->scale() + value);
    }

    void printScale(StringBuilder &str) const {
        if (scale() == -1) {
            str("Default");
        } else {
            str(Scale::name(scale()));
        }
    }

    // rootNote (-1 = Default, 0-11 = C to B)

    int rootNote() const { return _rootNote; }
    void setRootNote(int rootNote) {
        _rootNote = clamp(rootNote, -1, 11);
    }

    void editRootNote(int value, bool shift) {
        setRootNote(this->rootNote() + value);
    }

    void printRootNote(StringBuilder &str) const {
        if (rootNote() == -1) {
            str("Default");
        } else {
            Types::printNote(str, rootNote());
        }
    }

    // gateLength (0-100% scaling for gate duration)
    int gateLength() const { return _gateLength.get(isRouted(Routing::Target::GateLength)); }
    void setGateLength(int gateLength, bool routed = false) {
        _gateLength.set(clamp(gateLength, 0, 100), routed);
    }

    void editGateLength(int value, bool shift) {
        if (!isRouted(Routing::Target::GateLength)) {
            setGateLength(this->gateLength() + value * (shift ? 10 : 1));
        }
    }

    void printGateLength(StringBuilder &str) const {
        printRouted(str, Routing::Target::GateLength);
        str("%d%%", gateLength());
    }

    // gateOffset (0-100% user override for algorithmic gate timing)
    int gateOffset() const { return _gateOffset.get(isRouted(Routing::Target::GateOffset)); }
    void setGateOffset(int gateOffset, bool routed = false) {
        _gateOffset.set(clamp(gateOffset, 0, 100), routed);
    }

    void editGateOffset(int value, bool shift) {
        if (!isRouted(Routing::Target::GateOffset)) {
            setGateOffset(this->gateOffset() + value * (shift ? 10 : 1));
        }
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

    int rotate() const { return _rotate.get(isRouted(Routing::Target::Rotate)); }
    void setRotate(int rotate, bool routed = false) {
        int len = actualLoopLength();
        if (len > 0) {
            // Limit to loop length for easier use
            int maxRot = len - 1;
            _rotate.set(clamp(rotate, -maxRot, maxRot), routed);
        } else {
            // Infinite loop: no rotation
            _rotate.set(0, routed);
        }
    }

    void editRotate(int value, bool shift) {
        if (!isRouted(Routing::Target::Rotate)) {
            setRotate(this->rotate() + value);
        }
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
    void writeRouted(Routing::Target target, int intValue, float floatValue);

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
    Routable<uint8_t> _algorithm;
    Routable<uint8_t> _flow;
    Routable<uint8_t> _ornament;
    Routable<uint8_t> _power;
    uint8_t _start = 0;  // Default: 0 (start from beginning)
    uint8_t _loopLength = 0;  // Default: infinite (evolving patterns)
    Routable<uint8_t> _glide;  // Default 0% (no slides)
    Routable<uint8_t> _trill;  // Default 0% (no trills/re-triggers)
    Routable<uint8_t> _stepTrill;  // Default 0% (no intra-step subdivision)
    int8_t _skew = 0;  // Default: 0 (even distribution)
    uint8_t _cvUpdateMode = Free;  // Default: Free (CV updates every step)

    // Sequence parameters
    Routable<int8_t> _octave;  // Default: 0 (no transposition)
    Routable<int8_t> _transpose;  // Default: 0 (no transposition)
    Routable<uint16_t> _divisor;  // Default: 1/16 note
    Routable<uint8_t> _clockMultiplier;
    uint8_t _resetMeasure = 8;  // Default: 8 bars
    int8_t _scale = -1;  // Default: -1 (Project Scale)
    int8_t _rootNote = -1;  // Default: -1 (use project root)
    Routable<int8_t> _rotate;  // Default: 0 (no rotation)
    Routable<uint8_t> _gateLength; // Default: 50% (Standard)
    Routable<uint8_t> _gateOffset;  // Default: 0% (no gate timing offset)
    uint8_t _maskParameter = 0;  // Default: 0 (ALL = no skipping)
    uint8_t _timeMode = 0;  // Default: 0 (FREE mode)
    uint8_t _maskProgression = 0;  // Default: 0 (no progression)

    friend class TuesdayTrack;
};
