#include "UnitTest.h"
#include "apps/sequencer/ui/pages/CurveSequencePage.h"
#include "apps/sequencer/ui/Key.h"
#include "apps/sequencer/ui/Event.h"  // Contains KeyPressEvent
#include "apps/sequencer/ui/PageManager.h"
#include "apps/sequencer/ui/pages/BasePage.h"  // Contains PageContext
#include "apps/sequencer/model/Model.h"
#include "apps/sequencer/engine/Engine.h"
#include "apps/sequencer/model/Project.h"
#include "apps/sequencer/ui/Leds.h"
#include "core/gfx/Canvas.h"
#include "ui/MatrixMap.h"

// Mock PageManager to avoid complex dependencies
class MockPageManager : public PageManager {
public:
    MockPageManager(Model &model, Engine &engine) : PageManager(model, engine) {}
    
    void showContextMenu(const ContextMenu &menu) {
        // Mock implementation to track if context menu is shown
        _contextMenuShown = true;
        _contextMenuItems = menu.items();
        _contextMenuSize = menu.itemCount();
    }
    
    bool _contextMenuShown = false;
    const ContextMenuModel::Item* _contextMenuItems = nullptr;
    int _contextMenuSize = 0;
};

// Mock Context to avoid complex dependencies
class MockPageContext {
public:
    Model model;
    Engine engine;
    Project project;
    MockPageManager manager;
    
    MockPageContext() : manager(model, engine) {
        // Initialize project with default values
        project.init();
    }
};

// Test to verify that Page key alone does NOT open context menu (only Shift+Page should)
UNIT_TEST("CurveSequencePagePageKey") {

    CASE("Page key (Global7) alone does NOT open LFO context menu") {
        MockPageContext context;
        CurveSequencePage page(context.manager, context);

        // Set up the page properly
        page.enter();

        // Simulate Page key press (Global7) without shift
        KeyState keyState;
        keyState.reset();
        keyState.set(Key::Page, true); // Page key is Global7
        // No shift key pressed

        Key key(Key::Page, keyState);
        KeyPressEvent event(key);

        // Check initial state
        expectFalse(context.manager._contextMenuShown);

        // Trigger the key press
        page.keyPress(event);

        // Verify that context menu was NOT shown (only Shift+Page should open it)
        expectFalse(context.manager._contextMenuShown);
    }

    CASE("Shift+Page key combination opens context menu") {
        MockPageContext context;
        CurveSequencePage page(context.manager, context);

        page.enter();

        // Simulate Shift+Page key press
        KeyState keyState;
        keyState.reset();
        keyState.set(Key::Page, true); // Page key pressed
        keyState.set(Key::Shift, true); // Shift key also pressed

        Key key(Key::Page, keyState);
        KeyPressEvent event(key);

        expectFalse(context.manager._contextMenuShown);

        page.keyPress(event);

        expectTrue(context.manager._contextMenuShown);
    }

    CASE("Page key condition logic verification") {
        // Verify that the Page key condition logic is correct
        KeyState keyState;
        keyState.reset();
        keyState.set(Key::Page, true); // Page key is Global7

        Key key(Key::Page, keyState);

        // Verify that Page key is not a function key (this is important for our implementation)
        bool isPageKey = key.is(Key::Page);
        bool isFunction = key.isFunction();
        bool isContextMenu = key.isContextMenu(); // Should be false without shift

        expectTrue(isPageKey);
        expectFalse(isFunction); // Page key should not be considered a function key
        expectFalse(isContextMenu); // Should be false without shift modifier
    }
}