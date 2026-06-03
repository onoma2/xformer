#include "StochasticPerformanceListModel.h"

// Flat item order, top to bottom (sections grouped):
//   Playback, Pitch, Rhythm, Burst, Pattern sieve, Sequence window, State evolution.
// Out-of-line so the array is a single C++11 definition (no inline-variable).
const StochasticPerformanceListModel::Item StochasticPerformanceListModel::items[] = {
    // Playback
    StochasticPerformanceListModel::Rhythm,
    StochasticPerformanceListModel::Melody,
    StochasticPerformanceListModel::Refresh,
    // Pitch
    StochasticPerformanceListModel::Complexity,
    StochasticPerformanceListModel::Contour,
    StochasticPerformanceListModel::Repeat,
    StochasticPerformanceListModel::Bias,
    StochasticPerformanceListModel::Spread,
    StochasticPerformanceListModel::MaskMelody,
    StochasticPerformanceListModel::TiltMelody,
    StochasticPerformanceListModel::Range,
    // Rhythm
    StochasticPerformanceListModel::NoteDuration,
    StochasticPerformanceListModel::Variation,
    StochasticPerformanceListModel::Rest,
    StochasticPerformanceListModel::GateLength,
    StochasticPerformanceListModel::SlideProb,
    StochasticPerformanceListModel::LegatoProb,
    // Burst
    StochasticPerformanceListModel::Burst,
    StochasticPerformanceListModel::BurstCount,
    StochasticPerformanceListModel::BurstRate,
    StochasticPerformanceListModel::BurstHold,
    // Pattern sieve (rhythm)
    StochasticPerformanceListModel::MaskRhythm,
    StochasticPerformanceListModel::TiltRhythm,
    // Sequence window
    StochasticPerformanceListModel::Size,
    StochasticPerformanceListModel::First,
    StochasticPerformanceListModel::Rotate,
    // State evolution
    StochasticPerformanceListModel::Sleep,
    StochasticPerformanceListModel::Patience,
    StochasticPerformanceListModel::PatienceMelody,
    StochasticPerformanceListModel::Mutate,
    StochasticPerformanceListModel::Jump,
    StochasticPerformanceListModel::LastItem
};
