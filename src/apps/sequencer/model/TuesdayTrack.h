#pragma once

#include "Config.h"
#include "Types.h"
#include "Serialize.h"
#include "ModelUtils.h"

#include "core/math/Math.h"

class TuesdayTrack {
public:
    //----------------------------------------
    // Properties
    //----------------------------------------

    // algorithm

    int algorithm() const { return _algorithm; }
    void setAlgorithm(int algorithm) {
        _algorithm = clamp(algorithm, 0, 36);
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
        _loopLength = clamp(loopLength, 0, 25);
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

    friend class Track;
};
