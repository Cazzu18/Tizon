#include "InputSystem.h"
#include "Events.h"

void InputSystem::Update(){
    //I can poll input events from the platform here
    //...

    //example
    ProcessKeyPress(KEY_W, false);
}

void InputSystem::ProcessKeyPress(int keyCode, bool repeat){
    bool& keyState = keyStates[keyCode];

    if(!keyState || repeat){
        //key was not pressed before or this is a repeat
        KeyPressEvent event(keyCode, repeat);
        eventBus.PublishEvent(event);
    }

    keyState = true;
}

void InputSystem::ProcessKeyRelease(int keyCode, bool repeat){
    bool& keyState = keyStates[keyCode];

    if(!keyState || repeat){
        //key was pressed before
        KeyReleaseEvent event(keyCode, repeat);
        eventBus.PublishEvent(event);
    }

    keyState = false;
}