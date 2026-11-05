#pragma once

#include <iostream>
#include "Event.h"

class EventDispatcher {
    private:
        const Event& event;
    public:
        explicit EventDispatcher(const Event& e) : event(e) {}

        //Dispatching event to handler if types match
        //If this event is actually type T, call the handler with the event converted to T
        template<typename T,typename F>
        bool Dispatch(const F& handler){ //F represents the type of the function/lambda we pass
            //Is the event's type the same as the type I'm trying to dispatch?
            if(event.GetType() == T::GetStaticType()){
                handler(static_cast<const T&>(event)); //calling handler functino
                return true;
            }
            return false;
        }
};