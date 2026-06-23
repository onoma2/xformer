#include "UnitTest.h"

#include "apps/sequencer/model/StochasticSequence.h"
#include "apps/sequencer/model/RotatedScale.h"
#include "apps/sequencer/model/Scale.h"

// Composition of the two independent rotations a Stochastic track applies:
//   scaleRotate    — modal rotation of the degree->voltage mapping, carried by
//                    the RotatedScaleView the engine quantizes against.
//   degreeRotation — rotation of ticket selection WITHIN activeNotes, applied
//                    by StochasticSequence::effectiveDegreeTicket.
// They must compose without resizing activeNotes (notesPerOctave delegates to
// the base scale), so a degree picked under degreeRotation, fed through the
// scaleRotate-rotated scale, reflects BOTH shifts.

static constexpr int kMajor = 1;   // Major: 7 notes, semitones 0,2,4,5,7,9,11.

static bool nearly(float a, float b) {
    float d = a - b;
    return (d < 0 ? -d : d) < 1e-4f;
}

UNIT_TEST("StochasticScaleRotate") {

CASE("scaleRotate_rotates_degree_to_voltage_mapping") {
    const Scale &base = Scale::get(kMajor);
    expectEqual(base.notesPerOctave(), 7, "Major has 7 notes per octave");
    expectTrue(base.supportsRotation(), "Major supports modal rotation");

    RotatedScaleView rot(base, 2);   // start on the 3rd scale degree.
    expectEqual(rot.notesPerOctave(), 7, "rotation does not resize activeNotes");

    // Unrotated intervals from root: 0, 2, 4 semitones (E,F#-> in Major terms).
    expectTrue(nearly(base.noteToVolts(0), 0.f / 12.f), "base deg0 = 0");
    expectTrue(nearly(base.noteToVolts(1), 2.f / 12.f), "base deg1 = 2 semis");
    expectTrue(nearly(base.noteToVolts(2), 4.f / 12.f), "base deg2 = 4 semis");

    // Rotated by 2: deg0 -> base(2)-base(2)=0, deg1 -> base(3)-base(2),
    // deg2 -> base(4)-base(2). Major semis: idx2=4, idx3=5, idx4=7.
    expectTrue(nearly(rot.noteToVolts(0), 0.f / 12.f), "rot deg0 = 0");
    expectTrue(nearly(rot.noteToVolts(1), (5.f - 4.f) / 12.f), "rot deg1 = 1 semi");
    expectTrue(nearly(rot.noteToVolts(2), (7.f - 4.f) / 12.f), "rot deg2 = 3 semis");

    // The two mappings genuinely differ — scaleRotate is not a no-op.
    expectTrue(!nearly(rot.noteToVolts(1), base.noteToVolts(1)),
               "scaleRotate must change the degree->voltage mapping");
}

CASE("degreeRotation_rotates_ticket_selection_within_activeNotes") {
    StochasticSequence seq;
    seq.clear();
    const int N = 7;
    // Single ticket on class 0; everything else default-flat (0). Picking is
    // structural here, not random — we read the rotated ticket table directly.
    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 0);
    seq.setDegreeTicket(0, 100);

    // No degreeRotation: heavy ticket stays at output degree 0.
    seq.setDegreeRotation(0);
    expectEqual(seq.effectiveDegreeTicket(0, N), 100, "drot=0 keeps ticket at deg0");
    expectEqual(seq.effectiveDegreeTicket(2, N), 0,   "drot=0 leaves deg2 flat");

    // degreeRotation=2: ticket at source index 0 surfaces at output degree 2.
    seq.setDegreeRotation(2);
    expectEqual(seq.effectiveDegreeTicket(2, N), 100, "drot=2 moves ticket to deg2");
    expectEqual(seq.effectiveDegreeTicket(0, N), 0,   "drot=2 leaves deg0 flat");
}

CASE("scaleRotate_and_degreeRotation_compose") {
    // Both rotations on at once. degreeRotation=2 makes the heavy ticket land
    // on output degree 2; scaleRotate=2 maps degree 2 to base(4)-base(2)=3 semis.
    // The audible pitch therefore reflects BOTH: the selected degree index AND
    // the rotated scale voltage for that index.
    StochasticSequence seq;
    seq.clear();
    const int N = 7;
    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 0);
    seq.setDegreeTicket(0, 100);
    seq.setDegreeRotation(2);

    // The degree the ticket system steers toward, given degreeRotation.
    int steeredDegree = -1;
    for (int d = 0; d < N; ++d) {
        if (seq.effectiveDegreeTicket(d, N) == 100) { steeredDegree = d; break; }
    }
    expectEqual(steeredDegree, 2, "degreeRotation=2 steers the heavy ticket to degree 2");

    // Engine builds note = degree + octave*activeNotes, then quantizes through
    // the scaleRotate-rotated scale. Octave 0 here.
    RotatedScaleView rot(Scale::get(kMajor), 2);
    float volts = rot.noteToVolts(steeredDegree);

    // Composition target: 3 semitones above root (Major idx4=7 minus idx2=4).
    expectTrue(nearly(volts, (7.f - 4.f) / 12.f),
               "composed pitch reflects both ticket rotation and scale rotation");

    // Distinct from each rotation acting alone: with no scaleRotate the same
    // degree would be 4 semis (Major idx2); with no degreeRotation the ticket
    // would sit at degree 0 = 0 semis under the rotated scale.
    expectTrue(!nearly(volts, Scale::get(kMajor).noteToVolts(steeredDegree)),
               "scaleRotate changes the voltage vs the unrotated scale");
    expectTrue(!nearly(volts, rot.noteToVolts(0)),
               "degreeRotation changes the selected degree vs no rotation");
}

} // UNIT_TEST("StochasticScaleRotate")
