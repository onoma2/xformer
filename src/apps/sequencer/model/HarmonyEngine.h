#pragma once

#include "core/math/Math.h"
#include <cstdint>
#include <algorithm>

class VersionedSerializedWriter;
class VersionedSerializedReader;

class HarmonyEngine {
public:
    // FIXED: Plain enum (no "class"), no Last member
    enum Mode {
        Ionian = 0,
        Dorian = 1,
        Phrygian = 2,
        Lydian = 3,
        Mixolydian = 4,
        Aeolian = 5,
        Locrian = 6
    };

    enum ChordQuality {
        Minor7 = 0,      // -7
        Dominant7 = 1,   // 7
        Major7 = 2,      // ∆7
        HalfDim7 = 3     // ø
    };

    enum Voicing {
        Close = 0,
        Drop2 = 1,
        Drop3 = 2,
        Spread = 3
    };

    struct ChordNotes {
        int16_t root;
        int16_t third;
        int16_t fifth;
        int16_t seventh;
    };

    struct ChordIntervals {
        uint8_t intervals[4];
        const uint8_t& operator[](int i) const { return intervals[i]; }
    };

    // Constructor
    HarmonyEngine();

    // Getters (const!)
    Mode mode() const { return _mode; }
    bool diatonicMode() const { return _diatonicMode; }
    uint8_t inversion() const { return _inversion; }
    Voicing voicing() const { return _voicing; }
    int8_t transpose() const { return _transpose; }

    // Setters
    void setMode(Mode mode) { _mode = mode; }
    void setDiatonicMode(bool diatonic) { _diatonicMode = diatonic; }
    void setInversion(uint8_t inv) { _inversion = std::min(inv, uint8_t(3)); }
    void setVoicing(Voicing v) { _voicing = v; }
    void setTranspose(int8_t t) { _transpose = clamp(t, int8_t(-24), int8_t(24)); }

    // Core harmony functions (Phase 1)
    int16_t getScaleInterval(uint8_t degree) const;
    ChordQuality getDiatonicQuality(uint8_t scaleDegree) const;
    ChordIntervals getChordIntervals(ChordQuality quality) const;
    ChordNotes harmonize(int16_t rootNote, uint8_t scaleDegree) const;

    // Serialization
    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    Mode _mode;
    bool _diatonicMode;
    uint8_t _inversion;      // 0-3
    Voicing _voicing;
    int8_t _transpose;       // -24 to +24

    // Lookup tables (Phase 1: All modes included for Phase 2)
    static const uint8_t ScaleIntervals[7][7];
    static const ChordQuality DiatonicChords[7][7];
    static const uint8_t ChordIntervalsTable[4][4];

    // Helper methods
    int16_t applyInterval(int16_t baseNote, int16_t interval) const;
};
