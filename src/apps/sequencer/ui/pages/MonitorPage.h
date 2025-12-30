#pragma once

#include "BasePage.h"

#include "engine/MidiPort.h"

#include "core/midi/MidiMessage.h"

#include <array>

class MonitorPage : public BasePage {
public:
    MonitorPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;
    virtual void midi(MidiEvent &event) override;

    void setScopeActive(bool active);
    void toggleScope();

private:
    void drawCvIn(Canvas &canvas);
    void drawCvOut(Canvas &canvas);
    void drawMidi(Canvas &canvas);
    void drawStats(Canvas &canvas);
    void drawVersion(Canvas &canvas);
    void drawScope(Canvas &canvas);
    void sampleScope();
    void resetScope();
    int scopeTrackOption() const;
    int scopeTrackFromOption(int option) const;
    int scopeOptionFromTrack(int trackIndex) const;

    enum class Mode : uint8_t {
        CvIn,
        CvOut,
        Midi,
        Stats,
        Version,
    };

    Mode _mode = Mode::CvIn;
    bool _scopeActive = false;
    MidiMessage _lastMidiMessage;
    MidiPort _lastMidiMessagePort;
    uint32_t _lastMidiMessageTicks = -1;

    static constexpr int ScopeWidth = Width;
    static constexpr int ScopeHeight = Height;
    std::array<float, ScopeWidth> _scopeCv{};
    std::array<uint8_t, ScopeWidth> _scopeGate{};
    std::array<float, ScopeWidth> _scopeCvSecondary{};
    int _scopeWriteIndex = 0;
    int8_t _scopeSecondaryTrack = -1;
};
