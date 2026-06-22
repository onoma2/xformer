#pragma once

#include "BasePage.h"

// TT2 track I/O config — reached via the Track button (S2). Two views toggled by
// the footer NEXT key: OUTPUTS (8 CV × range/quant/offset/root) and INPUTS
// (trigger sources, CV-in sources, MIDI). Footer F-keys select the column,
// encoder rotates the row, encoder click toggles value-edit on the selected cell.
class TT2IoConfigPage : public BasePage {
public:
    TT2IoConfigPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;
    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    enum class View : uint8_t { Outputs, Inputs };

    int columnCount() const;
    int rowCount() const;
    void moveRow(int delta);
    void editCell(int delta);
    bool isTt2() const;

    void contextShow(bool doubleClick = false) override;
    void contextAction(int index);
    void loadScene();
    void saveScene();
    void loadSceneFromSlot(int slot);
    void saveSceneToSlot(int slot);

    View _view = View::Outputs;
    int _col = 0;     // active column (F1..F4)
    int _row = 0;     // selected row
    int _scroll = 0;  // row scroll offset (OUTPUTS)
    bool _edit = false;
};
