#pragma once

#include "ListPage.h"

#include "ui/model/RouteSourceSelectListModel.h"

#include "model/Routing.h"

class RouteSourceSelectPage : public ListPage {
public:
    RouteSourceSelectPage(PageManager &manager, PageContext &context);

    using ResultCallback = std::function<void(bool, Routing::Source)>;

    using ListPage::show;
    void show(Routing::Target target, Routing::Source current, ResultCallback callback);

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual bool isModal() const override { return true; }

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void keyboard(KeyboardEvent &event) override;

private:
    void closeWithResult(bool result);

    ResultCallback _callback;
    RouteSourceSelectListModel _listModel;
};
