#include "PhaseFluxSequencePage.h"

#include "Pages.h"

#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"

#include "model/RouteDraft.h"
#include "model/RouteResolve.h"
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

    if (handleContextMenuKey(event)) return;

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
        return RouteResolve::overrideParam(_project.track(_project.selectedTrackIndex()).trackMode(), target, key, range)
            || RouteResolve::overrideParamGlobal(target, key, range);
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
    Routing::Target target = _listModel.routingTarget(selectedRow());
    int trackIndex = _project.selectedTrackIndex();
    auto &routing = _project.routing();
    if (RouteDraft::isTrackModulated(routing, target, trackIndex)) {
        RouteDraft::removeTrack(routing, target, trackIndex);   // MOD-
    } else {
        beginNewModulation(target, trackIndex);                 // MOD+
    }
}
