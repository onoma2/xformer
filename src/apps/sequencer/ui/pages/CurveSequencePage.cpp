#include "CurveSequencePage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"

enum class ContextAction {
    Init,
    Copy,
    Paste,
    Duplicate,
    Route,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
    { "COPY" },
    { "PASTE" },
    { "DUPL" },
    { "ROUTE" },
};

enum class LfoContextAction {
    Triangle,
    Sine,
    Sawtooth,
    Square,
    RandomMinMax,
    Last
};

static const ContextMenuModel::Item lfoContextMenuItems[] = {
    { "TRI" },
    { "SINE" },
    { "SAW" },
    { "SQUA" },
    { "MM-RND" },
};

enum class MacroContextAction {
    Bell,
    Damp,
    Rise,
    Bounce,
    Raster,
    Last
};

static const ContextMenuModel::Item macroContextMenuItems[] = {
    { "M-BELL" },
    { "M-DAMP" },
    { "M-RISE" },
    { "M-BOUNCE" },
    { "M-RSTR" },
};

CurveSequencePage::CurveSequencePage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{}

void CurveSequencePage::enter() {
    _listModel.setSequence(&_project.selectedCurveSequence());
}

void CurveSequencePage::exit() {
    _listModel.setSequence(nullptr);
}

void CurveSequencePage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "SEQUENCE");
    WindowPainter::drawActiveFunction(canvas, Track::trackModeName(_project.selectedTrack().trackMode()));
    WindowPainter::drawFooter(canvas);

    ListPage::draw(canvas);
}

void CurveSequencePage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}

void CurveSequencePage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.pageModifier() && key.is(Key::Step5)) {
        lfoContextShow();
        event.consume();
        return;
    }

    if (key.pageModifier() && key.is(Key::Step4)) {
        macroContextShow();
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

void CurveSequencePage::contextShow() {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); }
    ));
}

void CurveSequencePage::contextAction(int index) {
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

bool CurveSequencePage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    case ContextAction::Paste:
        return _model.clipBoard().canPasteCurveSequence();
    case ContextAction::Route:
        return _listModel.routingTarget(selectedRow()) != Routing::Target::None;
    default:
        return true;
    }
}

void CurveSequencePage::initSequence() {
    _project.selectedCurveSequence().clear();
    showMessage("SEQUENCE INITIALIZED");
}

void CurveSequencePage::copySequence() {
    _model.clipBoard().copyCurveSequence(_project.selectedCurveSequence());
    showMessage("SEQUENCE COPIED");
}

void CurveSequencePage::pasteSequence() {
    _model.clipBoard().pasteCurveSequence(_project.selectedCurveSequence());
    showMessage("SEQUENCE PASTED");
}

void CurveSequencePage::duplicateSequence() {
    if (_project.selectedTrack().duplicatePattern(_project.selectedPatternIndex())) {
        showMessage("SEQUENCE DUPLICATED");
    }
}

void CurveSequencePage::initRoute() {
    _manager.pages().top.editRoute(_listModel.routingTarget(selectedRow()), _project.selectedTrackIndex());
}

void CurveSequencePage::lfoContextShow() {
    showContextMenu(ContextMenu(
        lfoContextMenuItems,
        int(LfoContextAction::Last),
        [&] (int index) { lfoContextAction(index); },
        [&] (int index) { return true; }
    ));
}

void CurveSequencePage::lfoContextAction(int index) {
    auto &sequence = _project.selectedCurveSequence();
    int firstStep = sequence.firstStep();
    int lastStep = sequence.lastStep();

    switch (LfoContextAction(index)) {
    case LfoContextAction::Triangle:
        sequence.populateWithTriangleWaveLfo(firstStep, lastStep);
        showMessage("LFO TRIANGLE POPULATED");
        break;
    case LfoContextAction::Sine:
        sequence.populateWithSineWaveLfo(firstStep, lastStep);
        showMessage("LFO SINE POPULATED");
        break;
    case LfoContextAction::Sawtooth:
        sequence.populateWithSawtoothWaveLfo(firstStep, lastStep);
        showMessage("LFO SAWTOOTH POPULATED");
        break;
    case LfoContextAction::Square:
        sequence.populateWithSquareWaveLfo(firstStep, lastStep);
        showMessage("LFO SQUARE POPULATED");
        break;
    case LfoContextAction::RandomMinMax:
        sequence.populateWithRandomMinMax(firstStep, lastStep);
        showMessage("MIN/MAX RANDOMIZED");
        break;
    case LfoContextAction::Last:
        break;
    }
}

void CurveSequencePage::macroContextShow() {
    showContextMenu(ContextMenu(
        macroContextMenuItems,
        int(MacroContextAction::Last),
        [&] (int index) { macroContextAction(index); },
        [&] (int index) { return true; }
    ));
}

void CurveSequencePage::macroContextAction(int index) {
    auto &sequence = _project.selectedCurveSequence();
    int firstStep = sequence.firstStep();
    int lastStep = sequence.lastStep();

    switch (MacroContextAction(index)) {
    case MacroContextAction::Bell:
        sequence.populateWithMacroBell(firstStep, lastStep);
        showMessage("MACRO BELL POPULATED");
        break;
    case MacroContextAction::Damp:
        sequence.populateWithMacroDamp(firstStep, lastStep);
        showMessage("MACRO DAMP POPULATED");
        break;
    case MacroContextAction::Rise:
        sequence.populateWithMacroRise(firstStep, lastStep);
        showMessage("MACRO RISE POPULATED");
        break;
    case MacroContextAction::Bounce:
        sequence.populateWithMacroBounce(firstStep, lastStep);
        showMessage("MACRO BOUNCE POPULATED");
        break;
    case MacroContextAction::Raster:
        sequence.populateWithRasterizedShape(firstStep, lastStep);
        showMessage("SHAPE RASTERIZED");
        break;
    case MacroContextAction::Last:
        break;
    }
}
