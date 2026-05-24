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
        BurstHold,    // toggle Hold/Roll burst-pitch mode
        Last
    };

    enum class Page {
        Live,
        Loop,
        Pitch,
        Duration,
        Count
    };

    // Batch 0 / docs/stoch-review.md finding #3 — guard every type-specific
    // callback against being invoked when the selected track is no longer
    // Stochastic. TopPage remapping covers the normal path, but project
    // load + future selected-track mutation paths may not. Mirrors the
    // pattern used by NoteSequenceEditPage::isActiveForSelectedTrack().
    bool isActiveForSelectedTrack() const;

    void contextShow(bool doubleClick = false);
    void contextAction(int index);
    void nextPage();

    // Hero page draws (parameterized — respond to current sequence values).
    void drawLivePage(Canvas &canvas);
    void drawLoopPage(Canvas &canvas);
    void drawPitchPage(Canvas &canvas);
    void drawDurationPage(Canvas &canvas);

    // Hero param edits (held-step + encoder turn).
    void editLiveStep(int step, int value, bool shift);
    void editLoopStep(int step, int value, bool shift);

    // Phase 14B follow-on: window-edit (first/last/size) requires immediate
    // engine sync — model-only edits leave _currentStep / queues / cache
    // pointing at the old window until the next event-boundary trigger.
    // Call this AFTER setFirst/setLast/setSize on the selected stochastic
    // sequence. No-op if the selected track isn't currently a Stochastic
    // engine.
    void notifyStochasticWindowEdit();

    // Codex review 2026-05-22 finding 2: edits to burst-shaping knobs (Burst,
    // BurstCount, BurstRate, BurstHold) and other cache-baked fields must
    // invalidate the engine cache; otherwise non-Repeat playback keeps using
    // pre-edit baked child cells. Lighter than syncWindowEdit (no queue
    // flush, no _currentStep snap) — just refreshStepCache.
    void notifyStochasticShapingEdit();

    // Per-page Fn handlers.
    bool handleLiveFunction(int fn, bool shift, int pressCount);
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

    Page _currentPage = Page::Live;
    int _selectedDurEntry = 0;
    DurFocus _durFocus = DurFocus::DurTicket;

    // Hero pages (Live/Loop): bitmask of currently held step buttons. Multiple
    // bits = multi-select; encoder turn applies the edit to every set bit.
    // Mirrors the ticket-page selection mask pattern (`_pitchSelectionMask`,
    // `_durSelectionMask`). Cleared on keyUp unless `_persistMode` is active.
    uint32_t _heroSelectionMask = 0;

    // DIRECT walker animation phase: advances per draw, wraps when it exceeds
    // one stride so the trail slides smoothly left between content refreshes.
    uint16_t _directWalkerTick = 0;

    // Event-driven walker trail, copied from the engine's recent Direct history
    // and transformed into viewport-relative coordinates.
    static constexpr int kDirectTrailMax = 12;
    struct DirectParticle { int16_t yOffset; uint8_t flags; uint8_t children; };
    DirectParticle _directTrail[kDirectTrailMax] = {};
    uint8_t _directTrailFilled = 0;            // how many slots are valid (0..max)
};

// Duration ticket labels are now generated at runtime via
// StochasticSequence::printDurationEntry() so they track the active clock divisor.
// The old hardcoded table assumed divisor = 1/16 and lied at any other divisor.
