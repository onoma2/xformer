#pragma once

#include "BasePage.h"
#include "ui/painters/ScopePainter.h"

#include "Config.h"

#include <cstdint>

class Modulator;

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

    // Destinations-page focus. Grid is the default (encoder moves the cursor,
    // push adds, F3 clears); F1/F2/F4/F5 focus the side fields.
    enum class RoutingFunction : uint8_t {
        Grid = 0,
        Mode,
        Gate,
        Event,
        CCNumber,
    };

    // The continuous MIDI value a modulator can drive on a destination.
    enum class ContEvent : uint8_t {
        CC = 0,
        Bend,
        Pressure,
        Last,
    };

    // Positioned so the menu binds CANCEL to F4 and COMMIT to F5, matching the
    // routing matrix footer (slots 0-2 are empty/inert).
    enum class ContextAction : uint8_t {
        Cancel = 3,
        Commit = 4,
        Last = 5,
    };

    void setSelectedModulator(int index);
    void setSelectedFunction(Function function);
    void setSelectedRoutingFunction(RoutingFunction function);
    void contextShow(bool doubleClick = false) override;
    void contextAction(int index);
    void loadRoutingFromMidiOutput();
    void applyRoutingToMidiOutput();
    void drawDestinationsBody(Canvas &canvas, Modulator &modulator);
    void drawMembershipGrid(Canvas &canvas);

    bool cursorIsCv() const { return _destCursor < CONFIG_CHANNEL_COUNT; }
    int cursorCvIndex() const { return _destCursor; }
    int cursorMidiIndex() const { return _destCursor - CONFIG_CHANNEL_COUNT; }

    int _selectedModulator = 0;
    Function _selectedFunction = Function::Shape;

    // Pagination state (for ADSR 2-page support)
    int _currentPage = 0;  // 0 or 1 for ADSR
    int _totalPages = 1;   // 1 for LFO/Random, 2 for ADSR

    // Destinations page (multi). The membership sets stage which CV jacks / MIDI
    // outs carry this modulator; written to the outputs only on Commit.
    static constexpr int DestCount = CONFIG_CHANNEL_COUNT + CONFIG_MIDI_OUTPUT_COUNT;
    bool _showRoutingOverlay = false;
    RoutingFunction _selectedRoutingFunction = RoutingFunction::Grid;
    int _destCursor = 0;                 // 0..CV-1 = CV jacks, then MIDI outs
    uint8_t _cvSet = 0;                  // CV jack membership bitmask
    uint16_t _midiSet = 0;              // MIDI output membership bitmask
    ContEvent _midiEvent[CONFIG_MIDI_OUTPUT_COUNT] = {};   // per-out continuous event
    uint8_t _midiCCNum[CONFIG_MIDI_OUTPUT_COUNT] = {};      // CC number (CC event only)

    // Rolling scope history of the modulator output (int8, +/-127 = full swing),
    // sampled once per draw. Replaces the static waveform-shape cache.
    static constexpr int ScopeWidth = 116;
    int8_t _scope[ScopeWidth] = {};
    int _scopeWrite = 0;
};
