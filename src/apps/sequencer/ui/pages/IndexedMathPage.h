#pragma once

#include "BasePage.h"
#include "model/IndexedSequence.h"
#include "core/utils/Random.h"

class IndexedMathPage : public BasePage {
public:
    IndexedMathPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

    virtual bool isModal() const override { return true; }

private:
    enum class ActiveOp {
        A,
        B
    };

    enum class EditParam {
        Target,
        Operator,
        Value,
        Groups
    };

    enum class MathOp {
        Add,
        Sub,
        Mul,
        Div,
        Set,
        Rand,
        Jitter,
        Ramp,
        Quant,
        Last
    };

    struct MathConfig {
        IndexedSequence::ModTarget target = IndexedSequence::ModTarget::Duration;
        MathOp op = MathOp::Add;
        int value = 0;
        uint8_t targetGroups = IndexedSequence::TargetGroupsAll;
    };

    MathConfig &activeConfig();
    const MathConfig &activeConfig() const;

    void drawMathConfig(Canvas &canvas, const MathConfig &cfg, int y, bool active, const char *label);
    void drawGroupMask(Canvas &canvas, uint8_t groupMask, int x, int y, int width, Color onColor);
    static const char *opName(MathOp op);

    void applyMath(const MathConfig &cfg);
    void applyMathToStep(IndexedSequence::Step &step, const MathConfig &cfg, int stepIndex, int stepCount);

    bool matchesGroup(const IndexedSequence::Step &step, uint8_t targetGroups) const;
    bool configChanged() const;
    void resetConfigs();

    int valueMin(const MathConfig &cfg) const;
    int valueMax(const MathConfig &cfg) const;
    int valueStep(const MathConfig &cfg, bool shift) const;
    void clampValue(MathConfig &cfg) const;
    void formatValue(const MathConfig &cfg, StringBuilder &str) const;

    ActiveOp _activeOp = ActiveOp::A;
    EditParam _editParam = EditParam::Target;

    MathConfig _opA;
    MathConfig _opB;
    MathConfig _opABase;
    MathConfig _opBBase;

    Random _rng;
};
