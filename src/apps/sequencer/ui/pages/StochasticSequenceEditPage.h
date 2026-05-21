#pragma once

#include "BasePage.h"

class StochasticSequenceEditPage : public BasePage {
public:
    StochasticSequenceEditPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyDown(KeyEvent &event) override;
    virtual void keyUp(KeyEvent &event) override;
    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    enum class EditFocus {
        Ticket,
        DegreeRotation,
        MaskRotation,
        Last
    };

    enum class DurFocus {
        DurTicket,
        Rest,
        Last
    };

    enum class ContextAction {
        Init,
        Even,
        Random,
        Last
    };

    enum class Page {
        Core,
        Marbles,
        Direct,
        Loop,
        Pitch,
        Duration,
        Count
    };

    void contextShow(bool doubleClick = false);
    void contextAction(int index);
    void nextPage();

    // Hero page draws (parameterized — respond to current sequence values).
    void drawCorePage(Canvas &canvas);
    void drawMarblesPage(Canvas &canvas);
    void drawDirectPage(Canvas &canvas);
    void drawLoopPage(Canvas &canvas);
    void drawPitchPage(Canvas &canvas);
    void drawDurationPage(Canvas &canvas);

    // Hero param edits (held-step + encoder turn).
    void editCoreStep(int step, int value, bool shift);
    void editMarblesStep(int step, int value, bool shift);
    void editDirectStep(int step, int value, bool shift);
    void editLoopStep(int step, int value, bool shift);

    // Per-page Fn handlers.
    bool handleCoreFunction(int fn, bool shift);
    bool handleMarblesFunction(int fn, bool shift);
    bool handleDirectFunction(int fn, bool shift, int pressCount);
    bool handleLoopFunction(int fn, bool shift, int pressCount);

    // Legacy ticket pages (existing).
    void handlePitchEncoder(EncoderEvent &event);
    void handleDurationEncoder(EncoderEvent &event);
    void handlePitchKeyDown(KeyEvent &event);
    void handleDurationKeyDown(KeyEvent &event);
    void handlePitchKeyPress(KeyPressEvent &event);
    void handleDurationKeyPress(KeyPressEvent &event);

    int _selectedDegree = 0;
    int _bank = 0;
    EditFocus _editFocus = EditFocus::Ticket;
    uint32_t _pitchSelectionMask = 0;
    uint32_t _durSelectionMask = 0;
    bool _persistMode = false;

    Page _currentPage = Page::Core;
    int _selectedDurSlot = 0;
    DurFocus _durFocus = DurFocus::DurTicket;

    // Hero pages: which step is currently held (encoder writes that step's param).
    // -1 = no step held; encoder writes the page-default param (step 0 of the page).
    int _heroHeldStep = -1;

    // DIRECT walker animation phase: advances per draw, wraps when it exceeds
    // one stride so the trail slides smoothly left between content refreshes.
    uint16_t _directWalkerTick = 0;

    // Event-driven walker trail. Ring of particles; each gate rising edge
    // shifts the ring left (older particles age, oldest falls off) and writes
    // a new particle at slot 0 with the live CV-derived pitch.
    static constexpr int kDirectTrailMax = 12;
    struct DirectParticle { int16_t yOffset; uint8_t flags; uint8_t children; };
    DirectParticle _directTrail[kDirectTrailMax] = {};
    uint8_t _directTrailFilled = 0;            // how many slots are valid (0..max)
    bool _directLastGate = false;
    uint32_t _directLastEventEpoch = 0;        // tick count of last event for sub-tick smoothing
};

// Duration ticket labels are now generated at runtime via
// StochasticSequence::printSlotDuration() so they track the active clock divisor.
// The old hardcoded table assumed divisor = 1/16 and lied at any other divisor.
