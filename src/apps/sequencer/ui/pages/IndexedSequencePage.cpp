#include "IndexedSequencePage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"

enum class ContextAction {
    Init,
    Route,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
    { "ROUTE" },
};

enum class RhythmContextAction {
    Euclidean,
    Clave,
    Tuplet,
    Poly,
    RandomRhythm,
    Last
};

static const ContextMenuModel::Item rhythmContextMenuItems[] = {
    { "EUCL" },
    { "CLAVE" },
    { "TUPLET" },
    { "POLY" },
    { "M-RHY" },
};

enum class WaveformContextAction {
    Triangle,
    Sine,
    Sawtooth,
    Pulse,
    Target,
    Last
};

static const ContextMenuModel::Item waveformContextMenuItems[] = {
    { "TRI" },
    { "SINE" },
    { "SAW" },
    { "PULSE" },
    { "TARGET" },
};

enum class MelodicContextAction {
    Scale,
    Arpeggio,
    Chord,
    Modal,
    RandomMelody,
    Last
};

static const ContextMenuModel::Item melodicContextMenuItems[] = {
    { "SCALE" },
    { "ARP" },
    { "CHORD" },
    { "MODAL" },
    { "M-MEL" },
};

enum class DurationTransformContextAction {
    DurationLog,
    DurationExp,
    DurationTriangle,
    Reverse,
    Mirror,
    Last
};

static const ContextMenuModel::Item durationTransformContextMenuItems[] = {
    { "D-LOG" },
    { "D-EXP" },
    { "D-TRI" },
    { "REV" },
    { "MIRR" },
};

IndexedSequencePage::IndexedSequencePage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{}

void IndexedSequencePage::enter() {
    _listModel.setSequence(&_project.selectedIndexedSequence());
}

void IndexedSequencePage::exit() {
}

void IndexedSequencePage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "SEQUENCE");
    WindowPainter::drawActiveFunction(canvas, Track::trackModeName(_project.selectedTrack().trackMode()));
    WindowPainter::drawFooter(canvas);

    ListPage::draw(canvas);
}

void IndexedSequencePage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);

    if (globalKeyState()[Key::Page] && !globalKeyState()[Key::Shift]) {
        // Indexed Macro Shortcuts - YELLOW
        // Step 4: Rhythm Generators
        // Step 5: Waveforms
        // Step 6: Melodic Generators
        // Step 14: Duration & Transform
        const int shortcuts[] = { 4, 5, 6, 14 };
        for (int step : shortcuts) {
            int index = MatrixMap::fromStep(step);
            leds.unmask(index);
            leds.set(index, true, true);
            leds.mask(index);
        }
    }
}

void IndexedSequencePage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.pageModifier() && key.is(Key::Step4)) {
        rhythmContextShow();
        event.consume();
        return;
    }

    if (key.pageModifier() && key.is(Key::Step5)) {
        waveformContextShow();
        event.consume();
        return;
    }

    if (key.pageModifier() && key.is(Key::Step6)) {
        melodicContextShow();
        event.consume();
        return;
    }

    if (key.pageModifier() && key.is(Key::Step14)) {
        durationTransformContextShow();
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

void IndexedSequencePage::contextShow() {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); }
    ));
}

void IndexedSequencePage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Init:
        initSequence();
        break;
    case ContextAction::Route:
        initRoute();
        break;
    case ContextAction::Last:
        break;
    }
}

bool IndexedSequencePage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    case ContextAction::Route:
        return _listModel.routingTarget(selectedRow()) != Routing::Target::None;
    default:
        return true;
    }
}

void IndexedSequencePage::initSequence() {
    _project.selectedIndexedSequence().clear();
    showMessage("SEQUENCE INITIALIZED");
}

void IndexedSequencePage::initRoute() {
    _manager.pages().top.editRoute(_listModel.routingTarget(selectedRow()), _project.selectedTrackIndex());
}

void IndexedSequencePage::rhythmContextShow() {
    showContextMenu(ContextMenu(
        rhythmContextMenuItems,
        int(RhythmContextAction::Last),
        [&] (int index) { rhythmContextAction(index); },
        [&] (int index) { return true; }
    ));
}

void IndexedSequencePage::rhythmContextAction(int index) {
    auto &sequence = _project.selectedIndexedSequence();

    // Determine range: use active length (loop range is implicit via firstStep/activeLength)
    int firstStep = 0;
    int lastStep = sequence.activeLength() - 1;

    switch (RhythmContextAction(index)) {
    case RhythmContextAction::Euclidean:
        // TODO: Implement Euclidean rhythm generator
        showMessage("EUCLIDEAN - NOT YET IMPLEMENTED");
        break;
    case RhythmContextAction::Clave:
        // TODO: Implement Clave patterns
        showMessage("CLAVE - NOT YET IMPLEMENTED");
        break;
    case RhythmContextAction::Tuplet:
        // TODO: Implement Tuplet subdivision
        showMessage("TUPLET - NOT YET IMPLEMENTED");
        break;
    case RhythmContextAction::Poly:
        // TODO: Implement Polyrhythmic subdivision
        showMessage("POLY - NOT YET IMPLEMENTED");
        break;
    case RhythmContextAction::RandomRhythm:
        // TODO: Implement Random rhythm generator
        showMessage("M-RHY - NOT YET IMPLEMENTED");
        break;
    case RhythmContextAction::Last:
        break;
    }
}

void IndexedSequencePage::waveformContextShow() {
    showContextMenu(ContextMenu(
        waveformContextMenuItems,
        int(WaveformContextAction::Last),
        [&] (int index) { waveformContextAction(index); },
        [&] (int index) { return true; }
    ));
}

void IndexedSequencePage::waveformContextAction(int index) {
    auto &sequence = _project.selectedIndexedSequence();
    int firstStep = 0;
    int lastStep = sequence.activeLength() - 1;

    switch (WaveformContextAction(index)) {
    case WaveformContextAction::Triangle:
        // TODO: Implement Triangle waveform
        showMessage("TRI - NOT YET IMPLEMENTED");
        break;
    case WaveformContextAction::Sine:
        // TODO: Implement Sine waveform
        showMessage("SINE - NOT YET IMPLEMENTED");
        break;
    case WaveformContextAction::Sawtooth:
        // TODO: Implement Sawtooth waveform
        showMessage("SAW - NOT YET IMPLEMENTED");
        break;
    case WaveformContextAction::Pulse:
        // TODO: Implement Pulse waveform
        showMessage("PULSE - NOT YET IMPLEMENTED");
        break;
    case WaveformContextAction::Target:
        // TODO: Implement Target parameter selector
        showMessage("TARGET - NOT YET IMPLEMENTED");
        break;
    case WaveformContextAction::Last:
        break;
    }
}

void IndexedSequencePage::melodicContextShow() {
    showContextMenu(ContextMenu(
        melodicContextMenuItems,
        int(MelodicContextAction::Last),
        [&] (int index) { melodicContextAction(index); },
        [&] (int index) { return true; }
    ));
}

void IndexedSequencePage::melodicContextAction(int index) {
    auto &sequence = _project.selectedIndexedSequence();
    int firstStep = 0;
    int lastStep = sequence.activeLength() - 1;

    switch (MelodicContextAction(index)) {
    case MelodicContextAction::Scale:
        // TODO: Implement Scale fill
        showMessage("SCALE - NOT YET IMPLEMENTED");
        break;
    case MelodicContextAction::Arpeggio:
        // TODO: Implement Arpeggio patterns
        showMessage("ARP - NOT YET IMPLEMENTED");
        break;
    case MelodicContextAction::Chord:
        // TODO: Implement Chord voicings
        showMessage("CHORD - NOT YET IMPLEMENTED");
        break;
    case MelodicContextAction::Modal:
        // TODO: Implement Modal melodies
        showMessage("MODAL - NOT YET IMPLEMENTED");
        break;
    case MelodicContextAction::RandomMelody:
        // TODO: Implement Random melody generator
        showMessage("M-MEL - NOT YET IMPLEMENTED");
        break;
    case MelodicContextAction::Last:
        break;
    }
}

void IndexedSequencePage::durationTransformContextShow() {
    showContextMenu(ContextMenu(
        durationTransformContextMenuItems,
        int(DurationTransformContextAction::Last),
        [&] (int index) { durationTransformContextAction(index); },
        [&] (int index) { return true; }
    ));
}

void IndexedSequencePage::durationTransformContextAction(int index) {
    auto &sequence = _project.selectedIndexedSequence();
    int firstStep = 0;
    int lastStep = sequence.activeLength() - 1;

    switch (DurationTransformContextAction(index)) {
    case DurationTransformContextAction::DurationLog:
        // TODO: Implement Duration logarithmic curve
        showMessage("D-LOG - NOT YET IMPLEMENTED");
        break;
    case DurationTransformContextAction::DurationExp:
        // TODO: Implement Duration exponential curve
        showMessage("D-EXP - NOT YET IMPLEMENTED");
        break;
    case DurationTransformContextAction::DurationTriangle:
        // TODO: Implement Duration triangle curve
        showMessage("D-TRI - NOT YET IMPLEMENTED");
        break;
    case DurationTransformContextAction::Reverse:
        // TODO: Implement Reverse step order
        showMessage("REV - NOT YET IMPLEMENTED");
        break;
    case DurationTransformContextAction::Mirror:
        // TODO: Implement Mirror around midpoint
        showMessage("MIRR - NOT YET IMPLEMENTED");
        break;
    case DurationTransformContextAction::Last:
        break;
    }
}
