#include "TrackPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

#include "engine/TuesdayTrackEngine.h"

#include "core/utils/StringBuilder.h"

enum class ContextAction {
    Init,
    Copy,
    Paste,
    Route,
    Reseed,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
    { "COPY" },
    { "PASTE" },
    { "ROUTE" },
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
    if (_project.selectedTrack().trackMode() == Track::TrackMode::Teletype) {
        const char *functionNames[] = { "TI PRESET", nullptr, nullptr, nullptr, "SYNC OUTS" };
        WindowPainter::drawFooter(canvas, functionNames, pageKeyState());
    } else {
        WindowPainter::drawFooter(canvas);
    }

    ListPage::draw(canvas);
}

void TrackPage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}

void TrackPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

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
        if (!key.shiftModifier() && key.function() == 0) {
            auto &track = _project.selectedTrack();
            if (track.trackMode() == Track::TrackMode::Teletype) {
                int trackIndex = _project.selectedTrackIndex();
                int &presetIndex = _teletypeTriggerPresetIndex[trackIndex];
                presetIndex = (presetIndex + 1) % 6;
                applyTeletypeTriggerPreset(track.teletypeTrack(), presetIndex);
                static const char *presetNames[] = {
                    "TI-TR CV1-4",
                    "TI-TR G1-4",
                    "TI-TR G5-8",
                    "TI-TR L-G1-4",
                    "TI-TR L-G5-8",
                    "TI-TR NONE",
                };
                showMessage(presetNames[presetIndex]);
                event.consume();
                return;
            }
        }
        if (!key.shiftModifier() && key.function() == 4) {
            auto &track = _project.selectedTrack();
            if (track.trackMode() == Track::TrackMode::Teletype) {
                int trackIndex = _project.selectedTrackIndex();
                auto &teletypeTrack = track.teletypeTrack();
                int cvSlot = 0;
                for (int outputIndex = 0; outputIndex < CONFIG_CHANNEL_COUNT; ++outputIndex) {
                    if (_project.cvOutputTrack(outputIndex) == trackIndex) {
                        teletypeTrack.setCvOutputDest(cvSlot, TeletypeTrack::CvOutputDest(outputIndex));
                        ++cvSlot;
                        if (cvSlot >= TeletypeTrack::CvOutputCount) {
                            break;
                        }
                    }
                }
                int gateSlot = 0;
                for (int outputIndex = 0; outputIndex < CONFIG_CHANNEL_COUNT; ++outputIndex) {
                    if (_project.gateOutputTrack(outputIndex) == trackIndex) {
                        teletypeTrack.setTriggerOutputDest(gateSlot, TeletypeTrack::TriggerOutputDest(outputIndex));
                        ++gateSlot;
                        if (gateSlot >= TeletypeTrack::TriggerOutputCount) {
                            break;
                        }
                    }
                }
                showMessage("TT OUTS SYNCED");
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
    case Track::TrackMode::Teletype:
        _teletypeTrackListModel.setTrack(track.teletypeTrack(), _project, track.trackIndex());
        newListModel = &_teletypeTrackListModel;
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

void TrackPage::contextShow() {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); }
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
    case ContextAction::Route:
        return _listModel->routingTarget(selectedRow()) != Routing::Target::None;
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
    _manager.pages().top.editRoute(_listModel->routingTarget(selectedRow()), _project.selectedTrackIndex());
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

void TrackPage::applyTeletypeTriggerPreset(TeletypeTrack &track, int presetIndex) {
    using Source = TeletypeTrack::TriggerInputSource;
    static const Source presets[][4] = {
        { Source::CvIn1, Source::CvIn2, Source::CvIn3, Source::CvIn4 },
        { Source::GateOut1, Source::GateOut2, Source::GateOut3, Source::GateOut4 },
        { Source::GateOut5, Source::GateOut6, Source::GateOut7, Source::GateOut8 },
        { Source::LogicalGate1, Source::LogicalGate2, Source::LogicalGate3, Source::LogicalGate4 },
        { Source::LogicalGate5, Source::LogicalGate6, Source::LogicalGate7, Source::LogicalGate8 },
        { Source::None, Source::None, Source::None, Source::None },
    };
    int index = clamp(presetIndex, 0, 5);
    for (int i = 0; i < 4; ++i) {
        track.setTriggerInputSource(i, presets[index][i]);
    }
}
