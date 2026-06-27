#pragma once

#include "ListPage.h"
#include "ui/model/FractalSequenceListModel.h"

class FractalSequenceListPage : public ListPage {
public:
    FractalSequenceListPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

private:
    FractalSequenceListModel _listModel;
};
