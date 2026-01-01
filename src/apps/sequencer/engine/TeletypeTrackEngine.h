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
    static constexpr int PresetScriptCount = 10;

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

    void handleTr(uint8_t index, int16_t value);
    void beginPulse(uint8_t index, int16_t timeMs);
    void clearPulse(uint8_t index);
    void setPulseTime(uint8_t index, int16_t timeMs);

    void handleCv(uint8_t index, int16_t value, bool slew);
    void setCvSlew(uint8_t index, int16_t value);
    void setCvOffset(uint8_t index, int16_t value);
    uint16_t cvRaw(uint8_t index) const;
    void updateAdc(bool force);
    bool inputState(uint8_t index) const;
    void triggerManualScript();
    void selectNextManualScript();
    uint8_t manualScriptIndex() const { return _manualScriptIndex; }
    void applyPresetToScript(int scriptIndex, int presetIndex);
    static const char *presetName(int presetIndex);
    void syncMetroFromState();
    void resetMetroTimer();

private:
    void installBootScript();
    void runBootScript();
    void advanceTime(float dt);
    void runMetro(float dt);
    void updatePulses(float dt);
    void updateInputTriggers();
    void refreshActivity(float dt);
    float rawToVolts(int16_t value) const;
    int16_t voltsToRaw(float volts) const;
    static float midiNoteToVolts(int note);
    void installPresetScripts();
    bool applyPresetLine(scene_state_t &state, int scriptIndex, size_t lineIndex, const char *cmd);
    void runScript(int scriptIndex);

    TeletypeTrack &_teletypeTrack;

    bool _bootScriptPending = true;
    bool _activity = false;
    float _activityCountdownMs = 0.f;
    float _tickRemainderMs = 0.f;
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
};
