#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION
#include "tiny_gltf.h"

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Engine.h"

struct Buffer
{
    VkDevice device;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDescriptorBufferInfo descriptor;
    VkDeviceSize size = 0;
    VkDeviceSize alignment = 0;
    void *mapped = nullptr;
    /** @brief Usage flags to be filled by external source at buffer creation (to query at some later point) */
    VkBufferUsageFlags usageFlags;
    /** @brief Memory property flags to be filled by external source at buffer creation (to query at some later point) */
    VkMemoryPropertyFlags memoryPropertyFlags;
    uint64_t deviceAddress;

    VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
    {
        return vkMapMemory(device, memory, offset, size, 0, &mapped);
    }

    void unmap()
    {
        if (mapped)
        {
            vkUnmapMemory(device, memory);
            mapped = nullptr;
        }
    }

    VkResult bind(VkDeviceSize offset = 0)
    {
        return vkBindBufferMemory(device, buffer, memory, offset);
    }

    void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
    {
        descriptor.offset = offset;
        descriptor.buffer = buffer;
        descriptor.range = size;
    }

    void copyTo(void *data, VkDeviceSize size)
    {
        assert(mapped);
        memcpy(mapped, data, size);
    }

    VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
    {
        VkMappedMemoryRange mappedRange{
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = memory,
            .offset = offset,
            .size = size};
        return vkFlushMappedMemoryRanges(device, 1, &mappedRange);
    }

    VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
    {
        VkMappedMemoryRange mappedRange{
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = memory,
            .offset = offset,
            .size = size};
        return vkInvalidateMappedMemoryRanges(device, 1, &mappedRange);
    }

    void destroy()
    {
        if (buffer)
        {
            vkDestroyBuffer(device, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
        }
        if (memory)
        {
            vkFreeMemory(device, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    }
};

namespace Loader
{

    class Texture
    {
    public:
        VkImage image;
        VkImageLayout imageLayout;
        int32_t imageIndex;
        VkDeviceMemory deviceMemory;
        VkImageView view;
        uint32_t width, height;
        uint32_t mipLevels;
        uint32_t layerCount;
        VkDescriptorImageInfo descriptor;
        VkSampler sampler;
        VkFormat format;

        void fromBuffer(void *buffer, VkDeviceSize bufferSize, VkFormat format, uint32_t texWidth, uint32_t texHeight, Engine &engine)
        {
            assert(buffer);
            width = texWidth;
            height = texHeight;
            mipLevels = 1;

            // Create a host-visible staging buffer that contains the raw image data
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingMemory;

            engine.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingMemory);

            // Copy texture data into staging buffer
            void *data;
            vkMapMemory(engine.logicalDevice, stagingMemory, 0, bufferSize, 0, &data);
            memcpy(data, buffer, bufferSize);
            vkUnmapMemory(engine.logicalDevice, stagingMemory);

            VkBufferImageCopy bufferCopyRegion{
                .bufferOffset = 0,
                .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1},
                .imageExtent = {
                    .width = width,
                    .height = height,
                    .depth = 1,
                }};

            engine.createImage(texWidth, texHeight, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, deviceMemory);
            VkImageSubresourceRange subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mipLevels, .layerCount = 1};

            engine.transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            engine.copyBufferToImage(stagingBuffer, image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
            engine.transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            // Clean up staging resources
            vkDestroyBuffer(engine.logicalDevice, stagingBuffer, nullptr);
            vkFreeMemory(engine.logicalDevice, stagingMemory, nullptr);

            // Create sampler
            VkSamplerCreateInfo samplerCreateInfo{
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = VK_FILTER_LINEAR,
                .minFilter = VK_FILTER_LINEAR,
                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .mipLodBias = 0.0f,
                .maxAnisotropy = 1.0f,
                .compareOp = VK_COMPARE_OP_NEVER,
                .minLod = 0.0f,
                .maxLod = 0.0f,
            };

            if (vkCreateSampler(engine.logicalDevice, &samplerCreateInfo, nullptr, &sampler) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create texture sampler!");
            }

            engine.createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT);

            // Update descriptor image info member that can be used for setting up descriptor sets
            updateDescriptor();
        }

        void updateDescriptor()
        {
        }
    };

    struct Image
    {
        Texture texture;
        // We also store (and create) a descriptor set that's used to access this texture from the fragment shader
        VkDescriptorSet descriptorSet;
    };

    class Model
    {
    public:
        struct Vertex
        {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec2 uv;
            glm::vec3 color;
        };

        struct
        {
            VkBuffer buffer;
            VkDeviceMemory memory;
        } vertices;

        struct
        {
            int count;
            VkBuffer buffer;
            VkDeviceMemory memory;
        } indices;

        struct Primitive
        {
            uint32_t firstIndex;
            uint32_t indexCount;
            int32_t materialIndex;
        };

        struct Mesh
        {
            std::vector<Primitive> primitives;
        };

        struct Node
        {
            Node *parent;
            std::vector<Node *> children;
            Mesh mesh;
            glm::mat4 matrix;
            ~Node()
            {
                for (auto &child : children)
                {
                    delete child;
                }
            }
        };

        struct Material
        {
            glm::vec4 baseColorFactor = glm::vec4(1.0f);
            uint32_t baseColorTextureIndex;
        };

        /**
         * Model data
         */
        std::vector<Image> images;
        std::vector<Texture> textures;
        std::vector<Material> materials;
        std::vector<Node *> nodes;

        std::vector<uint32_t> indexBuffer;
        std::vector<Vertex> vertexBuffer;

        Engine &engine;

        ~Model()
        {
            for (auto node : nodes)
            {
                delete node;
            }
            // Release all Vulkan resources allocated for the model
            vkDestroyBuffer(engine.logicalDevice, vertices.buffer, nullptr);
            vkFreeMemory(engine.logicalDevice, vertices.memory, nullptr);
            vkDestroyBuffer(engine.logicalDevice, indices.buffer, nullptr);
            vkFreeMemory(engine.logicalDevice, indices.memory, nullptr);

            for (Image image : images)
            {
                vkDestroyImageView(engine.logicalDevice, image.texture.view, nullptr);
                vkDestroyImage(engine.logicalDevice, image.texture.image, nullptr);
                vkDestroySampler(engine.logicalDevice, image.texture.sampler, nullptr);
                vkFreeMemory(engine.logicalDevice, image.texture.deviceMemory, nullptr);
            }
        }

        void loadImages(tinygltf::Model &input, Engine &engine)
        {
            // Images can be stored inside the glTF (which is the case for the sample model), so instead of directly
            // loading them from disk, we fetch them from the glTF loader and upload the buffers
            images.resize(input.images.size());
            for (size_t i = 0; i < input.images.size(); i++)
            {
                tinygltf::Image &glTFImage = input.images[i];
                // Get the image data from the glTF loader
                unsigned char *buffer = nullptr;
                VkDeviceSize bufferSize = 0;
                bool deleteBuffer = false;
                // We convert RGB-only images to RGBA, as most devices don't support RGB-formats in Vulkan
                if (glTFImage.component == 3)
                {
                    bufferSize = glTFImage.width * glTFImage.height * 4;
                    buffer = new unsigned char[bufferSize];
                    unsigned char *rgba = buffer;
                    unsigned char *rgb = &glTFImage.image[0];
                    for (size_t i = 0; i < glTFImage.width * glTFImage.height; ++i)
                    {
                        memcpy(rgba, rgb, sizeof(unsigned char) * 3);
                        rgba += 4;
                        rgb += 3;
                    }
                    deleteBuffer = true;
                }
                else
                {
                    buffer = &glTFImage.image[0];
                    bufferSize = glTFImage.image.size();
                }
                // Load texture from image buffer
                images[i].texture.fromBuffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_UNORM, glTFImage.width, glTFImage.height, engine);
                if (deleteBuffer)
                {
                    delete[] buffer;
                }
            }

            if (!images.empty())
            {
                std::cout << "GLTF image resources are loaded!" << std::endl;
            }
        }

        void loadTextures(tinygltf::Model &input)
        {
            textures.resize(input.textures.size());
            for (size_t i = 0; i < input.textures.size(); i++)
            {
                textures[i].imageIndex = input.textures[i].source;
            }
        }

        void loadMaterials(tinygltf::Model &input)
        {
            materials.resize(input.materials.size());
            for (size_t i = 0; i < input.materials.size(); i++)
            {
                // We only read the most basic properties required for our sample
                tinygltf::Material glTFMaterial = input.materials[i];
                // Get the base color factor
                if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end())
                {
                    materials[i].baseColorFactor = glm::make_vec4(glTFMaterial.values["baseColorFactor"].ColorFactor().data());
                }
                // Get base color texture index
                if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end())
                {
                    materials[i].baseColorTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
                }
            }
        }

        void loadNode(const tinygltf::Node &inputNode, const tinygltf::Model &input, Node *parent, std::vector<uint32_t> &indexBuffer, std::vector<Vertex> &vertexBuffer)
        {
            Node *node = new Node{};
            node->matrix = glm::mat4(1.0f);
            node->parent = parent;

            // Get the local node matrix
            // It's either made up from translation, rotation, scale or a 4x4 matrix
            if (inputNode.translation.size() == 3)
            {
                node->matrix = glm::translate(node->matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
            }
            if (inputNode.rotation.size() == 4)
            {
                glm::quat q = glm::make_quat(inputNode.rotation.data());
                node->matrix *= glm::mat4(q);
            }
            if (inputNode.scale.size() == 3)
            {
                node->matrix = glm::scale(node->matrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
            }
            if (inputNode.matrix.size() == 16)
            {
                node->matrix = glm::make_mat4x4(inputNode.matrix.data());
            };

            // Load node's children
            if (inputNode.children.size() > 0)
            {
                for (size_t i = 0; i < inputNode.children.size(); i++)
                {
                    loadNode(input.nodes[inputNode.children[i]], input, node, indexBuffer, vertexBuffer);
                }
            }

            // If the node contains mesh data, we load vertices and indices from the buffers
            // In glTF this is done via accessors and buffer views
            if (inputNode.mesh > -1)
            {
                const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
                // Iterate through all primitives of this node's mesh
                for (size_t i = 0; i < mesh.primitives.size(); i++)
                {
                    const tinygltf::Primitive &glTFPrimitive = mesh.primitives[i];
                    uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
                    uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
                    uint32_t indexCount = 0;
                    // Vertices
                    {
                        const float *positionBuffer = nullptr;
                        const float *normalsBuffer = nullptr;
                        const float *texCoordsBuffer = nullptr;
                        size_t vertexCount = 0;

                        // Get buffer data for vertex positions
                        if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end())
                        {
                            const tinygltf::Accessor &accessor = input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
                            const tinygltf::BufferView &view = input.bufferViews[accessor.bufferView];
                            positionBuffer = reinterpret_cast<const float *>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                            vertexCount = accessor.count;
                        }
                        // Get buffer data for vertex normals
                        if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end())
                        {
                            const tinygltf::Accessor &accessor = input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
                            const tinygltf::BufferView &view = input.bufferViews[accessor.bufferView];
                            normalsBuffer = reinterpret_cast<const float *>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                        }
                        // Get buffer data for vertex texture coordinates
                        // glTF supports multiple sets, we only load the first one
                        if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end())
                        {
                            const tinygltf::Accessor &accessor = input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
                            const tinygltf::BufferView &view = input.bufferViews[accessor.bufferView];
                            texCoordsBuffer = reinterpret_cast<const float *>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                        }

                        // Append data to model's vertex buffer
                        for (size_t v = 0; v < vertexCount; v++)
                        {
                            Vertex vert{};
                            vert.pos = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
                            vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
                            vert.uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
                            vert.color = glm::vec3(1.0f);
                            vertexBuffer.push_back(vert);
                        }
                    }
                    // Indices
                    {
                        const tinygltf::Accessor &accessor = input.accessors[glTFPrimitive.indices];
                        const tinygltf::BufferView &bufferView = input.bufferViews[accessor.bufferView];
                        const tinygltf::Buffer &buffer = input.buffers[bufferView.buffer];

                        indexCount += static_cast<uint32_t>(accessor.count);

                        // glTF supports different component types of indices
                        switch (accessor.componentType)
                        {
                        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                        {
                            const uint32_t *buf = reinterpret_cast<const uint32_t *>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                            for (size_t index = 0; index < accessor.count; index++)
                            {
                                indexBuffer.push_back(buf[index] + vertexStart);
                            }
                            break;
                        }
                        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                        {
                            const uint16_t *buf = reinterpret_cast<const uint16_t *>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                            for (size_t index = 0; index < accessor.count; index++)
                            {
                                indexBuffer.push_back(buf[index] + vertexStart);
                            }
                            break;
                        }
                        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                        {
                            const uint8_t *buf = reinterpret_cast<const uint8_t *>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                            for (size_t index = 0; index < accessor.count; index++)
                            {
                                indexBuffer.push_back(buf[index] + vertexStart);
                            }
                            break;
                        }
                        default:
                            std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
                            return;
                        }
                    }
                    Primitive primitive{};
                    primitive.firstIndex = firstIndex;
                    primitive.indexCount = indexCount;
                    primitive.materialIndex = glTFPrimitive.material;
                    node->mesh.primitives.push_back(primitive);
                }
            }

            if (parent)
            {
                parent->children.push_back(node);
            }
            else
            {
                nodes.push_back(node);
            }
        }

        bool loadModel(tinygltf::Model &model, const char *filename, Engine &engine)
        {
            tinygltf::TinyGLTF loader;
            std::string err;
            std::string warn;

            std::cout << "Over Here!?!" << std::endl;

            bool res = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
            if (!warn.empty())
            {
                std::cout << "WARN: " << warn << std::endl;
            }

            if (!err.empty())
            {
                std::cout << "ERR: " << err << std::endl;
            }

            if (!res)
                std::cout << "Failed to load glTF: " << filename << std::endl;
            else
                std::cout << "Loaded glTF: " << filename << std::endl;

            if (res)
            {

                loadImages(model, engine);
                loadMaterials(model);
                loadTextures(model);
                const tinygltf::Scene &scene = model.scenes[0];
                for (size_t i = 0; i < scene.nodes.size(); i++)
                {
                    const tinygltf::Node node = model.nodes[scene.nodes[i]];
                    loadNode(node, model, nullptr, indexBuffer, vertexBuffer);
                }
            }

            return res;
        }

        // Draw a single node including child nodes (if present)
        void drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, Node *node)
        {
            if (node->mesh.primitives.size() > 0)
            {
                // Pass the node's matrix via push constants
                // Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
                glm::mat4 nodeMatrix = node->matrix;
                Node *currentParent = node->parent;
                while (currentParent)
                {
                    nodeMatrix = currentParent->matrix * nodeMatrix;
                    currentParent = currentParent->parent;
                }
                // Pass the final matrix to the vertex shader using push constants
                vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
                for (Primitive &primitive : node->mesh.primitives)
                {
                    if (primitive.indexCount > 0)
                    {
                        // Get the texture index for this primitive
                        Texture texture = textures[materials[primitive.materialIndex].baseColorTextureIndex];
                        // Bind the descriptor for the current primitive's texture
                        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &images[texture.imageIndex].descriptorSet, 0, nullptr);
                        vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
                    }
                }
            }
            for (auto &child : node->children)
            {
                drawNode(commandBuffer, pipelineLayout, child);
            }
        }

        // Draw the glTF scene starting at the top-level-nodes
        void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout)
        {
            // All vertices and indices are stored in single buffers, so we only need to bind once
            VkDeviceSize offsets[1] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
            vkCmdBindIndexBuffer(commandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
            // Render all nodes at top-level
            for (auto &node : nodes)
            {
                drawNode(commandBuffer, pipelineLayout, node);
            }
        }
    };

    class PipelineGLTF
    {
    public:

        bool wireframe = false;

        PipelineGLTF()
        {
            
        }

        Model model;
        Engine engine;
        Camera camera;

        bool prepared = false;

        struct UniformData
        {
            glm::mat4 projection;
            glm::mat4 model;
            glm::vec4 lightPos = glm::vec4(5.0f, 5.0f, -5.0f, 1.0f);
            glm::vec4 viewPos;
        } uniformData;

        std::array<Buffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers;

        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        struct Pipelines
        {
            VkPipeline solid{VK_NULL_HANDLE};
            VkPipeline wireframe{VK_NULL_HANDLE};
        } pipelines;

        struct DescriptorSetLayouts
        {
            VkDescriptorSetLayout matrices{VK_NULL_HANDLE};
            VkDescriptorSetLayout textures{VK_NULL_HANDLE};
        } descriptorSetLayouts;
        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets{};

        

        void loadglTFFile(std::string filename, Engine &engine)
        {
            tinygltf::Model glTFInput;
            tinygltf::TinyGLTF gltfContext;
            std::string error, warning;

#if defined(__ANDROID__)
            // On Android all assets are packed with the apk in a compressed form, so we need to open them using the asset manager
            // We let tinygltf handle this, by passing the asset manager of our app
            tinygltf::asset_manager = androidApp->activity->assetManager;
#endif
            bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, filename);

            std::vector<uint32_t> indexBuffer;
            std::vector<Model::Vertex> vertexBuffer;

            if (fileLoaded)
            {
                model.loadImages(glTFInput, engine);
                model.loadMaterials(glTFInput);
                model.loadTextures(glTFInput);
                const tinygltf::Scene &scene = glTFInput.scenes[0];
                for (size_t i = 0; i < scene.nodes.size(); i++)
                {
                    const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
                    model.loadNode(node, glTFInput, nullptr, indexBuffer, vertexBuffer);
                }
            }
            else
            {
                throw std::runtime_error("Could not open the glTF file.\n\nMake sure the assets submodule has been checked out and is up-to-date.");
            }
            // Create and upload vertex and index buffer
            // We will be using one single vertex buffer and one single index buffer for the whole glTF scene
            // Primitives (of the glTF model) will then index into these using index offsets

            size_t vertexBufferSize = vertexBuffer.size() * sizeof(Model::Vertex);
            size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
            model.indices.count = static_cast<uint32_t>(indexBuffer.size());

            Buffer vertexStaging, indexStaging;
            VkDeviceMemory vertexStagingMemory, indexStagingMemory;

            // Create host visible staging buffers (source)
            engine.createBuffer(
                vertexBufferSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                vertexStaging.buffer,
                vertexStagingMemory);
            // Index data
            engine.createBuffer(
                indexBufferSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                indexStaging.buffer,
                indexStagingMemory);

            // Create device local buffers (target)
            engine.createBuffer(
                vertexBufferSize,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                model.vertices.buffer,
                model.vertices.memory);

            engine.createBuffer(
                indexBufferSize,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                model.indices.buffer,
                model.indices.memory);

            // Copy data from staging buffers (host) do device local buffer (gpu)
            VkCommandBuffer copyCmd = engine.beginSingleTimeCommands();
            VkBufferCopy copyRegion = {};

            copyRegion.size = vertexBufferSize;
            vkCmdCopyBuffer(
                copyCmd,
                vertexStaging.buffer,
                model.vertices.buffer,
                1,
                &copyRegion);

            copyRegion.size = indexBufferSize;
            vkCmdCopyBuffer(
                copyCmd,
                indexStaging.buffer,
                model.indices.buffer,
                1,
                &copyRegion);

            engine.endSingleTimeCommands(copyCmd);

            vertexStaging.destroy();
            indexStaging.destroy();
        }

        void
        loadAssets()
        {
            loadglTFFile("../models/steampunk/scene.gltf", engine);
        }

        void setupDescriptors()
        {
            /*
                This sample uses separate descriptor sets (and layouts) for the matrices and materials (textures)
            */

            VkDescriptorPoolSize dpsUniformBuffer{};
            dpsUniformBuffer.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            dpsUniformBuffer.descriptorCount = MAX_FRAMES_IN_FLIGHT;

            VkDescriptorPoolSize dpsModelImages{};
            dpsModelImages.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            dpsModelImages.descriptorCount = static_cast<uint32_t>(model.images.size()) * MAX_FRAMES_IN_FLIGHT;

            std::vector<VkDescriptorPoolSize> poolSizes =
                {
                    dpsUniformBuffer, dpsModelImages};

            // One set for matrices and one per model image/texture
            const uint32_t maxSetCount = (static_cast<uint32_t>(model.images.size()) + 1) * MAX_FRAMES_IN_FLIGHT;

            VkDescriptorPoolCreateInfo descriptorPoolInfo{};
            descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            descriptorPoolInfo.pPoolSizes = poolSizes.data();
            descriptorPoolInfo.maxSets = maxSetCount;

            // Descriptor set layout for passing matrices
            VkDescriptorSetLayoutBinding setLayoutBinding{};
            setLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            setLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            setLayoutBinding.binding = 0;

            VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI{};
            descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorSetLayoutCI.pBindings = &setLayoutBinding;
            descriptorSetLayoutCI.bindingCount = 1;

            if (vkCreateDescriptorSetLayout(engine.logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.matrices) != VK_SUCCESS)
            {
                throw std::runtime_error("Could not create descriptor set layout for the matrices!");
            }

            // Descriptor set layout for passing material textures
            setLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            setLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            setLayoutBinding.binding = 0;

            if (vkCreateDescriptorSetLayout(engine.logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.textures) != VK_SUCCESS)
            {
                throw std::runtime_error("Could not create descriptor set layout for the textures!");
            }

            // Descriptor set for scene matrices per frame, just like the buffers themselves
            for (auto i = 0; i < uniformBuffers.size(); i++)
            {

                VkDescriptorSetAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.descriptorPool = engine.descriptorPool;
                allocInfo.pSetLayouts = &descriptorSetLayouts.matrices;
                allocInfo.descriptorSetCount = 1;

                if (vkAllocateDescriptorSets(engine.logicalDevice, &allocInfo, &descriptorSets[i]) != VK_SUCCESS)
                {
                    throw std::runtime_error("Could not allocate descriptor set!");
                }

                VkWriteDescriptorSet writeDescriptorSet{};
                writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSet.dstSet = descriptorSets[i];
                writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                writeDescriptorSet.dstBinding = 0;
                writeDescriptorSet.pBufferInfo = &uniformBuffers[i].descriptor;
                writeDescriptorSet.descriptorCount = 1;

                vkUpdateDescriptorSets(engine.logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
            }

            // Descriptor sets for materials, since they only use static images, no need to duplicate them per frame
            for (auto &image : model.images)
            {
                VkDescriptorSetAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.descriptorPool = engine.descriptorPool;
                allocInfo.pSetLayouts = &descriptorSetLayouts.textures;
                allocInfo.descriptorSetCount = 1;

                if (vkAllocateDescriptorSets(engine.logicalDevice, &allocInfo, &image.descriptorSet) != VK_SUCCESS)
                {
                    throw std::runtime_error("Could not allocate descriptor set!");
                }

                VkWriteDescriptorSet writeDescriptorSet{};
                writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSet.dstSet = image.descriptorSet;
                writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                writeDescriptorSet.dstBinding = 0;
                writeDescriptorSet.pImageInfo = &image.texture.descriptor;
                writeDescriptorSet.descriptorCount = 1;

                vkUpdateDescriptorSets(engine.logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
            }
        }

        void preparePipelines()
        {
            // Layout
            // The pipeline layout uses both descriptor sets (set 0 = matrices, set 1 = material)
            std::array<VkDescriptorSetLayout, 2> setLayouts = {descriptorSetLayouts.matrices, descriptorSetLayouts.textures};

            VkPipelineLayoutCreateInfo pipelineLayoutCI{};
            pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutCI.pSetLayouts = setLayouts.data();
            pipelineLayoutCI.setLayoutCount = static_cast<uint32_t>(setLayouts.size());

            // We will use push constants to push the local matrices of a primitive to the vertex shader
            VkPushConstantRange pushConstantRange{};
            pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            pushConstantRange.size = sizeof(glm::mat4);
            pushConstantRange.offset = 0;

            // Push constant ranges are part of the pipeline layout
            pipelineLayoutCI.pushConstantRangeCount = 1;
            pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
            if (vkCreatePipelineLayout(engine.logicalDevice, &pipelineLayoutCI, nullptr, &pipelineLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("Could not create pipeline layout");
            }

            // Pipeline
            VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI{};

            inputAssemblyStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssemblyStateCI.flags = 0;
            inputAssemblyStateCI.primitiveRestartEnable = VK_FALSE;

            VkPipelineRasterizationStateCreateInfo rasterizationStateCI{};

            rasterizationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizationStateCI.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterizationStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rasterizationStateCI.flags = 0;
            rasterizationStateCI.depthBiasEnable = VK_FALSE;
            rasterizationStateCI.lineWidth = 1.0f;

            VkPipelineColorBlendAttachmentState blendAttachmentStateCI{};

            blendAttachmentStateCI.colorWriteMask = 0xf;
            blendAttachmentStateCI.blendEnable = VK_FALSE;

            VkPipelineColorBlendStateCreateInfo colorBlendStateCI{};
            colorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlendStateCI.attachmentCount = 1;
            colorBlendStateCI.pAttachments = &blendAttachmentStateCI;

            VkPipelineDepthStencilStateCreateInfo depthStencilStateCI{};
            depthStencilStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencilStateCI.depthTestEnable = VK_TRUE;
            depthStencilStateCI.depthWriteEnable = VK_TRUE;
            depthStencilStateCI.back.compareOp = VK_COMPARE_OP_ALWAYS;
            depthStencilStateCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

            VkPipelineViewportStateCreateInfo viewportStateCI{};
            viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportStateCI.viewportCount = 1;
            viewportStateCI.scissorCount = 1;
            viewportStateCI.flags = 0;

            VkPipelineMultisampleStateCreateInfo multisampleStateCI{};
            multisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampleStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            multisampleStateCI.flags = 0;

            const std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamicStateCI{};
            dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicStateCI.pDynamicStates = dynamicStateEnables.data();
            dynamicStateCI.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
            dynamicStateCI.flags = 0;

            // Vertex input bindings and attributes
            VkVertexInputBindingDescription vibDesc{};
            vibDesc.binding = 0;
            vibDesc.stride = sizeof(Model::Vertex);
            vibDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            const std::vector<VkVertexInputBindingDescription> vertexInputBindings{vibDesc};

            VkVertexInputAttributeDescription viAttrDescPos{};
            viAttrDescPos.binding = 0;
            viAttrDescPos.location = 0;
            viAttrDescPos.format = VK_FORMAT_R32G32B32_SFLOAT;
            viAttrDescPos.offset = offsetof(Model::Vertex, pos);

            VkVertexInputAttributeDescription viAttrDescNormal{};
            viAttrDescNormal.binding = 0;
            viAttrDescNormal.location = 1;
            viAttrDescNormal.format = VK_FORMAT_R32G32B32_SFLOAT;
            viAttrDescNormal.offset = offsetof(Model::Vertex, normal);

            VkVertexInputAttributeDescription viAttrDescUV{};
            viAttrDescUV.binding = 0;
            viAttrDescUV.location = 2;
            viAttrDescUV.format = VK_FORMAT_R32G32B32_SFLOAT;
            viAttrDescUV.offset = offsetof(Model::Vertex, uv);

            VkVertexInputAttributeDescription viAttrDescColor{};
            viAttrDescColor.binding = 0;
            viAttrDescColor.location = 3;
            viAttrDescColor.format = VK_FORMAT_R32G32B32_SFLOAT;
            viAttrDescColor.offset = offsetof(Model::Vertex, color);

            const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
                viAttrDescPos,
                viAttrDescNormal,
                viAttrDescUV,
                viAttrDescColor,
            };

            VkPipelineVertexInputStateCreateInfo vertexInputStateCI{};
            vertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            vertexInputStateCI.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
            vertexInputStateCI.pVertexBindingDescriptions = vertexInputBindings.data();
            vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
            vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

            auto mesh_vertShaderCode = readFile("../shaders/mesh.vert.spv");
            auto mesh_fragShaderCode = readFile("../shaders/mesh.frag.spv");

            VkShaderModule vertShaderModule = engine.createShaderModule(mesh_vertShaderCode);
            VkShaderModule fragShaderModule = engine.createShaderModule(mesh_fragShaderCode);

            VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
            vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;

            vertShaderStageInfo.module = vertShaderModule;
            vertShaderStageInfo.pName = "main";

            VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
            fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

            fragShaderStageInfo.module = fragShaderModule;
            fragShaderStageInfo.pName = "main";

            VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

            VkGraphicsPipelineCreateInfo pipelineCI{};
            pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineCI.layout = pipelineLayout;
            pipelineCI.renderPass = engine.renderPass;
            pipelineCI.flags = 0;
            pipelineCI.basePipelineIndex = -1;
            pipelineCI.basePipelineHandle = VK_NULL_HANDLE;

            pipelineCI.pVertexInputState = &vertexInputStateCI;
            pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
            pipelineCI.pRasterizationState = &rasterizationStateCI;
            pipelineCI.pColorBlendState = &colorBlendStateCI;
            pipelineCI.pMultisampleState = &multisampleStateCI;
            pipelineCI.pViewportState = &viewportStateCI;
            pipelineCI.pDepthStencilState = &depthStencilStateCI;
            pipelineCI.pDynamicState = &dynamicStateCI;
            pipelineCI.stageCount = 2;
            pipelineCI.pStages = shaderStages;

            // Solid rendering pipeline
            if (vkCreateGraphicsPipelines(engine.logicalDevice, engine.pipelineCache, 1, &pipelineCI, nullptr, &pipelines.solid) != VK_SUCCESS)
            {
                throw std::runtime_error("Could not create graphics pipeline!");
            }

            // Wire frame rendering pipeline
            if (engine.supportedFeatures.fillModeNonSolid)
            {
                rasterizationStateCI.polygonMode = VK_POLYGON_MODE_LINE;
                rasterizationStateCI.lineWidth = 1.0f;

                if (vkCreateGraphicsPipelines(engine.logicalDevice, engine.pipelineCache, 1, &pipelineCI, nullptr, &pipelines.wireframe) != VK_SUCCESS)
                {
                    throw std::runtime_error("Could not create graphics pipeline with feature ( fillModeNonSolid )");
                }
            }
        }

        // Prepare and initialize uniform buffer containing shader uniforms
        void prepareUniformBuffers()
        {
            for (auto &buffer : uniformBuffers)
            {
                engine.createBuffer(buffer.size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buffer.buffer, buffer.memory);

                // sizeof(UniformData), &uniformData;

                void *data = &uniformData;
                buffer.map();
                memcpy(buffer.mapped, data, buffer.size);
                if ((VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
                    buffer.flush();

                buffer.unmap();

                buffer.map();
            }
        }

        void updateUniformBuffers()
        {
            // uniformData.projection = camera.matrices.perspective;
            // uniformData.model = camera.matrices.view;
            // uniformData.viewPos = camera.GetViewMatrix();

            memcpy(uniformBuffers[engine.currentFrame].mapped, &uniformData, sizeof(UniformData));
        }

        void prepare()
        {
            loadAssets();
            prepareUniformBuffers();
            setupDescriptors();
            preparePipelines();
            prepared = true;
        }

        void buildCommandBuffer()
        {
            VkCommandBuffer cmdBuffer = engine.commandBuffers[engine.currentFrame];

            VkCommandBufferBeginInfo cmdBufInfo{};
			cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            VkClearValue clearValues[2]{};
            clearValues[0].color = {{0.25f, 0.25f, 0.25f, 1.0f}};
            clearValues[1].depthStencil = {1.0f, 0};

            VkRenderPassBeginInfo renderPassBeginInfo{};
            renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassBeginInfo.renderPass = engine.renderPass;
            renderPassBeginInfo.renderArea.offset.x = 0;
            renderPassBeginInfo.renderArea.offset.y = 0;
            renderPassBeginInfo.renderArea.extent = engine.swapChainExtent;
            renderPassBeginInfo.clearValueCount = 2;
            renderPassBeginInfo.pClearValues = clearValues;
            renderPassBeginInfo.framebuffer = engine.swapChainFramebuffers[engine.currentFrame];

            if(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo) != VK_SUCCESS)
            {
                throw std::runtime_error("Could not begin command buffer");
            }

            vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            const VkViewport viewport{
                .width = (float)engine.swapChainExtent.width,
                .height = (float)engine.swapChainExtent.height,
                .minDepth = 0.0f,
                .maxDepth = 1.0f
            };

            vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

            const VkRect2D scissor{
                .offset = {0, 0},
                .extent = engine.swapChainExtent,
            };

            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

            // Bind scene matrices descriptor to set 0
            vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[engine.currentFrame], 0, nullptr);
            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? pipelines.wireframe : pipelines.solid);
            model.draw(cmdBuffer, pipelineLayout);
            vkCmdEndRenderPass(cmdBuffer);
            if(vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to record command buffer (GLTF)");
            }
        }

        void render()
        {
            if (!prepared)
                return;
            updateUniformBuffers();
            buildCommandBuffer();
        }
    };

};
