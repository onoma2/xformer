#pragma once

#include "ListPage.h"

#include "ui/model/HarmonyListModel.h"

class HarmonyPage : public ListPage {
public:
    HarmonyPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    void updateListModel();
    void drawStatusLine(Canvas &canvas);

    HarmonyListModel _listModel;
    NoteSequence *_sequence = nullptr;
};
