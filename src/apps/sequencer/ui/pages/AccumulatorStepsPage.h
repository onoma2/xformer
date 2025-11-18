#pragma once

#include "ListPage.h"

#include "ui/model/AccumulatorStepsListModel.h"

class AccumulatorStepsPage : public ListPage {
public:
    AccumulatorStepsPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

protected:
    virtual void drawCell(Canvas &canvas, int row, int column, int x, int y, int w, int h) override;

private:
    void updateListModel();

    AccumulatorStepsListModel _listModel;
    int _currentStep = -1; // Track current playhead position
};