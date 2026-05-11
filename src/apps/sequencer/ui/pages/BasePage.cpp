#include "BasePage.h"
#include "Pages.h"
#include "ui/MatrixMap.h"

#include "ui/model/ContextMenuModel.h"

BasePage::BasePage(PageManager &manager, PageContext &context) :
    Page(manager),
    _context(context),
    _model(context.model),
    _project(context.model.project()),
    _engine(context.engine)
{}

void BasePage::showMessage(const char *text, uint32_t duration) {
    _context.messageManager.showMessage(text, duration);
}

void BasePage::showContextMenu(const ContextMenu &contextMenu) {
    _context.contextMenu = contextMenu;
    _manager.pages().contextMenu.show(_context.contextMenu, _context.contextMenu.actionCallback());
}

void BasePage::pressFunctionButton(int functionIndex, bool shift) {
    KeyState state;
    state.reset();
    if (shift) {
        state.set(Key::Shift);
    }
    Key key(MatrixMap::fromFunction(functionIndex), state);
    KeyEvent downEvent(KeyEvent::KeyDown, key);
    keyDown(downEvent);
    KeyPressEvent pressEvent(KeyEvent::KeyPress, key, 1);
    keyPress(pressEvent);
    KeyEvent upEvent(KeyEvent::KeyUp, key);
    keyUp(upEvent);
}

void BasePage::keyboard(KeyboardEvent &event) {
    static constexpr uint8_t stepKeycodes[] = {
        KeyboardEvent::KeyQ, KeyboardEvent::KeyW, KeyboardEvent::KeyE, KeyboardEvent::KeyR,
        KeyboardEvent::KeyT, KeyboardEvent::KeyY, KeyboardEvent::KeyU, KeyboardEvent::KeyI,
        KeyboardEvent::KeyA, KeyboardEvent::KeyS, KeyboardEvent::KeyD, KeyboardEvent::KeyF,
        KeyboardEvent::KeyG, KeyboardEvent::KeyH, KeyboardEvent::KeyJ, KeyboardEvent::KeyK,
    };

    for (int i = 0; i < 16; i++) {
        if (event.keycode() == stepKeycodes[i]) {
            Key key(MatrixMap::fromStep(i), _context.globalKeyState);
            if (event.isPressed()) {
                KeyEvent downEvent(KeyEvent::KeyDown, key);
                keyDown(downEvent);
                KeyPressEvent pressEvent(KeyEvent::KeyPress, key, 1);
                keyPress(pressEvent);
            } else {
                KeyEvent upEvent(KeyEvent::KeyUp, key);
                keyUp(upEvent);
            }
            event.consume();
            return;
        }
    }

    if (event.keycode() == KeyboardEvent::KeyTab && event.isPressed()) {
        KeyState state;
        state.set(Key::Shift);
        state.set(Key::Page);
        Key key(Key::Page, state);
        KeyEvent downEvent(KeyEvent::KeyDown, key);
        keyDown(downEvent);
        KeyPressEvent pressEvent(KeyEvent::KeyPress, key, 1);
        keyPress(pressEvent);
        event.consume();
    }
}
