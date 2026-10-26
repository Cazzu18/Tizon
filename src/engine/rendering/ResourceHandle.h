#pragma once
#include <iostream>
#include <string>

class ResourceManager;

/*
    Using handles instead of direct pointers provides several benefits:\
        Indirection - The resource manager can move resources in memory without invalidating references.
        Validation - Handles can be checked for validity before use.
        Automatic Resource Management - The resource manager can track which resources are in use.
*/

template<typename T>
class ResourceHandle {
    private:
        std::string resourceId;
        ResourceManager* resourceManager;

    public:

        ResourceHandle() : resourceManager(nullptr) {}

        ResourceHandle(const std::string& id, ResourceManager* manager) : resourceId(id), resourceManager(manager) {}

        T* Get() const {
            if(!resourceManager) return nullptr;
            return resourceManager->GetResource<T>(resourceId);
        }

        bool IsValid() const {
            return resourceManager && resourceManager->HasResource<T>(resourceId);
        }

        const std::string& GetId const {
            return resourceId;
        }

        //convenience operators
        T* operator->() const {
            return Get();
        }

        T& operator*() const {
            return *Get();
        }

        operator bool() const {
            return IsValid();
        }
};