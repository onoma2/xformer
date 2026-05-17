#pragma once

#include "ListPage.h"
#include "ui/model/StochasticSequenceListModel.h"

class StochasticSequenceEditPage : public ListPage {
public:
    StochasticSequenceEditPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyDown(KeyEvent &event) override;
    virtual void keyPress(KeyPressEvent &event) override;

private:
    StochasticSequenceListModel _listModel;
    int _stepIndex = 0;
};
