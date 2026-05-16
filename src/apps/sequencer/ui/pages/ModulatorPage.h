#pragma once

#include "BasePage.h"
#include "engine/WaveformGenerator.h"

class ModulatorPage : public BasePage {
public:
    ModulatorPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    enum class Function : uint8_t {
        Shape = 0,
        Rate = 1,
        Depth = 2,
        Offset = 3,
        Phase = 4,
    };

    // Routing overlay functions (Shift+Page to toggle)
    enum class RoutingFunction : uint8_t {
        Mode = 0,
        Gate = 1,
        Target = 2,
        Event = 3,
        CCNumber = 4,
    };

    enum class RoutingTargetType : uint8_t {
        None,
        Midi,
        CV,
    };

    enum class ContextAction : uint8_t {
        Route = 0,
        Last
    };

    void setSelectedModulator(int index);
    void setSelectedFunction(Function function);
    void setSelectedRoutingFunction(RoutingFunction function);
    void updateWaveformCache();
    void contextShow();
    void contextAction(int index);
    void loadRoutingFromMidiOutput();
    void applyRoutingToMidiOutput();

    int _selectedModulator = 0;
    Function _selectedFunction = Function::Shape;

    // Pagination state (for ADSR 2-page support)
    int _currentPage = 0;  // 0 or 1 for ADSR
    int _totalPages = 1;   // 1 for LFO/Random, 2 for ADSR

    // Routing overlay state
    bool _showRoutingOverlay = false;
    RoutingFunction _selectedRoutingFunction = RoutingFunction::Mode;
    RoutingTargetType _routingTargetType = RoutingTargetType::None;
    int _routingTargetIndex = 0;     // 0-15 = MIDI output, 0-7 = CV output
    bool _routingEventIsCC = true;   // false = Note, true = CC (MIDI only)
    int _routingCCNum = 0;           // CC number 0-127 (MIDI CC only)

    // Waveform cache
    static constexpr int WaveformCacheSize = 112;
    int8_t _waveformCache[WaveformCacheSize];
    bool _waveformCacheValid = false;
    Modulator::Shape _lastShape = Modulator::Shape::Sine;
    int _lastDepth = 0;
    int _lastOffset = 0;
    int _lastPhase = 0;
    int _lastAttack = 0;
    int _lastDecay = 0;
};
