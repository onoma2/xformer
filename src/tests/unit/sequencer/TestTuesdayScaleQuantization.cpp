#include "UnitTest.h"
#include "apps/sequencer/model/TuesdaySequence.h"
#include "apps/sequencer/model/Scale.h"
#include "core/utils/StringBuilder.h"

UNIT_TEST("TuesdayScaleQuantization") {

CASE("scale_defaults_to_project") {
    TuesdaySequence seq;
    expectEqual(seq.scale(), -!, "scale should default to -1 Project");
}

CASE("scale_0_is_semitones_chromatic") {
    const Scale &scale = Scale::get(0);
    expectTrue(scale.isChromatic(), "Scale 0 should be chromatic");
    expectEqual(scale.notesPerOctave(), 12, "Semitones scale should have 12 notes per octave");
}

CASE("scale_negative1_means_project_scale") {
    TuesdaySequence seq;
    seq.setScale(-1);
    expectEqual(seq.scale(), -1, "scale -1 should be project scale");
}

CASE("scale_clamping") {
    TuesdaySequence seq;

    seq.setScale(-5);
    expectEqual(seq.scale(), -1, "scale should clamp to -1 minimum");

    int maxScale = Scale::Count - 1;
    seq.setScale(maxScale + 10);
    expectEqual(seq.scale(), maxScale, "scale should clamp to Scale::Count - 1");
}

CASE("scale_editing") {
    TuesdaySequence seq;
    seq.setScale(5);

    seq.editScale(1, false);
    expectEqual(seq.scale(), 6, "editScale should increment");

    seq.editScale(-2, false);
    expectEqual(seq.scale(), 4, "editScale should decrement");
}

CASE("scale_print_formatting") {
    TuesdaySequence seq;
    FixedStringBuilder<32> str;

    seq.setScale(-1);
    seq.printScale(str);
    expectEqual((const char *)str, "Default", "scale -1 should print 'Default'");

    str.reset();
    seq.setScale(0);
    seq.printScale(str);
    expectEqual((const char *)str, "Semitones", "scale 0 should print 'Semitones'");

    str.reset();
    seq.setScale(1);
    seq.printScale(str);
    expectEqual((const char *)str, "Major", "scale 1 should print 'Major'");
}

CASE("clear_resets_to_chromatic") {
    TuesdaySequence seq;
    seq.setScale(5);
    seq.clear();
    expectEqual(seq.scale(), 0, "clear() should reset scale to 0 (chromatic)");
}

CASE("scale_values_correspond_to_scale_names") {
    // Verify scale indices match expected scale names
    expectEqual(Scale::name(0), "Semitones", "Scale 0 should be Semitones");
    expectEqual(Scale::name(1), "Major", "Scale 1 should be Major");
    expectEqual(Scale::name(2), "Minor", "Scale 2 should be Minor");
}

} // UNIT_TEST("TuesdayScaleQuantization")
