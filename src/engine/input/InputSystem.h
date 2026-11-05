#pragma once
#include <unordered_map>
#include "EventBus.h"


//potential use of the event system
class InputSystem{
    private:
        EventBus& eventBus;

        //Key states
        std::unordered_map<int, bool> keyStates;

    public:
        explicit InputSystem(EventBus& bus) : eventBus(bus) {}

        void Update();
        void ProcessKeyPress(int keyCode, bool repeat);
        void ProcessKeyRelease(int keyCode, bool repeat);
};