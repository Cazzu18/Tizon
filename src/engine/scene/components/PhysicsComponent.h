#pragma once
#include "Component.h"
#include "EventListener.h"
#include "Event.h"

//This EventSystem approach decouples components but adds complexity. Crucially, a decoupled component is a component that can be tested independently of any other component.

class PhysicsComponent : public Component, public EventListener {
    public:
        void Initialize() override;
        ~PhysicsComponent() override;
        void onEvent(const Event& event) override;
};