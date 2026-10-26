#pragma once
#include <iostream>
#include <string>

/*
    The Resource base class provides the foundational contract that all resource types must fulfill. The resource ID serves as a unique identifier that allows the resource manager to locate and reference specific resources without ambiguity. 
    This string-based approach enables human-readable resource names like "main_character_texture" or "level_1_audio" while maintaining the flexibility to use file paths or other naming schemes.

    The loading state management through the boolean flag provides essential lifecycle tracking. This simple approach allows systems to quickly determine whether a resource is ready for use without expensive validation checks. 
    The virtual loading interface enables polymorphic behavior where different resource types can implement their own specialized loading logic while presenting a consistent interface to the management system.
*/
class Resource {
    private:
        std::string resourceId;
        bool loaded = false;
    public:
        explicit Resource(const std::string& id) : resourceId(id) {}
        virtual ~Resource() = default;

        //Core resource identity and state access methods
        const std::string& GetId() const {return resourceId; }
        bool IsLoaded() const {return loaded; }

        //Virtual interface for resource specific loading and unloading behavior
        bool Load() {
            loaded = doLoad();
            return loaded;
        }

        void Unload() {
            doUnload();
            loaded = false;
        }

    protected:
        virtual bool doLoad() = 0;
        virtual bool doUnload() = 0;
        
};