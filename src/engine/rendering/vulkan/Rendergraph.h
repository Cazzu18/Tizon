#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstddef>
#include <vulkan/vulkan_raii.hpp>


//TODO: Extend for frames?

/*
    A rendergraph is a higher-level abstraction that represents the entire rendering process as a directed acyclic graph (DAG), where nodes are render passes and edges represent dependencies between them. 
    This approach offers several advantages over traditional render pass management:

    What is a Rendergraph?
        A rendergraph is a data structure that:

            Describes Resources: Tracks all resources (textures, buffers) used in rendering.
            Defines Operations: Specifies what operations (render passes) will be performed.
            Manages Dependencies: Automatically determines the dependencies between operations.
            Handles Synchronization: Automatically inserts necessary synchronization primitives.
            Optimizes Memory: Can perform memory aliasing and other optimizations.

    The following is an ARCHITECTURAL PATTERN. The engine issues its render passses directly, witohut going through a rendergraph abstraction

    The data structure architecture reflects the core philosophy of rendergraphs: treating rendering as a series of transformations on resources rather than imperative GPU commands. 
    The Resource structure encapsulates everything needed to create and manage GPU resources, while the Pass structure defines rendering operations in terms of their resource dependencies rather than their implementation details.

    This approach enables powerful optimizations like automatic memory aliasing (where multiple resources share the same memory if their lifetimes don’t overlap) and optimal resource layout transitions. 
    The separation between resource description and actual GPU objects allows the rendergraph to make informed decisions about resource management during the compilation phase.

    Dynamic rendering is a modern Vulkan feature that simplifies the rendering process and works particularly well with rendergraphs
    Dynamic rendering offers several advantages over traditional render passes:
        Simplified API: No need to create and manage VkRenderPass and VkFramebuffer objects, reducing code complexity.
        More Flexible Rendering: Easier to change render targets and attachment formats at runtime.
        Improved Compatibility: Works better with modern rendering techniques that don’t fit well into the traditional render pass model.
        Reduced State Management: Fewer objects to track and synchronize.
        Easier Debugging: Simpler rendering code is easier to debug and maintain.

    When combined with rendergraphs, dynamic rendering becomes even more powerful. 
    The rendergraph handles the resource dependencies and synchronization, while dynamic rendering simplifies the actual rendering process. This combination provides both flexibility and performance.

    Deferred rendering represents a sophisticated rendering technique that separates geometry processing from lighting calculations, enabling efficient handling of complex lighting scenarios.
*/

//a comprehensive rendergraph IMPLEMENTATION for AUTOMATED dependency management
class Rendergraph{
    private:
        //Resource description and management structure
        //Represents Image resource during rendering(textures)
        struct Resource {
            std::string name; //human-readable identifier
            vk::Format format; //Pixel format(RGBA, Depth24Stencil8, etc)
            vk::Extent2D extent; //Dimensions in pixels for 2D resources
            vk::ImageUsageFlags usage; //How this will be used (color attachment, texture, etc)
            vk::ImageLayout initialLayout; //expected layout when frame begins
            vk::ImageLayout finalLayout; // required layout when the frame ends


            //Actual GPU resources - populated during compilation
            vk::raii::Image image = nullptr; //GPU image object
            vk::raii::DeviceMemory memory = nullptr; //Backing memory allocation
            vk::raii::ImageView view = nullptr; //shader accessible view of the image
        };

        //Render pass representation within the graph structure
        //Each pass represents a distinct rendering operation with defined inputs and outputs
        struct Pass {
            std::string name;
            std::vector<std::string> inputs; //Resources this pass reads from (dependencies)
            std::vector<std::string> outputs; //Resources this pass writes to (products)
            std::function<void(vk::raii::CommandBuffer&)> executeFunc; //rendering code
        };

        //Core data storage for the rendergraph system
        std::unordered_map<std::string, Resource> resources; //all resources referenced in the graph
        std::vector<Pass> passes; //all rendering passes in definition order
        std::vector<std::size_t> executionOrder; //computed optimal execution sequence

        //automatic synchronization management
        //These objects ensure correct GPU execution order witohut manual barriers
        std::vector<vk::raii::Semaphore> semaphores;
        std::vector<std::pair<std::size_t, std::size_t>> semaphoreSignalWaitPairs; //(signaling pass, waiting pass)

        vk::raii::Device& device; //Vulkan Device for resource creation

    public:
        explicit Rendergraph(vk::raii::Device& dev) : device(dev) {}

        /*
            The registration interface enables declarative rendergraph construction where developers specify what they want to achieve rather than how to achieve it. This high-level approach allows the rendergraph to analyze the entire rendering pipeline before making resource allocation and scheduling decisions.

            The deferred execution model (where passes store function objects rather than immediate GPU commands) enables powerful compile-time optimizations. The rendergraph can reorder passes, merge compatible operations, and optimize resource usage based on the complete dependency graph rather than making local decisions for each pass.

        */

        //Resource registration interface for declaring all resources used during rendering
        //This method establishes resource metadata without creating actual GPU resources
        void AddResource(const std::string& name, vk::Format format, vk::Extent2D extent, vk::ImageUsageFlags usage, vk::ImageLayout initialLayout, vk::ImageLayout finalLayout) {

            Resource resource;
            resource.name= name;
            resource.format = format;
            resource.extent = extent;
            resource.usage = usage;
            resource.initialLayout = initialLayout;
            resource.finalLayout = finalLayout;

            resources[name] = resource; //registering in the global resource map
        }

        //Pass registration interface for defining rendering operations and their dependencies
        //This method establishes the logical structure of rendering without immediate execution
        void AddPass(const std::string& name, const std::vector<std::string>& inputs, std::vector<std::string>& outputs, std::function<void(vk::raii::CommandBuffer&)>  executeFunc){
            Pass pass;
            pass.name = name;
            pass.inputs = inputs;
            pass.outputs = outputs;
            pass.executeFunc = executeFunc;

            passes.push_back(pass); //add to ordered pass list
        }

        //Rendergraph compilation - transforms declarative descriptions into executable pipeline
        //This method performs dependency analysis, resource allocation, and execution planning
        void Compile(){
            //dependency graph construction
            //building bidirectional dependency relationships between passes
            std::vector<std::vector<std::size_t>> dependencies(passes.size());
            std::vector<std::vector<std::size_t>> dependents(passes.size());

            //Tracking which pass produces each resource (write-after-write dependencies)
            std::unordered_map<std::string, std::size_t> resourceWriters;

            // Resource Production Registration
            // Register every pass's outputs *before* looking at any inputs, in its own pass over
            // `passes`. Producers and consumers can appear in any order in the `passes` vector -
            // a pass added later might produce a resource a pass added earlier consumes - so
            // dependency discovery below must not assume producers come first positionally.

            for(std::size_t i = 0; i < passes.size(); i++){
                for(const auto& output : passes[i].outputs){
                    resourceWriters[output] = i; //record this pass as producer
                }
            }

            // Dependency Discovery Through Resource Usage Analysis
            // Now that every producer is known, analyze each pass's inputs to determine data flow
            // relationships, regardless of where the producer sits in the passes vector.
            for(std::size_t i = 0; i < passes.size(); i++){
                const auto& pass = passes[i];

                //process input dependencies - this pass must wait for producers
                for (const auto& input : pass.inputs){
                    auto it = resourceWriters.find(input);

                    if(it != resourceWriters.end()){
                        //Found the pass that produces this input - create dependency link
                        dependencies[i].push_back(it->second); //This pass depends on the producer
                        dependents[it->second].push_back(i); //Producer has this as a dependent
                    }
                }
            }

            //Topological Sort for Optimal Execution Order
            //Usin DFS to compute valid executions sequence while detecting cycles
            std::vector<bool> visited(passes.size(), false); //tracking completed nodes
            std::vector<bool> inStack(passes.size(), false); // tracking current recursion path

            //[&] part of the lambda capture list that tells the lambda how it is allowed to access variables from the surrounding function, in this case variables from the surrounding scope that I use, by reference
            //so we can access visited, inStack, etc even though they are NOT parameters
            std::function<void(std::size_t)> visit = [&](std::size_t node) {
                //bool at position node
                if(inStack[node]){
                    //Cycle detection - circular dependency found
                    throw std::runtime_error("Cycle detected in rendergraph");
                }

                if(visited[node]){
                    return; //already processed this node and its dependencies
                }

                inStack[node] = true; //mark as currently being processed

                // Recursively process all passes this one depends on first (post-order traversal LRN),
                // so a producer is always pushed onto executionOrder before its consumers.
                for(auto dependency : dependencies[node]){
                    visit(dependency);
                }

                inStack[node] = false; //removing from current path
                visited[node] = true; //marking as completely processed
                executionOrder.push_back(node); //add to execution order
            };

            //Process all unvisited nodes to handle disconnected graph components
            for(std::size_t i = 0; passes.size(); i++){
                if(!visited[i]){
                    visit(i);
                }
            }

            //Automatic Synchronization Object Creation
            //Generating Semaphores for all dependencies identified during analysis
            for(std::size_t i = 0; i < passes.size(); i++){
                for(auto dep : dependencies[i]){
                    //Create a GPU semaphore for this dependency relationship
                    //The dependent pass will wait on this semaphore before execution
                    semaphores.emplace_back(device.createSemaphore({}));
                    semaphoreSignalWaitPairs.emplace_back(dep, i);
                }
            }

            //Physical Resource Allocation and Creation
            //Transform resource descriptions into ACTUAL GPU objects
            for(auto& [name, resource] : resources){
                //Configuring image creation parameters based on resource descriptions
                vk::ImageCreateInfo imageInfo;
                imageInfo.setImageType(vk::ImageType::e2D)                  //2D texture/render target
                         .setFormat(resource.format)                        //Pixel format from description  
                         .setExtent({resource.extent.width, resource.extent.height, 1}) // Dimensions
                         .setMipLevels(1)                                   //Single mip level for simplicity(for now)
                         .setArrayLayers(1)                                 //Single layer(not array texture)
                         .setSamples(vk::SampleCountFlagBits::e1)           //no multisampling (for now)
                         .setTiling(vk::ImageTiling::eOptimal)              //GPU-optimal memory layout
                         .setUsage(resource.usage)                          //usage
                         .setSharingMode(vk::SharingMode::eExclusive)       //single queue family access
                         .setInitialLayout(vk::ImageLayout::eUndefined);    //Initial Layout (will be transitioned)
                
                resource.image = device.createImage(imageInfo);

                //allocating backing memory for the image
                vk::MemoryRequirements memRequirements = resource.image.getMemoryRequirements();

                vk::MemoryAllocateInfo allocInfo;
                allocInfo.setAllocationSize(memRequirements.size).setMemoryTypeIndex(FindMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)); //GPU local memory

                resource.memory = device.allocateMemory(allocInfo); //Allocating GPU memory
                resource.image.bindMemory(*resource.memory, 0); //binding memory to image


                //Create image view for shader access
                vk::ImageViewCreateInfo viewInfo;
                viewInfo.setImage(*resource.image)
                        .setViewType(vk::ImageViewType::e2D)
                        .setFormat(resource.format)
                        .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}); //Full iamge access
                
                        resource.view = device.createImageView(viewInfo);
            }
        }

        //Resource access interface for retrieving compiled resources
        Resource* GetResource(const std::string& name){
            auto it = resources.find(name);
            return (it != resources.end()) ? &it->second : nullptr;
        }

        //Rendergraph execution engine - coordinates pass execution with automatic synchronization
        //This method transforms the compiled rendergraph into actual GPU work
        /*
            The execution engine represents the culmination of the rendergraph system, where all the analysis and preparation work pays off in coordinated GPU execution. 
            The automatic synchronization ensures that passes execute in the correct order without manual barrier management, while the automatic layout transitions handle the complex state management that Vulkan requires for optimal performance.

            This execution model demonstrates the power of the rendergraph abstraction: complex multi-pass rendering with dozens of resources and dependencies gets reduced to a simple Execute() call, with all the synchronization and resource management handled automatically based on the declarative pass and resource descriptions.
        */
        void Execute(vk::raii::CommandBuffer& commandBuffer, vk::Queue queue){
            //Execution state management for dynamic synchroniation
            std::vector<vk::CommandBuffer> cmdBuffers; // command buffer storage
            std::vector<vk::Semaphore> waitSemaphores; //synchronization dependencies for current pass
            std::vector<vk::PipelineStageFlags> waitStages; //pipeline stages to wait on
            std::vector<vk::Semaphore> signalSemaphores; // semaphores to signal after current pass

            //Ordered Pass Execution with Automatic Dependency Management
            //Execure each pass in the computed dependency sage order
            for(auto passIdx : executionOrder){
                const auto& pass = passes[passIdx];

                //Synchronization Setup - Collect dependency-sage order
                waitSemaphores.clear();
                waitStages.clear();

                for(std::size_t i = 0; i < semaphoreSignalWaitPairs.size(); i++){
                    if(semaphoreSignalWaitPairs[i].second == passIdx){
                        //This pass depends on completion of another pass
                        waitSemaphores.push_back(*semaphores[i]); //wait for dependency completion
                        waitStages.push_back(vk::PipelineStageFlagBits::eColorAttachmentOutput); //wait at output stage
                    }
                }

                //Collect semaphores that this pass will signal for dependent passes
                signalSemaphores.clear();
                for(std::size_t i = 0; i < semaphoreSignalWaitPairs.size(); i++){
                    if(semaphoreSignalWaitPairs[i].first == passIdx) {
                        //other passes depend on this pass's completion
                        signalSemaphores.push_back(*semaphores[i]);
                    }
                }

                //Command Buffer preparation and Resource Layout Transitions
                //Set up command recording and transition resource to appropriate layouts
                commandBuffer.begin({});

                //Transitioning input resources to shader-readable layouts
                for(const auto& input : pass.inputs){
                    auto& resource = resources[input];

                    TransitionImageLayout(commandBuffer, resource.image, resource.initialLayout, vk::ImageLayout::eShaderReadOnlyOptimal);

                    // vk::ImageMemoryBarrier barrier;
                    // barrier.setOldLayout(resource.initialLayout)
                    //         .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    //         .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    //         .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    //         .setImage(*resource.image)
                    //         .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0 , 1})
                    //         .setSrcAccessMask(vk::AccessFlagBits::eMemoryWrite)
                    //         .setDstAccessMask(vk::AccessFlagBits::eShaderRead);

                    // //Inserting pipeline barrier for safe layout transition 
                    // commandBuffer.pipelineBarrier(
                    //     vk::PipelineStageFlagBits::eAllCommands,
                    //     vk::PipelineStageFlagBits::eFragmentShader,
                    //     vk::DependencyFlagBits::eByRegion, //ByRegion dependency flag enables GPU optimizations for cases where different regions of the image can be processed independently, potentially improving performance on tile-based renderers and other advanced GPU architectures.
                    //     0, nullptr, 0, nullptr, 1, &barrier
                    // );
                }

                //Transitioning output resources to render target layouts
                for(const auto& output : pass.outputs){
                    auto& resource = resources[output];

                    TransitionImageLayout(commandBuffer, resource.image, resource.initialLayout, vk::ImageLayout::eColorAttachmentOptimal);
                    // vk::ImageMemoryBarrier barrier;
                    // barrier.setOldLayout(resource.initialLayout)
                    //         .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
                    //         .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    //         .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    //         .setImage(*resource.image)
                    //         .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
                    //         .setSrcAccessMask(vk::AccessFlagBits::eMemoryRead)
                    //         .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
                    
                    // //insert barrier for safe transition to writable state
                    // commandBuffer.pipelineBarrier(
                    //     vk::PipelineStageFlagBits::eAllCommands,
                    //     vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    //     vk::DependencyFlagBits::eByRegion,
                    //     0, nullptr, 0, nullptr, 1, &barrier
                    // );
                }
                
                //Pass Execution - Executing the Actual rendering logic
                //Calling the user provided rendering function with prepared command buffer
                pass.executeFunc(commandBuffer);

                //Final Layout Transitions - Prepare Resources for Subsequent Use
                //Transition output resources to their final required layouts

                for(const auto& output : pass.outputs){
                    auto& resource = resources[output];

                    TransitionImageLayout(commandBuffer, resource.image, vk::ImageLayout::eColorAttachmentOptimal, resource.finalLayout);
                    
                    // vk::ImageMemoryBarrier barrier;
                    // barrier.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
                    //         .setNewLayout(resource.finalLayout)
                    //         .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    //         .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    //         .setImage(*resource.image)
                    //         .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
                    //         .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                    //         .setDstAccessMask(vk::AccessFlagBits::eMemoryRead); //enables subsequent read
                    
                    // //insert final barrier for layout transition
                    // commandBuffer.pipelineBarrier(
                    //     vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    //     vk::PipelineStageFlagBits::eAllCommands,
                    //     vk::DependencyFlagBits::eByRegion,
                    //     0, nullptr, 0, nullptr, 1, &barrier
                    // );
                }

                //Command Submission with Synchronization
                //Submit command buffer with appropriate dependency and signaling semaphores
                commandBuffer.end();

                vk::SubmitInfo submitInfo;
                submitInfo.setWaitSemaphoreCount(static_cast<uint32_t>(waitSemaphores.size()))
                          .setPWaitSemaphores(waitSemaphores.data())
                          .setPWaitDstStageMask(waitStages.data())
                          .setCommandBufferCount(1)
                          .setPCommandBuffers(&*commandBuffer)
                          .setSignalSemaphoreCount(static_cast<uint32_t>(signalSemaphores.size()))
                          .setPSignalSemaphores(signalSemaphores.data());

                queue.submit(1, &submitInfo, nullptr);

            }
        }

    private:
        uint32_t FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties){
            
            
            return 0; //placeholder
        }

        //TODO: reintroduced mip levels(see Window.cpp)
        void TransitionImageLayout(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout){
            
            /*
                oldLayout and newLayout specify the the layout transition. It is possible to use vk::ImageLayout::eUndefined as oldLayout if you don’t care about the existing contents of the image.
                If you are using the barrier to transfer queue family ownership, then srcQueueFamilyIndex and dstQueueFamilyIndex fields should be the indices of the queue families. They must be set to vk::QueueFamilyIgnored if you don’t want to do this (not the default value!).
                The image and subresourceRange specify the image that is affected and the specific part of the image. Our image is not an array and does not have mipmapping levels, so only one level and layer are specified.


            */
            vk::ImageMemoryBarrier barrier;
            barrier.setOldLayout(oldLayout) //current image layout state
                   .setNewLayout(newLayout) //target layout after transition
                   .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored) //No queue family ownership transfer
                   .setDstQueueFamilyIndex(vk::QueueFamilyIgnored) //Same queue family throughout
                   .setImage(image) //target image after transition
                   .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}); //full color image range
            
            // Initialize pipeline stage tracking for synchronization timing
            // These stages define when operations must complete and when new operations can begin
            vk::PipelineStageFlags sourceStage; //When previous operations must finish. That is, which operations must complete before the barrier (source stage)
            vk::PipelineStageFlags destinationStage; //When subsequent operations can start. That is, which operations can begin after the barrier (destination stage)

            //transfer writes must occur in the pipeline transfer stage. Since the writings don’t have to wait on anything, you may specify an empty access mask and the earliest possible pipeline stage vk::PipelineStageFlagBits::eTopOfPipe for the pre-barrier operations. 
            //It should be noted that vk::PipelineStageFlagBits::eTransfer is not a real stage within the graphics and compute pipelines. It is more of a pseudo-stage where transfers happen.
            //See https://docs.vulkan.org/spec/latest/chapters/synchronization.html#VkPipelineStageFlagBits
            if(oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal){
                barrier.setSrcAccessMask(vk::AccessFlagBits::eNone) //No previous access to synchronize
                        .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

                sourceStage = vk::PipelineStageFlagBits::eTopOfPipe; //No previous work to wait for
                destinationStage = vk::PipelineStageFlagBits::eTransfer; //Transfer operations can proceed
            }
            //The image will be written in the same pipeline stage and subsequently read by the fragment shader, which is why we specify shader reading access in the fragment shader pipeline stage.
            //This pattern prepares uploaded images for shader sampling
            else if(oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal){
                barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                       .setDstAccessMask(vk::AccessFlagBits::eShaderRead);

                sourceStage = vk::PipelineStageFlagBits::eTransfer; //Transfer operations must complete
                destinationStage = vk::PipelineStageFlagBits::eFragmentShader; //Fragment shader can access
            }
            else {
                throw std::invalid_argument("unsupported layout transition!");
            }


            /*
                All types of pipeline barriers are submitted using the same function. The first parameter specifies in which pipeline stage the operations occur that should happen before the barrier. 
                The second parameter specifies the pipeline stage in which operations will wait on the barrier. The pipeline stages that you are allowed to specify before and after the barrier depend on how you use the resource before and after the barrier.
                The third parameter is either {} or vk::DependencyFlagBits::eByRegion. The latter turns the barrier into a per-region condition. That means that the implementation is allowed to already begin reading from the parts of a resource that were written so far, for example.
                The last three parameters reference arrays of pipeline barriers of the three available types: memory barriers, buffer memory barriers, and image memory barriers like the one we’re using here.
            */
            commandBuffer.pipelineBarrier(sourceStage, destinationStage, vk::DependencyFlagBits::eByRegion, {}, {}, barrier);

        }

        //Comprehensive frame rendering with proper synchronizaiton
        //This function demonstrates the complete cycle of frame rendering COORDINATION
        void RenderFrame(vk::raii::Device& device, vk::Queue graphicsQueue, vk::Queue presentQueue){

            //preventing CPU from submitting work faster than GPU can process it
            //The timeout value UINT64_MAX effectively means "wait indefinitely," which is appropriate for frame synchronization where we must ensure completion.
            vk::Result result = device.waitForFences(&*inFlightFence, VK_TRUE, UINT64_MAX);

            //resetting fence for this frame's completion tracking
            //preparing the fence to signal when this frame's GPU can process it
            device.resetFences(&*inFlightFence);

            //Acquiring next available image from the swapchain
            //This operation coordinates with the presentation engine and display system
            uint32_t imageIndex;

            //TODO use this pattern or pattern in Window.cpp
            //he acquired image index determines which swapchain image becomes the render target for this frame, affecting descriptor set bindings and render pass configuration in the subsequent command recording phase.
            result = vkAcquireNextImageKHR(device, *swapchain, UINT64_MAX, *imageAvailableSemaphore, nullptr, &imageIndex);


            //Record command buffer for this frame's rendering
            //Command buffer recording happens here with acquire image as render target
            //... (command recording implementation would go here)

            //Configure GPU work submission with comprehensive synchronization
            //This submission coordinates image availability, rendering, and presentation readiness
            vk::SubmitInfo submitInfo;
            vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

            submitInfo.setWaitSemaphoreCount(1)                             // Wait for one semaphore before execution
                      .setPWaitSemaphores(&*imageAvailableSemaphore)               // Don't start until image is available
                      .setPWaitDstStageMask(waitStages)                            // Specifically wait before color output
                      .setCommandBufferCount(1)                                    // Submit one command buffer
                      .setPCommandBuffers(&*commandBuffer)                        // The recorded rendering commands
                      .setSignalSemaphoreCount(1)                                  // Signal one semaphore when complete
                      .setPSignalSemaphores(&*renderFinishedSemaphore);            // Notify when rendering is finished

            //Submitting work to GPU with fence-based completion tracking
            //The fence allows CPU to know when this frame's GPU work has completed
            graphicsQueue.submit(1, &submitInfo, *inFlightFence);

            //presenting the rendered image to the display
            //this operation transfers the completed frame from rendering to display system
            vk::PresentInfoKHR presentInfo;
            presentInfo.setWaitSemaphoreCount(1)                                   // Wait for rendering completion
                    .setPWaitSemaphores(&*renderFinishedSemaphore)              // Don't present until rendering finishes
                    .setSwapchainCount(1)                                       // Present to one swapchain
                    .setPSwapchains(&*swapchain)                                // Target swapchain for presentation
                    .setPImageIndices(&imageIndex);

            //submitting presentation request to presentation engine
            result = presentQueue.presentKHR(&presentInfo);
        }
        
        // Comprehensive deferred renderer setup demonstrating rendergraph resource management
        // This implementation shows how to efficiently organize multi-pass rendering workflows
        /*
            The G-Buffer resource configuration represents the foundation of deferred rendering, where each buffer stores specific geometric information that will be consumed during lighting calculations. The format choices reflect a balance between precision requirements and memory efficiency: 
            positions and normals use 16-bit floating point for accurate lighting calculations, while albedo uses 8-bit integers for color data where gamma correction naturally reduces precision requirements.

            The usage flag combinations enable each resource to serve dual roles: first as render targets during the geometry pass, then as input textures during the lighting pass. This dual usage pattern is characteristic of deferred rendering workflows, where the same data moves through multiple pipeline stages with different access patterns.
        */
        void SetupDeferredRenderer(Rendergraph& graph, uint32_t width, uint32_t height){
            //Configuringd position buffer for world-space vertex positions
            //High precision format preserves positional accuracy for lighting calculations
            graph.AddResource("GBuffer_Position", vk::Format::eR16G16B16A16Sfloat, {width, height},
                            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
                            vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);

            //Configuring normal buffer for surface orientation data
            //High precision normals enable accurate lighting and reflection calculations
            graph.AddResource("GBuffer_Normal", vk::Format::eR16G16B16A16Sfloat, {width, height},
                            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
                            vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);

            //Configuring albedo buffer for surface color information
            //Standard 8-bit precision sufficient for color data with gamma encoding
            graph.AddResource("GBuffer_Albedo", vk::Format::eR8G8B8A8Unorm, {width, height},
                            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
                            vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);

            //Configuring depth buffer for occlusion and depth testing
            //High precision depth enables accurate depth reconstruction in lighting pass
            graph.AddResource("Depth", vk::Format::eD32Sfloat, {width, height},
                            vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment,
                            vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);

            //Configuring final color buffer for the completed lighting result
            //Standard color format with transfer capability for presentation or post-processing
            graph.AddResource("FinalColor", vk::Format::eR8G8B8A8Unorm, {width, height},
                            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
                            vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal);

            
            
            //Configuring geometry pass for Geometry-Buffer population
            //This pass renders all geometry and stores intermediate data for lighting calculations
            /*
                The geometry pass configuration demonstrates the power of deferred rendering’s separation of concerns, where geometric complexity is handled independently of lighting complexity. 
                The multiple render target setup enables simultaneous output to all G-Buffer components in a single rendering pass, maximizing GPU efficiency compared to multiple separate passes.

                The dynamic rendering approach eliminates the need to pre-configure render pass objects, providing flexibility to adjust G-Buffer formats or attachment counts based on runtime requirements. 
                This flexibility is particularly valuable for techniques like adaptive quality settings or optional G-Buffer components for different material types.
            */
            graph.AddPass("GeometryPass",
                        {},                                                        // No input dependencies - first pass in pipeline
                        {"GBuffer_Position", "GBuffer_Normal", "GBuffer_Albedo", "Depth"},  // Outputs all G-Buffer components
                        [&](vk::raii::CommandBuffer& cmd) {

                            //Configure multiple render target attachments for G-Buffer output
                            //Each attachment corresponds to a different geometric property
                            std::array<vk::RenderingAttachmentInfoKHR, 3> colorAttachments;

                            //Configuring position attachment - world space vertex positions
                            colorAttachments[0].setImageView(/* GBuffer_Position view */)    //Target position buffer
                                            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)  //Optimal for writes
                                            .setLoadOp(vk::AttachmentLoadOp::eClear)       //Clear to known state
                                            .setStoreOp(vk::AttachmentStoreOp::eStore);    //Preserve for lighting pass

                            //Configure normal attachment - surface normals in world space
                            colorAttachments[1].setImageView(/* GBuffer_Normal view */)      //Target normal buffer
                                            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                            .setLoadOp(vk::AttachmentLoadOp::eClear)       //Clear to default normal
                                            .setStoreOp(vk::AttachmentStoreOp::eStore);    //Preserve for lighting

                            //Configure albedo attachment - surface color and material properties
                            colorAttachments[2].setImageView(/* GBuffer_Albedo view */)      //Target albedo buffer
                                            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                            .setLoadOp(vk::AttachmentLoadOp::eClear)       //Clear to default color
                                            .setStoreOp(vk::AttachmentStoreOp::eStore);    //Preserve for lighting

                            //Configure depth attachment for occlusion culling
                            vk::RenderingAttachmentInfoKHR depthAttachment;
                            depthAttachment.setImageView(/* Depth view */)                   //Target depth buffer
                                        .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)  //Optimal for depth ops
                                        .setLoadOp(vk::AttachmentLoadOp::eClear)           //Clear to far plane
                                        .setStoreOp(vk::AttachmentStoreOp::eStore)         //Preserve for lighting pass
                                        .setClearValue({1.0f, 0});                        //Clear to maximum depth

                            //Assembling complete rendering configuration
                            vk::RenderingInfoKHR renderingInfo;
                            renderingInfo.setRenderArea({{0, 0}, {width, height}})          //Full screen rendering
                                        .setLayerCount(1)                                   //Single layer rendering
                                        .setColorAttachmentCount(colorAttachments.size())  //Number of G-Buffer targets
                                        .setPColorAttachments(colorAttachments.data())     //G-Buffer attachment array
                                        .setPDepthAttachment(&depthAttachment);            //Depth testing configuration

                            //Executing geometry rendering with dynamic rendering
                            cmd.beginRendering(renderingInfo);                              //Begin G-Buffer population

                            //Bind geometry pipeline and render all scene objects
                            //Each draw call populates position, normal, and albedo for visible fragments
                            // ... (geometry rendering implementation would go here)

                            cmd.endRendering();                                             //Complete G-Buffer population
                        });
            

            //Configuring lighting pass for screen-space illumination calculations
            //This pass reads G-Buffer data and computes final lighting for each pixel
            /*
                The lighting pass represents the core advantage of deferred rendering: decoupling lighting complexity from geometric complexity. By processing lighting in screen space, the cost becomes proportional to screen resolution rather than scene complexity, enabling efficient handling of scenes with many lights or complex lighting models.

                The single render target configuration reflects the unified nature of the lighting pass, where all lighting contributions are accumulated into the final color buffer. This approach enables advanced lighting techniques like physically-based rendering or global illumination algorithms that would be prohibitively expensive in forward rendering scenarios with complex geometry.

            */
            graph.AddPass("LightingPass",
                        {"GBuffer_Position", "GBuffer_Normal", "GBuffer_Albedo", "Depth"},  //Read all G-Buffer components
                        {"FinalColor"},                                            //Output final lit result
                        [&](vk::raii::CommandBuffer& cmd) {

                            //Configuring single color output for final lighting result
                            vk::RenderingAttachmentInfoKHR colorAttachment;
                            colorAttachment.setImageView(/* FinalColor view */)             //Target final color buffer
                                        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)  //Optimal for color writes
                                        .setLoadOp(vk::AttachmentLoadOp::eClear)          //Clearing to background color
                                        .setStoreOp(vk::AttachmentStoreOp::eStore)        //Preserving final result
                                        .setClearValue({0.0f, 0.0f, 0.0f, 1.0f});        //Clear to black background

                            //Configuring lighting pass rendering without depth testing
                            //Depth testing unnecessary since we're processing each pixel exactly once
                            vk::RenderingInfoKHR renderingInfo;
                            renderingInfo.setRenderArea({{0, 0}, {width, height}})          //Full screen processing
                                        .setLayerCount(1)                                   //Single layer output
                                        .setColorAttachmentCount(1)                        //Single color output
                                        .setPColorAttachments(&colorAttachment);           //Final color attachment

                            //Executing screen-space lighting calculations
                            cmd.beginRendering(renderingInfo);                              //Begin lighting pass

                            //Bind lighting pipeline and draw full-screen quad
                            //Fragment shader reads G-Buffer textures and computes lighting for each pixel
                            //All scene lights are processed in a single screen-space pass
                            // ... (lighting calculation implementation would go here)

                            cmd.endRendering();                                             //Complete lighting calculations
                        });

            //Compiling the complete rendergraph for execution
            //This analyzes dependencies and generates optimal execution plan
            graph.Compile();
            
        }
};

//TODO: TransitionImageLayout function to simplify