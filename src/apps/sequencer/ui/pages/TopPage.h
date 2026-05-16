#pragma once

#include "BasePage.h"

#include "ui/PageKeyMap.h"

#include "model/Routing.h"

class TopPage : public BasePage {
public:
    TopPage(PageManager &manager, PageContext &context);

    void init();

    void editRoute(Routing::Target target, int trackIndex);
    void editIndexedRouteConfig();
    void editIndexedMath();

    virtual void updateLeds(Leds &leds) override;

    virtual void keyDown(KeyEvent &event) override;
    virtual void keyUp(KeyEvent &event) override;
    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;
    virtual void keyboard(KeyboardEvent &event) override;

private:
    enum Mode : uint8_t {
        // main modes
        Project         = PageKeyMap::Project,
        Layout          = PageKeyMap::Layout,
        Track           = PageKeyMap::Track,
        Sequence        = PageKeyMap::Sequence,
        SequenceEdit    = PageKeyMap::SequenceEdit,
        Song            = PageKeyMap::Song,
        Routing         = PageKeyMap::Routing,
        MidiOutput      = PageKeyMap::MidiOutput,
        Pattern         = PageKeyMap::Pattern,
        Performer       = PageKeyMap::Performer,
        Overview        = PageKeyMap::Overview,
        Clock           = PageKeyMap::Clock,
        CvRoute         = PageKeyMap::CvRoute,

        // aux modes
        UserScale       = PageKeyMap::UserScale,
        Monitor         = PageKeyMap::Monitor,
        System          = PageKeyMap::System,

        // accumulator modes
        Accumulator     = 64,
        AccumulatorSteps,
        // modulator mode
        Modulator,
        Last,
    };

    enum class SequenceView : uint8_t {
        NoteSequence,
        Accumulator,
    };

    enum class TrackView : uint8_t {
        Track,
        Harmony,
    };

    void setMode(Mode mode);
    void setMainPage(Page &page);
    void setSequenceView(SequenceView view);
    void setTrackView(TrackView view);

    void setSequencePage();
    void setTrackPage();
    void setSequenceEditPage();
    void selectTrackWithViewSync(int trackIndex);

    Mode _mode;
    Mode _lastMode;
    SequenceView _sequenceView = SequenceView::NoteSequence;
    TrackView _trackView = TrackView::Track;
};
