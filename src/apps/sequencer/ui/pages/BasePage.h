#pragma once

#include "ui/MessageManager.h"
#include "ui/Page.h"
#include "ui/PageManager.h"
#include "ui/Key.h"
#include "ui/pages/ContextMenu.h"

#include "model/Model.h"

#include "engine/Engine.h"

#include <cstdint>

struct PageContext {
    MessageManager &messageManager;
    KeyState &pageKeyState;
    KeyState &globalKeyState;
    Model &model;
    Engine &engine;

    ContextMenu contextMenu;
};

class BasePage : public Page {
public:
    BasePage(PageManager &manager, PageContext &context);

protected:
    void showMessage(const char *text, uint32_t duration = 1000);
    void showContextMenu(const ContextMenu &contextMenu);

    virtual void contextShow(bool doubleClick = false) {}

    const KeyState &pageKeyState() const { return _context.pageKeyState; }
    const KeyState &globalKeyState() const { return _context.globalKeyState; }

    // Dispatch a function button press (F1=0 .. F5=4) to keyPress().
    // Used by keyboard() to map USB F-keys to hardware button actions.
    void pressFunctionButton(int functionIndex, bool shift = false);

    // Returns true if event was a handled function key (F1-F5 → pressFunctionButton).
    // Pages that want custom F-key handling can call this and fall through for other keys.
    // Pages that just need F1-F5 + BasePage fallback don't need to override keyboard() at all.
    bool handleFunctionKeys(KeyboardEvent &event);

    virtual void keyboard(KeyboardEvent &event) override;

    PageContext &_context;
    Model &_model;
    Project &_project;
    Engine &_engine;
};
