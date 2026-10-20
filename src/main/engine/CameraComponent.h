#pragma once

#include "Component.h"
#include "TransformComponent.h"

class CameraComponent : public Component {
    private:
        float fieldOfView = 45.0f;
        float aspectRatio = 16.0f / 9.0f;
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;

        glm::mat4 viewMatrix = glm::mat4(1.0f);

        // mutable: GetProjectionMatrix() is logically const (it just returns a
        // matrix) even though it lazily recomputes and caches that matrix on
        // first access after the projection parameters change.
        mutable glm::mat4 projectionMatrix = glm::mat4(1.0f);
        mutable bool projectionDirty = true;

    public:
        void SetPerspective(float fov, float aspect, float near, float far){
            fieldOfView = fov;
            aspectRatio = aspect;
            nearPlane = near;
            farPlane = far;
            projectionDirty = true;
        }

        glm::mat4 GetViewMatrix() const {
            auto transform = GetOwner()->GetComponent<TransformComponent>(); //Direct Reference(prefer Event System)
            if(transform){
                //calculating view matrix from transform
                glm::vec3 position = transform->GetPosition();
                glm::quat rotation = transform->GetRotation();

                //Forward vector(local Z)
                glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);

                //up vector(local +Y)
                glm::vec3 up = rotation * glm::vec(0.0f, 1.0f, 0.0f);

                return glm::lookAt(position, position + forward, up);
            }

            return glm::mat4(1.0f); //returning identity matrix
        }

        glm::mat4 GetProjectionMatrix() const {
            if(projectionDirty){
                projectionMatrix = glm::perspective(
                    glm::radians(fieldOfView),
                    aspectRatio,
                    nearPlane,
                    farPlane
                );
                projectionDirty = false;
            }

            return projectionMatrix;
        }
};