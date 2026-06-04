#include "TuesdaySequencePage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"

enum class ContextAction {
    Init,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
};

TuesdaySequencePage::TuesdaySequencePage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{}

void TuesdaySequencePage::enter() {
    _listModel.setSequence(&_project.selectedTuesdaySequence());
}

void TuesdaySequencePage::exit() {
}

void TuesdaySequencePage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "SEQUENCE");
    WindowPainter::drawActiveFunction(canvas, Track::trackModeName(_project.selectedTrack().trackMode()));
    WindowPainter::drawFooter(canvas);

    ListPage::draw(canvas);
}

void TuesdaySequencePage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}

void TuesdaySequencePage::keyPress(KeyPressEvent &event) {
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

void TuesdaySequencePage::contextShow(bool doubleClick) {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); },
        doubleClick
    ));
}

void TuesdaySequencePage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Init:
        initSequence();
        break;
    case ContextAction::Last:
        break;
    }
}

bool TuesdaySequencePage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    default:
        return true;
    }
}

void TuesdaySequencePage::initSequence() {
    _project.selectedTuesdaySequence().clear();
    showMessage("SEQUENCE INITIALIZED");
}
