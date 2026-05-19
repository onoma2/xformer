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
        Pitch,
        Duration,
        Count
    };

    void contextShow(bool doubleClick = false);
    void contextAction(int index);
    void nextPage();

    void drawPitchPage(Canvas &canvas);
    void drawDurationPage(Canvas &canvas);
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

    Page _currentPage = Page::Pitch;
    int _selectedDurSlot = 0;
    DurFocus _durFocus = DurFocus::DurTicket;
};

static const char *durationTicketLabels[] = {
    "1/2", "1/4", "1/8", "1/16", "3/16", "5/16", "7/16", "1/8T"
};
