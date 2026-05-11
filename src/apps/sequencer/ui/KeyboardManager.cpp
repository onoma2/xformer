#include "KeyboardManager.h"

KeyboardManager::KeyboardManager()
{
}

void KeyboardManager::init(Engine &engine)
{
    (void)engine;
}

void KeyboardManager::process()
{
    if (_processHandler) {
        _processHandler();
    }
}
