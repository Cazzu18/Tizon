#include "PhysicsComponent.h"
#include "CollisionEvent.h"

void PhysicsComponent::Initialize() {
    //register as event listener
    GetEventSystem().AddListener(this);
}

PhysicsComponent::~PhysicsComponent() {
    //unregister as event listener
    GetEventSystem().RemoveListener(this);
}

void PhysicsComponent::onEvent(const Event& event) {
    if(auto collisionEvent = dynamic_cast<const CollisionEvent*>(&event)) {
        //handle collision
    }
}
