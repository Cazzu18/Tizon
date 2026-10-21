#pragma once
#include "Component.h"
#include <glm/glm.hpp>

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
        void SetPerspective(float fov, float aspect, float near, float far);
        glm::mat4 GetViewMatrix() const;
        glm::mat4 GetProjectionMatrix() const;
};