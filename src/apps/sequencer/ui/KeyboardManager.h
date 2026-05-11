#pragma once

#include <cstdint>
#include <functional>

#include "engine/Engine.h"

class KeyboardManager {
public:
    using ProcessHandler = std::function<void()>;

    KeyboardManager();

    void init(Engine &engine);
    void process();
    void setProcessHandler(ProcessHandler handler) { _processHandler = handler; }

private:
    ProcessHandler _processHandler;
};
