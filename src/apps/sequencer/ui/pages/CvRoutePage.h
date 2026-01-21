#pragma once

#include "BasePage.h"

class CvRoutePage : public BasePage {
public:
    CvRoutePage(PageManager &manager, PageContext &context);

    void draw(Canvas &canvas) override;
    void keyPress(KeyPressEvent &event) override;
    void encoder(EncoderEvent &event) override;

private:
    enum class EditRow : uint8_t {
        Input,
        Output
    };

    void cycleInput(int lane, int delta);
    void cycleOutput(int lane, int delta);

    static const char *inputLabel(int lane, int value);
    static const char *outputLabel(int lane, int value);

    int _activeCol = 0;
    EditRow _editRow = EditRow::Input;
};
