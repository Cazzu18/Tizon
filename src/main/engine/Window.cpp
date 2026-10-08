#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstdint> //necessary for uint32_t
#include <limits> //necessary for std::numeric_limits
#include <algorithm> //necessary for std::clamp
#include <fstream>


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

/*
    readFile function will read all the bytes from the specified file and return them in a byte array managed by std::vector. 
    We start by opening the file with two flags:
        ate: Start reading at the end of the file
        binary: Read the file as a binary file (avoid text transformations)

    The advantage of starting to read at the end of the file is that we can use the read position to determine the size of the file and allocate a buffer:
*/
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
class Window{
    private:
        GLFWwindow *window = nullptr;
        vk::raii::SurfaceKHR surface = nullptr;
        uint32_t queueIndex = ~0; //initializes the variable to 4294967295 (or 0xFFFFFFFF in hexadecimal). ~ is the Bitwise NOT that flips each of the 32 bits of that integer
        //vk::KHRSwapChainExtensionName is a constant that evaluates to the string "VK_KHR_swapchain", which is the name of the Vulkan extension that enables swap chain functionality. The swap chain is a series of images that are presented to the screen in a loop, allowing for smooth rendering and display of graphics.
        //needed to check if the physical device supports the swap chain extension, which is required for presenting images to the window surface.
        std::vector<const char*> requiredDeviceExtension = {
            vk::KHRSwapchainExtensionName
        }; //list of required device extensions.
        vk::raii::SwapchainKHR swapChain = nullptr;
        std::vector<vk::Image> swapChainImages;
        vk::SurfaceFormatKHR   swapChainSurfaceFormat;
        vk::Extent2D           swapChainExtent;
        std::vector<vk::raii::ImageView> swapChainImageViews;
        vk::raii::PipelineLayout pipelineLayout = nullptr;
        vk::raii::Pipeline graphicsPipeline = nullptr:
        vk::raii::CommandPool commandPool = nullptr;
        vk::raii::CommandBuffer commandBuffer = nullptr;

        //semaphores for GPU synchronization and Fence to keep GPU and HOST(CPU) in sync
        vk::raii::Semaphore presentCompleteSemaphore = nullptr;
        vk::raii::Semaphore renderFinishedSemaphore = nullptr;
        vk::raii::Fence drawFence = nullptr;

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
            createSwapChain();
            createImageViews();
            createGraphicsPipeline();
            createCommandPool();
            createCommandBuffer();
            createSyncObjects();

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

        void createGraphicsPipeline(){
            auto shaderCode = readFile("shaders/slang.spv");
            vk::raii::ShaderModule shaderModule = createShaderModule(shaderCode);
            
            vk::PipelineShaderStageCreateInfo vertShaderStageInfo{.stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule, .pName = "vertMain"};
            vk::PipelineShaderStageCreateInfo fragShaderStageInfo{.stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain"};
            
            //array containing these two structs
            vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

            //dynamic state allows us to change certain aspects of the pipeline without recreating it. For example, we can change the viewport and scissor rectangle dynamically during rendering. This is useful for applications that need to render to different parts of the screen or use multiple viewports.
            // std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
            // vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data()};

            /*
            The vk::PipelineVertexInputStateCreateInfo structure describes the format of the vertex data that will be passed to the vertex shader. It describes this in roughly two ways:
                Bindings: spacing between data and whether the data is per-vertex or per-instance (see instancing)
                Attribute descriptions: type of the attributes passed to the vertex shader, which binding to load them from and at which offset
            */
            vk::PipelineVertexInputStateCreateInfo vertexInputInfo;//we hard coded vertex data in the shader, so we don't need to specify any vertex input bindings or attribute descriptions. Therefore, we can leave the fields of this structure at their default values.

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
                .frontFace = vk::FrontFace::eClockwise,
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
            vk::PipelineMultisampleStateCreateInfo multisampling{.rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnabled = vk::False};

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

            vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 0, .pushConstantRangeCount = 0};
            pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

            //dynamic rendering allows us to render without creating a render pass and framebuffer objects. Instead, we can specify the attachments and their formats directly in the graphics pipeline. This is useful for applications that need to render to different targets or use multiple render passes.
            vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{.setColorAttachmentCount = 1, .pColorAttachmentFormats &swapChainSurfaceFormat.format};//one color attachement with the format of our swap chainm images
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

        void createCommandBuffer(){
            //the level parametr specifies if the allocated cb are primary or secondary
            //vk::CommandBufferLevel::ePrimary: Can be submitted to a queue for execution, but cannot be called from other command buffers.
            //vk::CommandBufferLevel::eSecondary: Cannot be submitted directly, but can be called from primary command buffers.
            vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};

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

        void recordCommandBuffer(uin32_t imageIndex){
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

            //because we specifyed viewport and scissor state to dynamic for the pipeline, we neet to set then in the CB before isuing drawing command
            commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
            commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

            //draw command
            /*
                Paramters:
                    vertexCount: Even though we don’t have a vertex buffer, we technically still have 3 vertices to draw.
                    instanceCount: Used for instanced rendering, use 1 if you’re not doing that.
                    firstVertex: Used as an offset into the vertex buffer, defines the lowest value of SV_VertexId.
                    firstInstance: Used as an offset for instanced rendering, defines the lowest value of SV_InstanceID.
            */
            commandBuffer.draw(3, 1, 0, 0);

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
            std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
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
            assert(std::range::any_of(availablePresentModes, [](auto presentMode){return presentMode == vk::PresentModeKHR::eFifo;})); //check if the list of available present modes contains vk::PresentModeKHR::eFifo
            
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
            vk::ShaderModuleCreateInfo createInfo{ .codeSize  = code.size() * sizeof(char), pCode = reinterpret_cast<const uint32_t*>(code.data()) };
            vk::raii::ShaderModule shaderModule{device, createInfo};
            
            //TODO: free the buffer
            free((void*)code.data()); //free the memory allocated for the shader code after creating the shader module. This is important to avoid memory leaks, as the shader code is no longer needed once the shader module has been created.
            return shaderModule;
        }

        void createSyncObjects(){
            presentCompleteSemaphore = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
            renderFinishedSemaphore  = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
            drawFence                = vk::raii::Fence(device, {.flags = vk::FenceCreateFlagBits::eSignaled});
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
            auto fenceResult = device.waitForFences(*drawFence, vk::True, UINT64_MAX);//last parameter is a timeout(setting to max uint64 basically disables it)
            if(fenceResult != vk::Result::eSuccess){
                throw std::runtime_error("Failed to wait for FENCE!");
            }
            device.resetFences(*drawFence); //takes an array of fences and waits on the host for either any or all of the fences to be signaled before returning.

            auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphore, nullptr);//the last two parameters specify synchronization objects that are to be signaled when the presentation engine is finished using the image.

            recordCommandBuffer(imageIndex);

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
                                            .pWaitSemaphores      = &*presentCompleteSemaphore,
                                            .pWaitDstStageMask    = &waitDestinationStageMask,
                                            .commandBufferCount   = 1,
                                            .pCommandBuffers      = &*commandBuffer,
                                            .signalSemaphoreCount = 1,
                                            .pSignalSemaphores    = &*renderFinishedSemaphore};

            queue.submit(submitInfo, *drawFence); //We can now submit the command buffer to the graphics queue using vk::raii::Queue::submit

            //The last step of drawing a frame is submitting the result back to the swap chain to have it eventually show up on the screen. Presentation is configured through a vk::PresentInfoKHR structure at the end of the drawFrame function.
            //The first two parameters specify which semaphores to wait on before presentation can happen, just like vk::SubmitInfo. Since we want to wait on the command buffer to finish execution, thus our triangle being drawn, we take the semaphores which will be signaled and wait on them, thus we use renderFinishedSemaphore.
            //The next two parameters specify the swap chains to present images to and the index of the image for each swap chain. This will almost always be single.
            const vk::PresentInfoKHR presentInfoKHR{
                .waitSemaphoreCount = 1,
                .pWaitSemaphores    = &*renderFinishedSemaphore,
                .swapchainCount     = 1,
                .pSwapchains        = &*swapChain,
                .pImageIndices      = &imageIndex};
            
            //submitting request to present an image to the swap chain
            result = queue.presentKHR(presentInfoKHR);
            
        }

        void mainLoop(){
            while(!glfwWindowShouldClose(window)){
                glfwPollEvents(); //processes all pending events currently sitting in the window system's event queue and returns immediately
                drawFrame();
            }

            device.waitIdle();
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


