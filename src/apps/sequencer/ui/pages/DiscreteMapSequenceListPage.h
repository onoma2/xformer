#pragma once

#include "ListPage.h"

#include "ui/model/DiscreteMapSequenceListModel.h"

class DiscreteMapSequenceListPage : public ListPage {
public:
    DiscreteMapSequenceListPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyPress(KeyPressEvent &event) override;

private:
    enum class ContextAction {
        Init,
        Copy,
        Paste,
        Route,
        Last
    };

    void contextShow();
    void contextAction(int index);
    bool contextActionEnabled(int index) const;

    void initSequence();
    void copySequence();
    void pasteSequence();
    void initRoute();
    void invalidateThresholds();

    DiscreteMapSequenceListModel _listModel;
};
