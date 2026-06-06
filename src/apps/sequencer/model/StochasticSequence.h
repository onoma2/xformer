#pragma once

#include "Config.h"
#include "Bitfield.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Types.h"
#include "Scale.h"

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
    int scale() const { return Routing::routedValueInt(ParamKey::Scale, _trackIndex, _scale, 0, 23); }
    void setScale(int scale) { _scale = clamp(scale, -1, Scale::Count - 1); }
    const Scale &selectedScale(int defaultScale) const {
        int s = scale();
        return Scale::get(s != -1 ? s : defaultScale);
    }

    // rootNote
    int rootNote() const { return Routing::routedValueInt(ParamKey::RootNote, _trackIndex, _rootNote, 0, 11); }
    void setRootNote(int rootNote) { _rootNote = clamp(rootNote, -1, 11); }
    int selectedRootNote(int defaultRootNote) const {
        int r = rootNote();
        return r != -1 ? r : defaultRootNote;
    }

    // divisor
    int divisor() const { return _divisor; }
    void setDivisor(int divisor) { _divisor = ModelUtils::clampDivisor(divisor); }

    // clockMultiplier
    int clockMultiplier() const { return Routing::routedValueInt(ParamKey::ClockMultiplier, _trackIndex, _clockMultiplier.base, 50, 150); }
    void setClockMultiplier(int clockMultiplier, bool routed = false) {
        _clockMultiplier.set(clamp(clockMultiplier, 50, 150), routed);
    }
    void editClockMultiplier(int value, bool shift) {
        setClockMultiplier(_clockMultiplier.base + value * (shift ? 10 : 1));
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
        setScale(_scale + value);
    }


    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    inline void printRouted(StringBuilder &str, Routing::Target target) const { Routing::printRouted(str, target, _trackIndex); }

    void writeRouted(Routing::Target target, int intValue, float floatValue) {
        switch (target) {
        case Routing::Target::StochasticMask: setMaskRhythm(intValue, true); break;
        case Routing::Target::StochasticGateLength: setGateLength(intValue, true); break;
        case Routing::Target::StochasticTilt: setTiltRhythm(intValue, true); break;
        case Routing::Target::StochasticBurst: setBurst(intValue, true); break;
        case Routing::Target::StochasticComplexity: setComplexity(intValue, true); break;
        case Routing::Target::StochasticContour: setContour(intValue, true); break;
        case Routing::Target::StochasticNoteDuration: setNoteDuration(intValue, true); break;
        case Routing::Target::StochasticVariation: setVariation(intValue, true); break;
        case Routing::Target::StochasticRest: setRest(intValue, true); break;
        case Routing::Target::StochasticSlide: setSlide(intValue, true); break;
        case Routing::Target::StochasticSleep: setSleep(intValue, true); break;
        case Routing::Target::StochasticPatienceRhythm: setPatienceRhythm(intValue, true); break;
        case Routing::Target::StochasticMutate: setMutate(intValue, true); break;
        case Routing::Target::StochasticJump: setJump(intValue, true); break;
        case Routing::Target::StochasticFeel: setFeel(intValue, true); break;
        case Routing::Target::Rotate: setRotate(intValue, true); break;
        case Routing::Target::ClockMult: setClockMultiplier(intValue, true); break;
        default: break;
        }
    }

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

    int complexity() const { return Routing::routedValueInt(ParamKey::Complexity, _trackIndex, _complexity.base, 0, 100); }
    int complexityBase() const { return _complexity.base; }
    void setComplexity(int complexity, bool routed = false) { _complexity.set(clamp(complexity, 0, 100), routed); }

    int contour() const { return Routing::routedValueInt(ParamKey::Contour, _trackIndex, _contour.base, -100, 100); }
    int contourBase() const { return _contour.base; }
    void setContour(int contour, bool routed = false) { _contour.set(clamp(contour, -100, 100), routed); }

    // Phase 12: _repeatProb field repurposed as Repeat probability. Per-event
    // chance to reuse the previously-generated event verbatim (note, octave,
    // duration, articulation). 100% freezes on the last event. Mid values
    // cluster durations like a human player.
    int repeatProb() const { return _repeatProb; }
    void setRepeatProb(int value) { _repeatProb = clamp(value, 0, 100); }

    // Live-mode duration LUT entry index 0..7. Divisor-relative: see
    // StochasticTrackEngine::getDurationFraction() for the multiplier table.
    int noteDuration() const { return Routing::routedValueInt(ParamKey::NoteDuration, _trackIndex, _noteDuration.base, 0, 7); }
    int noteDurationBase() const { return _noteDuration.base; }
    void setNoteDuration(int v, bool routed = false) { _noteDuration.set(clamp(v, 0, 7), routed); }

    // Phase 11: storage may be signed (legacy patches); read always returns |v|.
    int variation() const { int b = _variation.base; return Routing::routedValueInt(ParamKey::Variation, _trackIndex, b < 0 ? -b : b, 0, 100); }
    int variationBase() const { int b = _variation.base; return b < 0 ? -b : b; }
    // Phase 11: variation is symmetric (sign meaningless). New writes clamp to
    // [0, 100]. Legacy patches with negative storage get abs()'d at getter.
    void setVariation(int variation, bool routed = false) { _variation.set(clamp(variation, 0, 100), routed); }

    int rest() const { return Routing::routedValueInt(ParamKey::Rest, _trackIndex, _rest.base, 0, 100); }
    int restBase() const { return _rest.base; }
    void setRest(int rest, bool routed = false) { _rest.set(clamp(rest, 0, 100), routed); }

    int slide() const { return Routing::routedValueInt(ParamKey::Slide, _trackIndex, _slide.base, 0, 100); }
    int slideBase() const { return _slide.base; }
    void setSlide(int slide, bool routed = false) { _slide.set(clamp(slide, 0, 100), routed); }

    int burstRate() const { return _burstRate; }
    void setBurstRate(int rate) { _burstRate = clamp(rate, 0, 100); }

    int burstCount() const { return _burstCount; }
    void setBurstCount(int count) { _burstCount = clamp(count, 0, 100); }

    StochasticBurstHold burstHold() const { return _burstHold; }
    void setBurstHold(StochasticBurstHold pitch) { _burstHold = ModelUtils::clampedEnum(pitch); }

    int sleep() const { return Routing::routedValueInt(ParamKey::Sleep, _trackIndex, _sleep.base, 0, 100); }
    int sleepBase() const { return _sleep.base; }
    void setSleep(int sleep, bool routed = false) { _sleep.set(clamp(sleep, 0, 100), routed); }


    // mutate — probability 0..100 that one randomly-picked step inside the
    // active window gets destructively re-rolled at each cycle-end. Unipolar.
    int mutate() const { return Routing::routedValueInt(ParamKey::Mutate, _trackIndex, _mutate.base, 0, 100); }
    int mutateBase() const { return _mutate.base; }
    // Bipolar: -100..0 = Proteus destructive (regenerate one event), 0 = lock,
    // 0..+100 = Marbles permutation (swap two existing events). Magnitude is
    // probability per loop iteration.
    void setMutate(int mutate, bool routed = false) { _mutate.set(clamp(mutate, 0, 100), routed); }

    int jump() const { return Routing::routedValueInt(ParamKey::Jump, _trackIndex, _jump.base, 0, 100); }
    int jumpBase() const { return _jump.base; }
    void setJump(int jump, bool routed = false) { _jump.set(clamp(jump, 0, 100), routed); }

    // Range — bipolar field width / jump-chance knob, 0..100 centered at 50.
    // 50 = single-octave field (no decision in Steps 3..5). >50 expands the
    // candidate set up to 4 octaves. <50 keeps single octave plus a per-slot
    // octave-displacement chance. See PITCH-LAW-FINAL.md Step 3.
    int range() const { return _range; }
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
    int patienceRhythm() const { return Routing::routedValueInt(ParamKey::PatienceRhythm, _trackIndex, _patienceRhythm.base, 0, 100); }
    int patienceRhythmBase() const { return _patienceRhythm.base; }
    void setPatienceRhythm(int value, bool routed = false) { _patienceRhythm.set(clamp(value, 0, 100), routed); }

    // degreeRotation
    int legatoProb() const { return _legatoProb; }
    void setLegatoProb(int prob) { _legatoProb = clamp(prob, 0, 100); }

    // marblesMode
    MarblesMode marblesMode() const { return _marblesMode; }
    void setMarblesMode(MarblesMode mode) { _marblesMode = ModelUtils::clampedEnum(mode); }

    // marblesSpread
    int marblesSpread() const { return _marblesSpread; }
    void setMarblesSpread(int spread) { _marblesSpread = clamp(spread, 0, 100); }

    // marblesBias
    int marblesBias() const { return _marblesBias; }
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
    int maskRhythm() const { return Routing::routedValueInt(ParamKey::MaskRhythm, _trackIndex, _maskRhythm.base, 0, 100); }
    int maskRhythmBase() const { return _maskRhythm.base; }
    void setMaskRhythm(int value, bool routed = false) { _maskRhythm.set(clamp(value, 0, 100), routed); }

    // gateLength — repurposed from the Phase 11 `_gateLength` reserved field. Knob
    // 0..100 controls the spread of per-event gate length around a hardcoded
    // 50% center. 0 = exact 50% every event; 100 = triangular distribution
    // 10..100% peaked at 50%. Floored at 10% of duration AND at an absolute
    // audible-tick minimum so very short events still produce a real trigger.
    // Routing target keeps the legacy `StochasticGeneratorDensity` enum name
    // for save-file compatibility; UI label changes to "Gate Length".
    int gateLength() const { return Routing::routedValueInt(ParamKey::StochasticGateLength, _trackIndex, _gateLength.base, 0, 100); }
    int gateLengthBase() const { return _gateLength.base; }
    void setGateLength(int value, bool routed = false) { _gateLength.set(clamp(value, 0, 100), routed); }

    // tiltRhythm — paired with maskRhythm. 0..100, center 50 = pure salt cut,
    // 0 = max negative rank-cut, 100 = max positive rank-cut. Engine recovers
    // signed magnitude as (knob - 50) and uses |signed| * 2 as the rank weight.
    int tiltRhythm() const { return Routing::routedValueInt(ParamKey::TiltRhythm, _trackIndex, _tiltRhythm.base, 0, 100); }
    int tiltRhythmBase() const { return _tiltRhythm.base; }
    void setTiltRhythm(int value, bool routed = false) { _tiltRhythm.set(clamp(value, 0, 100), routed); }

    // burs
    int burst() const { return Routing::routedValueInt(ParamKey::Burst, _trackIndex, _burst.base, 0, 100); }
    int burstBase() const { return _burst.base; }
    void setBurst(int burst, bool routed = false) { _burst.set(clamp(burst, 0, 100), routed); }

    // feel (Phase 16 P4, 2026-05-23) — knob 0..100, default 50. Detent
    // [45..55] = off (no scaling). knob < 45 lerps toward 3 beats per
    // cycle, knob > 55 lerps toward 5 beats per cycle. Generator reads
    // this in the post-walk scaling pass; inert until P5 wires it.
    int feel() const { return Routing::routedValueInt(ParamKey::Feel, _trackIndex, _feel.base, 0, 100); }
    int feelBase() const { return _feel.base; }
    void setFeel(int feel, bool routed = false) { _feel.set(clamp(feel, 0, 100), routed); }

    // rotate
    int rotate() const { return Routing::routedValueInt(ParamKey::Rotate, _trackIndex, _rotate.base, -64, 64); }
    int rotateBase() const { return _rotate.base; }
    void setRotate(int rotate, bool routed = false) { _rotate.set(clamp(rotate, -64, 64), routed); }

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
    void editMaskRhythm(int value, bool shift) { setMaskRhythm(_maskRhythm.base + value); }

    // Level 1 Density macro: writes density only (no fan-out to rest)
    void printGateLength(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticGateLength); str("%d%%", gateLength()); }
    void editGateLength(int value, bool shift) {
        setGateLength(_gateLength.base + value);
    }

    void editComplexityMacro(int value, bool shift) {
        int c = clamp(_complexity.base + value, 0, 100);
        setComplexity(c);
    }

    void printTiltRhythm(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticTilt); str("%d%%", tiltRhythm()); }
    void editTiltRhythm(int value, bool shift) { setTiltRhythm(_tiltRhythm.base + value); }

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
    void editBurst(int value, bool shift) { setBurst(_burst.base + value); }

    void printPatienceMelody(StringBuilder &str) const { str("%d%%", patienceMelody()); }
    void editPatienceMelody(int value, bool shift) { setPatienceMelody(patienceMelody() + value); }

    void printPatienceRhythm(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticPatienceRhythm); str("%d%%", patienceRhythm()); }
    void editPatienceRhythm(int value, bool shift) { setPatienceRhythm(_patienceRhythm.base + value); }

    void printLegatoProb(StringBuilder &str) const { str("%d%%", legatoProb()); }
    void editLegatoProb(int value, bool shift) { setLegatoProb(ModelUtils::adjustedByStep(legatoProb(), value, 1, !shift)); }

    void printComplexity(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticComplexity); str("%d%%", complexity()); }
    void editComplexity(int value, bool shift) { setComplexity(_complexity.base + value); }

    void printContour(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticContour); str("%+d%%", contour()); }
    void editContour(int value, bool shift) { setContour(_contour.base + value); }

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
    void editNoteDuration(int value, bool shift) { setNoteDuration(_noteDuration.base + value); }

    void printVariation(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticVariation); str("%d%%", variation()); }
    void editVariation(int value, bool shift) { setVariation(_variation.base + value); }

    void printRest(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticRest); str("%d%%", rest()); }
    void editRest(int value, bool shift) { setRest(_rest.base + value); }

    void printSlide(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticSlide); str("%d%%", slide()); }
    void editSlide(int value, bool shift) { setSlide(_slide.base + value); }

    void printSleep(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticSleep); str("%d%%", sleep()); }
    void editSleep(int value, bool shift) { setSleep(_sleep.base + value); }


    void printMutate(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticMutate); str("%d%%", mutate()); }
    void editMutate(int value, bool shift) { setMutate(_mutate.base + value); }

    void printJump(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticJump); str("%d%%", jump()); }
    void editJump(int value, bool shift) { setJump(_jump.base + value); }

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
    void editRotate(int value, bool shift) { setRotate(_rotate.base + value); }

    void printRootNote(StringBuilder &str) const {
        if (rootNote() == -1) {
            str("Default");
        } else {
            Types::printNote(str, rootNote());
        }
    }

    void editRootNote(int value, bool shift) {
        setRootNote(_rootNote + value);
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

    void printNote(StringBuilder &str, int note, int defaultRootNote, int defaultScale) const {
        int rootNote = selectedRootNote(defaultRootNote);
        const auto &scale = selectedScale(defaultScale);
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
        writer.write(_scale);
        writer.write(_rootNote);
        writer.write(_divisor);
        writer.write(_resetMeasure);
        _clockMultiplier.write(writer);

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
        _maskRhythm.write(writer);
        _tiltRhythm.write(writer);
        _burst.write(writer);
        _feel.write(writer);
        writer.write(uint8_t(0));   // reserved (was _minDegree)
        writer.write(uint8_t(0));   // reserved (was _maxDegree)
        _rotate.write(writer);
        _complexity.write(writer);
        _contour.write(writer);
        _noteDuration.write(writer);
        _variation.write(writer);
        _rest.write(writer);
        _slide.write(writer);
        writer.write(_burstRate);
        writer.write(_burstCount);
        writer.write(static_cast<uint8_t>(_burstHold));
        _sleep.write(writer);
        _patienceRhythm.write(writer);
        _mutate.write(writer);
        _jump.write(writer);
        writer.write(_range);
        writer.write(static_cast<uint8_t>(_rhythmMode));
        writer.write(static_cast<uint8_t>(_melodyMode));
        _gateLength.write(writer);
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
        reader.read(_scale);
        reader.read(_rootNote);
        reader.read(_divisor);
        _divisor = ModelUtils::clampDivisor(_divisor);
        reader.read(_resetMeasure);
        _resetMeasure = clamp(int(_resetMeasure), 0, 128);
        _clockMultiplier.read(reader);

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
        _maskRhythm.read(reader);
        _tiltRhythm.read(reader);
        _burst.read(reader);
        _feel.read(reader);
        { uint8_t reserved; reader.read(reserved); }   // was _minDegree
        { uint8_t reserved; reader.read(reserved); }   // was _maxDegree
        _rotate.read(reader);
        _complexity.read(reader);
        _contour.read(reader);
        _noteDuration.read(reader);
        _variation.read(reader);
        _rest.read(reader);
        _slide.read(reader);
        reader.read(_burstRate);
        _burstRate = clamp(int(_burstRate), 0, 100);
        reader.read(_burstCount);
        _burstCount = clamp(int(_burstCount), 0, 100);
        uint8_t burstHold;
        reader.read(burstHold);
        _burstHold = burstHold < uint8_t(StochasticBurstHold::Last) ? static_cast<StochasticBurstHold>(burstHold) : StochasticBurstHold::HoldOver;
        _sleep.read(reader);
        _patienceRhythm.read(reader);
        _mutate.read(reader);
        _jump.read(reader);
        reader.read(_range);
        _range = clamp(int(_range), 0, 100);

        uint8_t rhythmMode, melodyMode;
        reader.read(rhythmMode);
        _rhythmMode = rhythmMode < uint8_t(StochasticSourceMode::Last) ? static_cast<StochasticSourceMode>(rhythmMode) : StochasticSourceMode::Loop;
        reader.read(melodyMode);
        _melodyMode = melodyMode < uint8_t(StochasticSourceMode::Last) ? static_cast<StochasticSourceMode>(melodyMode) : StochasticSourceMode::Loop;

        _gateLength.read(reader);
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
        snap.scale = _scale;
        snap.rootNote = _rootNote;
        snap.divisor = _divisor;
        snap.resetMeasure = _resetMeasure;
        snap.clockMultiplier = _clockMultiplier.base;
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
        snap.maskRhythm = _maskRhythm.base;
        snap.gateLength = _gateLength.base;
        snap.tiltRhythm = _tiltRhythm.base;
        snap.burst = _burst.base;
        snap.feel = _feel.base;
        snap.rotate = _rotate.base;
        snap.complexity = _complexity.base;
        snap.contour = _contour.base;
        snap.noteDuration = _noteDuration.base;
        snap.variation = _variation.base;
        snap.rest = _rest.base;
        snap.slide = _slide.base;
        snap.burstRate = _burstRate;
        snap.burstCount = _burstCount;
        snap.burstHold = uint8_t(_burstHold);
        snap.sleep = _sleep.base;
        snap.patienceRhythm = _patienceRhythm.base;
        snap.mutate = _mutate.base;
        snap.jump = _jump.base;
        snap.range = _range;
        snap.rhythmMode = uint8_t(_rhythmMode);
        snap.melodyMode = uint8_t(_melodyMode);
        for (int i = 0; i < 8; ++i) snap.durationTickets[i] = _durationTickets[i];
    }

    void restoreContentFrom(const ContentSnapshot &snap) {
        _rhythmSeed = snap.rhythmSeed;
        _melodySeed = snap.melodySeed;
        _scale = snap.scale;
        _rootNote = snap.rootNote;
        _divisor = snap.divisor;
        _resetMeasure = snap.resetMeasure;
        _clockMultiplier.setBase(snap.clockMultiplier);
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
        _maskRhythm.setBase(snap.maskRhythm);
        _gateLength.setBase(snap.gateLength);
        _tiltRhythm.setBase(snap.tiltRhythm);
        _burst.setBase(snap.burst);
        _feel.setBase(snap.feel);
        _rotate.setBase(snap.rotate);
        _complexity.setBase(snap.complexity);
        _contour.setBase(snap.contour);
        _noteDuration.setBase(snap.noteDuration);
        _variation.setBase(snap.variation);
        _rest.setBase(snap.rest);
        _slide.setBase(snap.slide);
        _burstRate = snap.burstRate;
        _burstCount = snap.burstCount;
        _burstHold = StochasticBurstHold(snap.burstHold < uint8_t(StochasticBurstHold::Last) ? snap.burstHold : 0);
        _sleep.setBase(snap.sleep);
        _patienceRhythm.setBase(snap.patienceRhythm);
        _mutate.setBase(snap.mutate);
        _jump.setBase(snap.jump);
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
        _scale = -1;
        _rootNote = -1;
        _divisor = 12;
        _resetMeasure = 0;
        _clockMultiplier.setBase(100);

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
        _maskRhythm.setBase(100);
        _gateLength.setBase(0);
        _tiltRhythm.setBase(50);
        _burst.setBase(0);
        _feel.setBase(50);
        _rotate.setBase(0);
        _complexity.setBase(50);
        _contour.setBase(0);
        _noteDuration.setBase(5);   // LUT entry 5 = ×1 (= divisor)
        _variation.setBase(16);     // audible variation at fresh roll
        _rest.setBase(0);
        _slide.setBase(0);
        _burstRate = 50;
        _burstCount = 0;
        _burstHold = StochasticBurstHold::HoldOver;   // default matches today's "uniform burst, shared pitch"
        _sleep.setBase(0);
        _patienceRhythm.setBase(100);
        _mutate.setBase(0);
        _jump.setBase(0);
        _range = 50;
        _rhythmMode = StochasticSourceMode::Live;
        _melodyMode = StochasticSourceMode::Live;

        for (int i = 0; i < 8; ++i) _durationTickets[i] = 0;
    }

    void sanitizeAfterRead() {
        _scale = clamp(int(_scale), -1, Scale::Count - 1);
        _rootNote = clamp(int(_rootNote), -1, 11);
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
    int8_t _scale = -1;
    int8_t _rootNote = -1;
    uint8_t _divisor = 12;
    uint8_t _resetMeasure = 0;
    Routable<uint8_t> _clockMultiplier;

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

    Routable<uint8_t> _maskRhythm;
    Routable<uint8_t> _gateLength;
    Routable<uint8_t> _tiltRhythm;
    Routable<uint8_t> _burst;
    Routable<uint8_t> _feel;

    Routable<int8_t> _rotate;
    Routable<uint8_t> _complexity;
    Routable<int8_t> _contour;
    Routable<uint8_t> _noteDuration;
    Routable<int8_t> _variation;
    Routable<uint8_t> _rest;
    Routable<uint8_t> _slide;
    uint8_t _burstRate;
    uint8_t _burstCount;
    StochasticBurstHold _burstHold;
    Routable<uint8_t> _sleep;
    Routable<uint8_t> _patienceRhythm;
    Routable<uint8_t> _mutate;
    Routable<uint8_t> _jump;
    uint8_t _range;

    int8_t _durationTickets[8];

    StochasticSourceMode _rhythmMode;
    StochasticSourceMode _melodyMode;


    friend class StochasticTrack;
};

static_assert(sizeof(StochasticSequence) <= 560, "StochasticSequence too large");
