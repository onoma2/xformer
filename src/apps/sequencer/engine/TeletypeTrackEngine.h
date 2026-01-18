#pragma once

#include "TrackEngine.h"
#include "GeodeEngine.h"

#include "model/TeletypeTrack.h"

#include <array>
#include <cstdint>

class TeletypeTrackEngine : public TrackEngine {
public:
    static constexpr int TriggerInputCount = 4;
    static constexpr int TriggerOutputCount = 4;
    static constexpr int CvOutputCount = 4;
    static constexpr int PerformerGateCount = 8;
    static constexpr int PerformerCvCount = 8;

    TeletypeTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine);

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::Teletype; }

    virtual void reset() override;
    virtual void restart() override;
    void runBootScriptNow();
    void panic();
    virtual TickResult tick(uint32_t tick) override;
    virtual void update(float dt) override;

    virtual bool activity() const override { return _activity; }
    virtual bool gateOutput(int index) const override {
        if (index < 0 || index >= PerformerGateCount) {
            return false;
        }
        return _performerGateOutput[index];
    }
    virtual float cvOutput(int index) const override {
        if (index < 0 || index >= PerformerCvCount) {
            return 0.f;
        }
        return _performerCvOutput[index];
    }

    virtual bool receiveMidi(MidiPort port, const MidiMessage &message) override;
    virtual void clearMidiMonitoring() override;

    void handleTr(uint8_t index, int16_t value);
    void beginPulse(uint8_t index, int16_t timeMs);
    bool allowPulse(uint8_t index);
    void clearPulse(uint8_t index);
    void setPulseTime(uint8_t index, int16_t timeMs);
    void setTrDiv(uint8_t index, int16_t div);
    void setTrWidth(uint8_t index, int16_t pct);

    void handleCv(uint8_t index, int16_t value, bool slew);
    void setCvSlew(uint8_t index, int16_t value);
    void setCvOffset(uint8_t index, int16_t value);
    bool anyCvSlewActive() const;
    bool isTransportRunning() const;
    float routingSource(int index) const;
    uint16_t cvRaw(uint8_t index) const;
    void updateAdc(bool force);
    bool inputState(uint8_t index) const;
    void triggerManualScript();
    void selectNextManualScript();
    uint8_t manualScriptIndex() const { return _manualScriptIndex; }
    void triggerScript(int scriptIndex);
    void syncMetroFromState();
    void resetMetroTimer();
    void setMetroPeriod(int16_t periodMs);
    void setMetroActive(bool active);
    uint32_t timeTicks() const;
    float busCv(int index) const;
    void setBusCv(int index, float volts);
    float measureFraction() const;
    float measureFractionBars(uint8_t bars) const;
    TeletypeTrack::TimeBase timeBase() const { return _teletypeTrack.timeBase(); }
    void showMessage(const char *text);
    int trackPattern(int trackIndex) const;
    float tempo() const;
    void setTempo(float bpm);
    void setTrackPattern(int trackIndex, int patternIndex);
    void setTransportRunning(int16_t state);
    void setMetroAllPeriod(int16_t periodMs);
    void setMetroAllActive(bool active);
    void resetMetroAll();
    void setEnvTarget(uint8_t index, int16_t value);
    void setEnvAttack(uint8_t index, int16_t ms);
    void setEnvDecay(uint8_t index, int16_t ms);
    void triggerEnv(uint8_t index);
    void setEnvOffset(uint8_t index, int16_t value);
    void setEnvLoop(uint8_t index, int16_t count);
    void setEnvEor(uint8_t index, int16_t tr);
    void setEnvEoc(uint8_t index, int16_t tr);
    void setLfoRate(uint8_t index, int16_t ms);
    void setLfoWave(uint8_t index, int16_t value);
    void setLfoAmp(uint8_t index, int16_t value);
    void setLfoFold(uint8_t index, int16_t value);
    void setLfoOffset(uint8_t index, int16_t value);
    void setLfoStart(uint8_t index, int16_t state);
    void setCvInterpolation(int index, bool enabled);
    int16_t noteGateGet(int trackIndex, int stepIndex) const;
    void noteGateSet(int trackIndex, int stepIndex, int16_t value);
    int16_t noteNoteGet(int trackIndex, int stepIndex) const;
    void noteNoteSet(int trackIndex, int stepIndex, int16_t value);
    int16_t noteGateHere(int trackIndex) const;
    int16_t noteNoteHere(int trackIndex) const;

    // Geode engine control
    void setGeodeTime(int16_t value);
    void setGeodeIntone(int16_t value);
    void setGeodeRamp(int16_t value);
    void setGeodeCurve(int16_t value);
    void setGeodeRun(int16_t value);
    void setGeodeMode(int16_t value);
    void setGeodeOffset(int16_t value);
    void setGeodeTune(uint8_t voiceIndex, int16_t numerator, int16_t denominator);
    void setGeodeBars(int16_t bars);
    void setGeodeOut(uint8_t cvIndex, int16_t voiceIndex);
    void triggerGeodeVoice(uint8_t voiceIndex, int16_t divs, int16_t repeats);

    int16_t getGeodeTime() const;
    int16_t getGeodeIntone() const;
    int16_t getGeodeRamp() const;
    int16_t getGeodeCurve() const;
    int16_t getGeodeRun() const;
    int16_t getGeodeMode() const;
    int16_t getGeodeOffset() const;
    int16_t getGeodeBars() const;
    int16_t getGeodeVal() const;
    int16_t getGeodeVoice(uint8_t voiceIndex) const;
    int16_t getGeodeTuneNumerator(uint8_t voiceIndex) const;
    int16_t getGeodeTuneDenominator(uint8_t voiceIndex) const;

private:
    void runMidiTriggeredScript(int scriptIndex);
    void processMidiMessage(const MidiMessage &message);

    void installBootScript();
    void runBootScript();
    void loadScriptsFromModel();
    void seedScriptsFromPresets();
    float advanceTime(float dt);
    float advanceClockTime();
    void runMetro(float timeDelta);
    void updatePulses(float timeDelta);
    void updateEnvelopes();
    void updateLfos(float timeDeltaMs);
    void updateInputTriggers();
    void refreshActivity(float dt);
    void updateGeode(float dt);
    float rawToVolts(int16_t value) const;
    int16_t voltsToRaw(float volts) const;
    float normalizeUnipolar(int16_t value) const;
    float normalizeBipolar(int16_t value) const;
    float applyCvQuantize(int index, float volts) const;
    static float midiNoteToVolts(int note);
    bool applyScriptLine(scene_state_t &state, int scriptIndex, size_t lineIndex, const char *cmd);
    void runScript(int scriptIndex);

    TeletypeTrack &_teletypeTrack;

    bool _bootScriptPending = true;
    bool _activity = false;
    float _activityCountdownMs = 0.f;
    float _tickRemainderMs = 0.f;
    double _clockRemainder = 0.0;
    double _lastClockPos = 0.0;
    bool _clockPosValid = false;
    uint32_t _clockTickCounter = 0;
    float _metroRemainingMs = 0.f;
    int16_t _metroPeriodMs = 0;
    bool _metroActive = false;

    std::array<bool, PerformerGateCount> _performerGateOutput{};
    std::array<float, PerformerCvCount> _performerCvOutput{};
    std::array<int16_t, CvOutputCount> _teletypeCvRaw{};
    std::array<int16_t, CvOutputCount> _teletypeCvOffset{};
    std::array<float, TriggerOutputCount> _teletypePulseRemainingMs{};
    std::array<bool, TriggerInputCount> _teletypeInputState{};
    std::array<bool, TriggerInputCount> _teletypePrevInputState{};
    std::array<uint8_t, TriggerOutputCount> _trDiv{};
    std::array<uint8_t, TriggerOutputCount> _trDivCounter{};
    std::array<uint8_t, TriggerOutputCount> _trWidthPct{};
    std::array<bool, TriggerOutputCount> _trWidthEnabled{};
    std::array<float, TriggerOutputCount> _trLastPulseMs{};
    std::array<float, TriggerOutputCount> _trLastIntervalMs{};

    // CV slew state
    std::array<float, CvOutputCount> _cvOutputTarget{};
    std::array<int16_t, CvOutputCount> _cvSlewTime{};
    std::array<bool, CvOutputCount> _cvSlewActive{};
    std::array<int16_t, CvOutputCount> _envTargetRaw{};
    std::array<int16_t, CvOutputCount> _envOffsetRaw{};
    std::array<int16_t, CvOutputCount> _envAttackMs{};
    std::array<int16_t, CvOutputCount> _envDecayMs{};
    std::array<int16_t, CvOutputCount> _envLoopSetting{};
    std::array<int16_t, CvOutputCount> _envLoopsRemaining{};
    std::array<int16_t, CvOutputCount> _envSlewMs{};
    std::array<float, CvOutputCount> _envStageRemainingMs{};
    std::array<int8_t, CvOutputCount> _envEorTr{};
    std::array<int8_t, CvOutputCount> _envEocTr{};
    std::array<uint8_t, CvOutputCount> _envState{};
    std::array<bool, CvOutputCount> _lfoActive{};
    std::array<int16_t, CvOutputCount> _lfoCycleMs{};
    std::array<int16_t, CvOutputCount> _lfoWave{};
    std::array<uint8_t, CvOutputCount> _lfoAmp{};
    std::array<uint8_t, CvOutputCount> _lfoFold{};
    std::array<int16_t, CvOutputCount> _lfoOffsetRaw{};
    std::array<float, CvOutputCount> _lfoPhase{};

    // Tick interpolation state (for smooth LFO/envelope outputs)
    std::array<bool, CvOutputCount> _cvInterpolationEnabled{};
    std::array<float, CvOutputCount> _cvRealTarget{};  // Actual target set by script
    std::array<float, CvOutputCount> _prevCvOutputTarget{};
    std::array<float, CvOutputCount> _cvTickPhase{};
    float _tickDurationMs = 20.0f;  // Updated from clock tick

    bool _initialized = false;

    uint8_t _manualScriptIndex = 0;
    int _cachedPattern = -1;
    float _pulseClockMs = 0.f;

    // Geode engine state
    struct GeodeParams {
        int16_t time = 8192;
        int16_t intone = 8192;
        int16_t ramp = 8192;
        int16_t curve = 8192;
        int16_t run = 0;
        int16_t offset = 0;
        int16_t bars = 4;
        uint8_t mode = 0;
        std::array<int16_t, GeodeEngine::VoiceCount> tuneNum{};
        std::array<int16_t, GeodeEngine::VoiceCount> tuneDen{};

        void clear() {
            time = 8192;
            intone = 8192;
            ramp = 8192;
            curve = 8192;
            run = 0;
            offset = 0;
            bars = 4;
            mode = 0;
            for (int i = 0; i < GeodeEngine::VoiceCount; ++i) {
                tuneNum[i] = i + 1;
                tuneDen[i] = 1;
            }
        }
    };

    GeodeParams _geodeParams;
    GeodeEngine _geodeEngine;
    bool _geodeActive = false;
    std::array<int8_t, CvOutputCount> _geodeOutRouting{};
    float _geodeFreePhase = 0.f;
    float _geodePhaseOffset = 0.f;
    float _geodeBarSeconds = 2.f;
    bool _geodeWasClockRunning = false;
};
