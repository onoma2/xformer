#pragma once

#include "BasePage.h"

#include "ui/painters/ScopePainter.h"

#include "engine/MidiPort.h"

#include "core/midi/MidiMessage.h"

#include <array>
#include <cstdint>

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
    virtual void keyboard(KeyboardEvent &event) override;

    void setScopeActive(bool active);
    void toggleScope();

private:
    void drawCvIn(Canvas &canvas);
    void drawCvOut(Canvas &canvas);
    void drawMidi(Canvas &canvas);
    void drawStats(Canvas &canvas);
    void drawVersion(Canvas &canvas);
    void drawSizes(Canvas &canvas);
    void drawScope(Canvas &canvas);
    void sampleScope();
    void resetScope();

    enum class Mode : uint8_t {
        CvIn,
        CvOut,
        Midi,
        Stats,
        Sizes,
        Version,
    };

    Mode _mode = Mode::CvIn;
    bool _scopeActive = false;
    MidiMessage _lastMidiMessage;
    MidiPort _lastMidiMessagePort;
    uint32_t _lastMidiMessageTicks = -1;
    int _sizePage = 0;
    static constexpr int SizePageCount = 5;

    static constexpr int ScopeWidth = Width;
    static constexpr int ScopeHeight = Height;
    std::array<int8_t, ScopeWidth> _scopeCv{};            // normalized +/-127 (= +/-6V)
    std::array<uint8_t, ScopeWidth> _scopeGate{};
    std::array<int8_t, ScopeWidth> _scopeCvSecondary{};
    float _scopeLastCv = 0.f;                              // last raw primary CV, for the readout
    int _scopeWriteIndex = 0;
    int8_t _scopeChannel = 0;
    int8_t _scopeSecondaryChannel = -1;
};
