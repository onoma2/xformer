#pragma once

#include "Config.h"
#include "Bitfield.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Types.h"
#include "Scale.h"

#include "core/math/Math.h"

#include "Routing.h"
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
    int scale() const { return _scale; }
    void setScale(int scale) { _scale = clamp(scale, -1, Scale::Count - 1); }
    const Scale &selectedScale(int defaultScale) const {
        return Scale::get(_scale != -1 ? _scale : defaultScale);
    }

    // rootNote
    int rootNote() const { return _rootNote; }
    void setRootNote(int rootNote) { _rootNote = clamp(rootNote, -1, 11); }
    int selectedRootNote(int defaultRootNote) const {
        return _rootNote != -1 ? _rootNote : defaultRootNote;
    }

    // divisor
    int divisor() const { return _divisor; }
    void setDivisor(int divisor) { _divisor = ModelUtils::clampDivisor(divisor); }

    // clockMultiplier
    int clockMultiplier() const { return _clockMultiplier.get(isRouted(Routing::Target::ClockMult)); }
    void setClockMultiplier(int clockMultiplier, bool routed = false) {
        _clockMultiplier.set(clamp(clockMultiplier, 50, 150), routed);
    }
    void editClockMultiplier(int value, bool shift) {
        if (!isRouted(Routing::Target::ClockMult)) {
            setClockMultiplier(clockMultiplier() + value * (shift ? 10 : 1));
        }
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
        setScale(scale() + value);
    }


    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    inline void printRouted(StringBuilder &str, Routing::Target target) const { Routing::printRouted(str, target, _trackIndex); }

    void writeRouted(Routing::Target target, int intValue, float floatValue) {
        switch (target) {
        case Routing::Target::StochasticMask: setMask(intValue, true); break;
        case Routing::Target::StochasticGateLength: setGateLength(intValue, true); break;
        case Routing::Target::StochasticTilt: setTilt(intValue, true); break;
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

    int complexity() const { return _complexity.get(isRouted(Routing::Target::StochasticComplexity)); }
    void setComplexity(int complexity, bool routed = false) { _complexity.set(clamp(complexity, 0, 100), routed); }

    int contour() const { return _contour.get(isRouted(Routing::Target::StochasticContour)); }
    void setContour(int contour, bool routed = false) { _contour.set(clamp(contour, -100, 100), routed); }

    // Phase 12: _repeatProb slot repurposed as Repeat probability. Per-event
    // chance to reuse the previously-generated event verbatim (note, octave,
    // duration, articulation). 100% freezes on the last event. Mid values
    // cluster durations like a human player.
    int repeatProb() const { return _repeatProb; }
    void setRepeatProb(int value) { _repeatProb = clamp(value, 0, 100); }

    // Live-mode duration LUT slot index 0..7. Divisor-relative: see
    // StochasticTrackEngine::getDurationFraction() for the multiplier table.
    int noteDuration() const { return _noteDuration.get(isRouted(Routing::Target::StochasticNoteDuration)); }
    void setNoteDuration(int v, bool routed = false) { _noteDuration.set(clamp(v, 0, 7), routed); }

    // Phase 11: storage may be signed (legacy patches); read always returns |v|.
    int variation() const { int v = _variation.get(isRouted(Routing::Target::StochasticVariation)); return v < 0 ? -v : v; }
    // Phase 11: variation is symmetric (sign meaningless). New writes clamp to
    // [0, 100]. Legacy patches with negative storage get abs()'d at getter.
    void setVariation(int variation, bool routed = false) { _variation.set(clamp(variation, 0, 100), routed); }

    int rest() const { return _rest.get(isRouted(Routing::Target::StochasticRest)); }
    void setRest(int rest, bool routed = false) { _rest.set(clamp(rest, 0, 100), routed); }

    int slide() const { return _slide.get(isRouted(Routing::Target::StochasticSlide)); }
    void setSlide(int slide, bool routed = false) { _slide.set(clamp(slide, 0, 100), routed); }

    int burstRate() const { return _burstRate; }
    void setBurstRate(int rate) { _burstRate = clamp(rate, 0, 100); }

    int burstCount() const { return _burstCount; }
    void setBurstCount(int count) { _burstCount = clamp(count, 0, 100); }

    StochasticBurstPitch burstPitch() const { return _burstPitch; }
    void setBurstPitch(StochasticBurstPitch pitch) { _burstPitch = ModelUtils::clampedEnum(pitch); }

    int sleep() const { return _sleep.get(isRouted(Routing::Target::StochasticSleep)); }
    void setSleep(int sleep, bool routed = false) { _sleep.set(clamp(sleep, 0, 100), routed); }


    int mutate() const { return _mutate.get(isRouted(Routing::Target::StochasticMutate)); }
    // Bipolar: -100..0 = Proteus destructive (regenerate one event), 0 = lock,
    // 0..+100 = Marbles permutation (swap two existing events). Magnitude is
    // probability per loop iteration.
    void setMutate(int mutate, bool routed = false) { _mutate.set(clamp(mutate, -100, 100), routed); }

    int jump() const { return _jump.get(isRouted(Routing::Target::StochasticJump)); }
    void setJump(int jump, bool routed = false) { _jump.set(clamp(jump, 0, 100), routed); }

    int range() const { return _range; }
    void setRange(int range) { _range = clamp(range, 1, 4); }

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
    // Phase 12: _patienceMelody slot repurposed as melody patience (independent of
    // rhythm patience). Same Poisson-CDF behavior. Knob 0..99 → λ ∈ [1, 50];
    // knob 100 = off sentinel. No routing target (matches Repeat treatment).
    int patienceMelody() const { return _patienceMelody; }
    void setPatienceMelody(int value) { _patienceMelody = clamp(value, 0, 100); }
    int patienceRhythm() const { return _patienceRhythm.get(isRouted(Routing::Target::StochasticPatienceRhythm)); }
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

    // stepsSieve — pitch sieve cutoff (Phase 11). 0..100, 100=open.
    int stepsSieve() const { return _stepsSieve; }
    void setStepsSieve(int value) { _stepsSieve = clamp(value, 0, 100); }

    // mask (deterministic loop playback thinning, V5 rename from density)
    int mask() const { return _mask.get(isRouted(Routing::Target::StochasticMask)); }
    void setMask(int mask, bool routed = false) { _mask.set(clamp(mask, 0, 100), routed); }

    // gateLength — repurposed from the Phase 11 `_gateLength` reserved slot. Knob
    // 0..100 controls the spread of per-event gate length around a hardcoded
    // 50% center. 0 = exact 50% every event; 100 = triangular distribution
    // 10..100% peaked at 50%. Floored at 10% of duration AND at an absolute
    // audible-tick minimum so very short events still produce a real trigger.
    // Routing target keeps the legacy `StochasticGeneratorDensity` enum name
    // for save-file compatibility; UI label changes to "Gate Length".
    int gateLength() const { return _gateLength.get(isRouted(Routing::Target::StochasticGateLength)); }
    void setGateLength(int value, bool routed = false) { _gateLength.set(clamp(value, 0, 100), routed); }

    // tilt (attached to mask)
    int tilt() const { return _tilt.get(isRouted(Routing::Target::StochasticTilt)); }
    void setTilt(int tilt, bool routed = false) { _tilt.set(clamp(tilt, -100, 100), routed); }

    // burs
    int burst() const { return _burst.get(isRouted(Routing::Target::StochasticBurst)); }
    void setBurst(int burst, bool routed = false) { _burst.set(clamp(burst, 0, 100), routed); }

    // feel (Phase 16 P4, 2026-05-23) — knob 0..100, default 50. Detent
    // [45..55] = off (no scaling). knob < 45 lerps toward 3 beats per
    // cycle, knob > 55 lerps toward 5 beats per cycle. Generator reads
    // this in the post-walk scaling pass; inert until P5 wires it.
    int feel() const { return _feel.get(isRouted(Routing::Target::StochasticFeel)); }
    void setFeel(int feel, bool routed = false) { _feel.set(clamp(feel, 0, 100), routed); }

    // minDegree
    int minDegree() const { return _minDegree; }
    void setMinDegree(int degree) { _minDegree = clamp(degree, 0, 127); }

    // maxDegree
    int maxDegree() const { return _maxDegree; }
    void setMaxDegree(int degree) { _maxDegree = clamp(degree, 0, 127); }

    // rotate
    int rotate() const { return _rotate.get(isRouted(Routing::Target::Rotate)); }
    void setRotate(int rotate, bool routed = false) { _rotate.set(clamp(rotate, -64, 64), routed); }

    // durationTickets (Level 3 parent-duration weights)
    int durationTicket(int index) const { return _durationTickets[index]; }
    void setDurationTicket(int index, int value) { _durationTickets[index] = clamp(value, 0, 100); }
    bool durationTicketsActive() const {
        for (int i = 0; i < 8; ++i) if (_durationTickets[i] > 0) return true;
        return false;
    }
    bool pitchTicketsActive() const {
        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) if (_degreeTickets[i] > 0) return true;
        return false;
    }

    // Level (UI presentation, not engine)
    StochasticLevel level() const { return _level; }
    void setLevel(StochasticLevel level) { _level = ModelUtils::clampedEnum(level); }

    void printLevel(StringBuilder &str) const {
        switch (_level) {
        case StochasticLevel::Core:       str("Core"); break;
        case StochasticLevel::Direct:     str("Direct"); break;
        case StochasticLevel::Weights:    str("Weights"); break;
        case StochasticLevel::Last:       break;
        }
    }
    void editLevel(int value, bool shift) { setLevel(ModelUtils::adjustedEnum(level(), value)); }

    void printRepeatProb(StringBuilder &str) const { str("%d%%", repeatProb()); }
    void editRepeatProb(int value, bool shift) { setRepeatProb(repeatProb() + value); }

    void printStepsSieve(StringBuilder &str) const { str("%d", stepsSieve()); }
    void editStepsSieve(int value, bool shift) { setStepsSieve(stepsSieve() + value); }

    void printMarblesMode(StringBuilder &str) const { str(marblesMode() == MarblesMode::Off ? "Off" : "On"); }
    void editMarblesMode(int value, bool shift) { setMarblesMode(ModelUtils::adjustedEnum(marblesMode(), value)); }

    void printMarblesSpread(StringBuilder &str) const { str("%d%%", marblesSpread()); }
    void editMarblesSpread(int value, bool shift) { setMarblesSpread(marblesSpread() + value); }

    void printMarblesBias(StringBuilder &str) const { str("%d%%", marblesBias()); }
    void editMarblesBias(int value, bool shift) { setMarblesBias(marblesBias() + value); }


    void printMask(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticMask); str("%d%%", mask()); }
    void editMask(int value, bool shift) { if (!isRouted(Routing::Target::StochasticMask)) setMask(mask() + value); }

    // Level 1 Density macro: writes density only (no fan-out to rest)
    void printGateLength(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticGateLength); str("%d%%", gateLength()); }
    void editGateLength(int value, bool shift) {
        if (!isRouted(Routing::Target::StochasticGateLength)) {
            setGateLength(gateLength() + value);
        }
    }

    void editComplexityMacro(int value, bool shift) {
        if (!isRouted(Routing::Target::StochasticComplexity)) {
            int c = clamp(complexity() + value, 0, 100);
            setComplexity(c);
        }
    }

    void printTilt(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticTilt); str("%+d%%", tilt()); }
    void editTilt(int value, bool shift) { if (!isRouted(Routing::Target::StochasticTilt)) setTilt(tilt() + value); }

    // Phase 12: append "*" when current noteDuration + divisor combo can't
    // produce audible burst (parentTicks < 96). Engine still gates Burst
    // probability via Cluster F; this just tells the user why the knob is
    // inert at short durations so Burst/Count/Rate aren't silently dead.
    void printBurst(StringBuilder &str) const {
        printRouted(str, Routing::Target::StochasticBurst);
        static const int lutNum[] = { 8, 4, 3, 2, 4, 1, 2, 1 };
        static const int lutDen[] = { 1, 1, 1, 1, 3, 1, 3, 2 };
        int idx = clamp(int(noteDuration()), 0, 7);
        // parentTicks = divisor * (CONFIG_PPQN/CONFIG_SEQUENCE_PPQN=4) * num / den.
        int parentTicks = (int(divisor()) * 4 * lutNum[idx]) / lutDen[idx];
        const int kMinBurstParentTicks = 96;
        if (parentTicks < kMinBurstParentTicks) str("%d%% *", burst());
        else str("%d%%", burst());
    }
    void editBurst(int value, bool shift) { if (!isRouted(Routing::Target::StochasticBurst)) setBurst(burst() + value); }

    void printMinDegree(StringBuilder &str) const { str("%d", minDegree()); }
    void editMinDegree(int value, bool shift) { setMinDegree(minDegree() + value); }

    void printMaxDegree(StringBuilder &str) const { str("%d", maxDegree()); }
    void editMaxDegree(int value, bool shift) { setMaxDegree(maxDegree() + value); }

    void printPatienceMelody(StringBuilder &str) const { str("%d%%", patienceMelody()); }
    void editPatienceMelody(int value, bool shift) { setPatienceMelody(patienceMelody() + value); }

    void printPatienceRhythm(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticPatienceRhythm); str("%d%%", patienceRhythm()); }
    void editPatienceRhythm(int value, bool shift) { if (!isRouted(Routing::Target::StochasticPatienceRhythm)) setPatienceRhythm(patienceRhythm() + value); }

    void printLegatoProb(StringBuilder &str) const { str("%d%%", legatoProb()); }
    void editLegatoProb(int value, bool shift) { setLegatoProb(ModelUtils::adjustedByStep(legatoProb(), value, 1, !shift)); }

    void printComplexity(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticComplexity); str("%d%%", complexity()); }
    void editComplexity(int value, bool shift) { if (!isRouted(Routing::Target::StochasticComplexity)) setComplexity(complexity() + value); }

    void printContour(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticContour); str("%+d%%", contour()); }
    void editContour(int value, bool shift) { if (!isRouted(Routing::Target::StochasticContour)) setContour(contour() + value); }

    void printNoteDuration(StringBuilder &str) const {
        printRouted(str, Routing::Target::StochasticNoteDuration);
        printSlotDuration(str, noteDuration());
    }
    // Phase 11: divisor-aware label for any LUT slot. Used by both noteDuration
    // print and the duration ticket bars (so labels track clock-divisor changes
    // instead of lying with hardcoded 1/16-centered strings). Short form
    // ("x/y" / "x/yT") — no leading raw-tick value.
    void printSlotDuration(StringBuilder &str, int slot) const {
        // LUT mirrors StochasticTrackEngine::getDurationFraction().
        static const int lutNum[] = { 8, 4, 3, 2, 4, 1, 2, 1 };
        static const int lutDen[] = { 1, 1, 1, 1, 3, 1, 3, 2 };
        int idx = clamp(slot, 0, 7);
        int effectiveDivisor = (int(divisor()) * lutNum[idx]) / lutDen[idx];
        ModelUtils::printDivisorShort(str, effectiveDivisor);
    }
    void editNoteDuration(int value, bool shift) { if (!isRouted(Routing::Target::StochasticNoteDuration)) setNoteDuration(noteDuration() + value); }

    void printVariation(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticVariation); str("%d%%", variation()); }
    void editVariation(int value, bool shift) { if (!isRouted(Routing::Target::StochasticVariation)) setVariation(variation() + value); }

    void printRest(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticRest); str("%d%%", rest()); }
    void editRest(int value, bool shift) { if (!isRouted(Routing::Target::StochasticRest)) setRest(rest() + value); }

    void printSlide(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticSlide); str("%d%%", slide()); }
    void editSlide(int value, bool shift) { if (!isRouted(Routing::Target::StochasticSlide)) setSlide(slide() + value); }

    void printSleep(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticSleep); str("%d%%", sleep()); }
    void editSleep(int value, bool shift) { if (!isRouted(Routing::Target::StochasticSleep)) setSleep(sleep() + value); }


    void printMutate(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticMutate); str("%+d%%", mutate()); }
    void editMutate(int value, bool shift) { if (!isRouted(Routing::Target::StochasticMutate)) setMutate(mutate() + value); }

    void printJump(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticJump); str("%d%%", jump()); }
    void editJump(int value, bool shift) { if (!isRouted(Routing::Target::StochasticJump)) setJump(jump() + value); }

    void printBurstPitch(StringBuilder &str) const { str(burstPitch() == StochasticBurstPitch::Parent ? "Parent" : "Gen"); }
    void editBurstPitch(int value, bool shift) { setBurstPitch(ModelUtils::adjustedEnum(burstPitch(), value)); }

    void printRotate(StringBuilder &str) const { printRouted(str, Routing::Target::Rotate); str("%+d", rotate()); }
    void editRotate(int value, bool shift) { if (!isRouted(Routing::Target::Rotate)) setRotate(rotate() + value); }

    void printRootNote(StringBuilder &str) const {
        if (rootNote() == -1) {
            str("Default");
        } else {
            Types::printNote(str, rootNote());
        }
    }

    void editRootNote(int value, bool shift) {
        setRootNote(rootNote() + value);
    }

    // Phase 7 Pattern controls (Per-pattern metadata)
    int size() const { return _size; }
    void setSize(int size) {
        _size = clamp(size, 2, CONFIG_STEP_COUNT);
        _first = clamp(int(_first), 0, int(_size) - 1);
        _last = clamp(int(_last), int(_first), int(_size) - 1);
    }

    int first() const { return _first; }
    void setFirst(int first) { _first = clamp(first, 0, int(_size) - 1); }

    // Stubbed 2026-05-24: Last is collapsed into Size — playback end = size - 1.
    // _last field kept for serialization round-trip; setLast is no-op so the
    // stored value doesn't drift if UI dead-knob is moved. last() shadows
    // _last with size - 1 unconditionally.
    int last() const { return int(_size) - 1; }
    void setLast(int /*last*/) { /* no-op while stubbed */ }

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
    const std::array<StochasticSourceEvent, CONFIG_STEP_COUNT> &events() const { return _events; }
          std::array<StochasticSourceEvent, CONFIG_STEP_COUNT> &events()       { return _events; }

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
        for (auto &event : _events) event.clear();
    }

    void write(VersionedSerializedWriter &writer) const {
        writer.write(_scale);
        writer.write(_rootNote);
        writer.write(_divisor);
        writer.write(_resetMeasure);
        _clockMultiplier.write(writer);

        writer.write(_size);
        writer.write(_first);
        writer.write(_last);

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
        writer.write(_stepsSieve);
        _mask.write(writer);
        _tilt.write(writer);
        _burst.write(writer);
        _feel.write(writer);
        writer.write(_minDegree);
        writer.write(_maxDegree);
        _rotate.write(writer);
        _complexity.write(writer);
        _contour.write(writer);
        _noteDuration.write(writer);
        _variation.write(writer);
        _rest.write(writer);
        _slide.write(writer);
        writer.write(_burstRate);
        writer.write(_burstCount);
        writer.write(static_cast<uint8_t>(_burstPitch));
        _sleep.write(writer);
        _patienceRhythm.write(writer);
        _mutate.write(writer);
        _jump.write(writer);
        writer.write(_range);
        writer.write(static_cast<uint8_t>(_rhythmMode));
        writer.write(static_cast<uint8_t>(_melodyMode));
        _gateLength.write(writer);
        for (int i = 0; i < 8; ++i) writer.write(_durationTickets[i]);
        writer.write(static_cast<uint8_t>(_level));


        for (const auto &event : _events) {
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
        reader.read(_last);

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
        reader.read(_stepsSieve);
        _stepsSieve = clamp(int(_stepsSieve), 0, 100);
        _mask.read(reader);
        _tilt.read(reader);
        _burst.read(reader);
        _feel.read(reader);
        reader.read(_minDegree);
        _minDegree = clamp(int(_minDegree), 0, 127);
        reader.read(_maxDegree);
        _maxDegree = clamp(int(_maxDegree), 0, 127);
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
        uint8_t burstPitch;
        reader.read(burstPitch);
        _burstPitch = burstPitch < uint8_t(StochasticBurstPitch::Last) ? static_cast<StochasticBurstPitch>(burstPitch) : StochasticBurstPitch::Parent;
        _sleep.read(reader);
        _patienceRhythm.read(reader);
        _mutate.read(reader);
        _jump.read(reader);
        reader.read(_range);
        _range = clamp(int(_range), 1, 4);

        uint8_t rhythmMode, melodyMode;
        reader.read(rhythmMode);
        _rhythmMode = rhythmMode < uint8_t(StochasticSourceMode::Last) ? static_cast<StochasticSourceMode>(rhythmMode) : StochasticSourceMode::Loop;
        reader.read(melodyMode);
        _melodyMode = melodyMode < uint8_t(StochasticSourceMode::Last) ? static_cast<StochasticSourceMode>(melodyMode) : StochasticSourceMode::Loop;

        _gateLength.read(reader);
        for (int i = 0; i < 8; ++i) {
            reader.read(_durationTickets[i]);
            _durationTickets[i] = clamp(int(_durationTickets[i]), 0, 100);
        }
        uint8_t level;
        reader.read(level);
        _level = level < uint8_t(StochasticLevel::Last) ? static_cast<StochasticLevel>(level) : StochasticLevel::Core;


        for (auto &event : _events) {
            for (int i = 0; i < 6; ++i) {
                reader.read(event.bytes[i]);
            }
        }

        sanitizeAfterRead();
    }

private:
    // Shared defaults for clear() and the invalid-size recovery path in
    // sanitizeAfterRead(). Touches all model fields except _events (callers
    // decide whether to wipe events too).
    void setDefaults() {
        _scale = -1;
        _rootNote = -1;
        _divisor = 12;
        _resetMeasure = 0;
        _clockMultiplier.setBase(100);

        _size = 32;
        _first = 0;
        _last = 31;

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
        _stepsSieve = 100;
        _mask.setBase(100);
        _gateLength.setBase(0);
        _tilt.setBase(0);
        _burst.setBase(0);
        _feel.setBase(50);
        _minDegree = 0;
        _maxDegree = 127;
        _rotate.setBase(0);
        _complexity.setBase(50);
        _contour.setBase(0);
        _noteDuration.setBase(5);   // LUT slot 5 = ×1 (= divisor)
        _variation.setBase(16);     // audible variation at fresh roll
        _rest.setBase(0);
        _slide.setBase(0);
        _burstRate = 50;
        _burstCount = 0;
        _burstPitch = StochasticBurstPitch::Parent;
        _sleep.setBase(0);
        _patienceRhythm.setBase(100);
        _mutate.setBase(0);
        _jump.setBase(0);
        _range = 1;
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
            for (auto &event : _events) event.clear();
            return;
        }

        _first = clamp(int(_first), 0, int(_size) - 1);
        _last = clamp(int(_last), int(_first), int(_size) - 1);
        _rhythmValid = _rhythmValid ? true : false;
        _melodyValid = _melodyValid ? true : false;

        for (int i = _size; i < CONFIG_STEP_COUNT; ++i) {
            _events[i].clear();
        }
    }

    int8_t _trackIndex = -1;
    int8_t _scale = -1;
    int8_t _rootNote = -1;
    uint8_t _divisor = 12;
    uint8_t _resetMeasure = 0;
    Routable<uint8_t> _clockMultiplier;

    // Phase 7 Generation Parameters
    uint8_t _size;
    uint8_t _first;
    uint8_t _last;

    // Phase 8.2 Split Source Buffers
    std::array<StochasticSourceEvent, CONFIG_STEP_COUNT> _events;
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
    uint8_t _stepsSieve;

    Routable<uint8_t> _mask;
    Routable<uint8_t> _gateLength;
    Routable<int8_t> _tilt;
    Routable<uint8_t> _burst;
    Routable<uint8_t> _feel;

    uint8_t _minDegree;
    uint8_t _maxDegree;

    Routable<int8_t> _rotate;
    Routable<uint8_t> _complexity;
    Routable<int8_t> _contour;
    Routable<uint8_t> _noteDuration;
    Routable<int8_t> _variation;
    Routable<uint8_t> _rest;
    Routable<uint8_t> _slide;
    uint8_t _burstRate;
    uint8_t _burstCount;
    StochasticBurstPitch _burstPitch;
    Routable<uint8_t> _sleep;
    Routable<uint8_t> _patienceRhythm;
    Routable<int8_t> _mutate;
    Routable<uint8_t> _jump;
    uint8_t _range;

    uint8_t _durationTickets[8];

    StochasticLevel _level = StochasticLevel::Core;

    StochasticSourceMode _rhythmMode;
    StochasticSourceMode _melodyMode;


    friend class StochasticTrack;
};

static_assert(sizeof(StochasticSequence) <= 560, "StochasticSequence too large");
