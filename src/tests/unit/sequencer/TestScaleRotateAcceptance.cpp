#include "UnitTest.h"

#include "apps/sequencer/model/Scale.h"
#include "apps/sequencer/model/RotatedScale.h"
#include "apps/sequencer/model/StochasticSequence.h"

#include "core/utils/StringBuilder.h"

#include <cstring>

// Consolidated acceptance suite for the scaleRotate feature: one place that
// encodes the docs/scale-rotate-spec.md section 8 checklist. Storage/sizeof and
// serialization parity live in TestScaleGroupStorage; the per-rotation math and
// the Stochastic composition have dedicated suites — here we re-assert the load
// bearing invariant of each criterion plus the two cases not covered elsewhere.

static constexpr int kMajor = 1;   // "Major":  0 2 4 5 7 9 11
static constexpr int kMinor = 2;   // "H.Minor" after the Phase D swap

static bool nearly(float a, float b) {
    float d = a - b;
    return (d < 0 ? -d : d) < 1e-4f;
}

// Index of a many-degree n-tet scale, resolved by name so it survives any
// reordering of scales[].
static int scaleIndexByName(const char *want) {
    for (int i = 0; i < Scale::Count; ++i) {
        const char *n = Scale::name(i);
        if (n && std::strcmp(n, want) == 0) return i;
    }
    return -1;
}

UNIT_TEST("ScaleRotateAcceptance") {

// section 8: Major + scaleRotate 5 == the OLD built-in natural minor pitch set.
CASE("major_rotate_5_is_old_natural_minor") {
    RotatedScaleView v(Scale::get(kMajor), 5);
    const int expect[7] = {0, 2, 3, 5, 7, 8, 10};
    for (int n = 0; n < 7; ++n) {
        expectEqual(int(v.noteToVolts(n) * 12.f + 0.5f), expect[n], "natural minor semitone");
    }
}

// section 8: scaleRotate 0 is identity vs the base scale. The deliberate
// section 5 change means built-in index 2 is now Harmonic Minor (0 2 3 5 7 8 11).
CASE("rotate_0_is_identity_and_index2_is_harmonic_minor") {
    const Scale &major = Scale::get(kMajor);
    RotatedScaleView id(major, 0);
    for (int n = 0; n < 7; ++n) {
        expectTrue(nearly(id.noteToVolts(n), major.noteToVolts(n)), "rotate 0 identity");
    }

    expectEqual(std::strcmp(Scale::name(kMinor), "H.Minor"), 0, "index 2 renamed H.Minor");
    const Scale &hminor = Scale::get(kMinor);
    const int expect[7] = {0, 2, 3, 5, 7, 8, 11};   // harmonic minor: raised 7th
    for (int n = 0; n < 7; ++n) {
        expectEqual(int(hminor.noteToVolts(n) * 12.f + 0.5f), expect[n], "harmonic minor semitone");
    }
}

// section 8: Major + 1 quantises Dorian AND noteName octave-wraps at degree N.
CASE("major_rotate_1_is_dorian_with_octave_wrap") {
    RotatedScaleView v(Scale::get(kMajor), 1);
    const int expect[7] = {0, 2, 3, 5, 7, 9, 10};   // Dorian intervals
    for (int n = 0; n < 7; ++n) {
        expectEqual(int(v.noteToVolts(n) * 12.f + 0.5f), expect[n], "dorian semitone");
    }

    // The rotated view re-bases degree 0 to 0V, so on root C degree 0 prints the
    // tonic C; degree 7 is the same pitch class one octave up (the wrap).
    FixedStringBuilder<8> s0; v.noteName(s0, 0, 0, Scale::Long);
    expectEqual(std::strncmp((const char *)s0, "C", 1), 0, "dorian degree 0 names C tonic");
    FixedStringBuilder<8> s7; v.noteName(s7, 7, 0, Scale::Long);
    expectTrue(std::strncmp((const char *)s7, "C", 1) == 0 &&
               std::strstr((const char *)s7, "+1") != nullptr,
               "dorian degree 7 names C+1 (octave wrap)");
}

// section 8: VoltScale never rotates; toggling rotate on/off is stable.
CASE("voltage_scale_is_rotation_no_op") {
    VoltScale volt("V", 0.1f);
    expectTrue(!volt.supportsRotation(), "voltage scale not rotatable");
    RotatedScaleView r0(volt, 0);
    RotatedScaleView r3(volt, 3);
    for (int n = -3; n < 5; ++n) {
        expectTrue(nearly(r3.noteToVolts(n), volt.noteToVolts(n)), "rotate is no-op vs base");
        expectTrue(nearly(r3.noteToVolts(n), r0.noteToVolts(n)), "rotate stable across values");
    }
}

// section 8: a rotate >= notesPerOctave wraps modulo, no stored mutation.
CASE("rotate_wraps_modulo_notes_per_octave") {
    RotatedScaleView wrapped(Scale::get(kMajor), 9);   // 9 mod 7 == 2
    RotatedScaleView direct(Scale::get(kMajor), 2);
    for (int n = 0; n < 7; ++n) {
        expectTrue(nearly(wrapped.noteToVolts(n), direct.noteToVolts(n)), "9 wraps to 2");
    }
}

// section 8 / section 6: Stochastic composes scaleRotate (scale degree set) with
// degreeRotation (ticket selection) — heavy ticket lands on the rotated degree
// AND is voiced through the rotated scale.
CASE("stochastic_scale_rotate_composes_with_degree_rotation") {
    StochasticSequence seq;
    seq.clear();
    const int N = 7;
    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 0);
    seq.setDegreeTicket(0, 100);
    seq.setDegreeRotation(2);

    int steered = -1;
    for (int d = 0; d < N; ++d) {
        if (seq.effectiveDegreeTicket(d, N) == 100) { steered = d; break; }
    }
    expectEqual(steered, 2, "degreeRotation=2 steers heavy ticket to degree 2");

    RotatedScaleView rot(Scale::get(kMajor), 2);
    float volts = rot.noteToVolts(steered);
    // Major idx4=7 minus idx2=4 = 3 semitones above the rotated root.
    expectTrue(nearly(volts, (7.f - 4.f) / 12.f), "composed pitch reflects both rotations");
    expectTrue(!nearly(volts, Scale::get(kMajor).noteToVolts(steered)),
               "scaleRotate alters voltage vs unrotated scale");
    expectTrue(!nearly(volts, rot.noteToVolts(0)),
               "degreeRotation alters selected degree vs no rotation");
}

// NOT covered elsewhere (1): MIDI between-degree round-trip. An input voltage
// that lands BETWEEN two rotated degrees must quantize onto the rotated grid
// (floor to the lower degree, octave preserved), then invert cleanly. This is
// the scale math behind the live MIDI-record path (NoteTrackEngine.cpp:893-899).
CASE("midi_between_degree_round_trip") {
    RotatedScaleView v(Scale::get(kMajor), 2);   // Phrygian: 0 1 3 5 7 8 10 semis

    // Degree 1 = 1 semi (0.0833V), degree 2 = 3 semis (0.25V). Pick a voltage
    // strictly between them, off the grid.
    float deg1Volts = v.noteToVolts(1);
    float deg2Volts = v.noteToVolts(2);
    float offGrid = (deg1Volts + deg2Volts) * 0.5f;   // ~0.208V, between the two
    expectTrue(offGrid > deg1Volts && offGrid < deg2Volts, "probe voltage is off-grid");

    int note = v.noteFromVolts(offGrid);
    expectEqual(note, 1, "off-grid voltage floors to the lower rotated degree");
    expectTrue(nearly(v.noteToVolts(note), deg1Volts), "requantized note re-emits degree 1 voltage");

    // Octave is preserved when the off-grid sample sits one octave up.
    float offGridOctave = offGrid + 1.f;
    int noteOctave = v.noteFromVolts(offGridOctave);
    expectEqual(noteOctave, 1 + 7, "off-grid voltage one octave up floors to degree 1 + 1 octave");
    expectTrue(nearly(v.noteToVolts(noteOctave), deg1Volts + 1.f),
               "octave-shifted requantize tracks the octave");

    // Exact-degree inputs still invert (sanity vs the off-grid case above).
    for (int n = 0; n < 7; ++n) {
        expectEqual(v.noteFromVolts(v.noteToVolts(n)), n, "exact-degree inverse");
    }
}

// NOT covered elsewhere (2): rotated noteName on a many-degree non-chromatic
// n-tet scale. The octave-division rounding in the non-chromatic branch is least
// obvious here; assert a sane degree+octave, including the degree==N wrap.
CASE("rotated_note_name_on_19tet") {
    int idx = scaleIndexByName("19-tet");
    expectTrue(idx >= 0, "19-tet scale present by name");
    const Scale &tet19 = Scale::get(idx);
    expectEqual(tet19.notesPerOctave(), 19, "19-tet has 19 degrees");
    expectTrue(!tet19.isChromatic(), "19-tet is non-chromatic");

    RotatedScaleView v(tet19, 3);   // rotate onto the 4th degree

    // Non-chromatic noteName prints 1-based degree + signed octave. Degree 0 of
    // the rotated view is "1" at octave +0.
    FixedStringBuilder<8> s0; v.noteName(s0, 0, 0, Scale::Long);
    expectEqual(std::strcmp((const char *)s0, "1+0"), 0, "rotated 19-tet degree 0 names 1+0");

    // Mid-octave degree: index 5 -> degree 6, still octave +0.
    FixedStringBuilder<8> s5; v.noteName(s5, 5, 0, Scale::Long);
    expectEqual(std::strcmp((const char *)s5, "6+0"), 0, "rotated 19-tet degree 5 names 6+0");

    // The wrap: note == notesPerOctave folds to degree 1 of the next octave.
    FixedStringBuilder<8> s19; v.noteName(s19, 19, 0, Scale::Long);
    expectEqual(std::strcmp((const char *)s19, "1+1"), 0, "rotated 19-tet degree 19 names 1+1");
}

} // UNIT_TEST("ScaleRotateAcceptance")
