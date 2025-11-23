#pragma once

#include "BasePage.h"

#include "engine/TuesdayTrackEngine.h"

class TuesdayEditPage : public BasePage {
public:
    TuesdayEditPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyDown(KeyEvent &event) override;
    virtual void keyUp(KeyEvent &event) override;
    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    static constexpr int PageCount = 3;
    static constexpr int ParamsPerPage = 4;

    // Parameter indices for each page
    enum Param {
        // Page 1
        Algorithm = 0,
        Flow,
        Ornament,
        Power,
        // Page 2
        LoopLength,
        Scan,
        Rotate,
        Glide,
        // Page 3
        Skew,
        GateOffset,
        CvUpdateMode,
        // Total
        ParamCount
    };

    int paramForPage(int page, int slot) const;
    const char *paramName(int param) const;
    const char *paramShortName(int param) const;
    void formatParamValue(int param, StringBuilder &str) const;
    int paramValue(int param) const;
    int paramMax(int param) const;
    bool paramIsBipolar(int param) const;
    void editParam(int param, int value, bool shift);

    void drawParam(Canvas &canvas, int x, int slot, int param);
    void drawStatusBox(Canvas &canvas);
    void drawBar(Canvas &canvas, int x, int y, int width, int height, int value, int maxValue, bool bipolar);

    void nextPage();
    void selectParam(int slot);
    void contextAction(int index);

    const TuesdayTrackEngine &trackEngine() const;
    TuesdayTrack &tuesdayTrack();
    const TuesdayTrack &tuesdayTrack() const;

    int _currentPage = 0;
    int _selectedSlot = 0;  // 0-3 for F1-F4
};
