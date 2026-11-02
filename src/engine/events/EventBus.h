#pragma once
#include <iostream>
#include <vector>
#include <queue>
#include <mutex>
#include "EventListener.h"

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
        void SetImmediateMode(bool immediate){
            immediateMode = immediate;
        }

        void AddListener(EventListener* listener, int categoryFilter = -1, int priority = 0){
            listeners.push_back({listener, categoryFilter, priority});

            //sorting listeners by priority (higher priority first)
            std::sort(listeners.begin(), listeners.end(),
                        [](const ListenerInfo& a, const ListenerInfo& b){
                            return a.priority > b.priority;
                        });
        }

        void RemoveListener(EventListener* listener){
            auto it = std::find(listeners.begin(), listeners.end(), [listener](const ListenerInfo& info){return info.listener == listener;});
            if(it != listeners.end()){
                listeners.erase(it);
            }
        }

        void PublishEvent(const Event& event){
            if(immediateMode){
                //dispatch immediately
                for(const auto& info : listeners){
                    if(info.categoryFilter == -1 || (event.GetCategoryFlags() & info.categoryFilter)){
                        info.listener->OnEvent(event);
                    }
                }
            } else {
                //queue event for later processing
                std::lock_guard<std::mutex> lock(queueMutex);
                eventQueue.push(std::unique_ptr<Event>(event.Clone()));
            }
        }


        void processEvents(){
            if(immediateMode) return;

            std::queue<std::unique_ptr<Event>> currentEvents;

            {
                std::lock_guard<std::mutex> lock(queueMutex); //unlocks whne scope is terminated
                std::swap(currentEvents, eventQueue);
            } //scope terminatesd

            while(!currentEvents.empty()){
                auto& event = *currentEvents.front();

                for(auto listener : listeners){
                    listener->OnEvent(event);
                }

                currentEvents.pop();
            }
        }
};

