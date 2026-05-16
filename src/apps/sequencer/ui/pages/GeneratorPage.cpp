#include "GeneratorPage.h"

#include "ui/painters/WindowPainter.h"
#include "ui/LedPainter.h"
#include "ui/pages/Pages.h"

#include "engine/generators/Generator.h"
#include "engine/generators/EuclideanGenerator.h"
#include "engine/generators/RandomGenerator.h"

#include "engine/Engine.h"
#include "core/math/Math.h"

#include <cstddef>

void GeneratorContextQuickEditModel::configure(Generator *generator, int paramIndex, const char *label, const std::function<void()> &onEdited) {
    _generator = generator;
    _paramIndex = paramIndex;
    _label = label;
    _onEdited = onEdited;
}

int GeneratorContextQuickEditModel::rows() const {
    return 1;
}

int GeneratorContextQuickEditModel::columns() const {
    return 2;
}

void GeneratorContextQuickEditModel::cell(int row, int column, StringBuilder &str) const {
    if (row != 0 || !_generator) {
        return;
    }
    if (column == 0) {
        str("%s", _label);
    } else if (column == 1) {
        _generator->printParam(_paramIndex, str);
    }
}

void GeneratorContextQuickEditModel::edit(int row, int column, int value, bool shift) {
    if (row != 0 || column != 1 || !_generator) {
        return;
    }
    _generator->editParam(_paramIndex, value, shift);
    if (_onEdited) {
        _onEdited();
    }
}

void GeneratorContextQuickEditModel::setSelectedScale(int, bool) {}

static GeneratorContextQuickEditModel gGeneratorContextQuickEditModel;

enum class ContextAction {
    RandomizeSeed,
    Init,
    Revert,
    Commit,
    VariationInfo,
    Last
};

static bool seedDrivenGenerator(Generator::Mode mode) {
    return mode == Generator::Mode::Random;
}

static bool abPreviewGenerator(Generator::Mode mode) {
    return seedDrivenGenerator(mode) || mode == Generator::Mode::Euclidean;
}

static int paramIndexForFunction(Generator::Mode mode, int functionIndex) {
    if (mode == Generator::Mode::Random) {
        switch (functionIndex) {
        case 1: return int(RandomGenerator::Param::Variation);
        case 2: return int(RandomGenerator::Param::Scale);
        case 3: return int(RandomGenerator::Param::Bias);
        default: return -1;
        }
    }

    if (mode == Generator::Mode::Euclidean) {
        switch (functionIndex) {
        case 1: return int(EuclideanGenerator::Param::Offset);
        case 2: return int(EuclideanGenerator::Param::Steps);
        case 3: return int(EuclideanGenerator::Param::Beats);
        default: return -1;
        }
    }

    return functionIndex;
}

GeneratorPage::GeneratorPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void GeneratorPage::show(Generator *generator, StepSelection<CONFIG_STEP_COUNT> *stepSelection) {
    _generator = generator;
    _stepSelection = stepSelection;
    _previewArmed = false;
    _applied = false;
    _boundTrackIndex = _project.selectedTrackIndex();
    _boundTrackMode = _project.selectedTrack().trackMode();

    BasePage::show();
}

bool GeneratorPage::boundTrackContextValid() const {
    return _project.selectedTrackIndex() == _boundTrackIndex &&
           _project.selectedTrack().trackMode() == _boundTrackMode;
}

bool GeneratorPage::ensureBoundTrackContext() {
    if (boundTrackContextValid()) {
        return true;
    }
    revert();
    showMessage("GEN CANCELED");
    return false;
}

void GeneratorPage::enter() {
    _valueRange.first = 0;
    _valueRange.second = 7;
    _previewArmed = false;
    _section = 0;

    // Enter in ORIGINAL state — first preview is created only on explicit reroll action
    _generator->revert();
    _generator->showOriginal();
}

void GeneratorPage::exit() {
    if (!_applied) {
        _generator->revert();
    }
}

void GeneratorPage::draw(Canvas &canvas) {
    if (!_generator) {
        return;
    }

    const char *activeFunction = _generator->name();
    const char *functionNames[5];
    for (int i = 0; i < 5; ++i) {
        functionNames[i] = nullptr;
    }

    if (_generator->mode() == Generator::Mode::Random) {
        functionNames[0] = "A/B";
        functionNames[1] = "VAR";
        functionNames[2] = "RANGE";
        functionNames[3] = "BIAS";
        functionNames[4] = "NEW RAND";
    } else if (_generator->mode() == Generator::Mode::Euclidean) {
        functionNames[0] = "A/B";
        functionNames[1] = "OFFSET";
        functionNames[2] = "STEPS";
        functionNames[3] = "BEATS";
        functionNames[4] = "NEW EUCL";
    } else {
        for (int i = 0; i < 5; ++i) {
            functionNames[i] = i < _generator->paramCount() ? _generator->paramName(i) : nullptr;
        }
    }

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "GENERATOR");
    WindowPainter::drawActiveFunction(canvas, activeFunction);
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState());

    canvas.setFont(Font::Small);
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(Color::Bright);

    auto drawValue = [&] (int index, const char *str, bool tiny = false) {
        Font prevFont = canvas.font();
        Color color = Color::Bright;
        Font font = tiny ? Font::Tiny : Font::Small;

        if (tiny && !_generator->showingPreview()) {
            color = Color::Medium;
        }

        canvas.setFont(font);
        canvas.setColor(color);

        int w = Width / 5;
        int x = (Width * index) / 5;
        int y = Height - 16;
        canvas.drawText(x + (w - canvas.textWidth(str)) / 2, y, str);

        canvas.setColor(Color::Bright);
        canvas.setFont(prevFont);
    };

    auto drawParamValue = [&] (int footerIndex, int paramIndex, bool tiny = false) {
        if (paramIndex < 0 || paramIndex >= _generator->paramCount()) {
            return;
        }

        FixedStringBuilder<16> str;
        if (seedDrivenGenerator(_generator->mode()) && paramIndex == 0 && !_generator->showingPreview()) {
            str("ORIGINAL");
        } else {
            _generator->printParam(paramIndex, str);
        }
        drawValue(footerIndex, str, tiny);
    };

    if (_generator->mode() == Generator::Mode::Random) {
        drawParamValue(0, int(RandomGenerator::Param::Seed), true);
        drawParamValue(1, int(RandomGenerator::Param::Variation));
        drawParamValue(2, int(RandomGenerator::Param::Scale));
        drawParamValue(3, int(RandomGenerator::Param::Bias));
    } else if (_generator->mode() == Generator::Mode::Euclidean) {
        drawValue(0, _generator->showingPreview() ? "CURRENT" : "ORIGINAL", true);
        drawParamValue(1, int(EuclideanGenerator::Param::Offset));
        drawParamValue(2, int(EuclideanGenerator::Param::Steps));
        drawParamValue(3, int(EuclideanGenerator::Param::Beats));
    } else {
        for (int i = 0; i < _generator->paramCount(); ++i) {
            drawParamValue(i, i);
        }
    }

    switch (_generator->mode()) {
    case Generator::Mode::InitLayer:
    case Generator::Mode::InitSteps:
        // no page
        break;
    case Generator::Mode::Euclidean:
        drawEuclideanGenerator(canvas, *static_cast<const EuclideanGenerator *>(_generator));
        break;
    case Generator::Mode::Random:
        drawRandomGenerator(canvas, *static_cast<const RandomGenerator *>(_generator));
        break;
    case Generator::Mode::Last:
        break;
    }
}

void GeneratorPage::updateLeds(Leds &leds) {
    if (!_generator) {
        return;
    }

    // value range
    for (int i = 0; i < 8; ++i) {
        bool inRange = (i >= _valueRange.first && i <= _valueRange.second) || (i >= _valueRange.second && i <= _valueRange.first);
        bool inverted = _valueRange.first > _valueRange.second;
        leds.set(MatrixMap::toStep(i), inRange && inverted, inRange && !inverted);
    }
    for (int i = 0; i < 7; ++i) {
        leds.set(MatrixMap::toStep(8 + i), false, false);
    }

    if (_stepSelection) {
        // Highlight selected steps in the current bank
        for (int i = 0; i < StepCount; ++i) {
            int stepIndex = stepOffset() + i;
            if (stepIndex < CONFIG_STEP_COUNT && _stepSelection->selected()[stepIndex]) {
                leds.set(MatrixMap::toStep(i), true, true);
            }
        }
    }

    LedPainter::drawSelectedSequenceSection(leds, _section);
}

void GeneratorPage::keyDown(KeyEvent &event) {
    const auto &key = event.key();

    if (key.isGlobal()) {
        return;
    }

    if (_stepSelection && key.isStep()) {
        _stepSelection->keyDown(event, stepOffset());
    }

    event.consume();
}

void GeneratorPage::keyUp(KeyEvent &event) {
    const auto &key = event.key();

    if (key.isGlobal()) {
        return;
    }

    if (_stepSelection && key.isStep()) {
        _stepSelection->keyUp(event, stepOffset());
    }

    event.consume();
}

void GeneratorPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (!ensureBoundTrackContext()) {
        event.consume();
        return;
    }

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.pageModifier() && event.count() == 2) {
        contextShow(true);
        event.consume();
        return;
    }

    if (key.isGlobal()) {
        return;
    }

    // Step + Shift triggers re-roll (for seed-driven generators)
    if (key.isStep() && key.shiftModifier()) {
        if (abPreviewGenerator(_generator->mode())) {
            _previewArmed = true;
        }
        if (_generator->mode() == Generator::Mode::Random) {
            static_cast<RandomGenerator *>(_generator)->randomizeContextParams();
            _generator->update();
        } else {
            _generator->randomizeParams();
            _generator->update();
        }
        if (abPreviewGenerator(_generator->mode())) {
            _generator->showPreview();
        }
        event.consume();
        return;
    }

    // F0 toggles A/B preview for seed-driven generators
    if (key.isFunction() && key.function() == 0) {
        if (abPreviewGenerator(_generator->mode()) && !_previewArmed && !_generator->showingPreview()) {
            // Block toggle if preview was never armed (user didn't generate yet)
            event.consume();
            return;
        }
        togglePreview();
        event.consume();
        return;
    }

    // F4 triggers re-roll (NEW RAND)
    if (key.isFunction() && key.function() == 4) {
        if (abPreviewGenerator(_generator->mode())) {
            _previewArmed = true;
        }
        if (_generator->mode() == Generator::Mode::Random) {
            auto *random = static_cast<RandomGenerator *>(_generator);
            random->randomizeParams();
            random->update();
        } else {
            _generator->randomizeParams();
            _generator->update();
        }
        if (abPreviewGenerator(_generator->mode())) {
            _generator->showPreview();
        }
        event.consume();
        return;
    }

    // Step buttons still work for value range selection
    if (key.isStep()) {
        int secondStep = key.step();
        if (secondStep < 8) {
            int count = 0;
            int firstStep = -1;
            for (int step = 0; step < 8; ++step) {
                if (key.state()[MatrixMap::fromStep(step)]) {
                    ++count;
                    if (step != secondStep) {
                        firstStep = step;
                    }
                }
            }
            if (count == 2) {
                _valueRange.first = firstStep;
                _valueRange.second = secondStep;
            }
        }
        event.consume();
        return;
    }

    // Section navigation with Left/Right buttons
    if (key.isLeft()) {
        _section = (_section + 3) % 4;
        event.consume();
        return;
    }
    if (key.isRight()) {
        _section = (_section + 1) % 4;
        event.consume();
        return;
    }

    event.consume();
}

void GeneratorPage::togglePreview() {
    if (!ensureBoundTrackContext()) {
        return;
    }

    if (_generator->showingPreview()) {
        _generator->showOriginal();
        showMessage("ORIGINAL");
    } else {
        _generator->showPreview();
        if (_generator->showingPreview()) {
            showMessage("PREVIEW");
        }
    }
}

void GeneratorPage::encoder(EncoderEvent &event) {
    if (!ensureBoundTrackContext()) {
        return;
    }

    bool changed = false;
    bool rerollTriggered = false;

    for (int functionIndex = 0; functionIndex < 5; ++functionIndex) {
        int paramIndex = paramIndexForFunction(_generator->mode(), functionIndex);
        if (paramIndex >= 0 && pageKeyState()[Key::F0 + functionIndex]) {
            _generator->editParam(paramIndex, event.value(), event.pressed());
            changed = true;
        }
    }

    if (_generator->mode() == Generator::Mode::Random && !pageKeyState()[Key::F0] && !pageKeyState()[Key::F1] && !pageKeyState()[Key::F2] && !pageKeyState()[Key::F3] && !pageKeyState()[Key::F4] && event.pressed()) {
        auto *random = static_cast<RandomGenerator *>(_generator);
        random->randomizeParams();
        changed = true;
        rerollTriggered = true;
    }

    if (changed) {
        if (rerollTriggered && abPreviewGenerator(_generator->mode())) {
            _previewArmed = true;
        }
        _generator->update();
        if (rerollTriggered && abPreviewGenerator(_generator->mode())) {
            _generator->showPreview();
        } else if (!abPreviewGenerator(_generator->mode()) || _generator->showingPreview()) {
            _generator->showPreview();
        }
    }
}

void GeneratorPage::drawEuclideanGenerator(Canvas &canvas, const EuclideanGenerator &generator) const {
    auto steps = generator.steps();
    const auto &pattern = generator.pattern();

    int stepWidth = Width / steps;
    int stepHeight = 8;
    int x = (Width - steps * stepWidth) / 2;
    int y = Height / 2 - stepHeight / 2;

    for (int i = 0; i < steps; ++i) {
        canvas.setColor(Color::Medium);
        canvas.drawRect(x + 1, y + 1, stepWidth - 2, stepHeight - 2);
        if (pattern[i]) {
            canvas.setColor(Color::Bright);
            canvas.fillRect(x + 1, y + 1, stepWidth - 2, stepHeight - 2);
        }
        x += stepWidth;
    }
}

void GeneratorPage::drawRandomGenerator(Canvas &canvas, const RandomGenerator &generator) const {
    const auto &pattern = generator.pattern();
    int steps = pattern.size();

    int stepWidth = Width / steps;
    int stepHeight = 16;
    int x = (Width - steps * stepWidth) / 2;
    int y = 16;

    for (int i = 0; i < steps; ++i) {
        int h = stepHeight - 2;
        int h2 = (h * pattern[i]) / 255;
        canvas.setColor(Color::Low);
        canvas.drawRect(x + 1, y + 1, stepWidth - 2, h);
        canvas.setColor(Color::Bright);
        canvas.hline(x + 1, y + 1 + h - h2, stepWidth - 2);
        // canvas.fillRect(x + 1, y + 1 + h - h2 , stepWidth - 2, h2);
        x += stepWidth;
    }
}

void GeneratorPage::contextShow(bool doubleClick) {
    if (_generator->mode() == Generator::Mode::Random) {
        const auto *random = static_cast<const RandomGenerator *>(_generator);
        snprintf(_contextMenuAuxLabel, sizeof(_contextMenuAuxLabel), "SMOOTH %d", random->smooth());
        _contextMenuItems[0] = { "" };
        _contextMenuItems[1] = { _contextMenuAuxLabel };
        _contextMenuItems[2] = { "REGEN" };
        _contextMenuItems[3] = { "CANCEL" };
        _contextMenuItems[4] = { "COMMIT" };
    } else {
        _contextMenuItems[0] = { "NEW RAND" };
        _contextMenuItems[1] = { "RESEED" };
        _contextMenuItems[2] = { "REVERT" };
        _contextMenuItems[3] = { "COMMIT" };
        _contextMenuItems[4] = { "" };
    }

    showContextMenu(ContextMenu(
        _contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); },
        doubleClick
    ));
}

void GeneratorPage::contextAction(int index) {
    if (!ensureBoundTrackContext()) {
        return;
    }

    if (_generator->mode() == Generator::Mode::Random) {
        switch (index) {
        case 1:
            gGeneratorContextQuickEditModel.configure(_generator, int(RandomGenerator::Param::Smooth), "SMOOTH", [&] {
                _generator->update();
                if (abPreviewGenerator(_generator->mode()) && _generator->showingPreview()) {
                    _generator->showPreview();
                }
            });
            _manager.pages().quickEdit.show(gGeneratorContextQuickEditModel, 0);
            return;
        case 2:
            init();
            break;
        case 3:
            revert();
            break;
        case 4:
            commit();
            break;
        default:
            break;
        }
        return;
    }

    // Non-Random modes: NEW RAND, RESEED, REVERT, COMMIT
    switch (index) {
    case 0: // NEW RAND
        if (_generator->mode() == Generator::Mode::Random) {
            auto *random = static_cast<RandomGenerator *>(_generator);
            random->randomizeParams();
            random->update();
            if (abPreviewGenerator(_generator->mode())) {
                _previewArmed = true;
                _generator->showPreview();
            }
        }
        break;
    case 1: // RESEED
        if (_generator->mode() == Generator::Mode::Random) {
            auto *random = static_cast<RandomGenerator *>(_generator);
            random->randomizeSeed();
            random->update();
            if (abPreviewGenerator(_generator->mode())) {
                _previewArmed = true;
                _generator->showPreview();
            }
        }
        break;
    case 2: // REVERT
        revert();
        break;
    case 3: // COMMIT
        commit();
        break;
    default:
        break;
    }
}

bool GeneratorPage::contextActionEnabled(int index) const {
    if (index >= int(ContextAction::Last)) {
        return false;
    }

    if (_generator->mode() == Generator::Mode::Random) {
        // Empty slot at index 0, SMOOTH at 1, RESETGEN at 2, CANCEL at 3, COMMIT at 4
        return index >= 1 && index <= 4;
    }

    return index >= 0 && index <= 3;
}

void GeneratorPage::init() {
    if (!ensureBoundTrackContext()) {
        return;
    }

    if (_stepSelection) {
        _stepSelection->clear();
    }
    _generator->init();
    _previewArmed = false;
    _generator->showOriginal();
}

void GeneratorPage::revert() {
    if (_stepSelection) {
        _stepSelection->clear();
    }
    _generator->revert();
    _applied = false;
    close();
}

void GeneratorPage::commit() {
    if (!ensureBoundTrackContext()) {
        return;
    }

    if (abPreviewGenerator(_generator->mode()) && !_previewArmed && !_generator->showingPreview()) {
        // Allow commit even if preview wasn't armed — user made changes
        _applied = true;
        close();
        return;
    }

    if (_stepSelection) {
        _stepSelection->clear();
    }
    _generator->apply();
    _applied = true;
    close();
}
