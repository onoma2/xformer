#pragma once

#include "ListPage.h"
#include "ui/model/PhaseFluxSequenceListModel.h"

class PhaseFluxSequencePage : public ListPage {
public:
    PhaseFluxSequencePage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyPress(KeyPressEvent &event) override;

private:
    virtual void contextShow(bool doubleClick = false) override;
    void contextAction(int index);
    bool contextActionEnabled(int index) const;
    void initSequence();
    void initRoute();

    PhaseFluxSequenceListModel _listModel;
};
