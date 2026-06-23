#pragma once

#include "Config.h"
#include "Bitfield.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Types.h"
#include "Scale.h"
#include "RotatedScale.h"

#include "core/math/Math.h"

#include "Routing.h"
#include "RouteParamKey.h"
#include "ProjectVersion.h"
#include "StochasticTypes.h"

class StochasticSequence {
public:
    //----------------------------------------
    // Properties
    //----------------------------------------

    // trackIndex
    int trackIndex() const { return _trackIndex; }
    void setTrackIndex(int trackIndex) { _trackIndex = trackIndex; }

    // scale
    int rawScale() const { return int(_scaleGroup.scale) - 1; }
    int scale() const { return Routing::routedValueInt(ParamKey::Scale, _trackIndex, rawScale(), 0, Scale::Count - 1); }
    void setScale(int scale) { _scaleGroup.scale = clamp(scale, -1, Scale::Count - 1) + 1; }
    RotatedScaleView selectedScale(int projectScale, int projectRotate) const {
        int idx    = scale()       < 0 ? projectScale  : scale();
        int rotate = scaleRotate() < 0 ? projectRotate : scaleRotate();
        return RotatedScaleView(Scale::get(idx), rotate);
    }

    // rootNote
    int rawRootNote() const { return int(_scaleGroup.rootNote) - 1; }
    int rootNote() const { return Routing::routedValueInt(ParamKey::RootNote, _trackIndex, rawRootNote(), 0, 11); }
    void setRootNote(int rootNote) { _scaleGroup.rootNote = clamp(rootNote, -1, 11) + 1; }

    // scaleRotate
    int scaleRotate() const { return int(_scaleGroup.scaleRotate) - 1; }
    void setScaleRotate(int scaleRotate) { _scaleGroup.scaleRotate = clamp(scaleRotate, -1, 31) + 1; }
    int selectedRootNote(int defaultRootNote) const {
        int r = rootNote();
        return r != -1 ? r : defaultRootNote;
    }

    // divisor
    int divisor() const { return _divisor; }
    void setDivisor(int divisor) { _divisor = ModelUtils::clampDivisor(divisor); }

    // clockMultiplier
    int clockMultiplier() const { return Routing::routedValueInt(ParamKey::ClockMultiplier, _trackIndex, _clockMultiplier, 50, 150); }
    void setClockMultiplier(int clockMultiplier) {
        _clockMultiplier = clamp(clockMultiplier, 50, 150);
    }
    void editClockMultiplier(int value, bool shift) {
        setClockMultiplier(_clockMultiplier + value * (shift ? 10 : 1));
    }
    void printClockMultiplier(StringBuilder &str) const {
        printRouted(str, Routing::Target::ClockMult);
        str("%.2fx", clockMultiplier() * 0.01f);
    }

    // resetMeasure
    int resetMeasure() const { return _resetMeasure; }
    void setResetMeasure(int resetMeasure) { _resetMeasure = clamp(resetMeasure, 0, 128); }

    void printDivisor(StringBuilder &str) const {
        ModelUtils::printDivisor(str, _divisor);
    }

    void editDivisor(int value, bool shift) {
        setDivisor(ModelUtils::adjustedByDivisor(divisor(), value, shift));
    }

    void printResetMeasure(StringBuilder &str) const {
        if (_resetMeasure == 0) {
            str("Off");
        } else {
            str("%d", _resetMeasure);
        }
    }

    void editResetMeasure(int value, bool shift) {
        setResetMeasure(resetMeasure() + value);
    }

    void printScale(StringBuilder &str) const {
        if (scale() == -1) {
            str("Default");
        } else {
            str(Scale::name(scale()));
        }
    }

    void editScale(int value, bool shift) {
        setScale(rawScale() + value);
    }


    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    inline void printRouted(StringBuilder &str, Routing::Target target) const { Routing::printRouted(str, target, _trackIndex); }

    // Phase 8.1 Source Modes
    StochasticSourceMode rhythmMode() const { return _rhythmMode; }
    void setRhythmMode(StochasticSourceMode mode) { _rhythmMode = ModelUtils::clampedEnum(mode); }

    StochasticSourceMode melodyMode() const { return _melodyMode; }
    void setMelodyMode(StochasticSourceMode mode) { _melodyMode = ModelUtils::clampedEnum(mode); }

    void printCoupledMode(StringBuilder &str) const {
        if (rhythmMode() == melodyMode()) {
            str(rhythmMode() == StochasticSourceMode::Loop ? "Loop" : "Live");
        } else {
            str("Split");
        }
    }
    void editCoupledMode(int value, bool shift) {
        StochasticSourceMode mode = ModelUtils::adjustedEnum(rhythmMode(), value);
        setRhythmMode(mode);
        setMelodyMode(mode);
    }

    void refresh() {
        if (_rhythmMode == StochasticSourceMode::Loop) _rhythmValid = false;
        if (_melodyMode == StochasticSourceMode::Loop) _melodyValid = false;
    }

    int complexity() const { return Routing::routedValueInt(ParamKey::Complexity, _trackIndex, _complexity, 0, 100); }
    int complexityBase() const { return _complexity; }
    void setComplexity(int complexity) { _complexity = clamp(complexity, 0, 100); }

    int contour() const { return Routing::routedValueInt(ParamKey::Contour, _trackIndex, _contour, -100, 100); }
    int contourBase() const { return _contour; }
    void setContour(int contour) { _contour = clamp(contour, -100, 100); }

    // Phase 12: _repeatProb field repurposed as Repeat probability. Per-event
    // chance to reuse the previously-generated event verbatim (note, octave,
    // duration, articulation). 100% freezes on the last event. Mid values
    // cluster durations like a human player.
    int repeatProb() const { return _repeatProb; }
    void setRepeatProb(int value) { _repeatProb = clamp(value, 0, 100); }

    // Live-mode duration LUT entry index 0..7. Divisor-relative: see
    // StochasticTrackEngine::getDurationFraction() for the multiplier table.
    int noteDuration() const { return Routing::routedValueInt(ParamKey::NoteDuration, _trackIndex, _noteDuration, 0, 7); }
    int noteDurationBase() const { return _noteDuration; }
    void setNoteDuration(int v) { _noteDuration = clamp(v, 0, 7); }

    // Phase 11: storage may be signed (legacy patches); read always returns |v|.
    int variation() const { int b = _variation; return Routing::routedValueInt(ParamKey::Variation, _trackIndex, b < 0 ? -b : b, 0, 100); }
    int variationBase() const { int b = _variation; return b < 0 ? -b : b; }
    // Phase 11: variation is symmetric (sign meaningless). New writes clamp to
    // [0, 100]. Legacy patches with negative storage get abs()'d at getter.
    void setVariation(int variation) { _variation = clamp(variation, 0, 100); }

    int rest() const { return Routing::routedValueInt(ParamKey::Rest, _trackIndex, _rest, 0, 100); }
    int restBase() const { return _rest; }
    void setRest(int rest) { _rest = clamp(rest, 0, 100); }

    int slide() const { return Routing::routedValueInt(ParamKey::Slide, _trackIndex, _slide, 0, 100); }
    int slideBase() const { return _slide; }
    void setSlide(int slide) { _slide = clamp(slide, 0, 100); }

    int burstRate() const { return Routing::routedValueInt(ParamKey::BurstRate, _trackIndex, _burstRate, 0, 100); }
    int burstRateBase() const { return _burstRate; }
    void setBurstRate(int rate) { _burstRate = clamp(rate, 0, 100); }

    int burstCount() const { return Routing::routedValueInt(ParamKey::BurstCount, _trackIndex, _burstCount, 0, 100); }
    int burstCountBase() const { return _burstCount; }
    void setBurstCount(int count) { _burstCount = clamp(count, 0, 100); }

    StochasticBurstHold burstHold() const { return _burstHold; }
    void setBurstHold(StochasticBurstHold pitch) { _burstHold = ModelUtils::clampedEnum(pitch); }

    int sleep() const { return Routing::routedValueInt(ParamKey::Sleep, _trackIndex, _sleep, 0, 100); }
    int sleepBase() const { return _sleep; }
    void setSleep(int sleep) { _sleep = clamp(sleep, 0, 100); }


    // mutate — probability 0..100 that one randomly-picked step inside the
    // active window gets destructively re-rolled at each cycle-end. Unipolar.
    int mutate() const { return Routing::routedValueInt(ParamKey::Mutate, _trackIndex, _mutate, 0, 100); }
    int mutateBase() const { return _mutate; }
    // Bipolar: -100..0 = Proteus destructive (regenerate one event), 0 = lock,
    // 0..+100 = Marbles permutation (swap two existing events). Magnitude is
    // probability per loop iteration.
    void setMutate(int mutate) { _mutate = clamp(mutate, 0, 100); }

    int jump() const { return Routing::routedValueInt(ParamKey::Jump, _trackIndex, _jump, 0, 100); }
    int jumpBase() const { return _jump; }
    void setJump(int jump) { _jump = clamp(jump, 0, 100); }

    // Range — bipolar field width / jump-chance knob, 0..100 centered at 50.
    // 50 = single-octave field (no decision in Steps 3..5). >50 expands the
    // candidate set up to 4 octaves. <50 keeps single octave plus a per-slot
    // octave-displacement chance. See PITCH-LAW-FINAL.md Step 3.
    int range() const { return Routing::routedValueInt(ParamKey::Range, _trackIndex, _range, 0, 100); }
    int rangeBase() const { return _range; }
    void setRange(int range) { _range = clamp(range, 0, 100); }

    // degreeTickets
    int degreeTicket(int degree) const { return _degreeTickets[degree]; }
    void setDegreeTicket(int degree, int tickets) { _degreeTickets[degree] = clamp(tickets, -1, 100); }

    // Effective ticket after degree + mask rotation (same logic as generator)
    int effectiveDegreeTicket(int degree, int activeNotes) const {
        int dRot = (degreeRotation() % activeNotes + activeNotes) % activeNotes;
        int srcIdx = (degree - dRot + activeNotes * 2) % activeNotes;
        int ticket = _degreeTickets[srcIdx];
        if (ticket < 0) return -1;
        // Count non-excluded and apply mask rotation
        int nonNegCount = 0;
        int nonNegWeight[CONFIG_USER_SCALE_SIZE];
        int8_t effTickets[CONFIG_USER_SCALE_SIZE];
        for (int i = 0; i < activeNotes; ++i) {
            int idx = (i - dRot + activeNotes * 2) % activeNotes;
            effTickets[i] = _degreeTickets[idx];
            if (effTickets[i] >= 0) {
                nonNegWeight[nonNegCount++] = effTickets[i];
            }
        }
        int mRot = maskRotation();
        if (nonNegCount > 1 && mRot != 0) {
            int rotSteps = (mRot % nonNegCount + nonNegCount) % nonNegCount;
            int pos = 0;
            for (int i = 0; i < activeNotes; ++i) {
                if (effTickets[i] >= 0) {
                    int srcPos = (pos - rotSteps + nonNegCount) % nonNegCount;
                    effTickets[i] = nonNegWeight[srcPos];
                    pos++;
                }
            }
        }
        return effTickets[degree];
    }

    // degreeRotation
    int degreeRotation() const { return _degreeRotation; }
    void setDegreeRotation(int rotation) { _degreeRotation = clamp(rotation, -32, 32); }

    // maskRotation
    int maskRotation() const { return _maskRotation; }
    void setMaskRotation(int rotation) { _maskRotation = clamp(rotation, -32, 32); }

    // accentProb
    // Phase 12: _patienceMelody field repurposed as melody patience (independent of
    // rhythm patience). Same Poisson-CDF behavior. Knob 0..99 → λ ∈ [1, 50];
    // knob 100 = off sentinel. No routing target (matches Repeat treatment).
    int patienceMelody() const { return _patienceMelody; }
    void setPatienceMelody(int value) { _patienceMelody = clamp(value, 0, 100); }
    int patienceRhythm() const { return Routing::routedValueInt(ParamKey::PatienceRhythm, _trackIndex, _patienceRhythm, 0, 100); }
    int patienceRhythmBase() const { return _patienceRhythm; }
    void setPatienceRhythm(int value) { _patienceRhythm = clamp(value, 0, 100); }

    // degreeRotation
    int legatoProb() const { return _legatoProb; }
    void setLegatoProb(int prob) { _legatoProb = clamp(prob, 0, 100); }

    // marblesMode
    MarblesMode marblesMode() const { return _marblesMode; }
    void setMarblesMode(MarblesMode mode) { _marblesMode = ModelUtils::clampedEnum(mode); }

    // marblesSpread
    int marblesSpread() const { return Routing::routedValueInt(ParamKey::MarblesSpread, _trackIndex, _marblesSpread, 0, 100); }
    int marblesSpreadBase() const { return _marblesSpread; }
    void setMarblesSpread(int spread) { _marblesSpread = clamp(spread, 0, 100); }

    // marblesBias
    int marblesBias() const { return Routing::routedValueInt(ParamKey::MarblesBias, _trackIndex, _marblesBias, 0, 100); }
    int marblesBiasBase() const { return _marblesBias; }
    void setMarblesBias(int bias) { _marblesBias = clamp(bias, 0, 100); }

    // maskMelody — pitch-centrality threshold. 0..100, 100=bypass. Acts at
    // trigger time, gated on melodyMode==Loop. Inert in LiveM.
    int maskMelody() const { return _maskMelody; }
    void setMaskMelody(int value) { _maskMelody = clamp(value, 0, 100); }

    // tiltMelody — unipolar inversion magnitude for the melody mask.
    // 0 = natural (high-centrality survives), 100 = inverted (low survives).
    int tiltMelody() const { return _tiltMelody; }
    void setTiltMelody(int value) { _tiltMelody = clamp(value, 0, 100); }

    // maskRhythm (deterministic loop playback thinning, V5 rename from density)
    int maskRhythm() const { return Routing::routedValueInt(ParamKey::MaskRhythm, _trackIndex, _maskRhythm, 0, 100); }
    int maskRhythmBase() const { return _maskRhythm; }
    void setMaskRhythm(int value) { _maskRhythm = clamp(value, 0, 100); }

    // gateLength — repurposed from the Phase 11 `_gateLength` reserved field. Knob
    // 0..100 controls the spread of per-event gate length around a hardcoded
    // 50% center. 0 = exact 50% every event; 100 = triangular distribution
    // 10..100% peaked at 50%. Floored at 10% of duration AND at an absolute
    // audible-tick minimum so very short events still produce a real trigger.
    // Routing target keeps the legacy `StochasticGeneratorDensity` enum name
    // for save-file compatibility; UI label changes to "Gate Length".
    int gateLength() const { return Routing::routedValueInt(ParamKey::StochasticGateLength, _trackIndex, _gateLength, 0, 100); }
    int gateLengthBase() const { return _gateLength; }
    void setGateLength(int value) { _gateLength = clamp(value, 0, 100); }

    // tiltRhythm — paired with maskRhythm. 0..100, center 50 = pure salt cut,
    // 0 = max negative rank-cut, 100 = max positive rank-cut. Engine recovers
    // signed magnitude as (knob - 50) and uses |signed| * 2 as the rank weight.
    int tiltRhythm() const { return Routing::routedValueInt(ParamKey::TiltRhythm, _trackIndex, _tiltRhythm, 0, 100); }
    int tiltRhythmBase() const { return _tiltRhythm; }
    void setTiltRhythm(int value) { _tiltRhythm = clamp(value, 0, 100); }

    // burs
    int burst() const { return Routing::routedValueInt(ParamKey::Burst, _trackIndex, _burst, 0, 100); }
    int burstBase() const { return _burst; }
    void setBurst(int burst) { _burst = clamp(burst, 0, 100); }

    // feel (Phase 16 P4, 2026-05-23) — knob 0..100, default 50. Detent
    // [45..55] = off (no scaling). knob < 45 lerps toward 3 beats per
    // cycle, knob > 55 lerps toward 5 beats per cycle. Generator reads
    // this in the post-walk scaling pass; inert until P5 wires it.
    int feel() const { return Routing::routedValueInt(ParamKey::Feel, _trackIndex, _feel, 0, 100); }
    int feelBase() const { return _feel; }
    void setFeel(int feel) { _feel = clamp(feel, 0, 100); }

    // rotate
    int rotate() const { return Routing::routedValueInt(ParamKey::Rotate, _trackIndex, _rotate, -64, 64); }
    int rotateBase() const { return _rotate; }
    void setRotate(int rotate) { _rotate = clamp(rotate, -64, 64); }

    // durationTickets — per-LUT-slot duration emphasis. Three states (mirror
    // of pitch tickets): -1 excludes the slot, 0 default-flat, 1..100 weight.
    int durationTicket(int index) const { return _durationTickets[index]; }
    void setDurationTicket(int index, int value) { _durationTickets[index] = clamp(value, -1, 100); }
    bool durationTicketsActive() const {
        for (int i = 0; i < 8; ++i) if (_durationTickets[i] > 0) return true;
        return false;
    }
    bool pitchTicketsActive() const {
        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) if (_degreeTickets[i] > 0) return true;
        return false;
    }

    void printRepeatProb(StringBuilder &str) const { str("%d%%", repeatProb()); }
    void editRepeatProb(int value, bool shift) { setRepeatProb(repeatProb() + value); }

    void printMaskMelody(StringBuilder &str) const { str("%d%%", maskMelody()); }
    void editMaskMelody(int value, bool shift) { setMaskMelody(maskMelody() + value); }

    void printTiltMelody(StringBuilder &str) const { str("%d%%", tiltMelody()); }
    void editTiltMelody(int value, bool shift) { setTiltMelody(tiltMelody() + value); }

    void printMarblesMode(StringBuilder &str) const { str(marblesMode() == MarblesMode::Off ? "Off" : "On"); }
    void editMarblesMode(int value, bool shift) { setMarblesMode(ModelUtils::adjustedEnum(marblesMode(), value)); }

    void printMarblesSpread(StringBuilder &str) const { str("%d%%", marblesSpread()); }
    void editMarblesSpread(int value, bool shift) { setMarblesSpread(marblesSpread() + value); }

    void printMarblesBias(StringBuilder &str) const { str("%d%%", marblesBias()); }
    void editMarblesBias(int value, bool shift) { setMarblesBias(marblesBias() + value); }


    void printMaskRhythm(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticMask); str("%d%%", maskRhythm()); }
    void editMaskRhythm(int value, bool shift) { setMaskRhythm(_maskRhythm + value); }

    // Level 1 Density macro: writes density only (no fan-out to rest)
    void printGateLength(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticGateLength); str("%d%%", gateLength()); }
    void editGateLength(int value, bool shift) {
        setGateLength(_gateLength + value);
    }

    void editComplexityMacro(int value, bool shift) {
        int c = clamp(_complexity + value, 0, 100);
        setComplexity(c);
    }

    void printTiltRhythm(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticTilt); str("%d%%", tiltRhythm()); }
    void editTiltRhythm(int value, bool shift) { setTiltRhythm(_tiltRhythm + value); }

    // Phase 12: append "*" when current noteDuration + divisor combo can't
    // produce audible burst (parentTicks < 96). Engine still gates Burst
    // probability via Cluster F; this just tells the user why the knob is
    // inert at short durations so Burst/Count/Rate aren't silently dead.
    void printBurst(StringBuilder &str) const {
        printRouted(str, Routing::Target::StochasticBurst);
        auto frac = stochasticDurationFraction(int(noteDuration()));
        // parentTicks = divisor * (CONFIG_PPQN/CONFIG_SEQUENCE_PPQN=4) * num / den.
        int parentTicks = (int(divisor()) * 4 * frac.num) / frac.den;
        const int kMinBurstParentTicks = 96;
        if (parentTicks < kMinBurstParentTicks) str("%d%% *", burst());
        else str("%d%%", burst());
    }
    void editBurst(int value, bool shift) { setBurst(_burst + value); }

    void printPatienceMelody(StringBuilder &str) const { str("%d%%", patienceMelody()); }
    void editPatienceMelody(int value, bool shift) { setPatienceMelody(patienceMelody() + value); }

    void printPatienceRhythm(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticPatienceRhythm); str("%d%%", patienceRhythm()); }
    void editPatienceRhythm(int value, bool shift) { setPatienceRhythm(_patienceRhythm + value); }

    void printLegatoProb(StringBuilder &str) const { str("%d%%", legatoProb()); }
    void editLegatoProb(int value, bool shift) { setLegatoProb(ModelUtils::adjustedByStep(legatoProb(), value, 1, !shift)); }

    void printComplexity(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticComplexity); str("%d%%", complexity()); }
    void editComplexity(int value, bool shift) { setComplexity(_complexity + value); }

    void printContour(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticContour); str("%+d%%", contour()); }
    void editContour(int value, bool shift) { setContour(_contour + value); }

    void printNoteDuration(StringBuilder &str) const {
        printRouted(str, Routing::Target::StochasticNoteDuration);
        printDurationEntry(str, noteDuration());
    }
    // Phase 11: divisor-aware label for any LUT entry. Used by both noteDuration
    // print and the duration ticket bars (so labels track clock-divisor changes
    // instead of lying with hardcoded 1/16-centered strings). Short form
    // ("x/y" / "x/yT") — no leading raw-tick value.
    void printDurationEntry(StringBuilder &str, int entry) const {
        auto frac = stochasticDurationFraction(entry);
        int effectiveDivisor = (int(divisor()) * frac.num) / frac.den;
        ModelUtils::printDivisorShort(str, effectiveDivisor);
    }
    void editNoteDuration(int value, bool shift) { setNoteDuration(_noteDuration + value); }

    void printVariation(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticVariation); str("%d%%", variation()); }
    void editVariation(int value, bool shift) { setVariation(_variation + value); }

    void printRest(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticRest); str("%d%%", rest()); }
    void editRest(int value, bool shift) { setRest(_rest + value); }

    void printSlide(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticSlide); str("%d%%", slide()); }
    void editSlide(int value, bool shift) { setSlide(_slide + value); }

    void printSleep(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticSleep); str("%d%%", sleep()); }
    void editSleep(int value, bool shift) { setSleep(_sleep + value); }


    void printMutate(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticMutate); str("%d%%", mutate()); }
    void editMutate(int value, bool shift) { setMutate(_mutate + value); }

    void printJump(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticJump); str("%d%%", jump()); }
    void editJump(int value, bool shift) { setJump(_jump + value); }

    void printBurstHold(StringBuilder &str) const {
        switch (burstHold()) {
        case StochasticBurstHold::HoldFit:  str("Hold/Fit");  break;
        case StochasticBurstHold::HoldOver: str("Hold/Over"); break;
        case StochasticBurstHold::RollFit:  str("Roll/Fit");  break;
        case StochasticBurstHold::RollOver: str("Roll/Over"); break;
        default: str("?"); break;
        }
    }
    void editBurstHold(int value, bool shift) { setBurstHold(ModelUtils::adjustedEnum(burstHold(), value)); }

    void printRotate(StringBuilder &str) const { printRouted(str, Routing::Target::Rotate); str("%+d", rotate()); }
    void editRotate(int value, bool shift) { setRotate(_rotate + value); }

    void printRootNote(StringBuilder &str) const {
        if (rootNote() == -1) {
            str("Default");
        } else {
            Types::printNote(str, rootNote());
        }
    }

    void editRootNote(int value, bool shift) {
        setRootNote(rawRootNote() + value);
    }

    // Pattern window. last() resolves to size-1 — Last is collapsed into Size.
    int size() const { return _size; }
    void setSize(int size) {
        _size = clamp(size, 2, CONFIG_STEP_COUNT);
        _first = clamp(int(_first), 0, int(_size) - 1);
    }

    int first() const { return _first; }
    void setFirst(int first) { _first = clamp(first, 0, int(_size) - 1); }

    int last() const { return int(_size) - 1; }

    bool patternValid() const { return rhythmValid() && melodyValid(); }
    void setPatternValid(bool valid) { setRhythmValid(valid); setMelodyValid(valid); }

    uint32_t seed() const { return rhythmSeed(); }
    void setSeed(uint32_t seed) { setRhythmSeed(seed); }

    void printNote(StringBuilder &str, int note, int defaultRootNote, int defaultScale, int defaultScaleRotate) const {
        int rootNote = selectedRootNote(defaultRootNote);
        const auto scale = selectedScale(defaultScale, defaultScaleRotate);
        scale.noteName(str, note, rootNote, Scale::Short1);
    }

    // Phase 8.2 Split Source Buffers
    const std::array<StochasticStepContent, CONFIG_STEP_COUNT> &steps() const { return _steps; }
          std::array<StochasticStepContent, CONFIG_STEP_COUNT> &steps()       { return _steps; }

    bool rhythmValid() const { return _rhythmValid; }
    void setRhythmValid(bool valid) { _rhythmValid = valid; }

    bool melodyValid() const { return _melodyValid; }
    void setMelodyValid(bool valid) { _melodyValid = valid; }

    uint32_t rhythmSeed() const { return _rhythmSeed; }
    void setRhythmSeed(uint32_t seed) { _rhythmSeed = seed; }

    uint32_t melodySeed() const { return _melodySeed; }
    void setMelodySeed(uint32_t seed) { _melodySeed = seed; }

    void clear() {
        setDefaults();
        for (auto &event : _steps) event.clear();
    }

    void write(VersionedSerializedWriter &writer) const {
        int8_t scaleField = rawScale();
        int8_t rootNoteField = rawRootNote();
        int8_t scaleRotateField = int8_t(scaleRotate());
        writer.write(scaleField);
        writer.write(rootNoteField);
        writer.write(scaleRotateField);
        writer.write(_divisor);
        writer.write(_resetMeasure);
        writer.write(_clockMultiplier);

        writer.write(_size);
        writer.write(_first);
        writer.write(uint8_t(0));  // reserved (was _last; now derived as size-1)

        writer.write(_rhythmValid);
        writer.write(_melodyValid);
        writer.write(_rhythmSeed);
        writer.write(_melodySeed);

        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) writer.write(_degreeTickets[i]);
        writer.write(_repeatProb);
        writer.write(_degreeRotation);
        writer.write(_maskRotation);
        writer.write(_patienceMelody);
        writer.write(_legatoProb);
        writer.write(static_cast<uint8_t>(_marblesMode));
        writer.write(_marblesSpread);
        writer.write(_marblesBias);
        writer.write(_maskMelody);
        writer.write(_maskRhythm);
        writer.write(_tiltRhythm);
        writer.write(_burst);
        writer.write(_feel);
        writer.write(uint8_t(0));   // reserved (was _minDegree)
        writer.write(uint8_t(0));   // reserved (was _maxDegree)
        writer.write(_rotate);
        writer.write(_complexity);
        writer.write(_contour);
        writer.write(_noteDuration);
        writer.write(_variation);
        writer.write(_rest);
        writer.write(_slide);
        writer.write(_burstRate);
        writer.write(_burstCount);
        writer.write(static_cast<uint8_t>(_burstHold));
        writer.write(_sleep);
        writer.write(_patienceRhythm);
        writer.write(_mutate);
        writer.write(_jump);
        writer.write(_range);
        writer.write(static_cast<uint8_t>(_rhythmMode));
        writer.write(static_cast<uint8_t>(_melodyMode));
        writer.write(_gateLength);
        for (int i = 0; i < 8; ++i) writer.write(_durationTickets[i]);
        writer.write(uint8_t(0));   // reserved (was StochasticLevel enum)
        writer.write(_tiltMelody);

        for (const auto &event : _steps) {
            for (int i = 0; i < 6; ++i) {
                writer.write(event.bytes[i]);
            }
        }
    }

    void read(VersionedSerializedReader &reader) {
        int8_t scaleField = 0;
        int8_t rootNoteField = 0;
        int8_t scaleRotateField = 0;
        reader.read(scaleField);
        reader.read(rootNoteField);
        reader.read(scaleRotateField);
        setScale(scaleField);
        setRootNote(rootNoteField);
        setScaleRotate(scaleRotateField);
        reader.read(_divisor);
        _divisor = ModelUtils::clampDivisor(_divisor);
        reader.read(_resetMeasure);
        _resetMeasure = clamp(int(_resetMeasure), 0, 128);
        reader.read(_clockMultiplier);

        reader.read(_size);
        reader.read(_first);
        { uint8_t reserved; reader.read(reserved); }   // was _last

        reader.read(_rhythmValid);
        reader.read(_melodyValid);
        reader.read(_rhythmSeed);
        reader.read(_melodySeed);

        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) reader.read(_degreeTickets[i]);
        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) _degreeTickets[i] = clamp(int(_degreeTickets[i]), -1, 100);
        reader.read(_repeatProb);
        _repeatProb = clamp(int(_repeatProb), 0, 100);
        reader.read(_degreeRotation);
        _degreeRotation = clamp(int(_degreeRotation), -32, 32);
        reader.read(_maskRotation);
        _maskRotation = clamp(int(_maskRotation), -32, 32);
        reader.read(_patienceMelody);
        _patienceMelody = clamp(int(_patienceMelody), 0, 100);
        reader.read(_legatoProb);
        _legatoProb = clamp(int(_legatoProb), 0, 100);
        uint8_t marblesMode;
        reader.read(marblesMode);
        _marblesMode = marblesMode < uint8_t(MarblesMode::Last) ? static_cast<MarblesMode>(marblesMode) : MarblesMode::Off;
        reader.read(_marblesSpread);
        _marblesSpread = clamp(int(_marblesSpread), 0, 100);
        reader.read(_marblesBias);
        _marblesBias = clamp(int(_marblesBias), 0, 100);
        reader.read(_maskMelody);
        _maskMelody = clamp(int(_maskMelody), 0, 100);
        reader.read(_maskRhythm);
        reader.read(_tiltRhythm);
        reader.read(_burst);
        reader.read(_feel);
        { uint8_t reserved; reader.read(reserved); }   // was _minDegree
        { uint8_t reserved; reader.read(reserved); }   // was _maxDegree
        reader.read(_rotate);
        reader.read(_complexity);
        reader.read(_contour);
        reader.read(_noteDuration);
        reader.read(_variation);
        reader.read(_rest);
        reader.read(_slide);
        reader.read(_burstRate);
        _burstRate = clamp(int(_burstRate), 0, 100);
        reader.read(_burstCount);
        _burstCount = clamp(int(_burstCount), 0, 100);
        uint8_t burstHold;
        reader.read(burstHold);
        _burstHold = burstHold < uint8_t(StochasticBurstHold::Last) ? static_cast<StochasticBurstHold>(burstHold) : StochasticBurstHold::HoldOver;
        reader.read(_sleep);
        reader.read(_patienceRhythm);
        reader.read(_mutate);
        reader.read(_jump);
        reader.read(_range);
        _range = clamp(int(_range), 0, 100);

        uint8_t rhythmMode, melodyMode;
        reader.read(rhythmMode);
        _rhythmMode = rhythmMode < uint8_t(StochasticSourceMode::Last) ? static_cast<StochasticSourceMode>(rhythmMode) : StochasticSourceMode::Loop;
        reader.read(melodyMode);
        _melodyMode = melodyMode < uint8_t(StochasticSourceMode::Last) ? static_cast<StochasticSourceMode>(melodyMode) : StochasticSourceMode::Loop;

        reader.read(_gateLength);
        for (int i = 0; i < 8; ++i) {
            reader.read(_durationTickets[i]);
            _durationTickets[i] = clamp(int(_durationTickets[i]), -1, 100);
        }
        { uint8_t reserved; reader.read(reserved); }   // was StochasticLevel enum

        reader.read(_tiltMelody);
        _tiltMelody = clamp(int(_tiltMelody), 0, 100);

        for (auto &event : _steps) {
            for (int i = 0; i < 6; ++i) {
                reader.read(event.bytes[i]);
            }
        }

        sanitizeAfterRead();
    }

    // ContentSnapshot — Tier 4 seed-based snapshot for the A/B preview /
    // Lock mechanic. Captures every content-shaping field EXCEPT _steps[],
    // since events are deterministic outputs of the generator over
    // (seeds × knobs × tickets) in Loop mode. Restore writes fields back;
    // caller is responsible for re-running generateRhythm / generateMelody
    // to repopulate _steps[]. Roughly ~92 bytes (vs. ~560 for full sequence).
    struct ContentSnapshot {
        uint32_t rhythmSeed = 0;
        uint32_t melodySeed = 0;
        int8_t   scale = -1;
        int8_t   rootNote = -1;
        uint8_t  divisor = 12;
        uint8_t  resetMeasure = 0;
        uint8_t  clockMultiplier = 100;
        uint8_t  size = 32;
        uint8_t  first = 0;
        bool     rhythmValid = false;
        bool     melodyValid = false;
        int8_t   degreeTickets[CONFIG_USER_SCALE_SIZE] = {};
        uint8_t  repeatProb = 0;
        int8_t   degreeRotation = 0;
        int8_t   maskRotation = 0;
        uint8_t  patienceMelody = 100;
        uint8_t  legatoProb = 0;
        uint8_t  marblesMode = uint8_t(MarblesMode::Off);
        uint8_t  marblesSpread = 50;
        uint8_t  marblesBias = 50;
        uint8_t  maskMelody = 100;
        uint8_t  tiltMelody = 0;
        uint8_t  maskRhythm = 100;
        uint8_t  gateLength = 0;
        uint8_t  tiltRhythm = 50;
        uint8_t  burst = 0;
        uint8_t  feel = 50;
        int8_t   rotate = 0;
        uint8_t  complexity = 50;
        int8_t   contour = 0;
        uint8_t  noteDuration = 5;
        int8_t   variation = 16;
        uint8_t  rest = 0;
        uint8_t  slide = 0;
        uint8_t  burstRate = 50;
        uint8_t  burstCount = 0;
        uint8_t  burstHold = uint8_t(StochasticBurstHold::HoldOver);
        uint8_t  sleep = 0;
        uint8_t  patienceRhythm = 100;
        int8_t   mutate = 0;
        uint8_t  jump = 0;
        uint8_t  range = 50;
        uint8_t  rhythmMode = uint8_t(StochasticSourceMode::Live);
        uint8_t  melodyMode = uint8_t(StochasticSourceMode::Live);
        int8_t   durationTickets[8] = {};
    };

    void captureContentTo(ContentSnapshot &snap) const {
        snap.rhythmSeed = _rhythmSeed;
        snap.melodySeed = _melodySeed;
        snap.scale = rawScale();
        snap.rootNote = rawRootNote();
        snap.divisor = _divisor;
        snap.resetMeasure = _resetMeasure;
        snap.clockMultiplier = _clockMultiplier;
        snap.size = _size;
        snap.first = _first;
        snap.rhythmValid = _rhythmValid;
        snap.melodyValid = _melodyValid;
        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) snap.degreeTickets[i] = _degreeTickets[i];
        snap.repeatProb = _repeatProb;
        snap.degreeRotation = _degreeRotation;
        snap.maskRotation = _maskRotation;
        snap.patienceMelody = _patienceMelody;
        snap.legatoProb = _legatoProb;
        snap.marblesMode = uint8_t(_marblesMode);
        snap.marblesSpread = _marblesSpread;
        snap.marblesBias = _marblesBias;
        snap.maskMelody = _maskMelody;
        snap.tiltMelody = _tiltMelody;
        snap.maskRhythm = _maskRhythm;
        snap.gateLength = _gateLength;
        snap.tiltRhythm = _tiltRhythm;
        snap.burst = _burst;
        snap.feel = _feel;
        snap.rotate = _rotate;
        snap.complexity = _complexity;
        snap.contour = _contour;
        snap.noteDuration = _noteDuration;
        snap.variation = _variation;
        snap.rest = _rest;
        snap.slide = _slide;
        snap.burstRate = _burstRate;
        snap.burstCount = _burstCount;
        snap.burstHold = uint8_t(_burstHold);
        snap.sleep = _sleep;
        snap.patienceRhythm = _patienceRhythm;
        snap.mutate = _mutate;
        snap.jump = _jump;
        snap.range = _range;
        snap.rhythmMode = uint8_t(_rhythmMode);
        snap.melodyMode = uint8_t(_melodyMode);
        for (int i = 0; i < 8; ++i) snap.durationTickets[i] = _durationTickets[i];
    }

    void restoreContentFrom(const ContentSnapshot &snap) {
        _rhythmSeed = snap.rhythmSeed;
        _melodySeed = snap.melodySeed;
        setScale(snap.scale);
        setRootNote(snap.rootNote);
        _divisor = snap.divisor;
        _resetMeasure = snap.resetMeasure;
        _clockMultiplier = snap.clockMultiplier;
        _size = snap.size;
        _first = snap.first;
        _rhythmValid = snap.rhythmValid;
        _melodyValid = snap.melodyValid;
        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) _degreeTickets[i] = snap.degreeTickets[i];
        _repeatProb = snap.repeatProb;
        _degreeRotation = snap.degreeRotation;
        _maskRotation = snap.maskRotation;
        _patienceMelody = snap.patienceMelody;
        _legatoProb = snap.legatoProb;
        _marblesMode = MarblesMode(snap.marblesMode < uint8_t(MarblesMode::Last) ? snap.marblesMode : 0);
        _marblesSpread = snap.marblesSpread;
        _marblesBias = snap.marblesBias;
        _maskMelody = snap.maskMelody;
        _tiltMelody = snap.tiltMelody;
        _maskRhythm = snap.maskRhythm;
        _gateLength = snap.gateLength;
        _tiltRhythm = snap.tiltRhythm;
        _burst = snap.burst;
        _feel = snap.feel;
        _rotate = snap.rotate;
        _complexity = snap.complexity;
        _contour = snap.contour;
        _noteDuration = snap.noteDuration;
        _variation = snap.variation;
        _rest = snap.rest;
        _slide = snap.slide;
        _burstRate = snap.burstRate;
        _burstCount = snap.burstCount;
        _burstHold = StochasticBurstHold(snap.burstHold < uint8_t(StochasticBurstHold::Last) ? snap.burstHold : 0);
        _sleep = snap.sleep;
        _patienceRhythm = snap.patienceRhythm;
        _mutate = snap.mutate;
        _jump = snap.jump;
        _range = snap.range;
        _rhythmMode = StochasticSourceMode(snap.rhythmMode < uint8_t(StochasticSourceMode::Last) ? snap.rhythmMode : 0);
        _melodyMode = StochasticSourceMode(snap.melodyMode < uint8_t(StochasticSourceMode::Last) ? snap.melodyMode : 0);
        for (int i = 0; i < 8; ++i) _durationTickets[i] = snap.durationTickets[i];
        // _steps[] intentionally not touched — caller regenerates via
        // generateRhythm / generateMelody to repopulate the deterministic
        // events from the restored seeds + knobs.
    }

private:
    // Shared defaults for clear() and the invalid-size recovery path in
    // sanitizeAfterRead(). Touches all model fields except _steps (callers
    // decide whether to wipe events too).
    void setDefaults() {
        _scaleGroup.raw = 0;
        setScale(-1);
        setRootNote(-1);
        setScaleRotate(-1);
        _divisor = 12;
        _resetMeasure = 0;
        _clockMultiplier = 100;

        _size = 32;
        _first = 0;

        _rhythmValid = false;
        _melodyValid = false;
        _rhythmSeed = 0;
        _melodySeed = 0;

        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) {
            _degreeTickets[i] = 0;
        }
        _repeatProb = 0;
        _degreeRotation = 0;
        _maskRotation = 0;
        _patienceMelody = 100;
        _legatoProb = 0;
        _marblesMode = MarblesMode::Off;
        _marblesSpread = 50;
        _marblesBias = 50;
        _maskMelody = 100;
        _tiltMelody = 0;
        _maskRhythm = 100;
        _gateLength = 0;
        _tiltRhythm = 50;
        _burst = 0;
        _feel = 50;
        _rotate = 0;
        _complexity = 50;
        _contour = 0;
        _noteDuration = 5;   // LUT entry 5 = ×1 (= divisor)
        _variation = 16;     // audible variation at fresh roll
        _rest = 0;
        _slide = 0;
        _burstRate = 50;
        _burstCount = 0;
        _burstHold = StochasticBurstHold::HoldOver;   // default matches today's "uniform burst, shared pitch"
        _sleep = 0;
        _patienceRhythm = 100;
        _mutate = 0;
        _jump = 0;
        _range = 50;
        _rhythmMode = StochasticSourceMode::Live;
        _melodyMode = StochasticSourceMode::Live;

        for (int i = 0; i < 8; ++i) _durationTickets[i] = 0;
    }

    void sanitizeAfterRead() {
        setScale(rawScale());
        setRootNote(rawRootNote());
        _divisor = ModelUtils::clampDivisor(_divisor);
        _resetMeasure = clamp(int(_resetMeasure), 0, 128);

        if (_size < 2 || _size > CONFIG_STEP_COUNT) {
            setDefaults();
            for (auto &event : _steps) event.clear();
            return;
        }

        _first = clamp(int(_first), 0, int(_size) - 1);
        _rhythmValid = _rhythmValid ? true : false;
        _melodyValid = _melodyValid ? true : false;

        for (int i = _size; i < CONFIG_STEP_COUNT; ++i) {
            _steps[i].clear();
        }
    }

    int8_t _trackIndex = -1;
    union {
        uint16_t raw;
        BitField<uint16_t, 0, 5> scale;       // scale + 1   (-1..23 -> 0..24)
        BitField<uint16_t, 5, 4> rootNote;    // rootNote + 1 (-1..11 -> 0..12)
        BitField<uint16_t, 9, 6> scaleRotate; // scaleRotate + 1 (-1..31 -> 0..32)
    } _scaleGroup = { 0 };
    uint8_t _divisor = 12;
    uint8_t _resetMeasure = 0;
    uint8_t _clockMultiplier;

    uint8_t _size;
    uint8_t _first;
    // Last collapsed into Size — last() returns size-1. Wire format reserves
    // one byte for backward read compat; field dropped from RAM.

    // Split rhythm + melody storage
    std::array<StochasticStepContent, CONFIG_STEP_COUNT> _steps;
    bool _rhythmValid;
    bool _melodyValid;
    uint32_t _rhythmSeed;
    uint32_t _melodySeed;

    int8_t _degreeTickets[CONFIG_USER_SCALE_SIZE];
    uint8_t _repeatProb;
    int8_t _degreeRotation;
    int8_t _maskRotation;
    uint8_t _patienceMelody;
    uint8_t _legatoProb;
    MarblesMode _marblesMode;
    uint8_t _marblesSpread;
    uint8_t _marblesBias;
    uint8_t _maskMelody;
    uint8_t _tiltMelody;

    uint8_t _maskRhythm;
    uint8_t _gateLength;
    uint8_t _tiltRhythm;
    uint8_t _burst;
    uint8_t _feel;

    int8_t _rotate;
    uint8_t _complexity;
    int8_t _contour;
    uint8_t _noteDuration;
    int8_t _variation;
    uint8_t _rest;
    uint8_t _slide;
    uint8_t _burstRate;
    uint8_t _burstCount;
    StochasticBurstHold _burstHold;
    uint8_t _sleep;
    uint8_t _patienceRhythm;
    uint8_t _mutate;
    uint8_t _jump;
    uint8_t _range;

    int8_t _durationTickets[8];

    StochasticSourceMode _rhythmMode;
    StochasticSourceMode _melodyMode;


    friend class StochasticTrack;
};

static_assert(sizeof(StochasticSequence) <= 560, "StochasticSequence too large");
