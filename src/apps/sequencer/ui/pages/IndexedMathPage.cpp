#include "IndexedMathPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

#include "core/math/Math.h"

#include "model/Track.h"

#include "os/os.h"

namespace {
constexpr int kMulDivMax = 400;

const char *targetName(IndexedSequence::ModTarget target) {
    switch (target) {
    case IndexedSequence::ModTarget::Duration:   return "DUR";
    case IndexedSequence::ModTarget::GateLength: return "GATE";
    case IndexedSequence::ModTarget::NoteIndex:  return "NOTE";
    case IndexedSequence::ModTarget::Last:       break;
    }
    return "?";
}

} // namespace

IndexedMathPage::IndexedMathPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{
}

void IndexedMathPage::enter() {
    _activeOp = ActiveOp::A;
    _editParam = EditParam::Target;
    _opABase = _opA;
    _opBBase = _opB;
    _rng = Random(os::ticks());

    // Capture step selection from IndexedSequenceEditPage
    _selectedSteps = _manager.pages().indexedSequenceEdit.stepSelection().selected();
}

void IndexedMathPage::exit() {
}

void IndexedMathPage::draw(Canvas &canvas) {
    if (_project.selectedTrack().trackMode() != Track::TrackMode::Indexed) {
        close();
        return;
    }

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "MATH");

    drawMathConfig(canvas, _opA, 16, _activeOp == ActiveOp::A, "A");
    drawMathConfig(canvas, _opB, 36, _activeOp == ActiveOp::B, "B");

    bool shift = pageKeyState()[Key::Shift];
    const char *footerLabels[] = {
        "TARGET",
        "OP",
        "VALUE",
        "GROUPS",
        shift ? "BACK" : (configChanged() ? "COMMIT" : "BACK"),
    };
    WindowPainter::drawFooter(canvas, footerLabels, pageKeyState(), int(_editParam));
}

void IndexedMathPage::updateLeds(Leds &leds) {
    LedPainter::drawSelectedSequenceSection(leds, 0);
}

void IndexedMathPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }

    if (key.isFunction()) {
        int fn = key.function();
        if (fn >= 0 && fn <= 3) {
            _editParam = static_cast<EditParam>(fn);
            event.consume();
            return;
        }

        if (fn == 4) {
            if (key.shiftModifier()) {
                _manager.pop();
            } else if (configChanged()) {
                applyMath(_opA);
                applyMath(_opB);
                resetConfigs();
                showMessage("MATH APPLIED");
            } else {
                _manager.pop();
            }
            event.consume();
            return;
        }
    }

    if (key.isLeft()) {
        _activeOp = ActiveOp::A;
        event.consume();
        return;
    }

    if (key.isRight()) {
        _activeOp = ActiveOp::B;
        event.consume();
        return;
    }
}

void IndexedMathPage::encoder(EncoderEvent &event) {
    auto &cfg = activeConfig();

    switch (_editParam) {
    case EditParam::Target: {
        int target = static_cast<int>(cfg.target);
        target += event.value();
        if (target < 0) {
            target = static_cast<int>(IndexedSequence::ModTarget::Last) - 1;
        }
        if (target >= static_cast<int>(IndexedSequence::ModTarget::Last)) {
            target = 0;
        }
        cfg.target = static_cast<IndexedSequence::ModTarget>(target);
        clampValue(cfg);
        break;
    }
    case EditParam::Operator: {
        int op = static_cast<int>(cfg.op);
        op += event.value();
        if (op < 0) {
            op = static_cast<int>(MathOp::Last) - 1;
        }
        if (op >= static_cast<int>(MathOp::Last)) {
            op = 0;
        }
        cfg.op = static_cast<MathOp>(op);
        clampValue(cfg);
        break;
    }
    case EditParam::Value: {
        bool shift = globalKeyState()[Key::Shift];
        int step = valueStep(cfg, shift);
        int newValue = cfg.value + event.value() * step;
        cfg.value = clamp(newValue, valueMin(cfg), valueMax(cfg));
        break;
    }
    case EditParam::Groups: {
        static const uint8_t groupCycle[] = {
            1, 2, 3, 4,
            5, 6, 7, 8,
            9, 10, 11, 12,
            13, 14, 15,
            IndexedSequence::TargetGroupsUngrouped,
            IndexedSequence::TargetGroupsSelected,
            IndexedSequence::TargetGroupsAll,
        };
        static constexpr int cycleSize = int(sizeof(groupCycle) / sizeof(groupCycle[0]));
        int currentIndex = 0;
        for (int i = 0; i < cycleSize; ++i) {
            if (cfg.targetGroups == groupCycle[i]) {
                currentIndex = i;
                break;
            }
        }
        int nextIndex = (currentIndex + event.value()) % cycleSize;
        if (nextIndex < 0) {
            nextIndex += cycleSize;
        }
        cfg.targetGroups = groupCycle[nextIndex];
        break;
    }
    }

    event.consume();
}

IndexedMathPage::MathConfig &IndexedMathPage::activeConfig() {
    return _activeOp == ActiveOp::A ? _opA : _opB;
}

const IndexedMathPage::MathConfig &IndexedMathPage::activeConfig() const {
    return _activeOp == ActiveOp::A ? _opA : _opB;
}

void IndexedMathPage::drawMathConfig(Canvas &canvas, const MathConfig &cfg, int y, bool active, const char *label) {
    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);

    const int colWidth = CONFIG_LCD_WIDTH / CONFIG_FUNCTION_KEY_COUNT;
    auto drawCentered = [&canvas, y] (int col, const char *text, Color color) {
        int colX = col * (CONFIG_LCD_WIDTH / CONFIG_FUNCTION_KEY_COUNT);
        int textWidth = canvas.textWidth(text);
        int x = colX + ((CONFIG_LCD_WIDTH / CONFIG_FUNCTION_KEY_COUNT) - textWidth) / 2;
        canvas.setColor(color);
        canvas.drawText(x, y, text);
    };

    canvas.setColor(active ? Color::Bright : Color::Medium);
    canvas.drawText(2, y, label);

    drawCentered(0, targetName(cfg.target),
                 (active && _editParam == EditParam::Target) ? Color::Bright : Color::Medium);

    drawCentered(1, opName(cfg.op),
                 (active && _editParam == EditParam::Operator) ? Color::Bright : Color::Medium);

    FixedStringBuilder<16> valueStr;
    formatValue(cfg, valueStr);
    drawCentered(2, valueStr,
                 (active && _editParam == EditParam::Value) ? Color::Bright : Color::Medium);

    Color groupColor = (active && _editParam == EditParam::Groups) ? Color::Bright : Color::Medium;
    drawGroupMask(canvas, cfg.targetGroups, colWidth * 3, y, colWidth, groupColor);

    int affectedSteps = 0;
    const auto &sequence = _project.selectedIndexedSequence();
    for (int i = 0; i < sequence.activeLength(); ++i) {
        const auto &step = sequence.step(i);
        if (matchesGroup(step, cfg.targetGroups, i)) {
            affectedSteps++;
        }
    }
    FixedStringBuilder<8> countStr;
    countStr("N=%d", affectedSteps);
    drawCentered(4, countStr, active ? Color::Bright : Color::Medium);
}

void IndexedMathPage::drawGroupMask(Canvas &canvas, uint8_t groupMask, int x, int y, int width, Color onColor) {
    if (groupMask == IndexedSequence::TargetGroupsUngrouped) {
        const char *ungroupedLabel = "UNGR";
        int textWidth = canvas.textWidth(ungroupedLabel);
        canvas.setColor(onColor);
        canvas.drawText(x + (width - textWidth) / 2, y, ungroupedLabel);
        return;
    }

    if (groupMask == IndexedSequence::TargetGroupsSelected) {
        const char *selectedLabel = "SEL";
        int textWidth = canvas.textWidth(selectedLabel);
        canvas.setColor(onColor);
        canvas.drawText(x + (width - textWidth) / 2, y, selectedLabel);
        return;
    }

    if (groupMask == IndexedSequence::TargetGroupsAll) {
        const char *allLabel = "ALL";
        int textWidth = canvas.textWidth(allLabel);
        canvas.setColor(onColor);
        canvas.drawText(x + (width - textWidth) / 2, y, allLabel);
        return;
    }

    const char* groupLabels[] = {"A", "B", "C", "D"};
    int groupWidth = 4 * 8;
    int startX = x + (width - groupWidth) / 2;
    for (int i = 0; i < 4; ++i) {
        bool inGroup = (groupMask & (1 << i)) != 0;
        canvas.setColor(inGroup ? onColor : Color::Low);
        canvas.drawText(startX + i * 8, y, groupLabels[i]);
    }
}

void IndexedMathPage::applyMath(const MathConfig &cfg) {
    auto &sequence = _project.selectedIndexedSequence();

    for (int i = 0; i < sequence.activeLength(); ++i) {
        auto &step = sequence.step(i);
        if (!matchesGroup(step, cfg.targetGroups, i)) {
            continue;
        }
        applyMathToStep(step, cfg, i, sequence.activeLength());
    }
}

void IndexedMathPage::applyMathToStep(IndexedSequence::Step &step, const MathConfig &cfg, int stepIndex, int stepCount) {
    int range = std::abs(cfg.value);
    int randValue = 0;
    if (range > 0) {
        randValue = int(_rng.nextRange(range + 1));
        if (cfg.op == MathOp::Jitter && _rng.nextBinary()) {
            randValue = -randValue;
        }
    }

    switch (cfg.target) {
    case IndexedSequence::ModTarget::Duration: {
        int duration = step.duration();
        switch (cfg.op) {
        case MathOp::Add:    duration += cfg.value; break;
        case MathOp::Sub:    duration -= cfg.value; break;
        case MathOp::Mul:    duration = duration * cfg.value; break;
        case MathOp::Div:    duration = cfg.value > 0 ? duration / cfg.value : duration; break;
        case MathOp::Set:    duration = cfg.value; break;
        case MathOp::Rand:   duration = randValue; break;
        case MathOp::Jitter: duration += randValue; break;
        case MathOp::Ramp:
            if (stepCount > 1) {
                duration += (cfg.value * stepIndex) / (stepCount - 1);
            }
            break;
        case MathOp::Quant:
            if (cfg.value > 0) {
                int q = cfg.value;
                duration = ((duration + (q / 2)) / q) * q;
            }
            break;
        case MathOp::Last:   break;
        }
        duration = clamp(duration, 0, int(IndexedSequence::MaxDuration));
        step.setDuration(uint16_t(duration));
        break;
    }
    case IndexedSequence::ModTarget::GateLength: {
        int gate = step.gateLength();
        switch (cfg.op) {
        case MathOp::Add:    gate += cfg.value; break;
        case MathOp::Sub:    gate -= cfg.value; break;
        case MathOp::Mul:    gate = gate * cfg.value; break;
        case MathOp::Div:    gate = cfg.value > 0 ? gate / cfg.value : gate; break;
        case MathOp::Set:    gate = cfg.value; break;
        case MathOp::Rand:   gate = randValue; break;
        case MathOp::Jitter: gate += randValue; break;
        case MathOp::Ramp:
            if (stepCount > 1) {
                gate += (cfg.value * stepIndex) / (stepCount - 1);
            }
            break;
        case MathOp::Quant:
            if (cfg.value > 0) {
                int q = cfg.value;
                gate = ((gate + (q / 2)) / q) * q;
            }
            break;
        case MathOp::Last:   break;
        }
        gate = clamp(gate, 0, int(IndexedSequence::GateLengthTrigger));
        step.setGateLength(uint16_t(gate));
        break;
    }
    case IndexedSequence::ModTarget::NoteIndex: {
        int note = step.noteIndex();
        switch (cfg.op) {
        case MathOp::Add:    note += cfg.value; break;
        case MathOp::Sub:    note -= cfg.value; break;
        case MathOp::Mul:    note = note * cfg.value; break;
        case MathOp::Div:    note = cfg.value > 0 ? note / cfg.value : note; break;
        case MathOp::Set:    note = cfg.value; break;
        case MathOp::Rand:   note = randValue; break;
        case MathOp::Jitter: note += randValue; break;
        case MathOp::Ramp:
            if (stepCount > 1) {
                note += (cfg.value * stepIndex) / (stepCount - 1);
            }
            break;
        case MathOp::Quant:
            if (cfg.value > 0) {
                int q = cfg.value;
                if (note >= 0) {
                    note = ((note + (q / 2)) / q) * q;
                } else {
                    note = -(((-note + (q / 2)) / q) * q);
                }
            }
            break;
        case MathOp::Last:   break;
        }
        note = clamp(note, -63, 64);
        step.setNoteIndex(int8_t(note));
        break;
    }
    case IndexedSequence::ModTarget::Last:
        break;
    }
}

bool IndexedMathPage::matchesGroup(const IndexedSequence::Step &step, uint8_t targetGroups, int stepIndex) const {
    if (targetGroups == IndexedSequence::TargetGroupsAll) {
        return true;
    }
    if (targetGroups == IndexedSequence::TargetGroupsUngrouped) {
        return step.groupMask() == 0;
    }
    if (targetGroups == IndexedSequence::TargetGroupsSelected) {
        return stepIndex >= 0 && stepIndex < int(_selectedSteps.size()) && _selectedSteps[stepIndex];
    }
    return (step.groupMask() & targetGroups) != 0;
}

bool IndexedMathPage::configChanged() const {
    auto equal = [] (const MathConfig &a, const MathConfig &b) {
        return a.target == b.target &&
            a.op == b.op &&
            a.value == b.value &&
            a.targetGroups == b.targetGroups;
    };
    return !equal(_opA, _opABase) || !equal(_opB, _opBBase);
}

void IndexedMathPage::resetConfigs() {
    _opA = MathConfig{};
    _opB = MathConfig{};
    _opABase = _opA;
    _opBBase = _opB;
    _activeOp = ActiveOp::A;
    _editParam = EditParam::Target;
}

int IndexedMathPage::valueMin(const MathConfig &cfg) const {
    switch (cfg.op) {
    case MathOp::Set:
        if (cfg.target == IndexedSequence::ModTarget::NoteIndex) {
            return -63;
        }
        return 0;
    case MathOp::Mul:
        return 0;
    case MathOp::Div:
        return 1;
    case MathOp::Quant:
        return 1;
    case MathOp::Ramp:
        switch (cfg.target) {
        case IndexedSequence::ModTarget::Duration:
            return -int(IndexedSequence::MaxDuration);
        case IndexedSequence::ModTarget::GateLength:
            return -100;
        case IndexedSequence::ModTarget::NoteIndex:
            return -64;
        case IndexedSequence::ModTarget::Last:
            break;
        }
        break;
    default:
        return 0;
    }
    return 0;
}

int IndexedMathPage::valueMax(const MathConfig &cfg) const {
    switch (cfg.op) {
    case MathOp::Mul:
    case MathOp::Div:
        return kMulDivMax;
    case MathOp::Quant:
    case MathOp::Ramp:
        switch (cfg.target) {
        case IndexedSequence::ModTarget::Duration:
            return int(IndexedSequence::MaxDuration);
        case IndexedSequence::ModTarget::GateLength:
            return IndexedSequence::GateLengthTrigger;
        case IndexedSequence::ModTarget::NoteIndex:
            return 64;
        case IndexedSequence::ModTarget::Last:
            break;
        }
        break;
    case MathOp::Set:
    case MathOp::Add:
    case MathOp::Sub:
    case MathOp::Rand:
    case MathOp::Jitter:
        switch (cfg.target) {
        case IndexedSequence::ModTarget::Duration:
            return int(IndexedSequence::MaxDuration);
        case IndexedSequence::ModTarget::GateLength:
            return IndexedSequence::GateLengthTrigger;
        case IndexedSequence::ModTarget::NoteIndex:
            return 64;
        case IndexedSequence::ModTarget::Last:
            break;
        }
        break;
    case MathOp::Last:
        break;
    }
    return 0;
}

int IndexedMathPage::valueStep(const MathConfig &cfg, bool shift) const {
    if (cfg.op == MathOp::Mul || cfg.op == MathOp::Div) {
        return shift ? 10 : 1;
    }

    switch (cfg.target) {
    case IndexedSequence::ModTarget::Duration:
        return shift ? _project.selectedIndexedSequence().divisor() : 1;
    case IndexedSequence::ModTarget::GateLength:
        return shift ? 5 : 1;
    case IndexedSequence::ModTarget::NoteIndex:
        return shift ? 12 : 1;
    case IndexedSequence::ModTarget::Last:
        break;
    }
    return 1;
}

void IndexedMathPage::clampValue(MathConfig &cfg) const {
    cfg.value = clamp(cfg.value, valueMin(cfg), valueMax(cfg));
}

void IndexedMathPage::formatValue(const MathConfig &cfg, StringBuilder &str) const {
    switch (cfg.op) {
    case MathOp::Set:
        if (cfg.target == IndexedSequence::ModTarget::GateLength &&
            cfg.value == IndexedSequence::GateLengthTrigger) {
            str("T");
        } else {
            str("%d", cfg.value);
        }
        break;
    case MathOp::Mul:
    case MathOp::Div:
        str("%d", cfg.value);
        break;
    default:
        str("%d", cfg.value);
        break;
    }
}
const char *IndexedMathPage::opName(MathOp op) {
    switch (op) {
    case MathOp::Add:    return "ADD";
    case MathOp::Sub:    return "SUB";
    case MathOp::Mul:    return "MUL";
    case MathOp::Div:    return "DIV";
    case MathOp::Set:    return "SET";
    case MathOp::Rand:   return "RAND";
    case MathOp::Jitter: return "JIT";
    case MathOp::Ramp:   return "RAMP";
    case MathOp::Quant:  return "QNT";
    case MathOp::Last:   break;
    }
    return "?";
}
