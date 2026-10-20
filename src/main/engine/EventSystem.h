#pragma once

#include "EventListener.h"
#include "Event.h"
#include <vector>

class EventSystem {
    private:
        std::vector<EventListener*> listeners;

    public:
        void AddListener(EventListener* listener){
            listeners.push_back(listener);
        }

        void RemoveListener(EventListener* listener){
            auto it = std::find(listeners.begin(), listeners.end(), listener);
            if(it != listeners.end()){
                listeners.erase(it);
            }
        }

        void DispatchEvent(const Event& event){
            for(auto listener : listeners){
                listener->onEvent(event);
            }
        }
};