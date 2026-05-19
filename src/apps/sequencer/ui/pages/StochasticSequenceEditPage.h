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
    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    enum class EditFocus {
        Ticket,
        DegreeRotation,
        MaskRotation,
        Last
    };

    enum class ContextAction {
        Init,
        Even,
        Random,
        Last
    };

    void contextShow(bool doubleClick = false);
    void contextAction(int index);

    int _selectedDegree = 0;
    int _bank = 0;
    EditFocus _editFocus = EditFocus::Ticket;
};
