#pragma once
#include <cstddef>

class ComponentTypeIDSystem {
    private:
        static std::size_t nextTypeID;

    public:
        template<typename T>
        static std::size_t GetTypeID() {
            static std::size_t typeID = nextTypeID++;
            return typeID;
        }
};