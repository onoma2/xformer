#include "BasePage.h"
#include "Pages.h"
#include "ui/MatrixMap.h"
#include <cstdio>

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
    if (event.keycode() == KeyboardEvent::KeyTab) {
        // Simulate hardware Shift+Page press via proven keyDown/keyPress path.
        // No keyUp — let the user close menu via Escape/Fn keys.
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
