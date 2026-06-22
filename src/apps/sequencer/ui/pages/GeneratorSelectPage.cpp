#include "GeneratorSelectPage.h"

#include "ui/painters/WindowPainter.h"

enum class Function {
    Cancel  = 3,
    OK      = 4,
};

static const char *functionNames[] = { nullptr, nullptr, nullptr, "CANCEL", "OK" };


GeneratorSelectPage::GeneratorSelectPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{}

void GeneratorSelectPage::show(ResultCallback callback, const Generator::Mode *modes, int count) {
    _callback = callback;
    _listModel.setModes(modes, count);
    ListPage::show();
}

void GeneratorSelectPage::enter() {
}

void GeneratorSelectPage::exit() {
}

void GeneratorSelectPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "GENERATOR");
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState());

    ListPage::draw(canvas);
}

void GeneratorSelectPage::updateLeds(Leds &leds) {
}

void GeneratorSelectPage::keyPress(KeyPressEvent &event) {
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
    }

    if (key.is(Key::Encoder)) {
        closeWithResult(true);
        return;
    }

    ListPage::keyPress(event);
}

void GeneratorSelectPage::keyboard(KeyboardEvent &event) {
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

void GeneratorSelectPage::closeWithResult(bool result) {
    Page::close();
    if (_callback) {
        _callback(result, _listModel.modeForRow(selectedRow()));
    }
}
