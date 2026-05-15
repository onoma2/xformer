#pragma once

#include "ListPage.h"

#include "ui/model/ModulatorListModel.h"

class ModulatorPage : public ListPage {
public:
    ModulatorPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    void updateListModel();

    ModulatorListModel _listModel;
};
