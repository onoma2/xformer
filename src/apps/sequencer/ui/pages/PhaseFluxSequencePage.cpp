#include "PhaseFluxSequencePage.h"

#include "Pages.h"

#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"

enum class ContextAction {
    Init,
    Route,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
    { "ROUTE" },
};

PhaseFluxSequencePage::PhaseFluxSequencePage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{}

void PhaseFluxSequencePage::enter() {
    _listModel.setSequence(&_project.selectedPhaseFluxSequence());
}

void PhaseFluxSequencePage::exit() {
}

void PhaseFluxSequencePage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "SEQUENCE");
    WindowPainter::drawActiveFunction(canvas, Track::trackModeName(_project.selectedTrack().trackMode()));
    WindowPainter::drawFooter(canvas);
    ListPage::draw(canvas);
}

void PhaseFluxSequencePage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}

void PhaseFluxSequencePage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

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

    if (key.pageModifier()) return;

    if (!event.consumed()) {
        ListPage::keyPress(event);
    }
}

void PhaseFluxSequencePage::contextShow(bool doubleClick) {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); },
        doubleClick
    ));
}

void PhaseFluxSequencePage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Init:  initSequence(); break;
    case ContextAction::Route: initRoute(); break;
    case ContextAction::Last:  break;
    }
}

bool PhaseFluxSequencePage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    case ContextAction::Route:
        return _listModel.routingTarget(selectedRow()) != Routing::Target::None;
    default:
        return true;
    }
}

void PhaseFluxSequencePage::initSequence() {
    _project.selectedPhaseFluxSequence().clear();
    showMessage("SEQUENCE INITIALIZED");
}

void PhaseFluxSequencePage::initRoute() {
    _manager.pages().top.editRoute(_listModel.routingTarget(selectedRow()), _project.selectedTrackIndex());
}
