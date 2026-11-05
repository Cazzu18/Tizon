#pragma once
#include "Events.h"


//UI system with event bubbling
class UISystem {
    public:
        void DispatchEvent(UIEvent& event);
};