#pragma once
#include <vector>

class Event;
class EventListener;

class EventSystem {
    private:
        std::vector<EventListener*> listeners;
    public:
        void AddListener(EventListener* listener);
        void RemoveListener(EventListener* listener);
        void DispatchEvent(const Event& event);
};