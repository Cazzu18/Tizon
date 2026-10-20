#pragma once

#include "Component.h"
#include <glm/glm.hpp> //linear algebra related types like vectors and matrices
#include <glm/gtc/matrix_transform.hpp> // exposes functions that can be used to generate model transformations like glm::rotate, view transformations like glm::lookAt and projection transformations like glm::perspective.
#include <glm/gtx/quaternion.hpp> // Required for glm::mat4_cast

class TransformComponent : public Component {
    private:
        glm::vec3 position = glm::vec3(0.0f);
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); //identity quaternion
        glm::vec3 scale = glm::vec3(1.0f);

        //Cached transformation matrix
        //mutable is a storage class specifier that allows a class member variable to be modified even if it belongs to a const object or is accessed inside a const member function
        mutable glm::mat4 transformMatrix = glm::mat4(1.0f);
        mutable bool transformDirty = true;

    public:
        void SetPosition(const glm::vec3& pos){
            position = pos;
            transformDirty = true;
        }

        void SetRotation(const glm::quat& rot){
            rotation = rot;
            transformDirty = true;
        }

        void SetScale(const glm::vec3& s){
            scale = s;
            transformDirty = true;
        }

        const glm::vec3& GetPosition() const {return position; }
        const glm::quat& GetRotation() const {return rotation; }
        const glm::vec3& GetScale() const {return scale; }

        glm::mat4 GetTransformMatrix() const {
            if(transformDirty){
                //calculate transformation matrix
                glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0), position);
                glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
                glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0), scale);

                transformMatrix = translationMatrix * rotationMatrix * scaleMatrix;
                transformDirty = false;
            }

            return transformMatrix;
        }

};