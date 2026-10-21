#include "CameraComponent.h"
#include "TransformComponent.h"

void CameraComponent::SetPerspective(float fov, float aspect, float near, float far){
    CameraComponent::CamerfieldOfView = fov;
    CameraComponent::aspectRatio = aspect;
    CameraComponent::nearPlane = near;
    CameraComponent::farPlane = far;
    CameraComponent::projectionDirty = true;
}

glm::mat4 CameraComponent::GetViewMatrix() const {
    auto transform = GetOwner()->GetComponent<TransformComponent>(); //Direct Reference(prefer Event System)
    if(transform){
        //calculating view matrix from transform
        glm::vec3 position = transform->GetPosition();
        glm::quat rotation = transform->GetRotation();

        //Forward vector(local Z)
        glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);

        //up vector(local +Y)
        glm::vec3 up = rotation * glm::vec(0.0f, 1.0f, 0.0f);

        return glm::lookAt(position, position + forward, up); //technically this is the inverse of the camera's model matrix and it(view matrix) transforms vertices from world space(which came from multiplying vertices by Model matrix) to camera space. This is Everything viewable.
    }

    return glm::mat4(1.0f); //returning identity matrix
}

glm::mat4 CameraComponent::GetProjectionMatrix() const {
    if(CameraComponent::projectionDirty){
        projectionMatrix = glm::perspective(
            glm::radians(CameraComponent::fieldOfView),
            CameraComponent::aspectRatio,
            CameraComponent::nearPlane,
            CameraComponent::farPlane
        );
        CameraComponent::projectionDirty = false;
    }

    return projectionMatrix; //Transforms vertices from camera space to normalized device coordinates(How much of what's "viewable" we see and how we see it(orthographic or perspective(objects further away seems smaller)))
}