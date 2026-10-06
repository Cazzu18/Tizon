#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <fmt/core.h>


/* In short, to draw the first triangle using VULKAN, we need to:
    - Create an Instance
    - Select a supported graphics card (PhysicalDevice)
    - Create a Device and Queue for drawing and presentation
    - Create a window, window surface and swap chain
    - Wrap the swap chain images into vk::raii::ImageView
    - Set up dynamic rendering
    - Set up the graphics pipeline
    - Allocate and record a command buffer with the draw commands for every possible swap chain image
    - Draw frames by acquiring images, submitting the right draw command buffer and returning the images to the swap chain 
*/

/*
 object creation generally follows this pattern:

    vk::XXXCreateInfo createInfo{};
    createInfo.sType = vk::StructureType::eXXXCreateInfo;
    createInfo.pNext = nullptr;
    createInfo.foo = ...;
    createInfo.bar = ...;

    vk::XXX object;


    try {
        object = device.createXXX(createInfo);
    } catch (vk::SystemError& err) {
        std::cerr << "Failed to create object: " << err.what() << std::endl;
        return false;
    }

    Almost all functions return a vk::Result that is either vk::Result::eSuccess or an error code.

*/

/*
 CMake Commands:
    1. cmake -S . -B build -G "Visual Studio 17 2022" -A x64 "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
    2. cmake --build build --config Debug
    3. .\build\Debug\Tizon.exe

    1.CMake reads CMakeLists.txt from the current folder(-S .) and creates build files in (-B build).
      -G chooses Visual Studio 2022's build system, and -A x64 targets 64-bit Windows.
      -D setting points CMake to vcpkg's toolchain. The toolchain uses vcpkg.json to make the declared dependencies available

    2. Uses the files CMake generated in build to compile C++ source and produce Tizon.exe
    3. running the program
*/

class Window{
    private:
        GLFWwindow *window = nullptr;

        //constexpr specified a variable, function, or constructor can be evaluated at compile time
        static constexpr uint32_t WIDTH = 800;
        static constexpr uint32_t HEIGHT = 600;

        void initWindow(){
            glfwInit();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //do not create an OpenGL context
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); //disabling window resizing for now
            
            //(width, height, title, specify a monitor to open the window on, ONLY RELEVANT to OPENGL)
            window = glfwCreateWindow(WIDTH, HEIGHT, "Tizon Renderer", nullptr, nullptr);
        }

        void initVulkan(){

        }

        void mainLoop(){
            while(!glfwWindowShouldClose(window)){
                glfwPollEvents(); //processes all pending events currently sitting in the window system's event queue and returns immediately
            }
        }

        void cleanup(){
            glfwDestroyWindow(window);
            glfwTerminate();
        }

    public:
        void run(){
            initWindow();
            initVulkan();
            mainLoop();
            cleanup();
        }
        
};

int main()
{
    try{
        Window app;
        app.run();
    }
    catch (const std::exception& e){
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


