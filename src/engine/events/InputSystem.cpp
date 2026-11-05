#include <iostream>
#include <unordered_map>
#include "EventBus.h"
#include "Events.h"


//potential use of the event system

class InputSystem{
    private:
        EventBus& eventBus;

        //Key states
        std::unordered_map<int, bool> keyStates;

    public:
        explicit InputSystem(EventBus& bus) : eventBus(bus) {}

        void Update(){
            //I can poll input events from the platform here
            //...

            //example
            ProcessKeyPress(KEY_W, false);
        }

        void ProcessKeyPress(int keyCode, bool repeat){
            bool& keyState = keyStates[keyCode];

            if(!keyState || repeat){
                //key was not pressed before or this is a repeat
                KeyPressEvent event(keyCode, repeat);
                eventBus.PublishEvent(event);
            }

            keyState = true;
        }

        void ProcessKeyRelease(int keyCode, bool repeat){
            bool& keyState = keyStates[keyCode];

            if(!keyState || repeat){
                //key was pressed befoer
                KeyReleaseEvent event(keyCode);
                eventBus.PublishEvent(event);
            }

            keyState = false;
        }
};