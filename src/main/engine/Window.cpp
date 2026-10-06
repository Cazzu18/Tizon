#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
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
        vk::raii::SurfaceKHR surface = nullptr;

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
            createInstance();
            setupDebugMessenger();
            createSurface();
            pickPhysicalDevice();
            createLogicalDevice();

        }

        void createSurface(){
            VkSurfaceKHR _surface; //temporary Vulkan C API handle. GLFW’s surface function writes the new handle into it.

            //glfwCreateWindowSurface(VkInstance, GLFW window pointer, custom allocators and pointer to a VkSurfaceKHR variable)
            //connects Vulkan instance to the GLFW window. *instance supplies the underlying Vulkan handle
            if(glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0){
                throw std::runtime_error("Failed to create window surface!");
            }
            surface = vk:raii::SurfaceKHR(instance, _surface); // wraps the raw handle in class member. That wrapper manages the Vulkan surface’s destruction. The next function uses surface to test whether a queue can present to this window.
        }

        void createLogicalDevice(){

            /* Choosing a queue family. */

            //A GPU exposes groups of queues called queue families. The loop finds the first family that supports graphics commands and presentation to surface
            
            //find the index of the first queue family that supports graphics
            std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

            //get the first index into queueFamiltyProperties which supports both graphics and present
            uint32_t queueIndex = ~0; //initializes the variable to 4294967295 (or 0xFFFFFFFF in hexadecimal). ~ is the Bitwise NOT that flips each of the 32 bits of that integer
            for(uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++){
                if((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface)){
                    //found a queue family that supports both graphics and present
                    queueIndex = qfpIndex;
                    break;
                }
            }

            /* End Choosing a queue family. */

            if(queueIndex == ~0){
                throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
            }


            /* Start Requesting features. */
            
            //requests for Vulkan 1.3 features
            //links several feature structures through their pNext pointers.
            //C++ tempolate type. The names inside <...> are types that tell vk::StructureChain which Vulkan structures to hold and in what order
            //vk::StructureChain<...> holds one object of each listed type and connects their pNext pointers automatically.
            vk::StructureChain<vk::PhysicalDeviceFeatures2,
		                   vk::PhysicalDeviceVulkan11Features,
		                   vk::PhysicalDeviceVulkan13Features,
		                   vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
		    featureChain = {
		        {},                                    // vk::PhysicalDeviceFeatures2
		        {.shaderDrawParameters = true},        // vk::PhysicalDeviceVulkan11Features
		        {.dynamicRendering = true},            // vk::PhysicalDeviceVulkan13Features
		        {.extendedDynamicState = true}         // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
		    };

            /* End Requesting features. */

            //create a Device
            float queuePriority = 0.5f;


            //declaring and initializing variables of type DeviceQueueCreateInfo and DeviceCreateInfo
            //requests one queue
            vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = queueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};
            
            //describing the device
            vk::DeviceCreateInfo      deviceCreateInfo{.pNext           = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
                                               .queueCreateInfoCount    = 1,
                                               .pQueueCreateInfos       = &deviceQueueCreateInfo,
                                               .enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size()),
                                               .ppEnabledExtensionNames = requiredDeviceExtension.data()};

            //create the logical device and retrieve its queue
            device = vk::raii::Device(physicalDevice, deviceCreateInfo);
            queue = vk::raii::Queue(device, queueIndex, 0);
                                    

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


