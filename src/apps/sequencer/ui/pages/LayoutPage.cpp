#include "LayoutPage.h"

#include "Pages.h"

#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"
#include "model/Track.h"


LayoutPage::LayoutPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _trackModeListModel),
    _trackModeListModel(context.model.project()),
    _linkTrackListModel(context.model.project()),
    _gateOutputListModel(context.model.project()),
    _cvOutputListModel(context.model.project())
{
}

void LayoutPage::enter() {
    _trackModeListModel.fromProject(_project);
}

void LayoutPage::draw(Canvas &canvas) {
    bool showCommit = _mode == Mode::TrackMode && !_trackModeListModel.sameAsProject(_project);
    const char *functionNames[] = { "MODE", "LINK", "GATE", "CV", showCommit ? "COMMIT" : nullptr };

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "LAYOUT");
    WindowPainter::drawActiveFunction(canvas, modeName(_mode));
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), int(_mode));

    ListPage::draw(canvas);
}

void LayoutPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isFunction()) {
        if (key.function() == 4 && _mode == Mode::TrackMode && !_trackModeListModel.sameAsProject(_project)) {
            _manager.pages().confirmation.show("ARE YOU SURE?", [this] (bool result) {
                if (result) {
                    setEdit(false);
                    // we are about to change track engines -> lock the engine to avoid inconsistent state
                    _engine.lock();
                    Track::TrackMode oldModes[CONFIG_TRACK_COUNT];
                    std::array<int, CONFIG_TRACK_COUNT> teletypeTracks{};
                    int teletypeCount = 0;
                    for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
                        oldModes[trackIndex] = _project.track(trackIndex).trackMode();
                    }
                    _trackModeListModel.toProject(_project);
                    for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
                        Track::TrackMode newMode = _project.track(trackIndex).trackMode();
                        if (oldModes[trackIndex] != newMode && newMode == Track::TrackMode::Teletype) {
                            _project.track(trackIndex).teletypeTrack().requestBootScriptRun();
                            teletypeTracks[teletypeCount++] = trackIndex;
                        }
                    }
                    _engine.unlock();
                    if (teletypeCount > 0) {
                        startTeletypeOutputAssignments(teletypeTracks, teletypeCount);
                    } else {
                        showMessage("LAYOUT CHANGED");
                    }
                }
            });
        }
        setMode(Mode(key.function()));
        event.consume();
    }

    ListPage::keyPress(event);
}

void LayoutPage::setMode(Mode mode) {
    if (mode == _mode) {
        return;
    }
    switch (mode) {
    case Mode::TrackMode:
        setListModel(_trackModeListModel);
        break;
    case Mode::LinkTrack:
        setListModel(_linkTrackListModel);
        break;
    case Mode::GateOutput:
        setListModel(_gateOutputListModel);
        break;
    case Mode::CvOutput:
        setListModel(_cvOutputListModel);
        break;
    default:
        return;
    }
    _mode = mode;
}

void LayoutPage::startTeletypeOutputAssignments(const std::array<int, CONFIG_TRACK_COUNT> &tracks, int count) {
    _pendingTeletypeTracks = tracks;
    _pendingTeletypeCount = count;
    _pendingTeletypeIndex = 0;
    promptNextTeletypeOutputAssignment();
}

void LayoutPage::promptNextTeletypeOutputAssignment() {
    if (_pendingTeletypeIndex >= _pendingTeletypeCount) {
        showMessage("LAYOUT CHANGED");
        return;
    }

    int trackIndex = _pendingTeletypeTracks[_pendingTeletypeIndex];
    int startOut = trackIndex + 1;
    int endOut = std::min(trackIndex + 4, CONFIG_CHANNEL_COUNT);
    _teletypePromptText.reset();
    _teletypePromptText("ASSIGN OUTS %d-%d TO THIS T9TYPE TRACK?", startOut, endOut);
    _manager.pages().confirmation.show(_teletypePromptText, [this, trackIndex] (bool result) {
        if (result) {
            assignOutputsForTeletypeTrack(trackIndex);
        }
        ++_pendingTeletypeIndex;
        promptNextTeletypeOutputAssignment();
    });
}

void LayoutPage::assignOutputsForTeletypeTrack(int trackIndex) {
    int endIndex = std::min(trackIndex + 3, CONFIG_CHANNEL_COUNT - 1);
    for (int outputIndex = trackIndex; outputIndex <= endIndex; ++outputIndex) {
        _project.setGateOutputTrack(outputIndex, trackIndex);
        _project.setCvOutputTrack(outputIndex, trackIndex);
    }
}
