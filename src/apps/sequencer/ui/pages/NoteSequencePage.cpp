#include "NoteSequencePage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"

#include "model/RouteDraft.h"
#include "model/RouteFork.h"
#include "model/RouteParam.h"

enum class ContextAction {
    Init,
    Copy,
    Paste,
    Duplicate,
    Route,
    Last
};

static const ContextMenuModel::Item contextMenuItemsModAdd[] = {
    { "INIT" },
    { "COPY" },
    { "PASTE" },
    { "DUPL" },
    { "MOD+" },
};

static const ContextMenuModel::Item contextMenuItemsModRemove[] = {
    { "INIT" },
    { "COPY" },
    { "PASTE" },
    { "DUPL" },
    { "MOD-" },
};


NoteSequencePage::NoteSequencePage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{}

void NoteSequencePage::enter() {
    _listModel.setSequence(&_project.selectedNoteSequence());
}

void NoteSequencePage::exit() {
    _listModel.setSequence(nullptr);
}

void NoteSequencePage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "SEQUENCE");
    WindowPainter::drawActiveFunction(canvas, Track::trackModeName(_project.selectedTrack().trackMode()));

    const auto &sequence = _project.selectedNoteSequence();
    WindowPainter::drawAccumulatorValue(canvas, sequence.accumulator().currentValue(), sequence.accumulator().enabled());

    WindowPainter::drawFooter(canvas);

    ListPage::draw(canvas);
}

void NoteSequencePage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}

void NoteSequencePage::keyPress(KeyPressEvent &event) {
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

    if (key.pageModifier()) {
        return;
    }

    if (!event.consumed()) {
        ListPage::keyPress(event);
    }
}

void NoteSequencePage::contextShow(bool doubleClick) {
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

void NoteSequencePage::contextAction(int index) {
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
    case ContextAction::Duplicate:
        duplicateSequence();
        break;
    case ContextAction::Route:
        initRoute();
        break;
    case ContextAction::Last:
        break;
    }
}

bool NoteSequencePage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    case ContextAction::Paste:
        return _model.clipBoard().canPasteNoteSequence();
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

void NoteSequencePage::initSequence() {
    _project.selectedNoteSequence().clear();
    showMessage("SEQUENCE INITIALIZED");
}

void NoteSequencePage::copySequence() {
    _model.clipBoard().copyNoteSequence(_project.selectedNoteSequence());
    showMessage("SEQUENCE COPIED");
}

void NoteSequencePage::pasteSequence() {
    _model.clipBoard().pasteNoteSequence(_project.selectedNoteSequence());
    showMessage("SEQUENCE PASTED");
}

void NoteSequencePage::duplicateSequence() {
    if (_project.selectedTrack().duplicatePattern(_project.selectedPatternIndex())) {
        showMessage("SEQUENCE DUPLICATED");
    }
}

void NoteSequencePage::initRoute() {
    Routing::Target target = _listModel.routingTarget(selectedRow());
    int trackIndex = _project.selectedTrackIndex();
    auto &routing = _project.routing();
    if (RouteDraft::isTrackModulated(routing, target, trackIndex)) {
        RouteDraft::removeTrack(routing, target, trackIndex);   // MOD-
    } else {
        beginNewModulation(target, trackIndex);                 // MOD+
    }
}
