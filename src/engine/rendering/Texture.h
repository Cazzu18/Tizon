#pragma once
#include <iostream>
#include<string>
#include "Resource.h"
#include <vulkan/vulkan_raii.hpp>

/*
The Vulkan texture pipeline requires four distinct GPU objects that work together to provide complete texture functionality. The vk::Image represents the actual pixel data storage on the GPU, while vk::DeviceMemory provides the backing memory allocation. 
The separation between image and memory allows for advanced memory management techniques like suballocation and memory pooling.

The vk::ImageView serves as the interface between shaders and the image data, defining how shaders interpret the pixel format, mipmap levels, and array layers. The vk::Sampler encapsulates filtering and addressing modes that control how the GPU interpolates between pixels and handles texture coordinates outside the [0,1] range. 
This separation of concerns allows the same image to be used with different sampling configurations simultaneously.

The loading pipeline follows a clear sequence that handles the complex transformation from file-based data to GPU resources. The file path construction assumes a standard naming convention that maps resource IDs to physical files, enabling consistent asset organization across the project. 
Using the KTX format provides several advantages including GPU-native format storage, mipmap support, and compression compatibility.

Error handling at each stage prevents partial loading states that could leave the resource in an inconsistent condition. If image data loading fails, the function returns immediately without creating GPU resources, ensuring that the Texture object remains in a clean, unloaded state. 
This approach prevents resource leaks and makes error recovery more predictable for calling code.

The temporary nature of the CPU-side image data reflects the typical texture loading workflow where pixel data is needed only long enough to upload to the GPU. 
Once the GPU resources are created and populated, the CPU copy can be safely discarded, reducing memory pressure and preventing unnecessary data duplication.

*/

class Texture : public Resource {
    private:
        //Core Vulkan GPU resources for texture representation
        vk::Image image; //GPU image object containing pixel data
        vk::DeviceMemory memory; //GPU memory allocation backing the image
        vk::DeviceSize offset; //OFFSET WITHIN the memory allocation for this texture
        vk::ImageView imageView; //Shader-accessible view into the image
        vk::Sampler sampler; //Sampling configuration (filtering, wrapping, etc)

        //Texture metadata for validation and debug
        int width = 0; //in pixels
        int height = 0;
        int channels = 0;

    public:
        explicit Texture(const std::string& id) : Resource(id) {}

        ~Texture() override {
            Unload(); // ensure proper cleanup when object destroyed
        }

        //TODO: might be doLoad. See Resource.h
        bool Load() override {
            //Step 2a: construct file path using resource ID and expected format
            std::string filePath = "textures/" + GetId() + ".ktx";

            //Step 2b: Loading raw image data from disk with formoat detection
            unsigned char* data = LoadImageData(filePath, &width, &height, &channels); //&width = address of width. Pointer.
            if(!data){
                return false; //failed to load
            }

            //Step 2c: Transform raw pixel data into Vulkan GPU resources
            CreateVulkanImage(data, width, height, channels);

            //Step 2d: Clean up temporary CPU memory to preven any leaks
            FreeImageData(data);

            return Resource::Load(); //marking resource as successfully loaded
        }

        /*
            The cleanup sequence follows Vulkan’s object dependency requirements, where objects must be destroyed in reverse order of their creation to avoid validation errors and potential driver crashes. 
            The sampler and image view depend on the image, so they must be destroyed first. The memory allocation is released last since it backs the image object.
        */
        void Unload() override {
            if(IsLoaded()){
                //Step 3a: Obtain device handle for resource destruction
                vk::Device device = GetDevice();

                //Step 3b : Destroy GPU object in revers creation order
                device.destroySampler(sampler);
                device.destroyImageView(imageView);
                device.destroyImage(image);
                device.freeMemory(memory);

                //Step 3c: Update base class state to reflect unloaded state
                Resource::Unload();
            }
        }

        //public interface for accessing Vulkan resources safely
        vk::Image GetImage() const {return image;}
        vk::ImageView GetImageView() const {return imageView;}
        vk::Sampler GetSampler() const {return sampler;}

    private:
        unsigned char* LoadImageData(const std::string& filePath, int* width, int* height, int* channels){
            //implementation using stb_image or ktx library
            //this methods abstracts the details of different image format support and provides a consistent interface for pixel data loading
            return nullptr;
        }

        void FreeImageData(unsigned char* data){
            //implementation using stb_image or ktx
            //We ensure proper cleanup of image loader specific memory allocations
            //NOTE: different libraries may require different cleanup approached
        }

        void CreateVulkanImage(unsigned char* data, int width, int height, int channels){
            // Implementation to create Vulkan image, allocate memory, and upload data
            // This involves complex Vulkan operations including:
            // - Format selection based on channel count and data type
            // - Memory allocation with appropriate usage flags
            // - Image creation with optimal tiling and layout
            // - Data upload via staging buffers for efficiency
            // - Image view creation for shader access
            // - Sampler creation with appropriate filtering settings
        }

        vk::Device GetDevice(){
            // Get device from somewhere (e.g., singleton or parameter)
            // Production code would use dependency injection or service location
            // to provide the Vulkan device handle without tight coupling
            // ...
            return vk::Device(); // Placeholder
        }
};