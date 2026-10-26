#pragma once
#include <iostream>
#include <string>
#include <vector>
#include "Resource.h"
#include <vulkan/vulkan_raii.hpp>

/*
    The mesh resource architecture separates vertex and index data into distinct GPU buffers, following modern graphics API best practices. Vertex buffers contain per-vertex attributes such as positions, normals, texture coordinates, and color information, while index buffers define how vertices connect to form triangles. 
    This separation enables efficient vertex reuse, where a single vertex can be referenced by multiple triangles, significantly reducing memory usage for typical 3D models.

    The count tracking serves dual purposes: it provides essential information for rendering calls that specify how many vertices or indices to process, and it enables validation and debugging by allowing systems to verify that buffer contents match expected data sizes.
*/

class Mesh : public Resource {
    private:
        //vertex data management - stores per-vertex arributes like position, normal, UV coordinates
        vk::Buffer vertexBuffer; //GPU buffer containing vertex attribute data
        vk::DeviceMemory vertexBufferMemory; //GPU memory backing vertex buffer
        vk::DeviceSize vertexBufferOffset; //Offset withing the memory allocation for vertex buffer
        uint32_t vertexCount = 0; //Number of vertices in this mesh

        //Index data management - defines triangle connectivity using vertex indices
        vk::Buffer indexBuffer;
        vk::DeviceMemory indexBufferMemory;
        vk::DeviceSize indexBufferOffset;
        uint32_t indexCount = 0; //number fo indices in this mesh(typically 3 per triangle)

    public:
        explicit Mesh(const std::string& id) : Resource(id) {}

        ~Mesh() override {
            Unload(); //ensuring GPU resources are clean up
        }

        /*
            The loading pipeline follows a structured approach that separates file parsing from GPU resource creation, enabling better error handling and code reusability. The choice of glTF format provides several advantages including industry-standard mesh representation, embedded material information, and support for advanced features like skeletal animations and morph targets.

            The temporary CPU-side storage approach enables validation and processing of geometric data before committing to GPU resources. This intermediate step allows for mesh optimization techniques such as vertex cache optimization, triangle strip generation, or level-of-detail processing that can significantly improve rendering performance.

            The metadata caching strategy stores frequently accessed information locally to avoid expensive GPU queries during rendering. These counts are essential for draw calls, where the GPU needs to know exactly how many vertices to process and how many triangles to render, making local storage much more efficient than querying the GPU buffers repeatedly.
        */
        bool Load() override {
            //Step 2a: Construct file path using standardized naming convention
            std::string filePath = "models/" + GetId() + ".gltf";

            //Step 2b: Parse geometrix data from file format into CPU-accessible structs
            std::vector<Vertex> vertices; //temporary CPU storage for vertex attributes
            std::vector<uint32_t> indices; //temporary CPU storage for triangle indices

            if(!LoadMeshData(filepath, vertices, indices)){
                return false; //Failed to parse
            }

            //Step 2c: Transforming CPU data into OPTIMIZED GPU buffer resources
            CreateVertexBuffer(vertices); //uploading vertex attributes to GPU
            CreateIndexBuffer(indices); //uploading triangle connectivity to GPU

            //Step 2d: Cache metadata for efficient rendering operations
            vertexCount = static_cast<uint32_t>(vertices.size());
            indexCount = static_cast<uint32_t>(indices.size());

            return Resource::Load(); //mark resource successfully loaded

        }

        //if using vk::Device raither than vk::rai
        void Unload() override {
            if(IsLoaded()){

                //Phase 3a: Obtaine device handle for resource destruction
                vk::Device device = GetDevice();

                //Phase 3b: Destroy buffers and free GPU memory in proper sequence
                //index resources clean up first to maintain clear depencdency order
                device.destroyBuffer(indexBuffer);
                device.freeMemory(indexBufferMemory);

                //Vertex resources clean up second
                device.destroyBuffer(vertexBuffer);
                device.freeMemory(vertexBufferMemory);

                //Phase 3c: Update base class state to reflect unloaded condition
                Resource::Unload();

            }
        }

        //public interface for safe access to GPU resources and metadata
        vk::Buffer GetVertexBuffer() const { return vertexBuffer; }
        vk::Buffer GetIndexBuffer() const { return indexBuffer; }
        uin32_t GetVertexCount() const { return vertexCount; }
        uin32_t GetIndexCount() const { return indexCount; }

    private:
        bool LoadMeshData(const std::string& filePath, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
            // Implementation using tinygltf or similar library
            // This method handles the complex task of:
            // - Opening and validating the mesh file format
            // - Parsing vertex attributes (positions, normals, UVs, etc.)
            // - Extracting index data that defines triangle connectivity
            // - Converting from file format to engine-specific vertex structures
            // - Performing validation to ensure data integrity
            // ...
            return true; // Placeholder
        }

        /*
            The buffer creation methods represent some of the most performance-critical code in the mesh resource system, as inefficient GPU memory management can significantly impact rendering performance. 
            Production implementations typically use staging buffers for data upload, implement memory pooling to reduce allocation overhead, and carefully select memory types based on GPU architecture characteristics.

        */

        void CreateVertexBuffer(const std::vector<Vertex>& vertices) {
            // Implementation to create Vulkan buffer, allocate memory, and upload data
            // This involves several complex Vulkan operations:
            // - Calculating buffer size requirements based on vertex count and structure
            // - Creating buffer with appropriate usage flags (vertex buffer usage)
            // - Allocating GPU memory with optimal memory type selection
            // - Uploading data via staging buffer for efficient transfer
            // - Setting up memory barriers to ensure data availability
            // ...
        }

        void CreateIndexBuffer(const std::vector<uint32_t>& indices){
            // Implementation to create Vulkan buffer, allocate memory, and upload data
            // Similar to vertex buffer creation but optimized for index data:
            // - Buffer creation with index buffer specific usage flags
            // - Memory allocation optimized for read-heavy access patterns
            // - Efficient data transfer using appropriate staging mechanisms
            // - Index format validation (16-bit vs 32-bit indices)
        }

        vk::Device GetDevice(){
            // Get device from somewhere (e.g., singleton or parameter)
            // Production implementations typically use dependency injection or service locator patterns
            // to avoid tight coupling between resource classes and core engine systems
            // ...
            return vk::Device(); // Placeholder
        }
};