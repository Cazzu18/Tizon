#pragma once
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <vulkan/vulkan_raii.hpp>

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
};