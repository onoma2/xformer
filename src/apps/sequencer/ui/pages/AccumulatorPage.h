#pragma once

#include "ListPage.h"

#include "ui/model/AccumulatorListModel.h"

class AccumulatorPage : public ListPage {
public:
    AccumulatorPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    void updateListModel();

    AccumulatorListModel _listModel;
};