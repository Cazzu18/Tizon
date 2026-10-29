#pragma once
#include <iostream>
#include <vector>
#include "Entity.h"
#include "MeshComponent.h"
#include "TransformComponent.h"


/*
    Before rendering, we need to determine which object are visible to the camera. This is culling.
*/
class Camera;
class BoundingBox;

class CullingSystem {
    private:
        Camera* camera;
        std::vector<Entity*> visibleEntities;
        
    public:
        explicit CullingSystem(Camera* cam) : camera(cam) {}

        void SetCamera(Camera* cam){
            camera = cam;
        }

        void CullScene(const std::vector<Entity*>& allEntities){
            visibleEntities.clear();

            if(!camera) return;

            //Get camera frustum
            Frustum frustum = camera->GetFrustum();

            //Checking each entity against the frustum
            for(auto entity: allEntities){
                if(!entity->IsActive()) continue;

                auto meshComponent = entity->GetComponent<MeshComponent>();
                if(!meshComponent) continue;

                auto transformComponent = entity->GetComponent<TransformComponent>();
                if(!transformComponent) continue;

                //Get bounding box of the mesh
                BoundingBox boundingBox = meshComponent->GetBoundingBox();

                //Transform bounding box by entity transform
                boundingBox.Transform(transformComponent->GetTransformMatrix());

                //checking if bounding box is visible
                if(frustum.Intersect(boundingBox)){
                    visibleEntities.push_back(entity);
                }
            }
        }

        const std::vector<Entity*>& GetVisibleEntities() const {
            return visibleEntities;
        }
};

