#pragma once

#include "ListPage.h"
#include "ui/model/StochasticPerformanceListModel.h"

class StochasticPerformancePage : public ListPage {
public:
    StochasticPerformancePage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

private:
    StochasticPerformanceListModel _listModel;
};
