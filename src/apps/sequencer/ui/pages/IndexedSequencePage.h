#pragma once

#include "ListPage.h"

#include "ui/model/IndexedSequenceListModel.h"

class IndexedSequencePage : public ListPage {
public:
    IndexedSequencePage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyPress(KeyPressEvent &event) override;

private:
    void contextShow();
    void contextAction(int index);
    bool contextActionEnabled(int index) const;

    void initSequence();
    void initRoute();

    // Macro context menus
    void rhythmContextShow();
    void rhythmContextAction(int index);
    void waveformContextShow();
    void waveformContextAction(int index);
    void melodicContextShow();
    void melodicContextAction(int index);
    void durationTransformContextShow();
    void durationTransformContextAction(int index);

    IndexedSequenceListModel _listModel;
};
