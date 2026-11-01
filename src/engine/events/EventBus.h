#pragma once
#include <iostream>
#include <vector>
#include <queue>
#include <mutex>
#include "EventListener.h"

//enhanced event bus with category filtering for listeners
class EventBus {
    private:
        std::vector<EventListener*> listeners;
        std::queue<std::unique_ptr<Event>> eventQueue;
        std::mutex queueMutex;
        bool immediateMode = true;

    public:
        void SetImmediateMode(bool immediate){
            immediateMode = immediate;
        }

        void AddListener(EventListener* listener){
            listeners.push_back(listener);
        }

        void RemoveListener(EventListener* listener){
            auto it = std::find(listeners.begin(), listeners.end(), listener);
            if(it != listeners.end()){
                listeners.erase(it);
            }
        }

        void PublishEvent(const Event& event){
            if(immediateMode){
                //dispatch immediately
                for(auto listener : listeners){
                    listener->OnEvent(event);
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

