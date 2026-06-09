#pragma once

#include "BasePage.h"
#include "ui/painters/ScopePainter.h"

#include <cstdint>

class ModulatorPage : public BasePage {
public:
    ModulatorPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;
    virtual void keyboard(KeyboardEvent &event) override;

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
        Commit = 0,
        Cancel = 1,
        Route = 2,
        Last
    };

    void setSelectedModulator(int index);
    void setSelectedFunction(Function function);
    void setSelectedRoutingFunction(RoutingFunction function);
    void contextShow(bool doubleClick = false) override;
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

    // Rolling scope history of the modulator output (int8, +/-127 = full swing),
    // sampled once per draw. Replaces the static waveform-shape cache.
    static constexpr int ScopeWidth = 116;
    int8_t _scope[ScopeWidth] = {};
    int _scopeWrite = 0;
};
