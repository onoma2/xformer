#pragma once

#include "BasePage.h"

class TeletypePatternViewPage : public BasePage {
public:
    TeletypePatternViewPage(PageManager &manager, PageContext &context);

    void enter() override;
    void draw(Canvas &canvas) override;
    void updateLeds(Leds &leds) override;
    void keyPress(KeyPressEvent &event) override;
    void encoder(EncoderEvent &event) override;

private:
    void setPatternIndex(int pattern);
    void moveRow(int delta);
    void ensureRowVisible();
    void commitEdit();
    void backspaceDigit();
    void insertDigit(int digit);
    void insertRow();
    void deleteRow();
    void negateValue();
    void toggleTurtle();
    void setLength();
    void setStart();
    void setEnd();
    void syncPattern();
    void contextShow();
    void contextAction(int index);
    bool contextActionEnabled(int index) const;
    void saveTrack();
    void loadTrack();
    void saveTrackToSlot(int slot);
    void loadTrackFromSlot(int slot);

    int _patternIndex = 0;
    int _row = 0;   // absolute row 0..63
    int _offset = 0;
    bool _editingNumber = false;
    int32_t _editBuffer = 0;
};
