#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>

#include <vulkan/vulkan.h>

struct InstanceData
{
	glm::mat4 modelMatrix;
};

struct Vertex
{
	alignas(16) glm::vec3 pos;
	alignas(16) glm::vec3 color;
	alignas(16) glm::vec2 texCoord;

	static std::array<VkVertexInputBindingDescription, 2> getbindingDescription()
	{
		VkVertexInputBindingDescription bindingVertex{};

		bindingVertex.binding = 0;
		bindingVertex.stride = sizeof(Vertex);
		bindingVertex.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		VkVertexInputBindingDescription bindingInstance{};
		bindingInstance.binding = 1;
		bindingInstance.stride = sizeof(InstanceData);
		bindingInstance.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

		std::array bindings = {bindingVertex, bindingInstance};

		return bindings;
	}

	static std::array<VkVertexInputAttributeDescription, 7> getAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 7> attrDesc{};

		int index = 0;

		attrDesc[index].binding = 0;
		attrDesc[index].location = 0;
		attrDesc[index].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrDesc[index].offset = offsetof(Vertex, pos);

		index++;

		attrDesc[index].binding = 0;
		attrDesc[index].location = 1;
		attrDesc[index].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrDesc[index].offset = offsetof(Vertex, color);

		index++;

		attrDesc[index].binding = 0;
		attrDesc[index].location = 2;
		attrDesc[index].format = VK_FORMAT_R32G32_SFLOAT;
		attrDesc[index].offset = offsetof(Vertex, texCoord);

		// Instancing
		index++;

		attrDesc[index].location = 3;
		attrDesc[index].binding = 1;
		attrDesc[index].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attrDesc[index].offset = 0;

		index++;

		attrDesc[index].location = 4;
		attrDesc[index].binding = 1;
		attrDesc[index].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attrDesc[index].offset = 1 * sizeof(glm::vec4);

		index++;

		attrDesc[index].location = 5;
		attrDesc[index].binding = 1;
		attrDesc[index].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attrDesc[index].offset = 2 * sizeof(glm::vec4);

		index++;

		attrDesc[index].location = 6;
		attrDesc[index].binding = 1;
		attrDesc[index].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attrDesc[index].offset = 3 * sizeof(glm::vec4);

		return attrDesc;
	}
};

std::vector<Vertex> getCubeVertices()
{
	std::vector<Vertex> vertices = {
		// Front face (z = 1.0f)
		{glm::vec3(-1.0f, -1.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f)}, // Bottom-left
		{glm::vec3(1.0f, -1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)},	 // Bottom-right
		{glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)},	 // Top-right
		{glm::vec3(-1.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)},	 // Top-left

		// Back face (z = -1.0f)
		{glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f)}, // Bottom-left
		{glm::vec3(1.0f, -1.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)},  // Bottom-right
		{glm::vec3(1.0f, 1.0f, -1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)},	  // Top-right
		{glm::vec3(-1.0f, 1.0f, -1.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)},  // Top-left

		// Left face (x = -1.0f)
		{glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f)}, // Bottom-back
		{glm::vec3(-1.0f, -1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)},  // Bottom-front
		{glm::vec3(-1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)},	  // Top-front
		{glm::vec3(-1.0f, 1.0f, -1.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)},  // Top-back

		// Right face (x = 1.0f)
		{glm::vec3(1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f)}, // Bottom-back
		{glm::vec3(1.0f, -1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)},	 // Bottom-front
		{glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)},	 // Top-front
		{glm::vec3(1.0f, 1.0f, -1.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)},	 // Top-back

		// Bottom face (y = -1.0f)
		{glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f)}, // Back-left
		{glm::vec3(-1.0f, -1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)},  // Front-left
		{glm::vec3(1.0f, -1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)},	  // Front-right
		{glm::vec3(1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)},  // Back-right

		// Top face (y = 1.0f)
		{glm::vec3(-1.0f, 1.0f, -1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f)}, // Back-left
		{glm::vec3(-1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)},	 // Front-left
		{glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)},	 // Front-right
		{glm::vec3(1.0f, 1.0f, -1.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)}	 // Back-right
	};
	return vertices;
}

std::vector<uint16_t> getCubeIndices()
{
	std::vector<uint16_t> indices;
	for (uint32_t i = 0; i < 24; i += 4)
	{
		indices.push_back(i + 0);
		indices.push_back(i + 1);
		indices.push_back(i + 2);
		indices.push_back(i + 0);
		indices.push_back(i + 2);
		indices.push_back(i + 3);
	}
	return indices;
}

#pragma once

const std::vector<uint16_t> cube_indices = getCubeIndices();
const std::vector<Vertex> cube_vertices = getCubeVertices();

std::vector<InstanceData> cube_instances =
	{
		{glm::scale(glm::mat4(1.0f), glm::vec3(120.0f))},
		{glm::scale(glm::mat4(1.0f), glm::vec3(60.0f))},
		{glm::scale(glm::mat4(1.0f), glm::vec3(30.0f))},
		{glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 3.0f, 0.0f))},
		{glm::translate(glm::mat4(1.0f), glm::vec3(7.0f, 6.0f, 0.0f))},
		{glm::translate(glm::mat4(1.0f), glm::vec3(9.0f, 9.0f, 0.0f))},
};