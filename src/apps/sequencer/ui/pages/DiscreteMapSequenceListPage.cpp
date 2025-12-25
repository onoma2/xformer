#include "DiscreteMapSequenceListPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"

#include "engine/DiscreteMapTrackEngine.h"

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
    { "COPY" },
    { "PASTE" },
    { "ROUTE" },
};

DiscreteMapSequenceListPage::DiscreteMapSequenceListPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{}

void DiscreteMapSequenceListPage::enter() {
    _listModel.setSequence(&_project.selectedDiscreteMapSequence());
}

void DiscreteMapSequenceListPage::exit() {
    _listModel.setSequence(nullptr);
}

void DiscreteMapSequenceListPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "SEQUENCE");
    WindowPainter::drawActiveFunction(canvas, Track::trackModeName(_project.selectedTrack().trackMode()));
    WindowPainter::drawFooter(canvas);

    ListPage::draw(canvas);
}

void DiscreteMapSequenceListPage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}

void DiscreteMapSequenceListPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.pageModifier()) {
        return;
    }

    if (!event.consumed()) {
        ListPage::keyPress(event);
    }
}

void DiscreteMapSequenceListPage::contextShow() {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); }
    ));
}

void DiscreteMapSequenceListPage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Init:
        initSequence();
        break;
    case ContextAction::Copy:
        copySequence();
        break;
    case ContextAction::Paste:
        pasteSequence();
        break;
    case ContextAction::Route:
        initRoute();
        break;
    case ContextAction::Last:
        break;
    }
}

bool DiscreteMapSequenceListPage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    case ContextAction::Paste:
        return _model.clipBoard().canPasteDiscreteMapSequence();
    case ContextAction::Route:
        return _listModel.routingTarget(selectedRow()) != Routing::Target::None;
    default:
        return true;
    }
}

void DiscreteMapSequenceListPage::initSequence() {
    _project.selectedDiscreteMapSequence().clear();
    invalidateThresholds();
    showMessage("SEQUENCE INITIALIZED");
}

void DiscreteMapSequenceListPage::copySequence() {
    _model.clipBoard().copyDiscreteMapSequence(_project.selectedDiscreteMapSequence());
    showMessage("COPIED");
}

void DiscreteMapSequenceListPage::pasteSequence() {
    _model.clipBoard().pasteDiscreteMapSequence(_project.selectedDiscreteMapSequence());
    invalidateThresholds();
    showMessage("PASTED");
}

void DiscreteMapSequenceListPage::initRoute() {
    _manager.pages().top.editRoute(_listModel.routingTarget(selectedRow()), _project.selectedTrackIndex());
}

void DiscreteMapSequenceListPage::invalidateThresholds() {
    auto &trackEngine = _engine.trackEngine(_project.selectedTrackIndex());
    if (trackEngine.trackMode() == Track::TrackMode::DiscreteMap) {
        static_cast<DiscreteMapTrackEngine &>(trackEngine).invalidateThresholds();
    }
}
