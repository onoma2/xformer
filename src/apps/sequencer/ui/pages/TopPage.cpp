#include "TopPage.h"

#include "Pages.h"
#include "ui/PageKeyMap.h"

#include "ui/model/NoteSequenceListModel.h"
#include "ui/model/CurveSequenceListModel.h"

#include "ui/LedPainter.h"

TopPage::TopPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{
}

void TopPage::init() {
    setMode(Mode::Project);

    _context.model.project().watch([this] (Project::Event event) {
        auto &pages = _manager.pages();
        switch (event) {
        case Project::Event::ProjectCleared:
        case Project::Event::ProjectRead:
            // reset local state in pages
            pages.routing.reset();
            pages.midiOutput.reset();
            pages.song.reset();
            setMode(_mode);
            break;
        case Project::Event::TrackModeChanged:
        case Project::Event::SelectedTrackIndexChanged:
        case Project::Event::SelectedPatternIndexChanged:
            setMode(_mode);
            break;
        }
    });
}

void TopPage::editRoute(Routing::Target target, int trackIndex) {
    auto &routing = _project.routing();

    if (target == Routing::Target::None) {
        return;
    }

    int routeIndex = routing.findRoute(target, trackIndex);
    if (routeIndex >= 0) {
        setMode(Mode::Routing);
        _manager.pages().routing.showRoute(routeIndex);
        return;
    }

    routeIndex = routing.findEmptyRoute();
    if (routeIndex >= 0) {
        routing.route(routeIndex).clear();
        Routing::Route initRoute;
        initRoute.setTarget(target);
        initRoute.setTracks(1<<trackIndex);
        setMode(Mode::Routing);
        _manager.pages().routing.showRoute(routeIndex, &initRoute);
    } else {
        showMessage("All routes are used!");
    }
}

void TopPage::editIndexedRouteConfig() {
    auto &pages = _manager.pages();
    _manager.push(&pages.indexedRouteConfig);
}

void TopPage::editIndexedMath() {
    auto &pages = _manager.pages();
    _manager.push(&pages.indexedMath);
}

void TopPage::updateLeds(Leds &leds) {
    bool clockTick = _engine.clockRunning() && _engine.tick() % CONFIG_PPQN < (CONFIG_PPQN / 8);

    leds.set(
        Key::Play,
        _engine.recording() && !clockTick,
        clockTick
    );

    if (globalKeyState()[Key::Page] && !globalKeyState()[Key::Shift]) {
        LedPainter::drawSelectedPage(leds, _mode);
    } else {
        LedPainter::drawTrackGatesAndSelectedTrack(leds, _engine, _project.playState(), _project.selectedTrackIndex());
    }
}

void TopPage::keyDown(KeyEvent &event) {
    event.consume();
}

void TopPage::keyUp(KeyEvent &event) {
    event.consume();
}

void TopPage::keyPress(KeyPressEvent &event) {
    auto &pages = _manager.pages();
    const auto &key = event.key();

    if (key.isTrackSelect()) {
        // Store which page we're on BEFORE changing track
        Page* currentPage = _manager.top();
        bool onSequenceView = (currentPage == &pages.noteSequence ||
                              currentPage == &pages.accumulator);
        bool onTrackView = (currentPage == &pages.track ||
                           currentPage == &pages.harmony);

        // Sync view states with current page before track change
        if (currentPage == &pages.noteSequence) {
            _sequenceView = SequenceView::NoteSequence;
        } else if (currentPage == &pages.accumulator) {
            _sequenceView = SequenceView::Accumulator;
        } else if (currentPage == &pages.track) {
            _trackView = TrackView::Track;
        } else if (currentPage == &pages.harmony) {
            _trackView = TrackView::Harmony;
        }

        // Now change the track
        _project.setSelectedTrackIndex(key.trackSelect());

        // Navigate to same view for new track
        if (onSequenceView) {
            setSequenceView(_sequenceView);
        } else if (onTrackView) {
            setTrackView(_trackView);
        }

        event.consume();
    }
    if (key.isTrack() && event.count() == 2) {
        setMode(Mode::SequenceEdit);
        event.consume();
        return;
    }

    if (key.pageModifier() && PageKeyMap::isPageKey(key.code())) {
        setMode(Mode(key.code()));
        event.consume();
    } else if (!key.pageModifier()) {
        if (key.isPattern() && _mode != Mode::Pattern) {
            pages.pattern.setModal(true);
            pages.pattern.show();
            event.consume();
        }
        if (key.isPerformer() && _mode != Mode::Performer) {
            pages.performer.setModal(true);
            pages.performer.show();
            event.consume();
        }
    }

    if (key.isPlay()) {
        if (key.pageModifier()) {
            _engine.toggleRecording();
        } else {
            _engine.togglePlay(key.shiftModifier());
        }
        event.consume();
    }

    if (key.isTempo()) {
        if (!key.pageModifier()) {
            // tempo page
            pages.tempo.show();
        }
    }

    event.consume();
}

void TopPage::encoder(EncoderEvent &event) {
    event.consume();
}

void TopPage::setMode(Mode mode) {
    auto &pages = _manager.pages();

    _lastMode = _mode;

    switch (mode) {
    case Mode::Project:
        setMainPage(pages.project);
        break;
    case Mode::Layout:
        setMainPage(pages.layout);
        break;
    case Mode::Track:
        setTrackPage();
        break;
    case Mode::Sequence:
        setSequencePage();
        break;
    case Mode::SequenceEdit:
        setSequenceEditPage();
        break;
    case Mode::Pattern:
        pages.pattern.setModal(false);
        // do not re-enter pattern page when its already the selected page
        // the reason for this is that when changing a pattern in latched mode, we don't want to loose the latch
        // state on the page
        if (_manager.top() != &pages.pattern) {
            setMainPage(pages.pattern);
        }
        break;
    case Mode::Performer:
        pages.performer.setModal(false);
        setMainPage(pages.performer);
        break;
    case Mode::Overview:
        setMainPage(pages.overview);
        break;
    case Mode::Clock:
        setMainPage(pages.clockSetup);
        break;
    case Mode::Song:
        setMainPage(pages.song);
        break;
    case Mode::Routing:
        setMainPage(pages.routing);
        break;
    case Mode::MidiOutput:
        setMainPage(pages.midiOutput);
        break;
    case Mode::UserScale:
        setMainPage(pages.userScale);
        break;
    case Mode::Monitor:
        setMainPage(pages.monitor);
        break;
    case Mode::System:
        if (mode != _lastMode) {
            _manager.pages().confirmation.show("DO YOU REALLY WANT TO ENTER SYSTEM PAGE?", [this] (bool result) {
                if (result) {
                    setMainPage(_manager.pages().system);
                } else {
                    setMode(_lastMode);
                }
            });
        }
        break;
    default:
        return;
    }

    _mode = mode;
}

void TopPage::setMainPage(Page &page) {
    if (_manager.stackSize() < 2) {
        _manager.push(&page);
    } else {
        _manager.replace(1, &page);
    }
}

void TopPage::setSequencePage() {
    // Determine if we're cycling within sequence views (pressing Sequence key while already viewing sequence)
    Page* currentPage = _manager.top();
    auto &pages = _manager.pages();
    bool fromSequenceView = (currentPage == &pages.noteSequence ||
                            currentPage == &pages.accumulator ||
                            currentPage == &pages.tuesdaySequence ||
                            currentPage == &pages.discreteMapSequenceList ||
                            currentPage == &pages.discreteMapSequence ||
                            currentPage == &pages.discreteMapStages ||
                            currentPage == &pages.indexedSequence ||
                            currentPage == &pages.indexedSteps);

    // Cycle to next view only if we're currently on a sequence view
    if (fromSequenceView) {
        if (currentPage == &pages.noteSequence || currentPage == &pages.indexedSequence) {
            _sequenceView = SequenceView::NoteSequence;
        } else if (currentPage == &pages.accumulator || currentPage == &pages.indexedSteps) {
            _sequenceView = SequenceView::Accumulator;
        } else if (currentPage == &pages.discreteMapStages) {
            _sequenceView = SequenceView::NoteSequence;
        } else if (currentPage == &pages.discreteMapSequenceList) {
            _sequenceView = SequenceView::Accumulator;
        } else {
            _sequenceView = SequenceView::NoteSequence;
        }

        switch (_sequenceView) {
        case SequenceView::NoteSequence:
            _sequenceView = SequenceView::Accumulator;
            break;
        case SequenceView::Accumulator:
            _sequenceView = SequenceView::NoteSequence;
            break;
        }
    } else {
        _sequenceView = SequenceView::NoteSequence; // Default to note sequence for first visit
    }

    setSequenceView(_sequenceView);
}

void TopPage::setSequenceView(SequenceView view) {
    auto &pages = _manager.pages();

    switch (_project.selectedTrack().trackMode()) {
    case Track::TrackMode::Note:
        switch (view) {
        case SequenceView::NoteSequence:
            setMainPage(pages.noteSequence);
            break;
        case SequenceView::Accumulator:
            setMainPage(pages.accumulator);
            break;
        }
        break;
    case Track::TrackMode::Curve:
        // For curve tracks, just show the curve sequence page
        setMainPage(pages.curveSequence);
        break;
    case Track::TrackMode::MidiCv:
        setMainPage(pages.track);
        break;
    case Track::TrackMode::Tuesday:
        // Tuesday tracks use TuesdaySequencePage for sequence parameters
        setMainPage(pages.tuesdaySequence);
        break;
    case Track::TrackMode::DiscreteMap:
        switch (view) {
        case SequenceView::NoteSequence:
            setMainPage(pages.discreteMapStages);
            break;
        case SequenceView::Accumulator:
            setMainPage(pages.discreteMapSequenceList);
            break;
        }
        break;
    case Track::TrackMode::Indexed:
        switch (view) {
        case SequenceView::NoteSequence:
            setMainPage(pages.indexedSequence);
            break;
        case SequenceView::Accumulator:
            setMainPage(pages.indexedSteps);
            break;
        }
        break;
    case Track::TrackMode::Last:
        break;
    }
}

void TopPage::setTrackPage() {
    // Determine if we're cycling within track views (pressing Track key while already viewing track)
    Page* currentPage = _manager.top();
    auto &pages = _manager.pages();
    bool fromTrackView = (currentPage == &pages.track ||
                         currentPage == &pages.harmony);

    // Cycle to next view only if we're currently on a track view
    if (fromTrackView) {
        switch (_trackView) {
        case TrackView::Track:
            _trackView = TrackView::Harmony;
            break;
        case TrackView::Harmony:
            _trackView = TrackView::Track;
            break;
        }
    } else {
        _trackView = TrackView::Track; // Default to track page for first visit
    }

    setTrackView(_trackView);
}

void TopPage::setTrackView(TrackView view) {
    auto &pages = _manager.pages();

    switch (_project.selectedTrack().trackMode()) {
    case Track::TrackMode::Note:
        switch (view) {
        case TrackView::Track:
            setMainPage(pages.track);
            break;
        case TrackView::Harmony:
            setMainPage(pages.harmony);
            break;
        }
        break;
    case Track::TrackMode::Curve:
    case Track::TrackMode::MidiCv:
    case Track::TrackMode::Tuesday:
        // For non-note tracks, always show track page
        setMainPage(pages.track);
        break;
    case Track::TrackMode::DiscreteMap:
        setMainPage(pages.track);
        break;
    case Track::TrackMode::Indexed:
        setMainPage(pages.track);
        break;
    case Track::TrackMode::Last:
        break;
    }
}

void TopPage::setSequenceEditPage() {
    auto &pages = _manager.pages();

    switch (_project.selectedTrack().trackMode()) {
    case Track::TrackMode::Note:
        setMainPage(pages.noteSequenceEdit);
        break;
    case Track::TrackMode::Curve:
        setMainPage(pages.curveSequenceEdit);
        break;
    case Track::TrackMode::MidiCv:
        setMainPage(pages.track);
        break;
    case Track::TrackMode::Tuesday:
        // Tuesday tracks use TuesdayEditPage for main parameter editing
        setMainPage(pages.tuesdayEdit);
        break;
    case Track::TrackMode::DiscreteMap:
        setMainPage(pages.discreteMapSequence);
        break;
    case Track::TrackMode::Indexed:
        setMainPage(pages.indexedSequenceEdit);
        break;
    case Track::TrackMode::Last:
        break;
    }
}
