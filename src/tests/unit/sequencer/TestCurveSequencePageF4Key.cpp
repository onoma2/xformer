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

// No namespace used - classes are in global namespace

// Simple test to verify that the F4 key handling logic exists
// Since UI pages require complex mocking, we'll focus on compilation and basic logic
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
    }

    bool _contextMenuShown = false;
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

// Test to verify F4 key behavior - should NOT open context menu (only Shift+Page should)
UNIT_TEST("CurveSequencePageF4Key") {

    CASE("F4 key does NOT open context menu (only Shift+Page should)") {
        MockPageContext context;
        CurveSequencePage page(context.manager, context);

        // Set up the page properly
        page.enter();

        // Simulate F4 key press (which should NOT open context menu)
        KeyState keyState;
        keyState.reset();
        keyState.set(MatrixMap::fromFunction(4), true); // F4 key

        Key key(Key::F4, keyState);
        KeyPressEvent event(key);

        // Check initial state
        expectFalse(context.manager._contextMenuShown);

        // Trigger the key press
        page.keyPress(event);

        // F4 should NOT open the context menu (only Shift+Page should)
        expectFalse(context.manager._contextMenuShown);
    }

    CASE("F4 key is function key verification") {
        // Verify that F4 key is properly identified as a function key
        KeyState keyState;
        keyState.reset();
        keyState.set(MatrixMap::fromFunction(4), true); // F4 key

        Key key(Key::F4, keyState);

        // Verify that F4 is recognized as a function key
        bool isFunction = key.isFunction();
        bool functionIs4 = key.function() == 4;

        expectTrue(isFunction);
        expectTrue(functionIs4);
    }

    CASE("Other function keys are also not opening context menu") {
        // Verify that other function keys don't open the context menu either
        KeyState keyState;
        keyState.reset();
        keyState.set(MatrixMap::fromFunction(3), true); // F3 key

        Key key(Key::F3, keyState);
        KeyPressEvent event(key);

        MockPageContext context;
        CurveSequencePage page(context.manager, context);
        page.enter();

        expectFalse(context.manager._contextMenuShown);
        page.keyPress(event);
        expectFalse(context.manager._contextMenuShown);
    }
}