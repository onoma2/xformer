#pragma once

#include "TrackEngine.h"

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
    void clearPulse(uint8_t index);
    void setPulseTime(uint8_t index, int16_t timeMs);

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
    uint32_t timeTicks() const;
    float busCv(int index) const;
    void setBusCv(int index, float volts);
    float measureFraction() const;
    int trackPattern(int trackIndex) const;
    float tempo() const;
    void setTempo(float bpm);
    void setTrackPattern(int trackIndex, int patternIndex);
    void setTransportRunning(int16_t state);

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
    void updateInputTriggers();
    void refreshActivity(float dt);
    float rawToVolts(int16_t value) const;
    int16_t voltsToRaw(float volts) const;
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

    // CV slew state
    std::array<float, CvOutputCount> _cvOutputTarget{};
    std::array<int16_t, CvOutputCount> _cvSlewTime{};
    std::array<bool, CvOutputCount> _cvSlewActive{};

    uint8_t _manualScriptIndex = 0;
    int _cachedPattern = -1;
};
