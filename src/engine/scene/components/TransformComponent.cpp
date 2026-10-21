#include "TransformComponent.h"
#include <glm/gtc/matrix_transform.hpp> // exposes functions that can be used to generate model transformations like glm::rotate, view transformations like glm::lookAt and projection transformations like glm::perspective.
#include <glm/gtx/quaternion.hpp> // Required for glm::mat4_cast

glm::mat4 TransformComponent::GetTransformMatrix() const {
    if(TransformComponent::transformDirty){
        //calculate transformation matrix
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0), TransformComponent::position);
        glm::mat4 rotationMatrix = glm::mat4_cast(TransformComponent::rotation);
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0), TransformComponent::scale);

        transformMatrix = translationMatrix * rotationMatrix * scaleMatrix;
        TransformComponent::transformDirty = false;
    }

    return transformMatrix;
}
