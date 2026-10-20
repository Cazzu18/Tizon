#pragma once
#include "Component.h"
#include "EventListener.h"
#include "CollisionEvent.h"

//This EventSystem approach decouples components but adds complexity. Crucially, a decoupled component is a component that can be tested independently of any other component.

class PhysicsComponent : public Component, public EventListener {
    public:
        void Initialize() override {
            //register as event listener
            GetEventSystem().AddListener(this);
        }

        ~PhysicsComponent() override {
            //unregister as event listener
            GetEventSystem().RemoveListener(this);
        }

        void onEvent(const Event& event) override {
            if(auto collisionEvent = dynamic_cast<const CollisionEvent*>(&event)) {
                //handle collision
            }
        }
};