#include "MeshComponent.h"
#include "Material.h"
#include "Mesh.h"
#include "TransformComponent.h"

void MeshComponent::Render() {
    if(!MeshComponent::mesh || !MeshComponent::material) return;

    //getting transform component
    auto transform = GetOwner()->GetComponent<TransformComponent>();
    if(!transform) return;

    //render mesh with material and transform
    MeshComponent::material->Bind();
    MeshComponent::material->SetUniform("modelMatrix", transform->GetTransformMatrix());
    MeshComponent::mesh->Render();
}
