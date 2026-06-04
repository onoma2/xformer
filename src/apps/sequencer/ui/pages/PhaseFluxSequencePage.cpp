#include "PhaseFluxSequencePage.h"

#include "Pages.h"

#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"

#include "model/RouteDraft.h"
#include "model/RouteFork.h"
#include "model/RouteParam.h"

enum class ContextAction {
    Init,
    Route,
    Last
};

static const ContextMenuModel::Item contextMenuItemsModAdd[] = {
    { "INIT" },
    { "MOD+" },
};

static const ContextMenuModel::Item contextMenuItemsModRemove[] = {
    { "INIT" },
    { "MOD-" },
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
    auto target = _listModel.routingTarget(selectedRow());
    int track = _project.selectedTrackIndex();
    bool modulated = RouteDraft::isTrackModulated(_project.routing(), target, track);
    const auto &items = modulated ? contextMenuItemsModRemove : contextMenuItemsModAdd;
    showContextMenu(ContextMenu(
        items,
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
    case ContextAction::Route: {
        auto target = _listModel.routingTarget(selectedRow());
        if (target == Routing::Target::None) return false;
        uint8_t key; RouteParam::Range range;
        return RouteFork::migrated(_project.track(_project.selectedTrackIndex()).trackMode(), target, key, range)
            || RouteFork::migratedGlobal(target, key, range);
    }
    default:
        return true;
    }
}

void PhaseFluxSequencePage::initSequence() {
    _project.selectedPhaseFluxSequence().clear();
    showMessage("SEQUENCE INITIALIZED");
}

void PhaseFluxSequencePage::initRoute() {
    _manager.pages().top.toggleModulation(_listModel.routingTarget(selectedRow()), _project.selectedTrackIndex());
}
