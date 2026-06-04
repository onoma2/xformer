#include "IndexedSequencePage.h"

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
}

void IndexedSequencePage::keyPress(KeyPressEvent &event) {
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

void IndexedSequencePage::contextShow(bool doubleClick) {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); },
        doubleClick
    ));
}

void IndexedSequencePage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Init:
        initSequence();
        break;
    case ContextAction::Last:
        break;
    }
}

bool IndexedSequencePage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    default:
        return true;
    }
}

void IndexedSequencePage::initSequence() {
    _project.selectedIndexedSequence().clear();
    showMessage("SEQUENCE INITIALIZED");
}
