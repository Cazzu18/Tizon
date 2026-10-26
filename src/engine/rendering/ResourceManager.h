#pragma once
#include <iostream>
#include <unordered_map>
#include <typeindex>
#include <typeinfo>
#include <memory>
#include "Resource.h"
#include "ResourceHandle.h"
/*
    The storage architecture uses a sophisticated two-level mapping system that solves several critical problems in resource management. The outer map keyed by std::type_index ensures complete type separation, preventing name collisions between different resource types. 
    For example, you could have both a texture named "stone" and a sound effect named "stone" without conflicts, as they’re stored in separate type-specific containers.

    The inner maps provide O(1) average-case lookup performance for individual resources, which is crucial when the rendering system needs to access hundreds or thousands of resources per frame. 
    The use of std::shared_ptr provides automatic memory management and enables safe sharing of resources between different systems without manual lifetime management.

    The reference counting system operates independently of the shared_ptr reference counting to provide application-level lifecycle control. 
    This separation allows the resource manager to implement custom policies for resource retention and cleanup that go beyond simple memory management, such as keeping frequently used resources loaded even when not immediately referenced.
    The reference counting system provides automatic garbage collection for resources that are no longer actively used. This approach prevents memory leaks while avoiding the overhead of constantly monitoring resource usage across the entire application. 
    The decrement-and-check pattern ensures that resources are unloaded immediately when they become unused, helping to keep memory usage optimal.
    The cleanup process is designed to be thorough and safe, ensuring that resources have the opportunity to properly release their internal data (GPU memory, file handles, etc.) before being removed from the cache. 
    This two-phase cleanup approach prevents resource leaks and maintains system stability even under error conditions.
    

    TODO: Decide whether resource storage and reference counting should remain separate or be combined into a single type → ID → ResourceData map.

*/
class ResourceManager {
    private:
        //Two level storage system: organize by type first, then by unique identifier
        //This enables type-safe resource access while maintaining efficient lookup
        std::unordered_map<std::type_index, std::unordered_map<std::string, std::shared_ptr<Resource>>> resources;

        //Two level reference counting system for automatic resource lifecycle management
        //First level maps resource type, second level maps reource IDs to their data
        struct ResourceData {
            //a shared_ptr is a smart pointer that enables multiple pointers to share ownership of the same dynamically allocated object. 
            std::shared_ptr<Resource> resource; //the resource
            int refCount; // reference count for this particular resource
        };

        std::unordered_map<std::type_index, std::unordered_map<std::string, ResourceData>> refCounts;

    public:
        /*
            The loading logic implements a sophisticated caching strategy that balances performance with memory efficiency. The cache-first approach prevents redundant I/O operations and resource processing, which can be expensive for large textures, complex meshes, or compiled shaders. 
            This strategy is particularly important in rendering engines where the same resources may be referenced by multiple objects or systems.

            The template-based design with compile-time type checking ensures type safety while maintaining the flexibility to work with any resource type that derives from the base Resource class. 

            Error handling follows the principle of graceful degradation, where loading failures return invalid handles rather than throwing exceptions or corrupting the resource cache. This approach allows rendering systems to continue operating with fallback resources or alternative rendering paths when specific assets are unavailable or corrupted.
        */
        template<typename T>
        ResourceHandle<T> Load(const std::string& resourceId){
            static_assert(std::is_base_of<Resource, T>::value, "T must derive from Resource");

            //step 3a: we check exisiting resource cache to avoid redundant loading
            auto& typeResources = resources[std::type_index(typeid(T))];
            auto it = typeResources.find(resourceId);

            if(it != typeResources.end()){
                //resource exists in cache - we increment reference count and return the handle
                refCounts[resourceId]++
                return ResourceHandle<T>(resourceId, this);
            }

            //Step 3b: We Create new resource  instance and attempt loading
            auto resource = std::make_shared<T>(resourceId); //std::make_shared() is a function that returns a shared pointer to the object of specific type after dynamically creating it
            if(!resource->Load()){
                //loading failed so we return invalid handle rather than corrupting cache
                return ResourceHandle<T>();
            }

            //Step 3c: Cache successful resource and initialize reference tracking
            typeResources[resourceId] = resource;
            refCounts[resourceId] = 1;

            return ResourceHandle<T>(resourceId, this);

        }

        /*
            The resource access interface prioritizes safety and performance in equal measure. The template-based approach ensures that clients always receive correctly typed resource pointers, eliminating the need for manual casting and reducing the potential for type-related runtime errors. 
            The static_cast is safe because the type_index-based storage guarantees that only objects of type T are stored in each type-specific container.
        */
        template<typename T>
        T* GetResource(const std::string& resourceId){
            //access type-specific resource container using compiile time type information
            auto& typeResources = resources[std::type_index(typeid(T))];
            auto it = typeResources.find(resourceId);

            if(it != typeResources.end()){
                //resource found - so we perform safe downcast and return the type pointer
                return static_cast<T*>(it->second.get());
            }

            //resource not found - we return nullptr for safe handling by caller
            return nullptr;
        }

        /*
            The existence check provides an efficient way to validate resource availability without the overhead of full resource access. This capability is valuable for conditional rendering logic, where systems can choose alternative rendering paths based on resource availability without triggering expensive cache misses or I/O operations.
        */
        template<typename T>
        bool HasResource(const std::string& resourceId){
            //efficient existence check witohut resource access overhead
            auto resourceIt = resources.find(std::type_index(typeid(T)));
            return resourceIt != resource.end();
        }

        void Release(const std::string& resourceId){
            //locate reference count entry for this resource

            auto it = refCounts.find(resourceId);
            if(it != refCounts.end()){
                it->second--;

                //check if resource has no remaining references
                if(it->second <= 0){
                    //step 5a: We locate and unload the unreferenced resource across all type containers
                    for(auto& [type, typeResources] : resources) {
                        auto resourceIt = typeResources.find(resourceId);
                        if(resourceIt != typeResources.end()){
                            resourceIt->second->Unload(); //allowing resource to clean up its data
                            typeResources.erase(resourceIt); //removing from cache
                            break;
                        }
                    }

                    //Step 5b: We clean up reference counting entry
                    refCounts.erase(it);
                }
            }
        }

        void UnloadAll(){
            //emergency cleanup methjod for system shutdown or major state changes
            for(auto& [type, typeResources] : resources){
                for(auto& [id, resource] : typeResources){
                    resource->Unload(); //ensuring all resources clean up properly
                }
                typeResources.clear(); //clearing type specific containers
            }

            refCounts.clear(); //resetting all reference counts
        }
};

/*
    How I can use ResourceManager in my engine:
        // Create resource manager
        ResourceManager resourceManager;

        // Load resources
        auto texture = resourceManager.Load<Texture>("brick");
        auto mesh = resourceManager.Load<Mesh>("cube");
        auto vertexShader = resourceManager.Load<Shader>("basic", vk::ShaderStageFlagBits::eVertex);
        auto fragmentShader = resourceManager.Load<Shader>("basic", vk::ShaderStageFlagBits::eFragment);

        // Use resources
        if (texture && mesh && vertexShader && fragmentShader) {
            // Create material using shaders
            Material material(vertexShader, fragmentShader);

            // Set texture in material
            material.SetTexture("diffuse", texture);

            // Create entity with mesh and material
            Entity entity("MyEntity");
            auto meshComponent = entity.AddComponent<MeshComponent>(mesh.Get(), &material);
        }

        // Resources will be automatically released when handles go out of scope
        // or you can explicitly release them
        resourceManager.Release(texture.GetId());

*/