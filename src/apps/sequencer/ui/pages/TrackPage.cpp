#include "TrackPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

#include "engine/TuesdayTrackEngine.h"

#include "core/utils/StringBuilder.h"

#include "model/RouteDraft.h"
#include "model/RouteResolve.h"
#include "model/RouteParam.h"

enum class ContextAction {
    Init,
    Copy,
    Paste,
    Route,
    Reseed,
    Last
};

static const ContextMenuModel::Item contextMenuItemsModAdd[] = {
    { "INIT" },
    { "COPY" },
    { "PASTE" },
    { "MOD+" },
    { "RESEED" },
};

static const ContextMenuModel::Item contextMenuItemsModRemove[] = {
    { "INIT" },
    { "COPY" },
    { "PASTE" },
    { "MOD-" },
    { "RESEED" },
};

TrackPage::TrackPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _noteTrackListModel)
{}

void TrackPage::enter() {
    setTrack(_project.selectedTrack());
}

void TrackPage::exit() {
}

void TrackPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "TRACK");
    WindowPainter::drawActiveFunction(canvas, Track::trackModeName(_project.selectedTrack().trackMode()));
    WindowPainter::drawFooter(canvas);

    ListPage::draw(canvas);
}

void TrackPage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}

void TrackPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (handleContextMenuKey(event)) return;

    // Handle function keys exactly like NoteSequenceEditPage
    if (key.isFunction()) {
        // Shift+F5: Reseed Tuesday track loop
        if (key.shiftModifier() && key.function() == 4) {
            auto &track = _project.selectedTrack();
            if (track.trackMode() == Track::TrackMode::Tuesday) {
                int trackIndex = _project.selectedTrackIndex();
                auto &trackEngine = _engine.trackEngine(trackIndex);
                auto *tuesdayEngine = static_cast<TuesdayTrackEngine *>(&trackEngine);
                tuesdayEngine->reseed();
                showMessage("LOOP RESEEDED");
                event.consume();
                return;
            }
        }
    }

    if (key.pageModifier()) {
        return;
    }

    if (key.isTrackSelect()) {
        _project.setSelectedTrackIndex(key.trackSelect());
        setTrack(_project.selectedTrack());
    }

    ListPage::keyPress(event);
}

void TrackPage::setTrack(Track &track) {
    RoutableListModel *newListModel = _listModel;

    switch (track.trackMode()) {
    case Track::TrackMode::Note:
        _noteTrackListModel.setTrack(track.noteTrack());
        newListModel = &_noteTrackListModel;
        break;
    case Track::TrackMode::Curve:
        _curveTrackListModel.setTrack(track.curveTrack());
        newListModel = &_curveTrackListModel;
        break;
    case Track::TrackMode::MidiCv:
        _midiCvTrackListModel.setTrack(track.midiCvTrack());
        newListModel = &_midiCvTrackListModel;
        break;
    case Track::TrackMode::Tuesday:
        _tuesdayTrackListModel.setTrack(&track.tuesdayTrack());
        newListModel = &_tuesdayTrackListModel;
        break;
    case Track::TrackMode::DiscreteMap:
        _discreteMapTrackListModel.setTrack(&track.discreteMapTrack());
        newListModel = &_discreteMapTrackListModel;
        break;
    case Track::TrackMode::Indexed:
        _indexedTrackListModel.setTrack(&track.indexedTrack());
        newListModel = &_indexedTrackListModel;
        break;
    case Track::TrackMode::TeletypeV2:
    case Track::TrackMode::TeletypeMini:
        // No TT2 track-config list model yet (I/O grid deferred); TT2 routes its track
        // view to the script editor. Cover the enum to keep the switch exhaustive.
        break;
    case Track::TrackMode::Stochastic:
        _stochasticTrackListModel.setTrack(track.stochasticTrack(), _project, &_engine);
        newListModel = &_stochasticTrackListModel;
        break;
    case Track::TrackMode::Fractal:
        _fractalTrackListModel.setTrack(track.fractalTrack(), _project, &_engine);
        newListModel = &_fractalTrackListModel;
        break;
    case Track::TrackMode::PhaseFlux:
        _phaseFluxTrackListModel.setTrack(&track.phaseFluxTrack());
        newListModel = &_phaseFluxTrackListModel;
        break;
    case Track::TrackMode::Last:
        ASSERT(false, "invalid track mode");
        break;
    }

    if (newListModel != _listModel) {
        _listModel = newListModel;
        setListModel(*_listModel);
    }
}

void TrackPage::contextShow(bool doubleClick) {
    auto target = _listModel->routingTarget(selectedRow());
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

void TrackPage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Init:
        initTrackSetup();
        break;
    case ContextAction::Copy:
        copyTrackSetup();
        break;
    case ContextAction::Paste:
        pasteTrackSetup();
        break;
    case ContextAction::Route:
        initRoute();
        break;
    case ContextAction::Reseed:
        reseedTuesday();
        break;
    case ContextAction::Last:
        break;
    }
}

bool TrackPage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    case ContextAction::Paste:
        return _model.clipBoard().canPasteTrack();
    case ContextAction::Route: {
        auto target = _listModel->routingTarget(selectedRow());
        if (target == Routing::Target::None) return false;
        uint8_t key; RouteParam::Range range;
        return RouteResolve::overrideParam(_project.track(_project.selectedTrackIndex()).trackMode(), target, key, range)
            || RouteResolve::overrideParamGlobal(target, key, range);
    }
    case ContextAction::Reseed:
        return _project.selectedTrack().trackMode() == Track::TrackMode::Tuesday;
    default:
        return true;
    }
}

void TrackPage::initTrackSetup() {
    _project.selectedTrack().clear();
    setTrack(_project.selectedTrack());
    showMessage("TRACK INITIALIZED");
}

void TrackPage::copyTrackSetup() {
    _model.clipBoard().copyTrack(_project.selectedTrack());
    showMessage("TRACK COPIED");
}

void TrackPage::pasteTrackSetup() {
    // we are about to change track engines -> lock the engine to avoid inconsistent state
    _engine.lock();
    _model.clipBoard().pasteTrack(_project.selectedTrack());
    _engine.unlock();
    setTrack(_project.selectedTrack());
    showMessage("TRACK PASTED");
}

void TrackPage::initRoute() {
    Routing::Target target = _listModel->routingTarget(selectedRow());
    int trackIndex = _project.selectedTrackIndex();
    auto &routing = _project.routing();
    if (RouteDraft::isTrackModulated(routing, target, trackIndex)) {
        RouteDraft::removeTrack(routing, target, trackIndex);   // MOD-
    } else {
        beginNewModulation(target, trackIndex);                 // MOD+
    }
}

void TrackPage::reseedTuesday() {
    auto &track = _project.selectedTrack();
    if (track.trackMode() == Track::TrackMode::Tuesday) {
        int trackIndex = _project.selectedTrackIndex();
        auto &trackEngine = _engine.trackEngine(trackIndex);
        auto *tuesdayEngine = static_cast<TuesdayTrackEngine *>(&trackEngine);
        tuesdayEngine->reseed();
        showMessage("LOOP RESEEDED");
    }
}

void TrackPage::keyboard(KeyboardEvent &event) {
    if (!handleFunctionKeys(event)) {
        ListPage::keyboard(event);
    }
}
