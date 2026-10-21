#pragma once
#include <iostream>
#include <GLFW/glfw3.h>

class Window {
    private:
        static constexpr uint32_t WIDTH = 800;
        static constexpr uint32_t HEIGHT = 600;
        GLFWwindow *window = nullptr;
    
        void initWindow();
        void mainLoop();
        void cleanup();

    public:
        void run();
};