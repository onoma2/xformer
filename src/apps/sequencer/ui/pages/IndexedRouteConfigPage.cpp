#include "IndexedRouteConfigPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"

#include "model/Track.h"
#include <cmath>

namespace {
bool routeConfigEqual(const IndexedSequence::RouteConfig &a, const IndexedSequence::RouteConfig &b) {
    return a.targetGroups == b.targetGroups &&
        a.targetParam == b.targetParam &&
        std::fabs(a.amount - b.amount) < 0.0001f &&
        a.enabled == b.enabled;
}
} // namespace

IndexedRouteConfigPage::IndexedRouteConfigPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{
}

void IndexedRouteConfigPage::enter() {
    _activeRoute = ActiveRoute::RouteA;
    _editParam = EditParam::Enabled;
    const auto &sequence = _project.selectedIndexedSequence();
    _routeAStaged = sequence.routeA();
    _routeBStaged = sequence.routeB();
}

void IndexedRouteConfigPage::exit() {
}

void IndexedRouteConfigPage::draw(Canvas &canvas) {
    if (_project.selectedTrack().trackMode() != Track::TrackMode::Indexed) {
        close();
        return;
    }

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "ROUTE CONFIG");

    // Draw Route A
    drawRouteConfig(canvas, _routeAStaged, 16, _activeRoute == ActiveRoute::RouteA, "A");

    // Draw Route B
    drawRouteConfig(canvas, _routeBStaged, 36, _activeRoute == ActiveRoute::RouteB, "B");

    // Footer: F1-F4 for parameter selection, F5 to exit
    const char *footerLabels[] = { "ENABLE", "GROUPS", "TARGET", "AMOUNT", stagedChanged() ? "COMMIT" : "BACK" };
    WindowPainter::drawFooter(canvas, footerLabels, pageKeyState(), (int)_editParam);
}

void IndexedRouteConfigPage::drawRouteConfig(Canvas &canvas, const IndexedSequence::RouteConfig &cfg, int y, bool active, const char *label) {
    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);

    const int colWidth = CONFIG_LCD_WIDTH / CONFIG_FUNCTION_KEY_COUNT;
    auto drawCentered = [&canvas, colWidth, y] (int col, const char *text, Color color) {
        int colX = col * colWidth;
        int textWidth = canvas.textWidth(text);
        int x = colX + (colWidth - textWidth) / 2;
        canvas.setColor(color);
        canvas.drawText(x, y, text);
    };

    // Route label
    canvas.setColor(active ? Color::Bright : Color::Medium);
    canvas.drawText(2, y, label);

    // Enabled status
    drawCentered(0, cfg.enabled ? "ON" : "OFF",
                 (_editParam == EditParam::Enabled && active) ? Color::Bright : Color::Medium);

    if (!cfg.enabled) {
        // Don't show other params if disabled
        return;
    }

    // Target Groups
    drawGroupMask(canvas, cfg.targetGroups, colWidth, y, colWidth);

    // Target Parameter
    const char* targetName = "?";
    switch (cfg.targetParam) {
    case IndexedSequence::ModTarget::Duration:   targetName = "DUR"; break;
    case IndexedSequence::ModTarget::GateLength: targetName = "GATE"; break;
    case IndexedSequence::ModTarget::NoteIndex:  targetName = "NOTE"; break;
    case IndexedSequence::ModTarget::Last:       break;
    }
    drawCentered(2, targetName,
                 (_editParam == EditParam::TargetParam && active) ? Color::Bright : Color::Medium);

    // Amount
    FixedStringBuilder<16> amountStr;
    amountStr("%+.0f%%", cfg.amount);
    drawCentered(3, amountStr,
                 (_editParam == EditParam::Amount && active) ? Color::Bright : Color::Medium);
}

void IndexedRouteConfigPage::drawGroupMask(Canvas &canvas, uint8_t groupMask, int x, int y, int width) {
    if (groupMask == IndexedSequence::TargetGroupsUngrouped) {
        const char *ungroupedLabel = "UNGR";
        int textWidth = canvas.textWidth(ungroupedLabel);
        canvas.setColor(Color::Bright);
        canvas.drawText(x + (width - textWidth) / 2, y, ungroupedLabel);
        return;
    }

    if (groupMask == IndexedSequence::TargetGroupsAll) {
        const char *allLabel = "ALL";
        int textWidth = canvas.textWidth(allLabel);
        canvas.setColor(Color::Bright);
        canvas.drawText(x + (width - textWidth) / 2, y, allLabel);
        return;
    }

    const char* groupLabels[] = {"A", "B", "C", "D"};
    int groupWidth = 4 * 8;
    int startX = x + (width - groupWidth) / 2;
    for (int i = 0; i < 4; ++i) {
        bool inGroup = (groupMask & (1 << i)) != 0;
        canvas.setColor(inGroup ? Color::Bright : Color::Low);
        canvas.drawText(startX + i * 8, y, groupLabels[i]);
    }
}

void IndexedRouteConfigPage::updateLeds(Leds &leds) {
    LedPainter::drawSelectedSequenceSection(leds, 0);
}

void IndexedRouteConfigPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }

    // F1-F4: Select parameter to edit
    if (key.isFunction()) {
        int fn = key.function();
        if (fn >= 0 && fn <= 3) {
            _editParam = static_cast<EditParam>(fn);
        }

        // F5: Commit changes (if any) or go back
        if (fn == 4) {
            if (stagedChanged()) {
                auto &sequence = _project.selectedIndexedSequence();
                sequence.setRouteA(_routeAStaged);
                sequence.setRouteB(_routeBStaged);
                showMessage("ROUTE UPDATED");
            } else {
                _manager.pop();
            }
        }

        event.consume();
        return;
    }

    // Left/Right: Switch between Route A and Route B
    if (key.isLeft()) {
        _activeRoute = ActiveRoute::RouteA;
        event.consume();
        return;
    }
    if (key.isRight()) {
        _activeRoute = ActiveRoute::RouteB;
        event.consume();
        return;
    }
}

void IndexedRouteConfigPage::encoder(EncoderEvent &event) {
    auto &cfg = activeRouteConfig();

    switch (_editParam) {
    case EditParam::Enabled:
        cfg.enabled = !cfg.enabled;
        break;

    case EditParam::TargetGroups: {
        static const uint8_t groupCycle[] = {
            1, 2, 3, 4,
            5, 6, 7, 8,
            9, 10, 11, 12,
            13, 14, 15,
            IndexedSequence::TargetGroupsUngrouped,
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

    case EditParam::TargetParam: {
        int param = static_cast<int>(cfg.targetParam);
        param += event.value();
        if (param < 0) param = static_cast<int>(IndexedSequence::ModTarget::Last) - 1;
        if (param >= static_cast<int>(IndexedSequence::ModTarget::Last)) param = 0;
        cfg.targetParam = static_cast<IndexedSequence::ModTarget>(param);
        break;
    }

    case EditParam::Amount: {
        bool shift = globalKeyState()[Key::Shift];
        float step = shift ? 1.0f : 10.0f;
        cfg.amount = clamp(cfg.amount + event.value() * step, -200.0f, 200.0f);
        break;
    }
    }

    event.consume();
}

IndexedSequence::RouteConfig& IndexedRouteConfigPage::activeRouteConfig() {
    if (_activeRoute == ActiveRoute::RouteA) {
        return _routeAStaged;
    } else {
        return _routeBStaged;
    }
}

const IndexedSequence::RouteConfig& IndexedRouteConfigPage::activeRouteConfig() const {
    if (_activeRoute == ActiveRoute::RouteA) {
        return _routeAStaged;
    } else {
        return _routeBStaged;
    }
}

bool IndexedRouteConfigPage::stagedChanged() const {
    const auto &sequence = _project.selectedIndexedSequence();
    return !routeConfigEqual(_routeAStaged, sequence.routeA()) ||
        !routeConfigEqual(_routeBStaged, sequence.routeB());
}
