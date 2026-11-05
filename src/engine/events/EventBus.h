#pragma once
#include "EventListener.h"
#include <queue>
#include <memory>

//enhanced event bus with category filtering for listeners & priority
class EventBus {
    private:

        struct ListenerInfo {
            EventListener* listener;
            int categoryFilter;
            int priority;
        };

        std::vector<ListenerInfo> listeners;
        std::queue<std::unique_ptr<Event>> eventQueue;
        std::mutex queueMutex;
        bool immediateMode = true;

    public:
        void SetImmediateMode(bool immediate){ immediateMode = immediate;}
        void AddListener(EventListener* listener, int categoryFilter = -1, int priority = 0);
        void RemoveListener(EventListener* listener);
        void PublishEvent(const Event& event);
        void processEvents();
};

