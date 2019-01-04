#ifndef RAYTRACINGGLSLDEFINES_HXX
#define RAYTRACINGGLSLDEFINES_HXX

#include <stdint.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct SceneConstantBuffer {
    glm::mat4 projectionToWorld;
    glm::vec4 cameraPosition;
    glm::vec4 lightPosition;
    glm::vec4 lightAmbientColor;
    glm::vec4 lightDiffuseColor;
    float reflectance;
    float elapsedTime;
};

struct PrimitiveConstantBuffer {
    glm::vec4 albedo;
    float reflectanceCoef;
    float diffuseCoef;
    float specularCoef;
    float specularPower;
    float stepScale;

    glm::vec3 padding;
};

struct PrimitiveInstanceConstantBuffer
{
    uint32_t instanceIndex;
    uint32_t primitiveType; // Procedural primitive type

    glm::vec2 padding;
};

struct PrimitiveInstancePerFrameBuffer {
    glm::mat4 localSpaceToBottomLevelAS;
    glm::mat4 bottomLevelASToLocalSpace;
};

typedef uint16_t Index;

struct Vertex {
    glm::vec3 position;
    //glm::vec3 normal;
};

namespace RayType {
    enum Enum {
        Radiance = 0,
        Shadow,
        Count
    };
}

static const glm::vec4 kChromiumReflectance = glm::vec4(0.549f, 0.556f, 0.554f, 1.0f);

static const glm::vec4 kBackgroundColor = glm::vec4(0.8f, 0.9f, 1.0f, 1.0f);
static const float kInShadowRadiance = 0.35f;

namespace AnalyticPrimitive {
    enum Enum {
        AABB = 0,
        Spheres,
        Count
    };
}

namespace VolumetricPrimitive {
    enum Enum {
        Metaballs = 0,
        Count
    };
}

namespace SignedDistancePrimitive {
    enum Enum {
        MiniSpheres = 0,
        IntersectedRoundCube,
        SquareTorus,
        TwistedTorus,
        Cog,
        Cylinder,
        FractalPyramid,
        Count
    };
}

#endif // RAYTRACINGGLSLDEFINES_HXX
