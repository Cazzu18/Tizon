#include <iostream>
#include <glm/glm.hpp>
#include "Component.h"
#include "CameraComponent.h"
#include "EventListener.h"
#include "EventDispatcher.h"
#include "EventBus.h"
#include "TransformComponent.h"
#include "Events.h"


//Component that listens for events
class CameraController : public Component, public EventListener {
    private:
        CameraComponent* camera;
        float moveSpeed = 5.0f;
        float rotateSpeed = 0.1f;

        bool moveForward = false;
        bool moveBackward = false;
        bool moveLeft = false;
        bool moveRight = false;

    public:
        void Initialize(){
            camera = GetOwner()->GetComponent<CameraComponent>();

            //Registering as event listener
            GetEventBus().AddListener(this);
        }

        void Update(float deltaTime) override {
            if(!camera) return;

            //handling movement
            glm::vec3 movement(0.0f);

            if(moveForward) movement.z -= 1.0f;
            if(moveBackward) movement.z += 1.0f;
            if(moveLeft) movement.x -= 1.0f;
            if(moveRight) movement.x += 1.0f;

            //if magnitude(sqrt(x^2 + y^2 + z^2)) of the 3 dimensional vec > 0.0f
            if(glm::length(movement) > 0.0f){
                movement = glm::normalize(movement) * moveSpeed * deltaTime;
                
                auto transform = GetOwner()->GetComponent<TransformComponent>();
                if(transform){
                    glm::vec3 position = transform->GetPosition();
                    position += movement;
                    transform->SetPosition(position);
                }
            }
        }

        void OnEvent(const Event& event) override {
            EventDispatcher dispatcher(event);

            //Handling key press events
            dispatcher.Dispatch<KeyPressEvent>([this](const KeyPressEvent& e){
                switch(e.GetKeyCode()){
                    case KEY_W: moveForward = true; break;
                    case KEY_A: moveBackward = true; break;
                    case KEY_S: moveLeft = true; break;
                    case KEY_D: moveRight = true; break;
                }

                return false;
            });

            //Handline Key release events
            dispatcher.Dispatch<KeyReleaseEvent>([this](const KeyReleaseEvent& e){
                switch(e.GetKeyCode()){
                    case KEY_W: moveForward = false; break;
                    case KEY_A: moveBackward = false; break;
                    case KEY_S: moveLeft = false; break;
                    case KEY_D: moveRight = false; break;
                }

                return false;
            });

            //Handling window resize events
            dispatcher.Dispatch<WindowResizeEvent>([this](const WindowResizeEvent& e){
                if(camera){
                    float aspectRatio = static_cast<float>(e.GetWidth()) / static_cast<float>(e.GetHeight());
                    camera->SetAspectRatio(aspectRatio); //TODO handle setting aspect ratio
                }
                return false;
            });
        }

        ~CameraController() override {
            //unregistering as even listener
            GetEventBus().RemoveListener(this);
        }

    private:
        EventBus& GetEventBus(){
            //Get event bus from somewhere (singleton or parameter)
            static EventBus eventBus;
            return eventBus;
        }

};