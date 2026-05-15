#include "ModulatorPage.h"

#include "Pages.h"

#include "ui/painters/WindowPainter.h"

#include "core/math/Math.h"

ModulatorPage::ModulatorPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{
}

void ModulatorPage::enter() {
    updateListModel();
}

void ModulatorPage::exit() {
    _listModel.setProject(nullptr);
}

void ModulatorPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "MOD");

    char header[16];
    snprintf(header, sizeof(header), "MOD %d", _listModel.modulatorIndex() + 1);
    WindowPainter::drawActiveFunction(canvas, header);

    WindowPainter::drawFooter(canvas);

    ListPage::draw(canvas);
}

void ModulatorPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }

    // Left/Right to switch between modulators 1-8
    if (key.isLeft()) {
        _listModel.setModulatorIndex(_listModel.modulatorIndex() - 1);
        updateListModel();
        event.consume();
        return;
    }
    if (key.isRight()) {
        _listModel.setModulatorIndex(_listModel.modulatorIndex() + 1);
        updateListModel();
        event.consume();
        return;
    }

    if (!event.consumed()) {
        ListPage::keyPress(event);
    }
}

void ModulatorPage::encoder(EncoderEvent &event) {
    if (!event.consumed()) {
        ListPage::encoder(event);
    }
}

void ModulatorPage::updateListModel() {
    _listModel.setProject(&_project);
}
