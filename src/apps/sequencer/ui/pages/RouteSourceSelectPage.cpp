#include "RouteSourceSelectPage.h"

#include "ui/painters/WindowPainter.h"

enum class Function {
    Cancel  = 3,
    OK      = 4,
};

static const char *functionNames[] = { nullptr, nullptr, nullptr, "CANCEL", "OK" };


RouteSourceSelectPage::RouteSourceSelectPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{}

void RouteSourceSelectPage::show(Routing::Target target, Routing::Source current, ResultCallback callback) {
    _callback = callback;
    _listModel.setTarget(target);
    setSelectedRow(_listModel.indexOf(current));
    ListPage::show();
}

void RouteSourceSelectPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "SOURCE");
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState());

    ListPage::draw(canvas);
}

void RouteSourceSelectPage::updateLeds(Leds &leds) {
}

void RouteSourceSelectPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isFunction()) {
        switch (Function(key.function())) {
        case Function::Cancel:
            closeWithResult(false);
            break;
        case Function::OK:
            closeWithResult(true);
            break;
        }
        return;
    }

    if (key.is(Key::Encoder)) {
        closeWithResult(true);
        return;
    }

    ListPage::keyPress(event);
}

void RouteSourceSelectPage::keyboard(KeyboardEvent &event) {
    if (event.isPressed()) {
        switch (event.keycode()) {
        case KeyboardEvent::KeyEnter:
            closeWithResult(true);
            event.consume();
            break;
        case KeyboardEvent::KeyEscape:
            closeWithResult(false);
            event.consume();
            break;
        }
    }
    ListPage::keyboard(event);
}

void RouteSourceSelectPage::closeWithResult(bool result) {
    Page::close();
    if (_callback) {
        _callback(result, _listModel.sourceAt(selectedRow()));
    }
}
