
#include <vector>
#include <glm/glm.hpp>

struct Vertex
{

    alignas(16) glm::vec3 pos;
    alignas(16) glm::vec3 normal;
    alignas(16) glm::vec2 uv;
};

struct Planes
{
    std::vector<Vertex> getPlanesVertices()
    {
        std::vector<Vertex> planeVertices = {
            {{-100.0f, 0.0f, -100.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{100.0f, 0.0f, -100.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{100.0f, 0.0f, 100.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
            {{-100.0f, 0.0f, 100.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        };

        return planeVertices;
    }

    std::vector<uint32_t> getPlaneIndices()
    {
        std::vector<uint32_t> planeIndices = {0, 1, 2, 2, 3, 0};
        return planeIndices;
    }
};
