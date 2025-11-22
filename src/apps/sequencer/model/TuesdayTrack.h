#pragma once

#include "Config.h"
#include "Types.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Scale.h"

#include "core/math/Math.h"

class TuesdayTrack {
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

    int algorithm() const { return _algorithm; }
    void setAlgorithm(int algorithm) {
        _algorithm = clamp(algorithm, 0, 19);
    }

    void editAlgorithm(int value, bool shift) {
        setAlgorithm(algorithm() + value);
    }

    void printAlgorithm(StringBuilder &str) const;

    // flow

    int flow() const { return _flow; }
    void setFlow(int flow) {
        _flow = clamp(flow, 0, 16);
    }

    void editFlow(int value, bool shift) {
        setFlow(this->flow() + value);
    }

    void printFlow(StringBuilder &str) const {
        str("%d", flow());
    }

    // ornament

    int ornament() const { return _ornament; }
    void setOrnament(int ornament) {
        _ornament = clamp(ornament, 0, 16);
    }

    void editOrnament(int value, bool shift) {
        setOrnament(this->ornament() + value);
    }

    void printOrnament(StringBuilder &str) const {
        str("%d", ornament());
    }

    // power

    int power() const { return _power; }
    void setPower(int power) {
        _power = clamp(power, 0, 16);
    }

    void editPower(int value, bool shift) {
        setPower(this->power() + value);
    }

    void printPower(StringBuilder &str) const {
        str("%d", power());
    }

    // loopLength

    int loopLength() const { return _loopLength; }
    void setLoopLength(int loopLength) {
        _loopLength = clamp(loopLength, 0, 29);
        // Re-clamp scan and rotate to new loop length
        setScan(_scan);
        setRotate(_rotate);
    }

    void editLoopLength(int value, bool shift) {
        setLoopLength(this->loopLength() + value);
    }

    void printLoopLength(StringBuilder &str) const;

    // Get actual loop length value (for engine use)
    int actualLoopLength() const;

    // glide (slide probability 0-100%)

    int glide() const { return _glide; }
    void setGlide(int glide) {
        _glide = clamp(glide, 0, 100);
    }

    void editGlide(int value, bool shift) {
        setGlide(this->glide() + value * (shift ? 10 : 1));
    }

    void printGlide(StringBuilder &str) const {
        str("%d%%", glide());
    }

    // useScale (use project scale for note quantization)

    bool useScale() const { return _useScale; }
    void setUseScale(bool useScale) {
        _useScale = useScale;
    }

    void editUseScale(int value, bool shift) {
        setUseScale(value > 0 ? !_useScale : _useScale);
    }

    void printUseScale(StringBuilder &str) const {
        str(_useScale ? "Project" : "Free");
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

    int octave() const { return _octave; }
    void setOctave(int octave) {
        _octave = clamp(octave, -10, 10);
    }

    void editOctave(int value, bool shift) {
        setOctave(this->octave() + value);
    }

    void printOctave(StringBuilder &str) const {
        str("%+d", octave());
    }

    // transpose (-11 to +11)

    int transpose() const { return _transpose; }
    void setTranspose(int transpose) {
        _transpose = clamp(transpose, -11, 11);
    }

    void editTranspose(int value, bool shift) {
        setTranspose(this->transpose() + value);
    }

    void printTranspose(StringBuilder &str) const {
        str("%+d", transpose());
    }

    // divisor

    int divisor() const { return _divisor; }
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
        setDivisor(ModelUtils::adjustedByDivisor(divisor(), value, shift));
    }

    void printDivisor(StringBuilder &str) const {
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

    // scale (-1 = Default, 0+ = specific scale)

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

    // scan (0 to 128-loopLength, offsets window into buffer)

    int scan() const { return _scan; }
    void setScan(int scan) {
        int maxScan = 128 - actualLoopLength();
        if (maxScan < 0) maxScan = 0;
        _scan = clamp(scan, 0, maxScan);
    }

    void editScan(int value, bool shift) {
        setScan(this->scan() + value);
    }

    void printScan(StringBuilder &str) const {
        str("%d", scan());
    }

    // rotate (bipolar shift for finite loops, limited by loop length)

    int rotate() const { return _rotate; }
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
        setRotate(this->rotate() + value);
    }

    void printRotate(StringBuilder &str) const {
        str("%+d", rotate());
    }

    //----------------------------------------
    // Methods
    //----------------------------------------

    TuesdayTrack() { clear(); }

    void clear();

    void gateOutputName(int index, StringBuilder &str) const;
    void cvOutputName(int index, StringBuilder &str) const;

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    void setTrackIndex(int trackIndex) {
        _trackIndex = trackIndex;
    }

    int8_t _trackIndex = -1;
    uint8_t _algorithm = 0;
    uint8_t _flow = 0;
    uint8_t _ornament = 0;
    uint8_t _power = 0;
    uint8_t _loopLength = 16;
    uint8_t _glide = 0;  // Default 0% (no slides)
    bool _useScale = false;  // Default: free (chromatic)
    int8_t _skew = 0;  // Default: 0 (even distribution)
    uint8_t _cvUpdateMode = Free;  // Default: Free (CV updates every step)

    // Sequence parameters
    int8_t _octave = 0;  // Default: 0 (no transposition)
    int8_t _transpose = 0;  // Default: 0 (no transposition)
    uint16_t _divisor = 12;  // Default: 1/16 note
    uint8_t _resetMeasure = 8;  // Default: 8 bars
    int8_t _scale = -1;  // Default: -1 (use project scale)
    int8_t _rootNote = -1;  // Default: -1 (use project root)
    uint8_t _scan = 0;  // Default: 0 (no scan offset)
    int8_t _rotate = 0;  // Default: 0 (no rotation)

    friend class Track;
};
