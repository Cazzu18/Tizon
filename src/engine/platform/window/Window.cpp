#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Window.h"

void Window::initWindow(){
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //do not create an OpenGL context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); //disabling window resizing for now
    
    //(width, height, title, specify a monitor to open the window on, ONLY RELEVANT to OPENGL)
    Window::window = glfwCreateWindow(Window::WIDTH, Window::HEIGHT, "Tizon Renderer", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);//storing an arbitrary pointer inside the reference of the GLFWwindow
    //glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

//static because GLFW does not know how to properly call a member function with the right this pointer to our Tizon instance
// static void window::framebufferResizeCallback(GLFWwindow* window, int width, int height){
//     auto app = reinterpret_cast<window*>(glfwGetWindowUserPointer(window));
//     app->framebufferResized = true;
// }

void Window::mainLoop(){
    while(!glfwWindowShouldClose(Window::window)){
        glfwPollEvents(); //processes all pending events currently sitting in the window system's event queue and returns immediately
    }
}

void Window::cleanup(){
    glfwDestroyWindow(Window::window);
    glfwTerminate();
}

void Window::run(){
    initWindow();
    mainLoop();
    cleanup();
}


