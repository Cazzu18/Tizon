#pragma once
#include "Event.h"
class EventListener{
    public:
        virtual ~EventListener() = default;
        virtual void OnEvent(const Event& event) = 0;
};