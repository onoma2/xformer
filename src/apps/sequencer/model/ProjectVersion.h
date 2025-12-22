// Project version enum for serialization; guarded to avoid redefinition
#pragma once

enum ProjectVersion {
    // added NoteTrack::cvUpdateMode
    Version4 = 4,

    // added storing user scales with project
    // added Project::name
    // added UserScale::name
    Version5 = 5,

    // added Project::cvGateInput
    Version6 = 6,

    // added NoteSequence::Step::gateOffset
    Version7 = 7,

    // added CurveTrack::slideTime
    Version8 = 8,

    // added MidiCvTrack::arpeggiator
    Version9 = 9,

    // expanded divisor to 16 bits
    Version10 = 10,

    // added ClockSetup::clockOutputSwing
    // added Project::curveCvInput
    Version11 = 11,

    // added TrackState::fillAmount
    // added NoteSequence::Step::condition
    Version12 = 12,

    // added Routing::MidiSource::Event::NoteRange
    Version13 = 13,

    // swapped Curve::Type::Low with Curve::Type::High
    Version14 = 14,

    // added MidiCvTrack::lowNote/highNote
    // changed CurveSequence::Step layout
    // added CurveTrack::shapeProbabilityBias
    // added CurveTrack::gateProbabilityBias
    Version15 = 15,

    // added MidiCvTrack::notePriority
    Version16 = 16,

    // changed Arpeggiator::octaves
    Version17 = 17,

    // added Project::timeSignature
    Version18 = 18,

    // expanded Song::slots to 64 entries
    Version19 = 19,

    // added MidiCvTrack::slideTime
    Version20 = 20,

    // added MidiCvTrack::transpose
    Version21 = 21,

    // added CurveTrack::muteMode
    Version22 = 22,

    // added Route::Target::Scale and Route::Target::RootNote
    Version23 = 23,

    // expanded MidiOutput::outputs to 16 entries
    Version24 = 24,

    // added Song::Slot::mutes
    Version25 = 25,

    // added NoteTrack::fillMuted
    Version26 = 26,

    // expanded NoteSequence::Step to 64 bits, expanded NoteSequence::Step::Condition to 7 bits
    Version27 = 27,

    // added CurveTrack::offset
    Version28 = 28,

    // added Project::midiInput
    Version29 = 29,

    // added Project::monitorMode
    Version30 = 30,

    // changed MidiCvTrack::VoiceConfig to 8-bit value
    Version31 = 31,

    // added Project::midiIntegrationMode, Project::midiProgramOffset, Project::alwaysSync
    Version32 = 32,

    // added NoteSequence::Accumulator serialization
    Version33 = 33,

    // added NoteSequence harmony properties (harmonyRole, masterTrackIndex, harmonyScale)
    Version34 = 34,

    // added TuesdayTrack serialization
    Version35 = 35,

    // added TuesdayTrack::gateOffset
    Version40 = 40,

    // added TuesdayTrack::trill
    Version41 = 41,

    // added CurveTrack::globalPhase
    Version42 = 42,

    // added CurveTrack wavefolder properties
    Version43 = 43,

    // added CurveTrack dj filter
    Version44 = 44,

    // added CurveTrack feedback parameters
    Version45 = 45,

    // added CurveTrack xFade parameter
    Version46 = 46,

    // added Track::cvOutputRotate and Track::gateOutputRotate
    Version47 = 47,

    // added CurveTrack chaos parameters
    Version48 = 48,

    // added CurveTrack::chaosAlgo
    Version49 = 49,

    // added TuesdayTrack patterns
    Version50 = 50,

    // added TuesdaySequence stepTrill parameter
    Version51 = 51,

    // added TuesdaySequence tickMask parameter
    Version52 = 52,

    // added TuesdaySequence primeMaskPattern and primeMaskParameter
    Version53 = 53,

    // added TuesdaySequence timeMode
    Version54 = 54,

    // removed TuesdaySequence primeMaskPattern, kept primeMaskParameter with new meaning
    Version55 = 55,

    // added TuesdaySequence maskProgression
    Version56 = 56,

    // added Routing per-track bias/depth percentages
    Version57 = 57,

    // added DiscreteMap track mode
    Version58 = 58,

    // added DiscreteMap track octave/transpose
    Version59 = 59,

    // added DiscreteMap reset measure + sync mode
    Version60 = 60,

    // added DiscreteMapSequence::rangeHigh and rangeLow (ABOVE/BELOW parameters)
    Version61 = 61,

    // added DiscreteMapTrack::cvUpdateMode
    Version62 = 62,

    // added Routing::Route shaper mode
    Version63 = 63,

    // added Indexed track mode
    Version64 = 64,

    // added IndexedSequence::resetMeasure
    Version65 = 65,

    // added IndexedSequence::syncMode
    Version66 = 66,

    // added IndexedTrack::cvUpdateMode
    Version67 = 67,

    // added IndexedTrack octave/transpose
    Version68 = 68,

    // added Track::runGate
    Version69 = 69,

    // added ClockSetup::clockInputMultiplier
    Version70 = 70,

    // added IndexedSequence route combine mode
    Version71 = 71,

    // automatically derive latest version
    Last,
    Latest = Last - 1,
};
