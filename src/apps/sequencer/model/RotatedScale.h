#pragma once

#include "Scale.h"

#include "core/utils/StringBuilder.h"

class RotatedScaleView : public Scale {
public:
    RotatedScaleView(const Scale &base, int rotate) :
        Scale("rot"),
        _base(base),
        _rotate(normalize(base, rotate))
    {}

    bool isChromatic() const override { return _base.isChromatic(); }

    int notesPerOctave() const override { return _base.notesPerOctave(); }

    bool supportsRotation() const override { return _base.supportsRotation(); }

    float noteToVolts(int note) const override {
        if (_rotate == 0) {
            return _base.noteToVolts(note);
        }
        return _base.noteToVolts(note + _rotate) - _base.noteToVolts(_rotate);
    }

    int noteFromVolts(float volts) const override {
        if (_rotate == 0) {
            return _base.noteFromVolts(volts);
        }
        return _base.noteFromVolts(volts + _base.noteToVolts(_rotate)) - _rotate;
    }

    void noteName(StringBuilder &str, int note, int rootNote, Format format) const override {
        if (_rotate == 0) {
            _base.noteName(str, note, rootNote, format);
            return;
        }

        bool printNote = format == Short1 || format == Long;
        bool printOctave = format == Short2 || format == Long;

        if (isChromatic()) {
            float volts = noteToVolts(note);
            int totalSemis = int(volts * 12.f + (volts < 0 ? -0.5f : 0.5f)) + rootNote;
            int octave = (totalSemis >= 0 ? totalSemis : totalSemis - 11) / 12;
            int noteIndex = totalSemis - octave * 12;
            if (printNote) {
                Types::printNote(str, noteIndex);
            }
            if (printOctave) {
                str("%+d", octave);
            }
        } else {
            int n = notesPerOctave();
            int octave = (note >= 0 ? note : note - (n - 1)) / n;
            int degree = note - octave * n + 1;
            if (printNote) {
                str("%d", degree);
            }
            if (printOctave) {
                str("%+d", octave);
            }
        }
    }

private:
    static int normalize(const Scale &base, int rotate) {
        if (!base.supportsRotation()) {
            return 0;
        }
        int n = base.notesPerOctave();
        return ((rotate % n) + n) % n;
    }

    const Scale &_base;
    int _rotate;
};
