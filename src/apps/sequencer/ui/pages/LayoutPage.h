#pragma once

#include "ListPage.h"

#include "ui/model/TrackModeListModel.h"
#include "ui/model/LinkTrackListModel.h"
#include "ui/model/GateOutputListModel.h"
#include "ui/model/CvOutputListModel.h"

#include "core/utils/StringBuilder.h"

#include <array>

class LayoutPage : public ListPage {
public:
    LayoutPage(PageManager &manager, PageContext &context);

    virtual void enter() override;

    virtual void draw(Canvas &canvas) override;

    virtual void keyPress(KeyPressEvent &event) override;

private:
    enum class Mode : uint8_t {
        TrackMode,
        LinkTrack,
        GateOutput,
        CvOutput,
    };

    static const char *modeName(Mode mode) {
        switch (mode) {
        case Mode::TrackMode:   return "TRACK MODE"; break;
        case Mode::LinkTrack:   return "LINK TRACK"; break;
        case Mode::GateOutput:  return "GATE OUTPUT"; break;
        case Mode::CvOutput:    return "CV OUTPUT"; break;
        }
        return nullptr;
    }

    void setMode(Mode mode);
    void startTeletypeOutputAssignments(const std::array<int, CONFIG_TRACK_COUNT> &tracks, int count);
    void promptNextTeletypeOutputAssignment();
    void assignOutputsForTeletypeTrack(int trackIndex);

    Mode _mode = Mode::TrackMode;
    TrackModeListModel _trackModeListModel;
    LinkTrackListModel _linkTrackListModel;
    GateOutputListModel _gateOutputListModel;
    CvOutputListModel _cvOutputListModel;
    FixedStringBuilder<32> _teletypePromptText;
    std::array<int, CONFIG_TRACK_COUNT> _pendingTeletypeTracks{};
    int _pendingTeletypeCount = 0;
    int _pendingTeletypeIndex = 0;
};
