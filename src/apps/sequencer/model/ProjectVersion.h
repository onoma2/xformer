// Project version enum for serialization; guarded to avoid redefinition
#pragma once

// ============================================================================
// VERSION POLICY — READ BEFORE TOUCHING ANYTHING IN THIS FILE
// ============================================================================
// The Version4..Version35 chain below was relevant ONLY up to Version35.
// Each entry there represents a historical add-field-with-read-guard event
// that mattered when projects were shared between shipped firmware and dev
// builds.
//
// Current policy (post-Version35):
//   - Dev branch stays at the existing `Latest` version. Do NOT add a new
//     enum entry, do NOT add a `dataVersion() >= VersionN` guard, do NOT
//     reserve a placeholder constant.
//   - Add new fields to model structs by writing/reading them unconditionally.
//     Dev project files on SD card are accepted to break across branches —
//     that breakage is the trade we already chose.
//   - The historical entries below are NOT a pattern to extend. Do not
//     pattern-match from `NoteSequence.cpp` / `Project.cpp` read-guards;
//     those are frozen legacy code paths.
//
// A new `VersionN` enum entry is added ONLY when the user explicitly says
// so during release prep — never as a side effect of feature work, never
// "before merge", never as a punch-list item, never because a spec template
// or plan doc says it should be.
//
// If any spec, plan, or task wiki proposes a bump or a `Version_*_Pending`
// placeholder, treat it as stale and flag it for removal.
//
// Project root pointers: PROJECT.md:385 (HARD RULE), AGENTS.md:102.
// ============================================================================

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

    // BASELINE VERSION: Contains all features (Accumulator, Tuesday, Discrete, Indexed, etc.)
    Version33 = 33,

    // added NoteSequence::Mode and NoteSequence::divisorY
    // added CvRoute settings
    // added Project::busSafety
    Version34 = 34,

    // added StochasticSequence::tiltMelody (unipolar mask-melody inversion)
    Version35 = 35,

    // 0.8.0 baseline: Fractal track (model + sequence), file magic, strict load.
    // Pre-0.8 files are rejected wholesale — no migration below this version.
    Version36 = 36,

    // automatically derive latest version
    Last,
    Latest = Last - 1,
};
