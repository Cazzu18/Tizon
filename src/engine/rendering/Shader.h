#pragma once
#include <iostream>
#include <string>
#include <vulkan/vulkan_raii.hpp>
#include "Resource.h"

class Shader : public Resource {
    private:
        vk::ShaderModule shaderModule;
        vk::ShaderStageFlagBits stage;
    public:
        Shader(const std::string& id, vk::ShaderStageFlagBits shaderStage) : Resource(id), stage(shaderStage) {}

        ~Shader() override {
            Unload();
        }

        bool Load() override {
            //Determine file extension based on shader stage
            std::string extension;
            switch(stage){
                case vk::ShaderStageFlagBits::eVertex: extension = ".vert"; break;
                case vk::ShaderStageFlagBits::eFragment: extension = ".frag"; break;
                case vk::ShaderStageFlagBits::eCompute: extension = ".comp"; break;
                default: return false;
            }

            //Load shader from file
            std::string filePath = "shaders/" + GetId() + extension + ".spv";

            //reading shader code
            std::vector<char> shaderCode;
            if(!ReadFile(filePath, shaderCode)){
                return false;
            }

            //Creating shader module
            CreateShaderModule(shaderCode);

            return Resource::Load();

        }

        void Unload() override {
            //Destroying vulkan resources
            if(IsLoaded()){
                //Getting device
                vk::Device device = GetDevice();
                device.destroy(shaderModule);
                Resource::Unload();
            }
        }

        //Getters for Vulkan resources
        vk::ShaderModule GetShaderModule() const { return shaderModule; }
        vk::ShaderStageFlagBits GetStage() const { return stage; }

    private:
        bool ReadFile(const std::string& filePath, std::vector<char>& buffer) {
            //Implementation to read binary file
            // ...
            return true; // Placeholder
        }

        void CreateShaderModule(const std::vector<char>& code) {
            //Implementation to create Vulkan shader module
            // ...
        }

        vk::Device GetDevice() {
            //Get device from somewhere (e.g., singleton or parameter)
            // ...
            return vk::Device(); //Placeholder
        }
};