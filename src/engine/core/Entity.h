#pragma once //widely supported preprocessor directive in C and C++ designed to ensure that a header file is parsed only once during a single compilation
#include <iostream>
#include <vector>
#include <memory>
#include <utility>
#include <string>
#include <type_traits>

class Component; //we tell the compiler Component exists without needing to #include it. Prevents circular dependencies since Component.h also needs Entity.h

class Entity {
    private:
        std::string name;
        bool active = true;
        std::vector<std::unique_ptr<Component>> components;
        std::unordered_map<size_t, Component*> componentMap;

    public:
        //constructor and standard inline methods
        explicit Entity(const std::string& entityName) : name(entityName) {}

        const std::string& GetName() const {return name;}
        bool IsActive() const {return active;} //this function promises not to modify the object it belongs to(const). In essence, read only.
        void SetActive(bool isActive) {active = isActive; }

        //standard methods (We declare them here, and implement in the source file(.cpp))
        void Initialize();
        void Update(float deltaTime);
        void Render();
        
        //TEMPLATE METHODS(Must remain full implemented in header file)
        //two template parameters. T represents the component type. The ... means zero or more types(parameter pack). For instnace, AddComponent<Transform>() could have Args = {} while AddComponent<Transform>(10.0f, 20.0f, 30.0f) could have Args = {float, float, float}, etc..
        //&& is an rvalue declarator, it holds a reference to an rvalue expression.  An rvalue(rvalue) is  rvalue (right value) is an expression that represents a temporary value that does not have a persistent memory address. The easiest way to identify an rvalue is that you cannot take its memory address using the address-of operator (&).
        template<typename T, typename... Args>
        T* AddComponent(Args&&... args){ //Args&&... args is a parameter pack of forwarding references. Allows function to accept WHATEVER arguments the component's constructor needs. && is related to rvalue references, while the combination with a deduced template parameter makes these forwarding references.
            static_assert(std::is_base_of<Component, T>::value, "T MUST derive from Component");

            size_t typeID = Component::GetTypeID<T>();

            //checking if already exists and returning
            auto it = componentMap.find(typeID); //returns an iterator to the element matching the key. That is, if it is foundm, it returns an iterator pointing to the key-value pair otherwise it returns the end() iterator of the map
            if(it != componentMap.end()){
                return static_cast<T*>(it->second);//casting the value, not key(pointer)
            }

            //creating new component
            auto component = std::make_unique<T>(std::forward<Args>(args)...); //std::make_unique<T> creates a T dynamically and puts it inside a std::unique_ptr(smart pointer). I forward(std::forward<Args>(args)...) to pass the constructor arguments along while preserving whether they were originally lvalues or rvalues
            T* componentPtr = component.get();//retrieving the raw pointer that the smart pointer is managing
            componentMap[typeID] = componentPtr; //adding key value pair
            components.push_back(std::move(component)); //transfer ownership
            return componentPtr;
        }

        template<typename T>
        T* GetComponent(){
            size_t typeID = Component::GetTypeID<T>();
            auto it = componentMap.find(typeID);
            if(it != componentMap.end()){
                return static_cast<T*>(it->second);
            }

            return nullptr;

            //depricated
            // for(auto& component : components){
            //     //dynamic_cast is a runtime type conversion operator used in polymorphic inheritance hierarchies. It performs runtime type checking to ensure that a conversion is valid before it is applied.
            //     //commonly used for downcasting from a base class to a derived class. This can be costly, which is why I replace with ComponentTypeIDSystem.
            //     if(T* result = dynamic_cast<T*>(component.get())){
            //         return result;
            //     }
            // }
        }

        template<typename T>
        bool RemoveComponent(){
            //vector.begin() returns an iterator pointing to the first element of the vector
            size_t typeID = Component::GetTypeID<T>();
            auto it = componentMap.find(typeID);
            if(it != componentMap.end()){
                Component* componentPtr = it->second;
                componentMap.erase(it);//removing from map

                //removing from components vector
                for(auto compIt = components.begin(); compIt != components.end(); ++compIt){
                    if(compIt->get() == componentPtr){
                        components.erase(compIt);
                        return true;
                    }
                }
            }

            return false;
        }
};