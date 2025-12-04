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
        _algorithm.set(clamp(algorithm, 0, 20), routed);
    }

    void editAlgorithm(int value, bool shift) {
        if (!isRouted(Routing::Target::Algorithm)) {
            // Cycle only through valid algorithms: 0, 1, 2, 3(6), 4(7), 5(8), 6(9), 10, 18, 19, 20
            // 0=Test, 1=Tri, 2=Stomper, 6=Markov, 7=Chip1, 8=Chip2, 9=Wobble, 10=ScaleWalk, 18=Aphex, 19=Aut, 20=Step
            static const int VALID_ALGORITHMS[] = {0, 1, 2, 6, 7, 8, 9, 10, 18, 19, 20};
            const int VALID_COUNT = 11;

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

    // loopLength

    int loopLength() const { return _loopLength; }
    void setLoopLength(int loopLength) {
        _loopLength = clamp(loopLength, 0, 29);
        // Re-clamp scan and rotate to new loop length
        setScan(_scan.base);
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

    // scan (0 to 64-loopLength, offsets window into buffer)

    int scan() const { return _scan.get(isRouted(Routing::Target::Scan)); }
    void setScan(int scan, bool routed = false) {
        int maxScan = 64 - actualLoopLength();
        if (maxScan < 0) maxScan = 0;
        _scan.set(clamp(scan, 0, maxScan), routed);
    }

    void editScan(int value, bool shift) {
        if (!isRouted(Routing::Target::Scan)) {
            setScan(this->scan() + value);
        }
    }

    void printScan(StringBuilder &str) const {
        printRouted(str, Routing::Target::Scan);
        str("%d", scan());
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

    TuesdaySequence() { clear(); }

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
    uint8_t _loopLength = 0;  // Default: infinite (evolving patterns)
    Routable<uint8_t> _glide;  // Default 0% (no slides)
    Routable<uint8_t> _trill;  // Default 0% (no trills/re-triggers)
    int8_t _skew = 0;  // Default: 0 (even distribution)
    uint8_t _cvUpdateMode = Free;  // Default: Free (CV updates every step)

    // Sequence parameters
    Routable<int8_t> _octave;  // Default: 0 (no transposition)
    Routable<int8_t> _transpose;  // Default: 0 (no transposition)
    Routable<uint16_t> _divisor;  // Default: 1/16 note
    uint8_t _resetMeasure = 8;  // Default: 8 bars
    int8_t _scale = -1;  // Default: -1 (Project Scale)
    int8_t _rootNote = -1;  // Default: -1 (use project root)
    Routable<uint8_t> _scan;  // Default: 0 (no scan offset)
    Routable<int8_t> _rotate;  // Default: 0 (no rotation)
    Routable<uint8_t> _gateLength; // Default: 50% (Standard)
    Routable<uint8_t> _gateOffset;  // Default: 0% (no gate timing offset)

    friend class TuesdayTrack;
};
