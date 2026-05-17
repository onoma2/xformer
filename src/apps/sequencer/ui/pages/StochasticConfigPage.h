#pragma once

#include "ListPage.h"
#include "ui/model/StochasticTrackListModel.h"

class StochasticConfigPage : public ListPage {
public:
    StochasticConfigPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

private:
    StochasticTrackListModel _listModel;
};
