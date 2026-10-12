#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstdint> //necessary for uint32_t
#include <limits> //necessary for std::numeric_limits
#include <algorithm> //necessary for std::clamp
#include <fstream>
#include <map>
#include <glm/glm.hpp> //linear algebra related types like vectors and matrices
#include <glm/gtc/matrix_transform.hpp> // exposes functions that can be used to generate model transformations like glm::rotate, view transformations like glm::lookAt and projection transformations like glm::perspective.
#include <chrono> //exposes functions to do precise timekeeping


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

/*
 SwapChain
    Just checking if a swap chain is available is not enough because it may not be compatible with our window surface. 
    Creating a swap chain also involves a lot more settings than instance and device creation, 
    so we need to query for some more details.

    There are basically three kinds of properties we need to check:
        - Basic surface capabilities (min/max number of images in swap chain, min/max width and height of images)
        - Surface formats (pixel format, color space)
        -Available presentation modes


    There may still be many different modes of varying optimality in the swap chain. There are three types of settings to determine:
        - Surface format (color depth)
        - Presentation mode (conditions for "swapping" images to the screen): 
        - Swap extent (resolution of images in swapchain)

    Presentation mode:
        The presentation mode is arguably the most important setting for the swap chain, 
        because it represents the actual conditions for showing images to the screen. 
        There are four possible modes available in Vulkan:

            - vk::PresentModeKHR::eImmediate: Images submitted by your application are transferred to the screen right away, which may result in tearing.

            - vk::PresentModeKHR::eFifo: The swap chain is a queue where the display takes an image from the front of the queue when the display is refreshed, and the program inserts rendered images at the back of the queue. 
                If the queue is full, then the program has to wait. This is most similar to vertical sync as found in modern games. The moment that the display is refreshed is known as "vertical blank".

            - vk::PresentModeKHR::eFifoRelaxed: This mode only differs from the previous one if the application is late and the queue was empty at the last vertical blank. 
                Instead of waiting for the next vertical blank, the image is transferred right away when it finally arrives. This may result in visible tearing.

            - vk::PresentModeKHR::eMailbox: This is another variation of the second mode. Instead of blocking the application when the queue is full, the images that are already queued are simply replaced with the newer ones. 
                This mode can be used to render frames as fast as possible while still avoiding tearing, resulting in fewer latency issues than standard vertical sync. 
                This is commonly known as "triple buffering," although the existence of three buffers alone does not necessarily mean that the framerate is unlocked.

            ONLY the vk::PresentModeKHR::eFifo mode is guaranteed to be available
*/

//I SHOULD also store multiple buffers, like the vertex and index buffer, into a SINGLE vk::raii::Buffer and use offsets in commands like vk::raii::CommandBuffer::bindVertexBuffers. The advantage is that data is more cache friendly in that case, because it’s closer together.
/*
    The vertex buffer we have right now works correctly, but the memory type that allows us to access it from the CPU may not be the most optimal memory type for the graphics card itself to read from. 
    The most optimal memory has the vk::MemoryPropertyFlagBits::eDeviceLocal flag and is usually not accessible by the CPU on dedicated graphics cards. 
    So, I'm going to create two vertex buffers. One staging buffer in CPU accessible memory to upload the data from the vertex array to, and the final vertex buffer in device local memory. 
    I’ll then use a buffer copy command to move the data from the staging buffer to the actual vertex buffer.

    The buffer copy command requires a queue family that supports transfer operations, which is indicated using vk::QueueFlagBits::eTransfer. 
    The good news is that any queue family with vk::QueueFlagBits::eGraphics or vk::QueueFlagBits::eCompute capabilities already implicitly support vk::QueueFlagBits::eTransfer operations. 
    The implementation is not required to explicitly list it in queueFlags in those cases.

    you’re not supposed to actually call vk::raii::Device::allocateMemory for every individual buffer. The maximum number of simultaneous memory allocations is limited by the maxMemoryAllocationCount physical device limit,
    which may be as low as 4096 even on high end hardware like an NVIDIA GTX 1080. The right way to allocate memory for a large number of objects at the same time is to create a custom allocator that splits up a single allocation
    among many different objects by using the offset parameters that we’ve seen in many functions.
    I can either implement such an allocator myself, or use the VulkanMemoryAllocator library provided by the GPUOpen initiative. 

*/

/*
    Index Buffers
        An index buffer is essentially an array of pointers into the vertex buffer. It allows you to reorder the vertex data, and reuse existing data for multiple vertices. 
*/

/*
    Descriptors layout and buffer

    A descriptor is a way for shaders to freely access resources like buffers and images. I'm going to set up a buffer that contains the transformation matrices
    and have the vertex shader access them through a descriptor. Usage of descriptors consists of three parts:
        Specify a descriptor set layout during pipeline creation
        Allocate a descriptor set from a descriptor pool
        Bind the descriptor set during rendering

    The descriptor set layout specifies the types of resources that are going to be accessed by the pipeline, just like a render pass specifies the types of attachments that will be accessed. 
    A descriptor set specifies the actual buffer or image resources that will be bound to the descriptors, just like a framebuffer specifies the actual image views to bind to render pass attachments. 
    The descriptor set is then bound for the drawing commands just like the vertex buffers and framebuffer.

    Uniform Buffer Objects (UBOs)

    We’re going to copy new data to the uniform buffer every frame(to make the 3D rectangle spin), so it doesn’t really make any sense to have a staging buffer. 
    It would just add extra overhead in this case and likely degrade performance instead of improving it.
    We should have multiple buffers, because multiple frames may be in flight at the same time and we don’t want to update the buffer in preparation of the next frame while a previous one is still reading from it! 
    Thus, we need to have AS MANY uniform buffers as we have frames in flight, and write to a uniform buffer that is not currently being read by the GPU.
*/

/*
    Descriptor pool and sets
        Descriptor sets can’t be created directly, they must be allocated from a pool like command buffers. The equivalent for descriptor sets is unsurprisingly called a descriptor pool. 

        ALIGNMENT:
            Vulkan expects the data in your structure to be aligned in memory in a specific way, for example:

                Scalars have to be aligned by N (= 4 bytes given 32-bit floats).

                A float2 must be aligned by 2N (= 8 bytes)

                A float3 or float4 must be aligned by 4N (= 16 bytes)

                A nested structure must be aligned by the base alignment of its members rounded up to a multiple of 16.

                A float4x4 matrix must have the same alignment as a float4.
            list: https://docs.vulkan.org/spec/latest/chapters/interfaces.html#interfaces-resources-layout

            My original shader with just three mat4 fields already met the alignment requirements. As each mat4 is 4 x 4 x 4 = 64 bytes in size, model has an offset of 0, view has an offset of 64 and proj has an offset of 128. 
            All of these are multiples of 16 and that’s why it worked fine.

            if we were to update our shader and Uniform buffer object to:

            struct UniformBufferObject
            {
                glm::mat4 model;
                glm::mat4 view;
                glm::mat4 proj;
            };

            struct UniformBuffer
            {
                float4x4 model;
                float4x4 view;
                float4x4 proj;
            };
            ConstantBuffer<UniformBuffer> ubo;

            Which changes offsets such that: starts with a vec2 which is only 8 bytes in size and therefore throws off all of the offsets. Now model has an offset of 8, view an offset of 72 and proj an offset of 136, none of which are multiples of 16. To fix this problem we can use the alignas specifier introduced in C++11:

            The solution would be:
                struct UniformBufferObject
                {
                    glm::vec2 foo;
                    alignas(16) glm::mat4 model;
                    glm::mat4 view;
                    glm::mat4 proj;
                };

            OR

            #define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES which will force GLM to use a version of vec2 and mat4 that has the alignment requirements already specified for us.
            Unfortunately, this method can break down if you start using nested structures.

        Multiple Descriptor Sets:
            As some of the structures and function calls hinted at, it is actually possible to bind multiple descriptor sets simultaneously. You need to specify a descriptor set layout for each descriptor set when creating the pipeline layout. 
            Shaders can then reference specific descriptor sets like this:
                struct UniformBuffer
                {
                };
                ConstantBuffer<UniformBuffer> ubo;
            You can use this feature to put descriptors that vary per-object and descriptors that are shared into separate descriptor sets. In that case, you avoid rebinding most of the descriptors across draw calls which are potentially more efficient.
*/

/*
    readFile function will read all the bytes from the specified file and return them in a byte array managed by std::vector. 
    We start by opening the file with two flags:
        ate: Start reading at the end of the file
        binary: Read the file as a binary file (avoid text transformations)

    The advantage of starting to read at the end of the file is that we can use the read position to determine the size of the file and allocate a buffer:
*/

struct Vertex{
    glm::vec2 pos;
    glm::vec3 color;

    /*
    telling Vulkan how to pass this data format to the vertex shader once it’s been uploaded into GPU memory.
    A vertex binding describes at which rate to load data from memory throughout the vertices. It specifies the number of bytes between data entries and whether to move to the next data entry after each vertex or after each instance.
    All of our per-vertex data is packed together in one array, so we’re only going to have one binding. 

    The binding parameter specifies the index of the binding in the array of bindings. 
    The stride parameter specifies the number of bytes from one entry to the next, and the inputRate parameter can have one of the following values:
        vk::VertexInputRate::eVertex : Move to the next data entry after each vertex
        vk::VertexInputRate::eInstance: Move to the next data entry after each instance

    */
    static vk::VertexInputBindingDescription getBindingDescription()
    {
        return {.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
    }

    /*
        As the function prototype indicates, there are going to be two of these structures. An attribute description struct describes how to extract a vertex attribute from a chunk of vertex data originating from a binding description. 
        We have two attributes, position and color, so we need two attribute description structs.

        The location parameter references the location directive of the input in the vertex shader. The input in the vertex shader with location 0 is the position, which has two 32-bit float components. T
        he binding parameter tells Vulkan from which binding the per-vertex data comes.
        The format parameter describes the type of data for the attribute. 
        
        A bit confusingly, the formats are specified using the same enumeration as color formats. The following shader types and formats are commonly used together:
            float : vk::Format::eR32Sfloat

            float2: vk::Format::eR32G32Sfloat

            float3: vk::Format::eR32G32B32Sfloat

            float4: vk::Format::eR32G32B32A32Sfloat

        The format parameter implicitly defines the byte size of attribute data and the offset parameter has specified the number of bytes since the start of the per-vertex data to read from. 
        The binding is loading one Vertex at a time and the position attribute (pos) is at an offset of 0 bytes from the beginning of this struct. 
    */
    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
       return {{{.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, pos)},
                {.location = 1, .binding = 0, .format = vk::Format:: eR32G32B32Sfloat, .offset = offsetof(Vertex, color)}}};
    }
};


struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

//clockwise
const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, //top left
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, //top right
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}, // bottom right
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}} //bottom left
};

//2 bytes
const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0
};

static std::vector<char> readFile(const std::string& filename){
    std::ifstream file(filename, std::ios::ate | std::ios::binary); //open the file in binary mode and start reading from the end of the file (ate)

    if(!file.is_open()){
        throw std::runtime_error("failed to open file: " + filename);
    }

    std::vector<char> buffer(file.tellg()); //tellg() returns the current position of the get pointer, which is at the end of the file because we opened it with std::ios::ate.

    //seek back to the beginning of the file and read all bytes at once
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    return buffer;
}

//Desfining how many frames should be processed concurrently. That is, allow the rendering of one frame to not interfere with the recording of the next
//we choose 2 because although we have 3 images, we don't want the CPU to get too dar ahead of the GPU
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

class Window{
    private:
        GLFWwindow *window = nullptr;
        
        uint32_t queueIndex = ~0; //initializes the variable to 4294967295 (or 0xFFFFFFFF in hexadecimal). ~ is the Bitwise NOT that flips each of the 32 bits of that integer
        //vk::KHRSwapChainExtensionName is a constant that evaluates to the string "VK_KHR_swapchain", which is the name of the Vulkan extension that enables swap chain functionality. The swap chain is a series of images that are presented to the screen in a loop, allowing for smooth rendering and display of graphics.
        //needed to check if the physical device supports the swap chain extension, which is required for presenting images to the window surface.
        std::vector<const char*> requiredDeviceExtension = {
            vk::KHRSwapchainExtensionName
        }; //list of required device extensions.
    
        vk::raii::Context context;
        vk::raii::Instance instance = nullptr;

        vk::raii::PhysicalDevice physicalDevice = nullptr;
        vk::raii::Device device = nullptr;
        vk::raii::Queue queue{nullptr};
        vk::raii::SurfaceKHR surface = nullptr;
         vk::raii::SwapchainKHR swapChain = nullptr;
        std::vector<vk::Image> swapChainImages;
        vk::SurfaceFormatKHR   swapChainSurfaceFormat;
        vk::Extent2D           swapChainExtent;
        std::vector<vk::raii::ImageView> swapChainImageViews;
        vk::raii::PipelineLayout pipelineLayout = nullptr;
        vk::raii::Pipeline graphicsPipeline = nullptr;
        vk::raii::CommandPool commandPool = nullptr;

        //each frame should have its own CB, set of semaphores, and fence
        std::vector<vk::raii::CommandBuffer> commandBuffers;
        //semaphores for GPU synchronization and Fence to keep GPU and HOST(CPU) in sync
        std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
        std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
        std::vector<vk::raii::Fence> inFlightFences;

        std::vector<vk::PresentModeKHR> availablePresentModes;

        bool framebufferResized = false;

        vk::raii::Buffer vertexBuffer = nullptr;
        vk::raii::DeviceMemory vertexBufferMemory = nullptr;
        vk::raii::Buffer indexBuffer = nullptr;
        vk::raii::DeviceMemory indexBufferMemory = nullptr;

        std::vector<vk::raii::Buffer> uniformBuffers;
        std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
        std::vector<void *> uniformBuffersMapped; // void * is a generic pointer that can hold the memory address of an object of ANY datatype

        vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
        vk::raii::DescriptorPool descriptorPool = nullptr;
        std::vector<vk::raii::DescriptorSet> descriptorSets;

        //frame index
        uint32_t frameIndex = 0;

        //constexpr specified a variable, function, or constructor can be evaluated at compile time
        static constexpr uint32_t WIDTH = 800;
        static constexpr uint32_t HEIGHT = 600;

        void initWindow(){
            glfwInit();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //do not create an OpenGL context
            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); //disabling window resizing for now
            
            //(width, height, title, specify a monitor to open the window on, ONLY RELEVANT to OPENGL)
            window = glfwCreateWindow(WIDTH, HEIGHT, "Tizon Renderer", nullptr, nullptr);
            glfwSetWindowUserPointer(window, this);//storing an arbitrary pointer inside the reference of the GLFWwindow
            glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
        }

        //static because GLFW does not know how to properly call a member functino with the right this pointer to our Tizon instance
        static void framebufferResizeCallback(GLFWwindow* window, int width, int height){
            auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
            app->framebufferResized = true;
        }

        void initVulkan(){
            createInstance();
            //setupDebugMessenger();
            createSurface();
            pickPhysicalDevice();
            createLogicalDevice();
            createSwapChain();
            createImageViews();
            createDescriptorSetLayout();
            createGraphicsPipeline();
            createCommandPool();
            createVertexBuffer();
            createIndexBuffer();
            createUniformBuffers();
            createDescriptorPool();
            createDescriptorSets();
            createCommandBuffers();
            createSyncObjects();
        }

        bool isDeviceSuitable( vk::raii::PhysicalDevice const & physicalDevice )
        {
            //Checking if the physicalDevice supports the Vulkan 1.3 API version
            bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;

            //Checking if any of the queue families support graphics operations
            auto queueFamilies    = physicalDevice.getQueueFamilyProperties();
            bool supportsGraphics = std::ranges::any_of( queueFamilies, []( auto const & qfp ) { return !!( qfp.queueFlags & vk::QueueFlagBits::eGraphics ); } );

            //Checking if all required physicalDevice extensions are available
            auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
            bool supportsAllRequiredExtensions =
                std::ranges::all_of( requiredDeviceExtension,
                                    [&availableDeviceExtensions]( auto const & requiredDeviceExtension )
                                    {
                                    return std::ranges::any_of( availableDeviceExtensions,
                                                                [requiredDeviceExtension]( auto const & availableDeviceExtension )
                                                                { return strcmp( availableDeviceExtension.extensionName, requiredDeviceExtension ) == 0; } );
                                    } );

            //Checking if the physicalDevice supports the required features (shader draw parameters, dynamic rendering and extended dynamic state)
            auto features                 = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2,
                                                                                vk::PhysicalDeviceVulkan11Features,
                                                                                vk::PhysicalDeviceVulkan13Features,
                                                                                vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
            bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
                                            features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                            features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

            //Returning true if the physicalDevice meets all the criteria
            return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
        }

        void pickPhysicalDevice()
        {
            std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
            auto const devIter = std::ranges::find_if( physicalDevices, [&]( auto const & physicalDevice ) { return isDeviceSuitable( physicalDevice ); } );
            if ( devIter == physicalDevices.end() )
            {
                throw std::runtime_error( "failed to find a suitable GPU!" );
            }
            physicalDevice = *devIter;
        }

        void createInstance(){
            constexpr vk::ApplicationInfo appInfo{
                .pApplicationName = "Edmund's Triangle in Vulkan",
                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                .pEngineName = "No Engine",
                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                .apiVersion = vk::ApiVersion14}; //vulkan 1.4 as baseline

            //Getting the required instance extensions from GLFW
            uint32_t glfwExtensionCount = 0;

            //Retrieves platform-specific Window System Integration (WSI) extensions (such as VK_KHR_surface and surface extensions for Win32, X11, Wayland, or macOS
            //Returns  An array of ASCII-encoded extension names, or NULL if Vulkan is unavailable on the machine.
            auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            //Check if the required GLFW extensions are supported by the Vulkan implementation
            auto extensionProperties = context.enumerateInstanceExtensionProperties();
            for(uint32_t i = 0; i < glfwExtensionCount; i++){
                if(std::ranges::none_of(extensionProperties, [glfwExtension = glfwExtensions[i]](auto const& extensionProperty)
                    {return strcmp(extensionProperty.extensionName, glfwExtension) == 0;})){
                        throw std::runtime_error("Required GLFW extension not supported: " + std::string(glfwExtensions[i]));
                    }
            }

            vk::InstanceCreateInfo createInfo{.pApplicationInfo = &appInfo, .enabledExtensionCount = glfwExtensionCount, .ppEnabledExtensionNames = glfwExtensions};

            instance = vk::raii::Instance(context, createInfo);
        }

        void createSwapChain(){
            vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
            swapChainExtent = chooseSwapExtent(surfaceCapabilities);
            uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);


            std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
            swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);
            
            /*
                .imageArrayLayers = 1, specifies the number of layers each image consists of. This is always 1 unless we are developing a stereoscopic 3D application
                .imageUsage = vk::IMageUsageFlagBits::eColorAttachment, specifies what kind of operations we'll usee the images in the swap chain for. For now, since I'm rendering directly to the images, they're used as a color attachement
                .imageSharingMode = vk::SharingMode::eExclusive, specifies how to handle swap chain images that might be used across multiple queue families. There are two ways to handle these: 1. eExclusive, where an image is owned by on queue family at a time, and owenership must be explicitly transferred before using it in another queue family from multuple queues. 2. eConcurrent, Images can be used across multiple queue families without explicit ownership transfers
                .preTransform = surfaceCapabilities.currentTransform, specifies that a certain transforms should be applied to images in the swapchain if it is supported(supportedTransforms in capabilites), like 90 degree clockwise rotation or horizontal flip. To specift you DO NOT want any transform simply specify the current transformation
                .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque, specifies if the alpha channel should be used for blending iwth other windows in the window system. Almost always ignore the alpha channel
                .presentMode = chooseSwapPresentMode(availablePresentModes)
                .clipped = if set to vk::True, specifies that we don't care about the color of pixels that are obscured, for example, because another window is in front of them.

            */
            vk::SwapchainCreateInfoKHR swapChainCreateInfo{.surface          = *surface,
                                               .minImageCount    = minImageCount,
                                               .imageFormat      = swapChainSurfaceFormat.format,
                                               .imageColorSpace  = swapChainSurfaceFormat.colorSpace,
                                               .imageExtent      = swapChainExtent,
                                               .imageArrayLayers = 1,
                                               .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
                                               .imageSharingMode = vk::SharingMode::eExclusive,
                                               .preTransform     = surfaceCapabilities.currentTransform,
                                               .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
                                               .presentMode      = chooseSwapPresentMode(availablePresentModes),
                                               .clipped          = true};
            swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
            swapChainImages = swapChain.getImages();

        };

        /*
            That’s all it takes to recreate the swap chain! However, the disadvantage of this approach is that we need to stop all renderings before creating the new swap chain. 
            It is possible to create a new swap chain while drawing commands on an image from the old swap chain are still in-flight. 
            You need to pass the previous swap chain to the oldSwapchain field in the vk::SwapchainCreateInfoKHR struct and destroy the old swap chain as soon as you’ve finished using it.
        */
        void cleanupSwapChain(){
            swapChainImageViews.clear();
            swapChain = nullptr;
        }

        void recreateSwapChain(){

            //handlling window minimization where swap chain may become out of date.
            //it will resilt in a framebuffer size of 0
            int width = 0, height = 0;
            glfwGetFramebufferSize(window, &width, &height); //handles the case where the size is already correct and glfwWaitEvents would have nothing to wait on
            
            //The glfwWindowShouldClose check handles the case where the window is closed while still minimized - without it, the loop would spin forever waiting for a frame buffer size that will never become nonzero again.
            while((width == 0 || height == 0) && !glfwWindowShouldClose(window)){
                glfwGetFramebufferSize(window, &width, &height);
                glfwWaitEvents();
            }

            if(glfwWindowShouldClose(window)){
                return;
            }

            device.waitIdle(); //we shouldn't touch resources that are in use!

            cleanupSwapChain();

            createSwapChain();
            createImageViews();
        }

        //parameters for image view creation are specified in a vk::ImageViewCreateInfo structure.
        void createImageViews(){
            assert(swapChainImageViews.empty()); //check that the vector is empty before we start adding image views to it
            

            //he components field allows you to swizzle the color channels around. For example, you can map all the channels to the red channel for a monochrome texture. You can also map constant values of 0 and 1 to a channel. 
            vk::ImageViewCreateInfo imageViewCreateInfo{
                .viewType = vk::ImageViewType::e2D, //specifies that the image view will be a 2D texture. This is appropriate for most images used in rendering, such as color attachments or textures.
                .format = swapChainSurfaceFormat.format, //specifies  how the colorspace components are configured so you get the right color format in the renders
                .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1} //The subresourceRange field describes what the image’s purpose is and which part of the image should be accessed. My images will be used as color targets without any mipmapping levels or multiple layers. In this case, we are accessing the color aspect of the image, starting from mip level 0 and layer 0, and accessing only one mip level and one layer.
            };

            for(auto &image : swapChainImages){
                imageViewCreateInfo.image = image;
                swapChainImageViews.emplace_back(device, imageViewCreateInfo);
            }
        }


        void createSurface(){
            VkSurfaceKHR _surface; //temporary Vulkan C API handle. GLFW’s surface function writes the new handle into it.

            //glfwCreateWindowSurface(VkInstance, GLFW window pointer, custom allocators and pointer to a VkSurfaceKHR variable)
            //connects Vulkan instance to the GLFW window. *instance supplies the underlying Vulkan handle
            if(glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0){
                throw std::runtime_error("Failed to create window surface!");
            }
            surface = vk::raii::SurfaceKHR(instance, _surface); // wraps the raw handle in class member. That wrapper manages the Vulkan surface’s destruction. The next function uses surface to test whether a queue can present to this window.
        }

        void createDescriptorSetLayout(){
            /*
                The first two fields specify the binding used in the shader and the type of descriptor, which is a uniform buffer object. It is possible for the shader variable to represent an array of uniform buffer objects, and descriptorCount specifies the number of values in the array. 
                This could be used to specify a transformation for each of the bones in a skeleton for skeletal animation, for example. Our MVP transformation is in a single uniform buffer object, so we’re using a descriptorCount of 1.
                We also need to specify in which shader stages the descriptor is going to be referenced. The stageFlags field can be a combination of vk::ShaderStageFlagBits values or the value vk::ShaderStageFlagBits::eAllGraphics. 
                In our case, we’re only referencing the descriptor from the vertex shader.

                All the descriptor bindings are combined into a single vk::raii::DescriptorSetLayout object.

            */
            vk::DescriptorSetLayoutBinding uboLayoutBinding{
                .binding = 0, .descriptorType = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eVertex
            };

            vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = 1, .pBindings = &uboLayoutBinding};
            
            // /We need to specify the descriptor set layout during pipeline creation to tell Vulkan which descriptors the shaders will be using. Descriptor set layouts are specified in the pipeline layout object.
            descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
        }

        void createGraphicsPipeline(){
            auto shaderCode = readFile(TIZON_SHADER_PATH);
            vk::raii::ShaderModule shaderModule = createShaderModule(shaderCode);
            
            vk::PipelineShaderStageCreateInfo vertShaderStageInfo{.stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule, .pName = "vertMain"};
            vk::PipelineShaderStageCreateInfo fragShaderStageInfo{.stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain"};
            
            //array containing these two structs
            vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

            //dynamic state allows us to change certain aspects of the pipeline without recreating it. For example, we can change the viewport and scissor rectangle dynamically during rendering. This is useful for applications that need to render to different parts of the screen or use multiple viewports.
            std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
            vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data()};

            /*
            The vk::PipelineVertexInputStateCreateInfo structure describes the format of the vertex data that will be passed to the vertex shader. It describes this in roughly two ways:
                Bindings: spacing between data and whether the data is per-vertex or per-instance (see instancing)
                Attribute descriptions: type of the attributes passed to the vertex shader, which binding to load them from and at which offset
            */
           auto bindingDescription = Vertex::getBindingDescription();
           auto attributeDescriptions = Vertex::getAttributeDescriptions();
            vk::PipelineVertexInputStateCreateInfo vertexInputInfo{.vertexBindingDescriptionCount = 1,
                                                                   .pVertexBindingDescriptions = &bindingDescription,
                                                                   .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
                                                                   .pVertexAttributeDescriptions = attributeDescriptions.data()

            };

            /*
            Input assembly:
                    The vk::PipelineInputAssemblyStateCreateInfo struct describes two things: what kind of geometry will be drawn from the vertices and if primitive restart should be enabled. The former is specified in the topology member and can have values like:

                        vk::PrimitiveTopology::ePointList: points from vertices
                        vk::PrimitiveTopology::eLineList: line from every two vertices without reuse
                        vk::PrimitiveTopology::eLineStrip: the end vertex of every line is used as start vertex for the next line
                        vk::PrimitiveTopology::eTriangleList: triangle from every three vertices without reuse
                        vk::PrimitiveTopology::eTriangleStrip: the second and third vertex of every triangle is used as first two vertices of the next triangle

                    Normally, the vertices are loaded from the vertex buffer by index in sequential order, but with an element buffer you can specify the indices to use yourself. This allows you to perform optimizations like reusing vertices.
            */
            vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};

            /*
                Viewports and Scissors
                    A viewport basically describes the region of the framebuffer that the output will be rendered to. This will almost always be (0, 0) to (width, height)

                    While viewports define the transformation from the image to the framebuffer, scissor rectangles define in which region pixels will actually be stored. 
                    The rasterizer will discard any pixels outside the scissored rectangles.
            */
            vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f};
            vk::Rect2D scissor{vk::Offset2D{0 , 0}, swapChainExtent}; //scissor rectangle starts at (0, 0) and has the same width and height as the viewport
            vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .pViewports = &viewport, .scissorCount = 1, .pScissors = &scissor};

            /*
                Rasterizer
                    The rasterizer takes the geometry shaped by the vertices from the vertex shader and turns it into fragments to be colored by the fragment shader. 
                    It also performs depth testing, face culling and the scissor test, and it can be configured to output fragments that fill entire polygons or just the edges (wireframe rendering).
                    All this is configured using the vk::PipelineRasterizationStateCreateInfo structure.

                    If depthClampEnable is set to vk::True, then fragments that are beyond the near and far planes are clamped to them as opposed to discarding them. This is useful in some special cases like shadow maps. 
                    If rasterizerDiscardEnable is set to vk::True, then geometry never passes through the rasterizer stage. This basically disables any output to the framebuffer.
                    The polygonMode determines how fragments are generated for geometry. The following modes are available:
                        vk::PolygonMode::eFill: fill the area of the polygon with fragments
                        vk::PolygonMode::eLine: polygon edges are drawn as lines
                        vk::PolygonMode::ePoint: polygon vertices are drawn as points
                    The cullMode variable determines the type of face culling to use. You can disable culling, cull the front faces, cull the back faces or both.
                    The frontFace variable specifies the vertex order for the faces to be considered front-facing and can be clockwise or counterclockwise.
                    The lineWidth member is straightforward, it describes the thickness of lines in terms of number of fragments. The maximum line width that is supported depends on the hardware and any line thicker than 1.0f requires you to enable the wideLines GPU feature.
            */
            vk::PipelineRasterizationStateCreateInfo rasterizer{
                .depthClampEnable = vk::False,
                .rasterizerDiscardEnable = vk::False,
                .polygonMode = vk::PolygonMode::eFill,
                .cullMode = vk::CullModeFlagBits::eBack,
                .frontFace = vk::FrontFace::eCounterClockwise, //changed to counter clockwise because of the Y-flip we did in the projection matrix
                .depthBiasEnable = vk::False,
                .lineWidth = 1.0f
            };

            /*
                Multisampling
                    The vk::PipelineMultisampleStateCreateInfo struct configures multisampling, which is one of the ways to perform antialiasing. 
                    It works by combining the fragment shader results of multiple polygons that rasterize to the same pixel. 
                    This mainly occurs along edges, which is also where the most noticeable aliasing artifacts occur. 
                    Because it doesn’t need to run the fragment shader multiple times if only one polygon maps to a pixel, it is significantly less expensive than simply rendering to a higher resolution and then downscaling. 
                    Enabling it requires enabling a GPU feature.

                    vk::SampleCountFlagBits::e1 is a Vulkan enumeration token used in the Vulkan-Hpp C++ bindings to specify that a resource (like an image or a framebuffer) uses 1 sample per pixel
            */
            vk::PipelineMultisampleStateCreateInfo multisampling{.rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False};

            /*
                Depth and stencil testing
                    If we are using a depth and/or stencil buffer, then we also need to configure the depth and stencil tests using vk::PipelineDepthStencilStateCreateInfo.
                    We don’t have one right now, so we can simply pass a nullptr instead of a pointer to such a struct.
            */

            /*
                Color blending
                    After a fragment shader has returned a color, it needs to be combined with the color that is already in the framebuffer. This transformation is known as color blending, and there are two ways to do it:
                        Mix the old and new value to produce a final color
                        Combine the old and new value using a bitwise operation

                    There are two types of structs to configure color blending. The first struct, vk::PipelineColorBlendAttachmentState contains the configuration per attached framebuffer and the second struct, vk::PipelineColorBlendStateCreateInfo contains the global color blending settings. In our case, we only have one framebuffer:
            */

            vk::PipelineColorBlendAttachmentState colorBlendAttachment{
                .blendEnable = vk::False,
                .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
            };

            //alpha blending
            // vk::PipelineColorBlendAttachmentState colorBlendAttachment{
            //     .blendEnable         = vk::True,
            //     .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
            //     .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
            //     .colorBlendOp        = vk::BlendOp::eAdd,
            //     .srcAlphaBlendFactor = vk::BlendFactor::eOne,
            //     .dstAlphaBlendFactor = vk::BlendFactor::eZero,
            //     .alphaBlendOp        = vk::BlendOp::eAdd,
            //     .colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
            
            vk::PipelineColorBlendStateCreateInfo colorBlending{
                .logicOpEnable = vk::False, .logicOp = vk::LogicOp::eCopy, .attachmentCount = 1, .pAttachments = &colorBlendAttachment};
            

            /*
                Pipeline Layout
                    You can use uniform values in shaders, which are globals similar to dynamic state variables that can be changed at drawing time to alter the behavior of your shaders without having to recreate them. 
                    They are commonly used to pass the transformation matrix to the vertex shader, or to create texture samplers in the fragment shader.
                    These uniform values need to be specified during pipeline creation by creating a vk::PipelineLayout object.
            */
            //see class member declaration for pipelineLayout

            vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 1, .pSetLayouts = &*descriptorSetLayout, .pushConstantRangeCount = 0};
            pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

            //dynamic rendering allows us to render without creating a render pass and framebuffer objects. Instead, we can specify the attachments and their formats directly in the graphics pipeline. This is useful for applications that need to render to different targets or use multiple render passes.
            vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{.colorAttachmentCount = 1, .pColorAttachmentFormats = &swapChainSurfaceFormat.format};//one color attachement with the format of our swap chainm images
            vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
                {.stageCount = 2,
                .pStages             = shaderStages,
                .pVertexInputState   = &vertexInputInfo,
                .pInputAssemblyState = &inputAssembly,
                .pViewportState      = &viewportState,
                .pRasterizationState = &rasterizer,
                .pMultisampleState   = &multisampling,
                .pColorBlendState    = &colorBlending,
                .pDynamicState       = &dynamicState,
                .layout              = pipelineLayout,
                .renderPass          = nullptr}, //because we are using dynamic rendering
                {
                    .colorAttachmentCount = 1, 
                    .pColorAttachmentFormats = &swapChainSurfaceFormat.format
                }
            };

            graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
        }

        void createCommandPool(){
            /*
                There are two possible flags for command pools:
                    vk::CommandPoolCreateFlagBits::eTransient: Hint that command buffers are rerecorded with new commands very often (may change memory allocation behavior)
                    vk::CommandPoolCreateFlagBits::eResetCommandBuffer: Allow command buffers to be rerecorded individually, without this flag they all have to be reset together

                Command buffers are executed by submitting them on one of the device queues, like the graphics and presentation queues we retrieved. 
                Each command pool can only allocate command buffers that are submitted on a single type of queue. 
                We’re going to record commands for drawing, which is why we’ve chosen the graphics queue family.
            */

            vk::CommandPoolCreateInfo poolInfo{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer, .queueFamilyIndex = queueIndex};

            commandPool = vk::raii::CommandPool(device, poolInfo);
        }

        //The typeFilter parameter will be used to specify the bit field of memory types that are suitable. That means that we can find the index of a suitable memory type by simply iterating over them and checking if the corresponding bit is set to 1.
        uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties){
            vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
            for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++){
                //We may have more than one desirable property, so we should check if the result of the bitwise AND is not just non-zero, but equal to the desired properties bit field. 
                //If there is a memory type suitable for the buffer that also has all the properties we need, then we return its index, otherwise we throw an exception.
                //memProperties.memoryTypes[i].propertyFlags & properties returns, in bits, the result of the and, so we need to check if that result is the same as properties
                if((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties){
                    return i;
                }
            }

            throw std::runtime_error("failed to find suitable memory type!");
        }
        
        //we take reference instead of copies
        void copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size){
            vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};
            vk::raii::CommandBuffer commandCopyBuffer = std::move(device.allocateCommandBuffers(allocInfo).front()); //extracting the first commandbuffer

            //We’re only going to use the command buffer once and wait with returning from the function until the copy operation has finished executing. It’s good practice to tell the driver about our intent using vk::CommandBufferUsageFlagBits::eOneTimeSubmit.
            commandCopyBuffer.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
            
            //Contents of buffers are transferred using the vk:raii::CommandBuffer::copyBuffer command. It takes the source and destination buffers as arguments, and an array of regions to copy. 
            //The regions are defined in vk::BufferCopy structs and consist of a source buffer offset, destination buffer offset and size. It is not possible to specify vk::WholeSize here, unlike the vk::raii::DeviceMemory::mapMemory command.
            commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy(0, 0, size));

            commandCopyBuffer.end();//since the cb only contains the copy command
            queue.submit(vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandCopyBuffer}, nullptr);
            
            /*
                There are again two possible ways to wait on this transfer to complete. We could use a fence and wait with vk::raii::Device::waitForFences, 
                or simply wait for the transfer queue to become idle with vk::raii::Queue::waitIdle. 
                A fence would allow you to schedule multiple transfers simultaneously and wait for all of them complete, instead of executing one at a time. T
                hat may give the driver more opportunities to optimize.
            */
            queue.waitIdle();

        }

        std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties){
            /*
                The size field specifies the size of the buffer in bytes.
                The usage field indicates for which purposes the data in the buffer is going to be used. It is possible to specify multiple purposes using a bitwise or.
                
                Just like the images in the swap chain, buffers can also be owned by a specific queue family or be shared between multiple at the same time. 
                The buffer will only be used from the graphics queue, so we can stick to exclusive access.


            */
            vk::BufferCreateInfo bufferInfo{.size = size,
                                            .usage = usage,
                                            .sharingMode = vk::SharingMode::eExclusive                                
            };

            vk::raii::Buffer buffer = vk::raii::Buffer(device, bufferInfo);
            vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

            //The most optimal memory has the vk::MemoryPropertyFlagBits::eDeviceLocal flag and is usually not accessible by the CPU on dedicated graphics cards.
            vk::MemoryAllocateInfo memoryAllocateInfo{
                .allocationSize = memRequirements.size,
                .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)
            };

            vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(device, memoryAllocateInfo);
            buffer.bindMemory(*bufferMemory, 0); //0 is the offset withing the region  of memory. If the offset is non-zero, then it is required to be divisible by memRequirements.alignment.

            //std::move converts an object with a name into an rvalue reference, enabling move semantics without moving any data at runtime
            //used to indicate that an object may be "moved from"
            return {std::move(buffer), std::move(bufferMemory)};
        }


        //requires me to fill the vk::BufferCreateInfo struct
        void createVertexBuffer(){
            
            /*
                Unfortunately, the driver may not immediately copy the data into the buffer memory, for example, because of caching. 
                It is also possible that writes to the buffer are not visible in the mapped memory yet. There are two ways to deal with that problem:
                    Use a memory heap that is host coherent, indicated with vk::MemoryPropertyFlagBits::eHostCoherent: ensures that the mapped memory always matches the contents of the allocated memory.(may lead to slightly worse performance than explicit flushing)
                    Call vk::raii::Device::flushMappedMemoryRanges after writing to the mapped memory, and call vk::raii::Device::invalidateMappedMemoryRanges before reading from the mapped memory


                We are using a host visible buffer(eHostVisible) as TEMPORARY buffer and use a device local one as ACTUAL vertex buffer.
                BECAUSE the most optimal memory has the vk::MemoryPropertyFlagBits::eDeviceLocal flag and is usually not accessible by the CPU on dedicated graphics cards

                vk::BufferUsageFlagBits::eTransferSrc: Buffer can be used as source in a memory transfer operation.
                vk::BufferUsageFlagBits::eTransferDst: Buffer can be used as destination in a memory transfer operation.
            */
            //This function allows us to access a region of the specified memory resource defined by an offset and size.
            vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

            auto [stagingBuffer, stagingBufferMemory] = createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            
            void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
            memcpy(dataStaging, vertices.data(), bufferSize);
            stagingBufferMemory.unmapMemory();

            std::tie(vertexBuffer, vertexBufferMemory) = createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

            copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
        }

        void createIndexBuffer(){
            //We create a staging buffer to copy the contents of indices to and then copy it to the final device local index buffer.
            vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

            auto [stagingBuffer, stagingBufferMemory] = createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            
            void* data = stagingBufferMemory.mapMemory(0, bufferSize);
            memcpy(data, indices.data(), (size_t) bufferSize);
            stagingBufferMemory.unmapMemory();

            //std::tie is a function template that creates a std::tuple of Ivalue references to its arguments
            //it unpacks tuples/pairs, which is the return type of createBuffer
            std::tie(indexBuffer, indexBufferMemory) = createBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

            copyBuffer(stagingBuffer, indexBuffer, bufferSize);
        }

        void createUniformBuffers(){
            for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
                vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
                auto[buffer, bufferMem] = createBuffer(
                    bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
                );
                uniformBuffers.emplace_back(std::move(buffer));
                uniformBuffersMemory.emplace_back(std::move(bufferMem));
                
                //We map the buffer right after creation using vk::raii::DeviceMemory::mapMemory to get a pointer to which we can write the data later on. 
                //The buffer stays mapped to this pointer for the application’s whole lifetime. This technique is called "persistent mapping" and works on all Vulkan implementations.
                uniformBuffersMapped.emplace_back(uniformBuffersMemory.back().mapMemory(0, bufferSize));///.back() returns a reference to the last element in the vector. .mapMemory(offset, size) maps the device memory into application address space
            }
        }

        void updateUniformBuffer(uint32_t currentImage){
            static auto startTime = std::chrono::high_resolution_clock::now();

            auto currentTime = std::chrono::high_resolution_clock::now();

            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            UniformBufferObject ubo{};
            //The model rotation will be a simple rotation around the Z-axis using the time variable
            //Using a rotation angle of time * glm::radians(90.0f) accomplishes the purpose of rotating 90 degrees per second.
            ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

            //For the view transformation I’ve decided to look at the geometry from above at a 45 degree angle. The glm::lookAt function takes the eye position, center position and up axis as parameters.
            //we dont want the camera at the origin because we'd be inside the shape. we want to look at the origin so 0.0f, 0.0f, 0.f, and well look from above at a 45degree angle
            ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

            //perspective(fov, aspect ratio, near plane, far plane)
            //It is important to use the current swap chain extent to calculate the aspect ratio to take into account the new width and height of the window after a resize.
            ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height), 0.1f, 10.0f);

            //Using a UBO this way is not the most efficient way to pass frequently changing values to the shader. A more efficient way to pass a small buffer of data to shaders is push constants.
            memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));

        }

        void createDescriptorPool(){
            vk::DescriptorPoolSize poolSize{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = MAX_FRAMES_IN_FLIGHT};
            //The structure has an optional flag similar to command pools that determines if individual descriptor sets can be freed or not. As the vk::raii::DescriptorSets destroy the underlying vk::DescriptorSet on destruction we need to set it to vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet to allow that.
            vk::DescriptorPoolCreateInfo poolInfo{.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, .maxSets = MAX_FRAMES_IN_FLIGHT, .poolSizeCount = 1, .pPoolSizes = &poolSize};

            descriptorPool =vk::raii::DescriptorPool(device, poolInfo);
        }

        void createDescriptorSets(){

            //one descriptor set for each frame in flight, all with the same layout.
            std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
            vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = descriptorPool,
                                                    .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
                                                    .pSetLayouts = layouts.data()};

            //allocating descriptor sets. We still need to configure the descriptors within
            descriptorSets = device.allocateDescriptorSets(allocInfo);

            //Descriptors that refer to buffers, like our uniform buffer descriptor, are configured with a vk::DescriptorBufferInfo struct. This structure specifies the buffer and the region within it that contains the data for the descriptor.
            for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
                vk::DescriptorBufferInfo bufferInfo{.buffer = uniformBuffers[i], .offset = 0, .range = sizeof(UniformBufferObject)};
                /*
                    The first two fields specify the descriptor set to update and the binding. We gave our uniform buffer binding index 0. Remember that descriptors can be arrays, so we also need to specify the first index in the array that we want to update. 
                    We’re not using an array, so the index is simply 0. We need to specify the type of descriptor again. It’s possible to update multiple descriptors at once in an array, starting at index dstArrayElement. 
                    The descriptorCount field specifies how many array elements you want to update. The last field references an array with descriptorCount structs that actually configure the descriptors. 
                    It depends on the type of descriptor which one of the three you actually need to use. The pBufferInfo field is used for descriptors that refer to buffer data. There are additional elements in this struct, that are not used in this sample: pImageInfo is used for descriptors that refer to image data, and pTexelBufferView is used for descriptors that refer to buffer views. Our descriptor is based on buffers, so we’re just using pBufferInfo.
                */
                vk::WriteDescriptorSet descriptorWrite{.dstSet = descriptorSets[i],
                                                       .dstBinding = 0,
                                                       .dstArrayElement = 0,
                                                       .descriptorCount = 1,
                                                       .descriptorType = vk::DescriptorType::eUniformBuffer,
                                                       .pBufferInfo = &bufferInfo};
                //The updates are applied using vk::raii::Device::updateDescriptorSets. It accepts two arrays as parameters: an array of vk::WriteDescriptorSet and an array of vk::CopyDescriptorSet. 
                //The latter can be used to copy descriptors to each other, as its name implies.
                device.updateDescriptorSets(descriptorWrite, {});
            }
        }

        void createCommandBuffers(){
            //the level parametr specifies if the allocated cb are primary or secondary
            //vk::CommandBufferLevel::ePrimary: Can be submitted to a queue for execution, but cannot be called from other command buffers.
            //vk::CommandBufferLevel::eSecondary: Cannot be submitted directly, but can be called from primary command buffers.
            vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
            
            //The vk::raii::CommandBuffers constructor generates a std::vector<vk::raii::CommandBuffer>, but we just want a single one here, so we move it out of that vector into a singular variable
            commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
        }

        //Before we can start rendering to an image, we need to transition its layout to one that is suitable for rendering. In Vulkan, images can be in different layouts that are optimized for different operations. 
        //e’ll use a pipeline barrier to transition the image layout from vk::ImageLayout::eUndefined to vk::ImageLayout::eColorAttachmentOptimal:
        void transition_image_layout(
                uint32_t                imageIndex,
                vk::ImageLayout         old_layout,
                vk::ImageLayout         new_layout,
                vk::AccessFlags2        src_access_mask,
                vk::AccessFlags2        dst_access_mask,
                vk::PipelineStageFlags2 src_stage_mask,
                vk::PipelineStageFlags2 dst_stage_mask)
        {
            auto &commandBuffer = commandBuffers[frameIndex];
            vk::ImageMemoryBarrier2 barrier = {
                .srcStageMask        = src_stage_mask,
                .srcAccessMask       = src_access_mask,
                .dstStageMask        = dst_stage_mask,
                .dstAccessMask       = dst_access_mask,
                .oldLayout           = old_layout,
                .newLayout           = new_layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = swapChainImages[imageIndex],
                .subresourceRange    = {
                    .aspectMask     = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1}};
            vk::DependencyInfo dependency_info = {
                .dependencyFlags         = {},
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers    = &barrier};
            commandBuffer.pipelineBarrier2(dependency_info);
        }

        void recordCommandBuffer(uint32_t imageIndex){
            auto &commandBuffer = commandBuffers[frameIndex];
            commandBuffer.begin({});

            //start dynamic rendering
            // Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
            transition_image_layout(
                imageIndex,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eColorAttachmentOptimal,
                {},                                                        //srcAccessMask (no need to wait for previous operations)
                vk::AccessFlagBits2::eColorAttachmentWrite,                //dstAccessMask
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,        //srcStage
                vk::PipelineStageFlagBits2::eColorAttachmentOutput         //dstStage
            );

            //setup color attachement
            vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f); //black
            /*
                The imageView parameter specifies which image view to render to. The imageLayout parameter specifies the layout the image will be in during rendering. 
                The loadOp parameter specifies what to do with the image before rendering, and the storeOp parameter specifies what to do with the image after rendering. 
                We’re using vk::AttachmentLoadOp::eClear to clear the image to black before rendering, and vk::AttachmentStoreOp::eStore to store the rendered image for later use.
            */
            vk::RenderingAttachmentInfo attachmentInfo = {
                .imageView   = swapChainImageViews[imageIndex],
                .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .loadOp      = vk::AttachmentLoadOp::eClear,
                .storeOp     = vk::AttachmentStoreOp::eStore,
                .clearValue  = clearColor};


            //setup rendering info
            //The renderArea parameter defines the size of the render area, similar to the render area in a render pass. 
            //The layerCount parameter specifies the number of layers to render to, which is 1 for a non-layered image. 
            //The colorAttachmentCount and pColorAttachments parameters specify the color attachments to render to.
            vk::RenderingInfo renderingInfo = {
                .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
                .layerCount           = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments    = &attachmentInfo};
            
            //begin rendering
            commandBuffer.beginRendering(renderingInfo);

            //bind graphics pipeline
            //The first parameter specifies if the pipeline object is a graphics or compute pipeline.
            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

            // bind vertex buffers to bindings
            //bindVertexBuffers(offset, an array of buffers to bind, an array of the same size of byte offsets to start reading vertex data from)
            commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
            //binding index buffer
            commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint16);
    

            //because we specifyed viewport and scissor state to dynamic for the pipeline, we neet to set then in the CB before isuing drawing command
            //we give it a negative height to flip the Y axis during the viewport transform, which is applied after clipping - the net eeffect on the final image is identical to  flipping the projection matrix, but the projection matrix is left alone.
            commandBuffer.setViewport(0, vk::Viewport(0.0f, static_cast<float>(swapChainExtent.height), static_cast<float>(swapChainExtent.width), -static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
            commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

            //draw command
            /*
                Paramters:
                    vertexCount: Even though we don’t have a vertex buffer, we technically still have 3 vertices to draw.
                    instanceCount: Used for instanced rendering, use 1 if you’re not doing that.
                    firstVertex: Used as an offset into the vertex buffer, defines the lowest value of SV_VertexId.
                    firstInstance: Used as an offset for instanced rendering, defines the lowest value of SV_InstanceID.
            */
            

            /*
                Unlike vertex and index buffers, descriptor sets are not unique to graphics pipelines. Therefore, we need to specify if we want to bind descriptor sets to the graphics or compute pipeline. 
                The next parameter is the layout that the descriptors are based on. The next two parameters specify the index of the first descriptor set, and the array of sets to bind. 
                The last parameter specifies an array of offsets that are used for dynamic descriptors. We’ll look at these in a future chapter.
            */
            commandBuffers[frameIndex].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSets[frameIndex], nullptr);


            /*
                A call to this function is very similar to vk::raii::CommandBuffer::draw. The first two parameters specify the number of indices and the number of instances. 
                We’re not using instancing, so just specify 1 instance. The number of indices represents the number of vertices that will be passed to the vertex shader. 
                The next parameter specifies an offset into the index buffer, using a value of 1 would cause the graphics card to start reading at the second index. 
                The second to last parameter specifies an offset to add to the vertex index before indexing into the vertex buffer. 
                The final parameter specifies an offset for instancing, which we’re not using.
            */
            commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

            //end rendering
            commandBuffer.endRendering();

            //After rendering, we need to transition the image layout back to vk::ImageLayout::ePresentSrcKHR so it can be presented to the screen
            // After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
            transition_image_layout(
                imageIndex,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::ePresentSrcKHR,
                vk::AccessFlagBits2::eColorAttachmentWrite,             // srcAccessMask
                {},                                                     // dstAccessMask
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,     // srcStage
                vk::PipelineStageFlagBits2::eBottomOfPipe               // dstStage
            );

            //finish recording command buffer
            commandBuffer.end();
        }

        void createLogicalDevice(){

            /* Choosing a queue family. */

            //A GPU exposes groups of queues called queue families. The loop finds the first family that supports graphics commands and presentation to surface
            
            //find the index of the first queue family that supports graphics
            std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

            //get the first index into queueFamiltyProperties which supports both graphics and present
            for(uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++){
                //queueFamilyProperties[qfpIndex].queueFlags is a bitmask (usually an integer wrapper) containing a set of bits representing all the capabilities that a specific Vulkan queue family supports (e.g., Graphics, Compute, Transfer).
                //vk::QueueFlagBits::eGraphics is a constant value where only one specific bit is turned on (equal to 1), representing the graphics capability.
                //& compares the two values bit by bit. It will return a non-zero value only if the graphics bit is turned on in both the queue family's flags and the constant.
                //physicalDevice.getSurfaceSupportKHR is a Vulkan function used to check if a specific queue family on a physical graphics device can present images to a given window surface
                if((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface)){
                    //found a queue family that supports both graphics and present
                    queueIndex = qfpIndex;
                    break;
                }
            }

            /* End Choosing a queue family. */

            if(queueIndex == ~0){
                throw std::runtime_error("Could not find a queue for graphics and present -> terminating");//because queueIndex remained unchanged, no suitable queue family was found
            }


            /* Start Requesting features. */
            
            //requests for Vulkan 1.3 features
            //links several feature structures through their pNext pointers.
            //C++ template type. The names inside <...> are types that tell vk::StructureChain which Vulkan structures to hold and in what order
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
                
            //basic surface capabilities. These properties are straightforward to query and are returned into a single vk::SurfaceCapabilitiesKHR struct.
            auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);

            //querying the supported surface formats.
            std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);

            //querying the supported presentation modes.
            availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
        }

        vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> &availableFormats){
            assert(!availableFormats.empty());
            
            //search through availableFormats and find the FIRST format whose Vulkan pixel format is B8G8R8A8_SRGB and whose color space is SRGB_NONLINEAR.
            //if found, we store an iterator to the format in formatIt. If not found, formatIt will be equal to availableFormats.end().
            const auto formatIt = std::ranges::find_if(
                availableFormats,
                [](const auto &format){
                    return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; 
                }
            );

            return formatIt != availableFormats.end() ? *formatIt : availableFormats[0]; //return the first format in the list of available formats
        }

        vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes){
            assert(std::ranges::any_of(availablePresentModes, [](auto presentMode){return presentMode == vk::PresentModeKHR::eFifo;})); //check if the list of available present modes contains vk::PresentModeKHR::eFifo
            
            //[](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value;} this is a lamda function that takes a vk::PresentModeKHR value as input and returns true if the value is equal to vk::PresentModeKHR::eMailbox, and false otherwise. It is used as a predicate in the std::ranges::any_of algorithm to check if any of the available present modes is equal to vk::PresentModeKHR::eMailbox.
            //c++ lambda functions have the structure [captures](parameters) -> return_type { body }. Empty [] means that the lamda does not need to use any variables from the surrounding scope
            return std::ranges::any_of(availablePresentModes, [](const vk::PresentModeKHR value){return vk::PresentModeKHR::eMailbox == value;}) ?
                vk::PresentModeKHR::eMailbox :
                vk::PresentModeKHR::eFifo; //return the only guaranteed available mode
        }

        //The swap extent is the resolution of the swap chain images, and it’s almost always exactly equal to the resolution of the window that we’re drawing to in pixels 
        //The range of the possible resolutions is defined in the vk::SurfaceCapabilitiesKHR structure
        vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities){
            // currentExtent is only set to the special "undefined" value described above
            // when the window manager lets us choose the extent ourselves; any other value
            // means the surface already dictates a fixed extent that we must use as-is.
            if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()){
                return capabilities.currentExtent;
            }

            int width, height;
            glfwGetFramebufferSize(window, &width, &height); //retrieves the size of the framebuffer of the specified window in pixels. The function takes a pointer to two integers (width and height) where it will store the retrieved values. The framebuffer size may differ from the window size due to high-DPI displays or other factors. This function is useful for applications that need to know the actual pixel dimensions of the rendering area, such as when setting up a Vulkan swap chain or OpenGL viewport.
            
            //clamping the width and height to be within the min and max extents specified by the surface capabilities. This ensures that the swap chain images are created with a valid size that is supported by the surface.
            return {
                std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
                std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
            };
        }

        uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities){
            auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount); //we want at least 3 images in the swap chain to avoid waiting for the driver to finish rendering the current image before we can submit the next one. This allows us to have one image being displayed, one image being rendered, and one image waiting in the queue.

            if((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount)){
                minImageCount = surfaceCapabilities.maxImageCount; //if the maximum number of images allowed by the surface is less than our desired minimum, we have to reduce our minimum to the maximum allowed. This ensures that we do not exceed the capabilities of the surface.
            }
            return minImageCount;
        }

        //nodiscard is a standard attribute that encourages compiler to issue a warning if a function's return value is ignored. This is useful for functions that return important information or status codes that should be checked by the caller.
        [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const {
            
            //the size of the bytecode is specified in bytes but the bytecode pointer is a uin32_t poiniter rather than a char pointer. 
            //Therefore, we need to multiply the size of the code by the size of a char to get the correct size in bytes. The reinterpret_cast<const uint32_t*> is used to convert the pointer type from const char* to const uint32_t*, which is required by Vulkan's shader module creation function.
            vk::ShaderModuleCreateInfo createInfo{ .codeSize  = code.size() * sizeof(char), .pCode = reinterpret_cast<const uint32_t*>(code.data()) };
            vk::raii::ShaderModule shaderModule{device, createInfo};
            
            //TODO: free the buffer
            //free((void*)code.data()); //free the memory allocated for the shader code after creating the shader module. This is important to avoid memory leaks, as the shader code is no longer needed once the shader module has been created.
            return shaderModule;
        }

        void createSyncObjects(){
            /*
            Every one of our synchronization objects has two sides: something that signals it, and something that waits on it. 
            For presentCompleteSemaphores and inFlightFences, both sides are driven by our own frameIndex, so sizing them by MAX_FRAMES_IN_FLIGHT and indexing with frameIndex is correct and sufficient.
            renderFinishedSemaphores is different: it’s signaled when the graphics queue finishes rendering into a particular swap chain image, and waited on by the presentation engine before it presents that same image. 
            The presentation engine doesn’t know about our frameIndex - it only knows the imageIndex that vk::raii::SwapchainKHR::acquireNextImage handed us, and that’s the index we have to use to look up which renderFinishedSemaphores entry to signal and which one presentation will wait on.
            Because a swap chain can (and, depending on the present mode, often does) have more images than MAX_FRAMES_IN_FLIGHT, and because there’s no guarantee that consecutive acquireNextImage calls return image indices in the same repeating pattern as frameIndex, a semaphore sized and indexed by frameIndex could end up being signaled again by a new frame while the presentation engine is still waiting on it from a previous one.
            */
            assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFences.empty());


            for(size_t i = 0; i < swapChainImages.size(); i++){
                renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
            }

            for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
                presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
                inFlightFences.emplace_back(device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
            }
        }

        /*
        At a high level, rendering a frame in Vulkan consists of a common set of steps:
            Wait for the previous frame to finish
            Acquire an image from the swap chain
            Record a command buffer which draws the scene onto that image
            Submit the recorded command buffer
            Present the swap chain image
        */
       //at the start of the frame we want to wait until the previous frame has finished, so that the command buffer and semaphores are available to use. To do that, we call vk::raii::Device::waitForFences
        void drawFrame(){
            //Delay resetting the fence until after we know for sure, we will be submitting work with it. Thus, if we return early, the fence is still signaled and vk::raii::Device::waitForFences wont deadlock the next time we use the same fence object.
            auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);//last parameter is a timeout(setting to max uint64 basically disables it)
            if(fenceResult != vk::Result::eSuccess){
                throw std::runtime_error("Failed to wait for FENCE!");
            }

            auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);//the last two parameters specify synchronization objects that are to be signaled when the presentation engine is finished using the image.

            if(result == vk::Result::eErrorOutOfDateKHR){
                recreateSwapChain();
                return;
            }

            //both eSuccess and eSuboptimalKHR are considered "success" return codes
            if(result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR){
                assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
                throw std::runtime_error("Failed to acquire swap chain image!");
            }

            //Only reset fence if we are submitting work
            device.resetFences(*inFlightFences[frameIndex]); //takes an array of fences and waits on the host for either any or all of the fences to be signaled before returning.

            commandBuffers[frameIndex].reset();
            recordCommandBuffer(imageIndex);

            updateUniformBuffer(frameIndex); //generate a new transformation every frame to make the geometry spin around.

            //now we can submit the recorded command buffer
            //Queue submission and synchronization is configured through parameters in the vk::SubmitInfo structure.
            /*

                The first three parameters specify which semaphores to wait on before execution begins and in which stage(s) of the pipeline to wait. We want to wait for writing colors to the image until it’s available, so we’re specifying the stage of the graphics pipeline that writes to the color attachment. 
                That means that theoretically, the implementation can already start executing our vertex shader and such while the image is not yet available. Each entry in the waitStages array corresponds to the semaphore with the same index in pWaitSemaphores.
                The next parameter specifies which command buffers to actually submit for execution. We simply submit the single command buffer we have.
                The pSignalSemaphores parameter specifies which semaphores to signal once the command buffer(s) have finished execution. In our case we’re using the renderFinishedSemaphore for that purpose.
            */
            vk::PipelineStageFlags waitDestinationStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput );
            const vk::SubmitInfo   submitInfo{.waitSemaphoreCount   = 1,
                                            .pWaitSemaphores      = &*presentCompleteSemaphores[frameIndex],
                                            .pWaitDstStageMask    = &waitDestinationStageMask,
                                            .commandBufferCount   = 1,
                                            .pCommandBuffers      = &*commandBuffers[frameIndex],
                                            .signalSemaphoreCount = 1,
                                            .pSignalSemaphores    = &*renderFinishedSemaphores[imageIndex]};

            queue.submit(submitInfo, *inFlightFences[frameIndex]); //We can now submit the command buffer to the graphics queue using vk::raii::Queue::submit

            //The last step of drawing a frame is submitting the result back to the swap chain to have it eventually show up on the screen. Presentation is configured through a vk::PresentInfoKHR structure at the end of the drawFrame function.
            //The first two parameters specify which semaphores to wait on before presentation can happen, just like vk::SubmitInfo. Since we want to wait on the command buffer to finish execution, thus our triangle being drawn, we take the semaphores which will be signaled and wait on them, thus we use renderFinishedSemaphore.
            //The next two parameters specify the swap chains to present images to and the index of the image for each swap chain. This will almost always be single.
            const vk::PresentInfoKHR presentInfoKHR{
                .waitSemaphoreCount = 1,
                .pWaitSemaphores    = &*renderFinishedSemaphores[imageIndex],
                .swapchainCount     = 1,
                .pSwapchains        = &*swapChain,
                .pImageIndices      = &imageIndex};
            
            //submitting request to present an image to the swap chain
            result = queue.presentKHR(presentInfoKHR);

            if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || framebufferResized)
            {
                framebufferResized = false;
                recreateSwapChain();
            }
            else
            {
                //There are no other success codes than eSuccess; on any error code, presentKHR already threw an exception.
                assert(result == vk::Result::eSuccess);
            }
            
            //advancing to next frame each time
            frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT; //modulo ensures we loop around after every MAX_FRAMES_IN_FLIGHT enqueued frames
        }

        void mainLoop(){
            while(!glfwWindowShouldClose(window)){
                glfwPollEvents(); //processes all pending events currently sitting in the window system's event queue and returns immediately
                drawFrame();
            }

            device.waitIdle();
        }

        void cleanup(){

            cleanupSwapChain();

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


