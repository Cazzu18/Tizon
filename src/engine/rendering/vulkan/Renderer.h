#pragma once
#include <iostream>
#include <vulkan/vulkan_raii.hpp>
#include "RenderPassManager.h"
#include "CullingSystem.h"

class Renderer {
    private:
        vk::raii::Device device = nullptr;
        vk::Queue graphicsQueue;
        vk::raii::CommandPool commandPool = nullptr;

        RenderPassManager renderPassManager;
        CullingSystem cullingSystem;

        //Current frame resources
        vk::raii::CommandBuffer commandBuffer = nullptr;
        vk::raii::Fence fence = nullptr;
        vk::raii::Semaphore imageAvailableSemaphore = nullptr;
        vk::raii::Semaphore renderFinishedSemaphore = nullptr;

    public:
        Renderer(vk::raii::Device& dev, vk::Queue queue) : device(dev), graphicsQueue(queue){
            //Create command pool
            //...

            //Create synchronization objects
            //...

            //setting up render passes
            SetupRenderPasses();
        }

        //We dont need explicit destructor with RAII objects

        void SetCamera(Camera* camera){
            cullingSystem.SetCamera(camera);
        }

        void Render(const std::vector<Entity*>& entities){
            //waiting for previous frame to finish
            fence.wait(UINT64_MAX);
            fence.reset();

            //resetting command buffer
            commandBuffer.reset();

            //performing culling
            cullingSystem.CullScene(entities);

            //Recording commands
            vk::CommandBufferBeginInfo beginInfo;
            commandBuffer.begin(beginInfo);

            //executing render passes
            renderPassManager.Execute(commandBuffer);

            commandBuffer.end();


            //submitting command buffer
            vk::SubmitInfo submitInfo;

            //with vk::raii::, we need to dereference the command buffer
            vk::CommandBuffer rawCommandBuffer = *commandBuffer;
            submitInfo.setCommandBufferCount(1);
            submitInfo.setPCommandBuffers(&rawCommandBuffer);

            //setup wait and signla semaphores
            vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

            //with vl::raii, we need to dereference the semaphores
            vk::Semaphore rawImageAvailableSemaphore = *imageAvailableSemaphore;
            vk::Semaphore rawRenderFinishedSemaphore = *renderFinishedSemaphore;

            submitInfo.setWaitSemaphoreCount(1);
            submitInfo.setPWaitSemaphores(&rawImageAvailableSemaphore);
            submitInfo.setPWaitDstStageMask(waitStages);
            submitInfo.setSignalSemaphoreCount(1);
            submitInfo.setPSignalSemaphores(&rawRenderFinishedSemaphore);

            //with vk::raii, we need to dereference the fence
            vk::Fence rawFence = *fence;
            graphicsQueue.submit(1, &submitInfo, rawFence);
        }
    private:
        void SetupRenderPasses(){
            //creating geometryPass
            auto geometryPass = renderPassManager.AddRenderPass<GeometryPass>("GeometryPass", &cullingSystem);

            //creating lighting pass
            auto lightingPass = renderPassManager.AddRenderPass<LightingPass>("LightingPass", geometryPass);

            //creating post-process pass
            auto postProcessPass = renderPassManager.AddRenderPass<PostProcessPass>("PostProcessPass", lightingPass);

            //Add post-process effects...
            //...
        }
};