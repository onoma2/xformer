#include "TopPage.h"
#include "Pages.h"

#include "model/NoteSequence.h"
#include "ui/LedPainter.h"
#include "ui/PageKeyMap.h"

#include "core/utils/StringBuilder.h"

#include <algorithm>

TopPage::TopPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void TopPage::init() {
    _mode = Mode::Project;
    setMainPage(_manager.pages().project);
}

void TopPage::updateLeds(Leds &leds) {
    auto &pages = _manager.pages();
    Page *top = _manager.top();

    if (top == &pages.pattern || top == &pages.performer) {
        top->updateLeds(leds);
    } else {
        LedPainter::drawTrackGatesAndSelectedTrack(leds, _engine, _project.playState(), _project.selectedTrackIndex());
    }
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
    } else {
        routeIndex = routing.findEmptyRoute();
        if (routeIndex >= 0) {
            auto &route = routing.route(routeIndex);
            route.clear();
            route.setTarget(target);
            route.setTracks(1 << trackIndex);
            setMode(Mode::Routing);
            _manager.pages().routing.showRoute(routeIndex, &route);
        } else {
            showMessage("NO EMPTY ROUTES");
        }
    }
}


void TopPage::editIndexedRouteConfig() {
    auto &pages = _manager.pages();
    setMainPage(pages.indexedRouteConfig);
}

void TopPage::editIndexedMath() {
    auto &pages = _manager.pages();
    setMainPage(pages.indexedMath);
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

    if (key.pageModifier() && key.code() == PageKeyMap::Monitor) {
        if (_mode == Mode::Monitor && _manager.top() == &pages.monitor) {
            pages.monitor.toggleScope();
            event.consume();
            return;
        }
    }

    if (key.isTrackSelect()) {
        // Store which page we're on BEFORE changing track
        Page* currentPage = _manager.top();
        bool onSequenceView = (currentPage == &pages.noteSequence ||
                              currentPage == &pages.accumulator ||
                              currentPage == &pages.stochasticPerformance ||
                              currentPage == &pages.stochasticConfig);
        bool onTrackView = (currentPage == &pages.track ||
                           currentPage == &pages.harmony);

        // Sync view states with current page before track change
        if (currentPage == &pages.noteSequence) {
            _sequenceView = SequenceView::NoteSequence;
        } else if (currentPage == &pages.accumulator) {
            _sequenceView = SequenceView::Accumulator;
        } else if (currentPage == &pages.stochasticPerformance) {
            _sequenceView = SequenceView::NoteSequence;
        } else if (currentPage == &pages.stochasticConfig) {
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
        if (_project.selectedTrack().trackMode() == Track::TrackMode::Stochastic) {
            if (key.code() == PageKeyMap::SequenceEdit) { // PAGE+S1
                _sequenceView = SequenceView::NoteSequence;
                setSequenceView(_sequenceView);
                _mode = Mode::SequenceEdit;
                event.consume();
                return;
            }
            if (key.code() == PageKeyMap::Sequence) { // PAGE+S2
                _sequenceView = SequenceView::Accumulator;
                setSequenceView(_sequenceView);
                _mode = Mode::Sequence;
                event.consume();
                return;
            }
        }

        // Double-press on Routing key toggles to Modulator page
        if (key.code() == PageKeyMap::Routing && _mode == Mode::Routing) {
            setMode(Mode::Modulator);
        } else if (key.code() == PageKeyMap::Routing && _mode == Mode::Modulator) {
            setMode(Mode::Routing);
        } else {
            setMode(Mode(key.code()));
        }
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

    if (key.pageModifier() && key.isTrack() && key.track() == 6) {
        _engine.panicTeletype();
        showMessage("TT PANIC");
        event.consume();
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

void TopPage::keyboard(KeyboardEvent &event) {
    // TopPage is at stack position 0 — it only sees events
    // that no higher page consumed.

    // Escape: pop page / go back
    if (event.keycode() == KeyboardEvent::KeyEscape) {
        if (_manager.stackSize() > 1) {
            _manager.pop();
        }
        event.consume();
        return;
    }

    // Space: toggle play/stop
    if (event.keycode() == KeyboardEvent::KeySpace) {
        _engine.togglePlay(event.shift());
        event.consume();
        return;
    }
}

void TopPage::setTrackPage() {
    Page* currentPage = _manager.top();
    auto &pages = _manager.pages();
    bool fromTrackView = (currentPage == &pages.track ||
                         currentPage == &pages.harmony);

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
        _trackView = TrackView::Track; 
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
    case Track::TrackMode::Teletype:
        setMainPage(pages.track);
        break;
    case Track::TrackMode::Stochastic:
        setMainPage(pages.stochasticPerformance);
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

void TopPage::setSequencePage() {
    Page* currentPage = _manager.top();
    auto &pages = _manager.pages();
    bool fromSequenceView = (currentPage == &pages.noteSequence ||
                            currentPage == &pages.accumulator ||
                            currentPage == &pages.stochasticPerformance ||
                            currentPage == &pages.stochasticConfig);

    if (fromSequenceView) {
        switch (_sequenceView) {
        case SequenceView::NoteSequence:
            _sequenceView = SequenceView::Accumulator;
            break;
        case SequenceView::Accumulator:
            _sequenceView = SequenceView::NoteSequence;
            break;
        }
    } else {
        _sequenceView = SequenceView::NoteSequence; 
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
        setMainPage(pages.curveSequence);
        break;
    case Track::TrackMode::MidiCv:
        setMainPage(pages.track);
        break;
    case Track::TrackMode::Tuesday:
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
    case Track::TrackMode::Teletype:
        setMainPage(pages.teletypeScriptView);
        break;
    case Track::TrackMode::Stochastic:
        switch (view) {
        case SequenceView::NoteSequence:
            setMainPage(pages.stochasticPerformance);
            break;
        case SequenceView::Accumulator:
            setMainPage(pages.stochasticConfig);
            break;
        }
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
        setMainPage(pages.tuesdayEdit);
        break;
    case Track::TrackMode::DiscreteMap:
        setMainPage(pages.discreteMapSequence);
        break;
    case Track::TrackMode::Indexed:
        setMainPage(pages.indexedSequenceEdit);
        break;
    case Track::TrackMode::Teletype:
        setMainPage(pages.teletypeScriptView);
        break;
    case Track::TrackMode::Stochastic:
        setMainPage(pages.stochasticSequenceEdit);
        break;
    case Track::TrackMode::Last:
        break;
    }
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
    case Mode::CvRoute:
        setMainPage(pages.cvRoute);
        break;
    case Mode::Modulator:
        setMainPage(pages.modulator);
        break;
    case Mode::UserScale:
        setMainPage(pages.userScale);
        break;
    case Mode::Monitor:
        if (mode != _mode) {
            pages.monitor.setScopeActive(false);
        }
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
        _manager.replace(_manager.stackSize() - 1, &page);
    }
}
