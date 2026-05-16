#pragma once

#include "BasePage.h"

#include "ui/StepSelection.h"
#include "ui/model/ListModel.h"

#include <functional>

class Generator;
class EuclideanGenerator;
class RandomGenerator;

class GeneratorContextQuickEditModel : public ListModel {
public:
    void configure(Generator *generator, int paramIndex, const char *label, const std::function<void()> &onEdited);
    int rows() const override;
    int columns() const override;
    void cell(int row, int column, StringBuilder &str) const override;
    void edit(int row, int column, int value, bool shift) override;
    void setSelectedScale(int, bool) override;

private:
    Generator *_generator = nullptr;
    int _paramIndex = -1;
    const char *_label = "";
    std::function<void()> _onEdited;
};

class GeneratorPage : public BasePage {
public:
    GeneratorPage(PageManager &manager, PageContext &context);

    using BasePage::show;
    void show(Generator *generator, StepSelection<CONFIG_STEP_COUNT> *stepSelection);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual bool isModal() const override { return true; }

    virtual void keyDown(KeyEvent &event) override;
    virtual void keyUp(KeyEvent &event) override;
    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

    void contextShow(bool doubleClick = false);
    void contextAction(int index);
    bool contextActionEnabled(int index) const;
    void init();
    void revert();
    void commit();
    void togglePreview();

    static const int StepCount = 16;

    int stepOffset() const { return _section * StepCount; }

private:
    bool boundTrackContextValid() const;
    bool ensureBoundTrackContext();
    int currentStep() const;
    bool stepInCurrentBank(int step) const;

    void drawEuclideanGenerator(Canvas &canvas, const EuclideanGenerator &generator) const;
    void drawRandomGenerator(Canvas &canvas, const RandomGenerator &generator) const;

    Generator *_generator;
    StepSelection<CONFIG_STEP_COUNT> *_stepSelection;

    std::pair<uint8_t, uint8_t> _valueRange;

    int _section = 0;
    bool _previewArmed = false;
    bool _applied = false;
    int _boundTrackIndex = -1;
    Track::TrackMode _boundTrackMode = Track::TrackMode::Note;

    char _contextMenuAuxLabel[16] = "";
    char _variationMenuLabel[16] = "VAR";
    ContextMenuModel::Item _contextMenuItems[5];
};
