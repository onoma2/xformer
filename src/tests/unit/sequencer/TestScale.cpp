#include "UnitTest.h"

#include "core/utils/StringBuilder.h"

#include "apps/sequencer/model/Scale.cpp"
#include "apps/sequencer/model/UserScale.cpp"

#include <array>
#include <cstdint>
#include <cstring>

UNIT_TEST("Scale") {

    CASE("noteName/noteToVolts") {
        for (int i = 0; i < Scale::Count; ++i) {
            const auto &scale = Scale::get(i);
            int notesPerOctave = scale.notesPerOctave();

            DBG("----------------------------------------");
            DBG("%s", Scale::name(i));
            DBG("octave has %d notes", notesPerOctave);
            DBG("----------------------------------------");
            DBG("%-8s %-8s %-8s %-8s %-8s", "note", "volts", "short1", "short2", "long");

            for (int note = -notesPerOctave; note <= notesPerOctave; ++note) {
                FixedStringBuilder<8> short1Name;
                FixedStringBuilder<8> short2Name;
                FixedStringBuilder<16> longName;
                scale.noteName(short1Name, note, 0, Scale::Short1);
                scale.noteName(short2Name, note, 0, Scale::Short2);
                scale.noteName(longName, note, 0, Scale::Long);
                float volts = scale.noteToVolts(note);
                DBG("%-8d %-8.3f %-8s %-8s %-8s", note, volts, (const char *)(short1Name), (const char *)(short2Name), (const char *)(longName));
                // DBG("note = %d, volts = %f", note, volts);
            }
        }
    }

    CASE("noteFromVolts") {
        for (int i = 0; i < Scale::Count; ++i) {
            const auto &scale = Scale::get(i);
            int notesPerOctave = scale.notesPerOctave();

            DBG("----------------------------------------");
            DBG("%s", Scale::name(i));
            DBG("octave has %d notes", notesPerOctave);
            DBG("----------------------------------------");
            DBG("%-8s %-8s %-8s %-8s %-8s %-8s", "voltsin", "note", "volts", "short1", "short2", "long");

            for (int index = -12; index <= 12; ++index) {
                float voltsin = index * (1.f / 12.f);
                int note = scale.noteFromVolts(voltsin);
                FixedStringBuilder<8> short1Name;
                FixedStringBuilder<8> short2Name;
                FixedStringBuilder<16> longName;
                scale.noteName(short1Name, note, 0, Scale::Short1);
                scale.noteName(short2Name, note, 0, Scale::Short2);
                scale.noteName(longName, note, 0, Scale::Long);
                float volts = scale.noteToVolts(note);
                DBG("%-8.3f %-8d %-8.3f %-8s %-8s %-8s", voltsin, note, volts, (const char *)(short1Name), (const char *)(short2Name), (const char *)(longName));
            }

        }
    }

#ifdef PLATFORM_SIM

    CASE("markdown") {
        const int ColsPerRow = 8;

        DBG("----------------------------------------");
        for (int i = 0; i < Scale::Count - 4; ++i) {
            const auto &scale = Scale::get(i);
            int notesPerOctave = scale.notesPerOctave();

            DBG("<h4>%s</h4>", Scale::name(i));
            DBG("");

            for (int page = 0; page < (notesPerOctave + ColsPerRow - 1) / ColsPerRow; ++page) {
                FixedStringBuilder<4096> indices("| Index |");
                FixedStringBuilder<4096> separators("| :--- |");
                FixedStringBuilder<4096> volts("| Volts |");
                int begin = page * ColsPerRow;
                int end = (page + 1) * ColsPerRow;
                if (notesPerOctave < end) end = notesPerOctave;
                for (int note = begin; note < end; ++note) {
                    indices(" %d |", note + 1);
                    separators(" --- |");
                    volts(" %.3f |", scale.noteToVolts(note));
                }
                DBG("%s", (const char *)(indices));
                DBG("%s", (const char *)(separators));
                DBG("%s", (const char *)(volts));
                DBG("");
            }
        }
        DBG("----------------------------------------");
    }

#endif // PLATFORM_SIM

    CASE("built-in index 2 is H.Minor (harmonic minor)") {
        expectTrue(strcmp(Scale::name(2), "H.Minor") == 0, "index 2 named H.Minor");
        const auto &scale = Scale::get(2);
        const int expect[7] = {0,2,3,5,7,8,11};
        for (int n = 0; n < 7; ++n)
            expectEqual(int(scale.noteToVolts(n) * 12.f + 0.5f), expect[n], "harmonic minor semitone");
    }

    CASE("supportsRotation: note scales rotate, Voltage built-in does not") {
        expectTrue(Scale::get(1).supportsRotation(), "Major rotates");   // index 1 == Major
        int voltIdx = -1;
        for (int i = 0; i < Scale::Count; ++i)
            if (strcmp(Scale::name(i), "Voltage") == 0) { voltIdx = i; break; }
        expectTrue(voltIdx >= 0, "Voltage built-in present");
        expectTrue(!Scale::get(voltIdx).supportsRotation(), "Voltage scale does not rotate");
    }

}
