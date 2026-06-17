#include "LayoutPage.h"

#include "Pages.h"

#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"
#include "model/Track.h"


LayoutPage::LayoutPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _trackModeListModel),
    _trackModeListModel(context.model.project()),
    _gateOutputListModel(context.model.project()),
    _cvOutputListModel(context.model.project())
{
}

void LayoutPage::enter() {
    _trackModeListModel.fromProject(_project);
}

void LayoutPage::draw(Canvas &canvas) {
    bool showCommit = _mode == Mode::TrackMode && !_trackModeListModel.sameAsProject(_project);
    // 5 entries = CONFIG_FUNCTION_KEY_COUNT; slot 3 is the freed LINK slot.
    const char *functionNames[] = { "MODE", "GATE", "CV", nullptr, showCommit ? "COMMIT" : nullptr };

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "LAYOUT");
    WindowPainter::drawActiveFunction(canvas, modeName(_mode));
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), int(_mode));

    ListPage::draw(canvas);
}

void LayoutPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isFunction()) {
        if (key.function() == 4) {
            commitLayout();
        } else if (key.function() < 3) {
            setMode(Mode(key.function()));
        }
        event.consume();
    }

    ListPage::keyPress(event);
}

void LayoutPage::setMode(Mode mode) {
    if (mode == _mode) {
        return;
    }
    switch (mode) {
    case Mode::TrackMode:
        setListModel(_trackModeListModel);
        break;
    case Mode::GateOutput:
        setListModel(_gateOutputListModel);
        break;
    case Mode::CvOutput:
        setListModel(_cvOutputListModel);
        break;
    default:
        return;
    }
    _mode = mode;
}

void LayoutPage::commitLayout() {
    if (_mode == Mode::TrackMode && !_trackModeListModel.sameAsProject(_project)) {
        _manager.pages().confirmation.show("ARE YOU SURE?", [this] (bool result) {
            if (result) {
                setEdit(false);
                // we are about to change track engines -> lock the engine to avoid inconsistent state
                _engine.lock();
                _trackModeListModel.toProject(_project);
                _engine.unlock();
                showMessage("LAYOUT CHANGED");
            }
        });
    }
}

void LayoutPage::keyboard(KeyboardEvent &event) {
    if (event.isPressed()) {
        if (event.keycode() == KeyboardEvent::KeyEnter) {
            if (_mode == Mode::TrackMode && !_trackModeListModel.sameAsProject(_project)) {
                commitLayout();
            } else {
                _manager.pop();
            }
            event.consume();
            return;
        }
        if (event.keycode() == KeyboardEvent::KeyEscape) {
            _manager.pop();
            event.consume();
            return;
        }
    }
    if (!handleFunctionKeys(event)) {
        ListPage::keyboard(event);
    }
}
