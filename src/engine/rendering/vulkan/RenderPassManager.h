#pragma once
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <vulkan/vulkan_raii.hpp>
#include "CullingSystem.h"

//forward declarations
class RenderPass;
class RenderTarget;

class RenderPassManager{
    private:
        std::unordered_map<std::string, std::unique_ptr<RenderPass>> renderPasses;
        std::vector<RenderPass*> sortedPasses;
        bool dirty = true;

    public:
        //Args is a variadic template parameter pack
        //That is I don't know how many arguments you'll give me, or what their types are, but I'll accept all of them and pass them along to T's constructor.
        template<typename T, typename... Args>
        T* AddRenderPass(const std::string& name, Args&&... args){
            static_assert(std::is_base_of<RenderPass, T>::value, "T must derive from RenderPass");

            auto it = renderPasses.find(name);
            if(it != renderPasses.end()){
                return dynamic_cast<T*>(it->second.get()); //safe because we verified that T derives from RenderPass
            }

            //pass not present so we create it
            auto pass = std::make_unique<T>(std::forward<Args>(args)...); //perfect forwarding to retains lvalue or rvalue
            T* passPtr = pass.get()
            renderPasses[name] = std::move(pass);
            dirty = true;

            return passPtr;
        }

        RenderPass* GetRenderPass(const std::string& name){
            auto it = renderPasses.find(name);
            if(it != renderPasses.end()){
                return it->second.get();
            }

            return nullptr;
        }

        void RemoveRenderPass(const std::string& name){
            auto it = renderPasses.find(name);
            if(it != renderPasses.end()){
                renderPasses.erase(it);
                dirty = true;
            }
        }

        void Execute(vk::raii::CommandBuffer& commandBuffer){
            if(dirty){
                SortPasses();
                dirty = false;
            }

            for(auto pass : sortedPasses){
                pass->Execute(commandBuffer);
            }
        }

    private:
        void SortPasses(){
            //Topologically sort render passes based on dependencies
            sortedPasses.clear();

            //Create a copy of render passes for sorting
            std::unordered_map<std::string, RenderPass*> passMap;
            for(const auto& [name, pass] : renderPasses){
                passMap[name] = pass.get();
            }

            //Perform topological sort
            std::unordered_set<std::string> visited;
            std::unordered_set<std::string> visiting;

            for(const auto& [name, pass] : passMap){
                if(visited.find(name) == visited.end()){
                    TopologicalSort(name, passMap, visited, visiting);
                }
            }
        }   

        void TopologicalSort(const std::string& name, const std::unordered_map<std::string, RenderPass*>& passMap, std::unordered_set<std::string>& visited, std::unordered_set<std::string>& visiting){
            visiting.insert(name);

            auto pass = passMap.at(name); //at(Key& key) Returns a reference to the mapped value of the element with specified key. If no such element exists, an exception of type std::out_of_range is thrown.
            for(const auto& dep : pass->GetDependencies()){
                if(visiting.find(dep) != visiting.end()){
                    //circular dependency
                    throw std::runtime_error("Circular dependency detected in render passes.");
                }
                TopologicalSort(dep, passMap, visited, visiting); //recursive call
            }

            visiting.erase(name);
            visited.insert(name);
            sortedPasses.push_back(pass);
        }
};

class RenderPass{
    private:
        std::string name;
        std::vector<std::string> dependencies;
        RenderTarget* target = nullptr;
        bool enabled = true;

    public:
        explicit RenderPass(const std::string& passName) : name(passName) {}
        virtual ~RenderPass() = default;

        const std::string& GetName() const {return name; }

        void AddDependency(const std::string& dependency){
            dependencies.push_back(dependency);
        }

        const std::vector<std::string>& GetDependencies() const {
            return dependencies;
        }

        void SetRenderTarget(RenderTarget* renderTarget){
            target = renderTarget;
        }

        RenderTarget* GetRenderTarget() const {
            return target;
        }

        void SetEnabled(bool isEnabled) {
            enabled = isEnabled;
        }

        bool isEnabled() const {
            return enabled;
        }

        virtual void Execute(vk::raii::CommandBuffer& commandBuffer){
            if(!enabled) return;

            BeginPass(commandBuffer);
            Render(commandBuffer);
            EndPass(commandBuffer);
        }
    protected:
        //With dynamic rendering, BeginPass typically calls vkCmdBeginRendering
        //instead of vkCmdBeginRenderPass
        virtual void BeginPass(vk::raii::CommandBuffer& commandBuffer) = 0;
        virtual void Render(vk::raii::CommandBuffer& commandBuffer) = 0;

        //With dynamic rendering, EndPass typically calls vkCmdEndRendering instead of vkCmdEndRenderPass
        virtual void EndPass(vk::raii::CommandBuffer& commandBuffer) = 0;
};

//Render target
class RenderTarget{
    private:
        vk::raii::Image colorImage = nullptr;
        vk::raii::DeviceMemory colorMemory = nullptr;
        vk::raii::ImageView colorImageView = nullptr;

        vk::raii::Image depthImage = nullptr;
        vk::raii::DeviceMemory depthMemory = nullptr;
        vk::raii::ImageView depthImageView = nullptr;
        
        uint32_t width;
        uint32_t height;

    public:
        RenderTarget(uint32_t w, uint32_t h) : width(w), height(h) {
            //Creating color and depth images
            CreateColorResources();
            CreateDepthResources();

            //Note: With dynamic rendering, we don't need to create VkRenderPass
            //or VkFramebuffer objects. Instead, we just create the images and
            //image views that will be used directly with vkCmdBeginRendering.
        }

        //No need for explicit destructor with RAII objects

        vk::ImageView GetColorImageView() const { return *colorImageView; }
        vk::ImageView GetDepthImageView() const { return *depthImageView; }

        uint32_t GetWidth() const { return width; }
        uint32_t GetHeight() const { return height; }

    private:
        void CreateColorResources(){
            // Implementation to create color image, memory, and view
            // With dynamic rendering, we just need to create the image and image view
            // that will be used with vkCmdBeginRendering
            // ...
        }

        void CreateDepthResources(){
            // Implementation to create depth image, memory, and view
            // With dynamic rendering, we just need to create the image and image view
            // that will be used with vkCmdBeginRendering
            // ...
        }

        vk::raii::Device& GetDevice(){
            //Get device from somewhere (e.g., singleton or paramter)

            static vk::raii::Device device = nullptr; //placeholder
            return device;
        }
};

//Geometry pass for deferred rendering
class GeometryPass : public RenderPass {
    private:
        CullingSystem* cullingSystem;

        //G-buffer textures
        RenderTarget* gBuffer;
    public:
        GeometryPass(const std::string& name, CullingSystem* culling) : RenderPass(name), cullingSystem(culling){
            //Creating G-buffer render target
            gBuffer = new RenderTarget(1920, 1080); //example
            SetRenderTarget(gBuffer);
        }

        ~GeometryPass() override {
            delete gBuffer;
        }

    protected:
        void BeginPass(vk::raii::CommandBuffer& commandBuffer) override {
            //Begine rendering with dynamic rendering
            vk::RenderingInfoKHR renderingInfo;

            //setting up color attachement
            vk::RenderingAttachmentInfoKHR colorAttachment;
            colorAttachment.setImageView(gBuffer->GetColorImageView())
                            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                            .setLoadOp(vk::AttachmentLoadOp::eClear)
                            .setStoreOp(vk::AttachmentStoreOp::eStore)
                            .setClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}));
            //Setting up depth attachment
            vk::RenderingAttachmentInfoKHR depthAttachment;
            depthAttachment.setImageView(gBuffer->GetDepthImageView())
                        .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
                        .setLoadOp(vk::AttachmentLoadOp::eClear)
                        .setStoreOp(vk::AttachmentStoreOp::eStore)
                        .setClearValue(vk::ClearDepthStencilValue(1.0f, 0));

            //Configuring rendering info
            renderingInfo.setRenderArea(vk::Rect2D({0, 0}, {gBuffer->GetWidth(), gBuffer->GetHeight()}))
                        .setLayerCount(1)
                        .setColorAttachmentCount(1)
                        .setPColorAttachments(&colorAttachment)
                        .setPDepthAttachment(&depthAttachment);

            // Begin dynamic rendering
            commandBuffer.beginRendering(renderingInfo);
        }

        void Render(vk::raii::CommandBuffer& commandBuffer) override {
            //getting visible entities
            const auto& visibleEntities = cullingSystem->GetVisibleEntities();

            //Rendering each entity to G-buffer
            for(auto entity : visibleEntities){
                auto meshComponent = entity->GetComponent<MeshComponent>();
                auto transformComponent = entity->GetComponent<TransformComponent>();

                if(meshComponent && transformComponent){
                    // Bind pipeline for G-buffer rendering
                    // ...

                    // Set model matrix
                    // ...

                    // Draw mesh
                    // ...
                }
            }
        }

        void EndPass(vk::raii::CommandBuffer& commandBuffer) override {
            //Ending dynamic rendering
            commandBuffer.endRendering();
        }
};

//LightingPass for deferred rendering
class LightingPass : public RenderPass{
    private:
        GeometryPass* geometryPass;
        std::vector<Light*> lights;
    public:
        LightingPass(const std::string& name, GeometryPass* gPass) : RenderPass(name), geometryPass(gPass) {
            //Adding dependency on geometry pass
            AddDependency(gPass->GetName());
        }

        void AddLight(Light* light){
            lights.push_back(light);
        }

        void RemoveLight(Light* light){
            auto it = std::find(lights.begin(), lights.end(), light);
            if(it != lights.end()){
                lights.erase(it);
            }
        }

    protected:
        void BeginPass(vk::raii::CommandBuffer& commandBuffer) override {
            //Begin rendering with dynamic rendering
            vk::RenderingInfoKHR renderingInfo;

            //Setting up color attachment for the lighting pass
            vk::RenderingAttachmentInfoKHR colorAttachment;
            colorAttachment.setImageView(GetRenderTarget()->GetColorImageView())
                        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                        .setLoadOp(vk::AttachmentLoadOp::eClear)
                        .setStoreOp(vk::AttachmentStoreOp::eStore)
                        .setClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}));

            //Configuring rendering info
            renderingInfo.setRenderArea(vk::Rect2D({0, 0}, {GetRenderTarget()->GetWidth(), GetRenderTarget()->GetHeight()}))
                        .setLayerCount(1)
                        .setColorAttachmentCount(1)
                        .setPColorAttachments(&colorAttachment);

            //Begin dynamic rendering
            commandBuffer.beginRendering(renderingInfo);
        }

        void Render(vk::raii::CommandBuffer& commandBuffer) override {
            //Binding G-buffer textures from the geometry pass
            auto gBuffer = geometryPass->GetRenderTarget();

            //Set up descriptor sets for G-buffer textures
            //With dynamic rendering, we access the G-buffer textures directly as shader resources
            //rather than as subpass inputs

            //Render full-screen quad with lighting shader
            // ...

            //For each light
            for (auto light : lights) {
                //Set light properties
                // ...

                //Draw light volume
                // ...
            }
        }

        void EndPass(vk::raii::CommandBuffer& commandBuffer) override {
            //End dynamic rendering
            commandBuffer.endRendering();
        }
};

//Post-process effect base class
class PostProcessEffect{
    public:
        virtual ~PostProcessEffect() = default;
        virtual void Apply(vk::raii::CommandBuffer& commandBuffer) = 0;
};

//Post-processing pass
class PostProcessPass : public RenderPass {
    private:
        LightingPass* lightingPass;
        std::vector<PostProcessEffect*> effects;

    public:
        PostProcessPass(const std::string& name, LightingPass* lPass) : RenderPass(name), lightingPass(lPass) {
            //adding dependency on lighting pass
            AddDependency(lPass->GetName());
        }

        void AddEffect(PostProcessEffect* effect){
            effects.push_back(effect);
        }

        void RemoveEffect(PostProcessEffect* effect){
            auto it = std::find(effects.begin(), effects.end(), effect);
            if(it != effects.end()){
                effects.erase(it);
            }
        }
    protected:
        void BeginPass(vk::raii::CommandBuffer& commandBuffer) override {
            //Begin rendering with dynamic rendering
            vk::RenderingInfoKHR renderingInfo;

            //Setting up color attachment for the post-processing pass
            vk::RenderingAttachmentInfoKHR colorAttachment;
            colorAttachment.setImageView(GetRenderTarget()->GetColorImageView())
                        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                        .setLoadOp(vk::AttachmentLoadOp::eClear)
                        .setStoreOp(vk::AttachmentStoreOp::eStore)
                        .setClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}));

            //Configuring rendering info
            renderingInfo.setRenderArea(vk::Rect2D({0, 0}, {GetRenderTarget()->GetWidth(), GetRenderTarget()->GetHeight()}))
                        .setLayerCount(1)
                        .setColorAttachmentCount(1)
                        .setPColorAttachments(&colorAttachment);

            //Begin dynamic rendering
            commandBuffer.beginRendering(renderingInfo);
        }

        void Render(vk::raii::CommandBuffer& commandBuffer) override {
            //With dynamic rendering, each effect can set up its own rendering state and access input textures directly as shader resources

            //Apply each post-process effect
            for (auto effect : effects) {
                effect->Apply(commandBuffer);
            }
        }

        void EndPass(vk::raii::CommandBuffer& commandBuffer) override {
            //End dynamic rendering
            commandBuffer.endRendering();
        }
        
};