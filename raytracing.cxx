#include "raytracing.hxx"

#include <string.h>

#define ACTIVATE_RAYTRACING(methodname) \
    do { \
        methodname = reinterpret_cast<PFN_##methodname##NV>(vkGetDeviceProcAddr(m_device, #methodname"NV")); \
        if (!methodname) { \
            printf("Could not get "#methodname"NV pointer\n"); \
        } \
    } while (0)

#define DEFINE_RAYTRACING(methodname) \
    PFN_##methodname##NV methodname = nullptr

DEFINE_RAYTRACING(vkCreateAccelerationStructure);
DEFINE_RAYTRACING(vkDestroyAccelerationStructure);
DEFINE_RAYTRACING(vkGetAccelerationStructureMemoryRequirements);
DEFINE_RAYTRACING(vkBindAccelerationStructureMemory);
DEFINE_RAYTRACING(vkCmdBuildAccelerationStructure);
DEFINE_RAYTRACING(vkCmdCopyAccelerationStructure);
DEFINE_RAYTRACING(vkCmdTraceRays);
DEFINE_RAYTRACING(vkCreateRayTracingPipelines);
DEFINE_RAYTRACING(vkGetRayTracingShaderGroupHandles);
DEFINE_RAYTRACING(vkGetAccelerationStructureHandle);
DEFINE_RAYTRACING(vkCmdWriteAccelerationStructuresProperties);
DEFINE_RAYTRACING(vkCompileDeferred);

struct VkGeometryInstanceNV {
    float          transform[12];
    uint32_t       instanceCustomIndex : 24;
    uint32_t       mask : 8;
    uint32_t       instanceOffset : 24;
    uint32_t       flags : 8;
    uint64_t       accelerationStructureHandle;
};

const char kRaygenShaderSource[] =
        R"(#version 460 core
        #extension GL_NV_ray_tracing : require

        layout(set = 0, binding = 0) uniform accelerationStructureNV topLevelAS;
        layout(set = 0, binding = 1, rgba8) uniform image2D image;

        struct RayPayload {
            vec4 color;
            uint recursionDepth;
        };

        layout(location = 0) rayPayloadNV RayPayload rayPayload;

        struct SceneConstantBuffer {
            mat4x4 projectionToWorld;
            vec4 cameraPosition;
            vec4 lightPosition;
            vec4 lightAmbientColor;
            vec4 lightDiffuseColor;
            float reflectance;
            float elapsedTime;
        };

        layout(set = 0, binding = 2, std140) uniform appData {
            SceneConstantBuffer params;
        };

        struct Ray {
            vec3 origin;
            vec3 direction;
        };

        Ray generateCameraRay(uvec2 index, in vec3 cameraPosition, in mat4x4 projectionToWorld) {

            vec2 xy = index + 0.5;
            vec2 screenPos = xy / gl_LaunchSizeNV.xy * 2.0 - 1.0;

            screenPos.y = -screenPos.y;

            vec4 world = projectionToWorld * vec4(screenPos, 0.0, 1.0);
            world.xyz /= world.w;

            Ray ray;
            ray.origin = cameraPosition;
            ray.direction = normalize(world.xyz - ray.origin);

            return ray;
        }

        void main()
        {
           Ray ray = generateCameraRay(gl_LaunchIDNV.xy, params.cameraPosition.xyz, params.projectionToWorld);

           const uint rayFlags = gl_RayFlagsCullBackFacingTrianglesNV;
           const uint cullMask = 0xFF;
           const uint sbtRecordOffset = 0;
           const uint sbtRecordStride = 0;
           const uint missIndex = 0;
           const float tmin = 0;
           const float tmax = 10000;
           const int payloadLocation = 0;

           rayPayload.color = vec4(0.0, 0.0, 0.0, 0.0);
           rayPayload.recursionDepth = 1;

           traceNV(topLevelAS, rayFlags, cullMask, sbtRecordOffset, sbtRecordStride, missIndex, ray.origin, tmin, ray.direction, tmax, payloadLocation);

           //imageStore(image, ivec2(gl_LaunchIDNV.xy), vec4(inUV.x, inUV.y, 0.0, 0.0));
           imageStore(image, ivec2(gl_LaunchIDNV.xy), rayPayload.color);
        }
        )";

/*
const char kClosestHitShaderSource[] = R"(#version 460 core
#extension GL_NV_ray_tracing : require

layout(location = 0) rayPayloadInNV vec3 hitValue;
hitAttributeNV vec3 normal;

void main() {
    const vec3 lightPos = vec3(1.0, 1.0, -1.0);
    const vec3 origin = gl_WorldRayOriginNV + gl_RayTminNV * normalize(gl_WorldRayDirectionNV);
    vec3 lightDir = normalize(lightPos - vec3(0., 0., 0.));
    float diffuse = dot(normal, lightDir);
    vec3 reflectionDir = normalize(reflect(-lightDir, normal));
    vec3 viewDir = normalize(origin - vec3(0., 0., 0.));
    float specular = pow(max(0.0, dot(viewDir, reflectionDir)), 50.0);
    hitValue = vec3(0.1, 0.0, 0.0) + vec3(1.0, 0.0, 0.0) * clamp(diffuse, 0.0, 1.0) + vec3(1.0, 1.0, 1.0) * specular;
}
)";
*/

const char kClosestHitTriangleShaderSource[] =
        R"(#version 460 core
        #extension GL_NV_ray_tracing : require
        #extension GL_EXT_nonuniform_qualifier : require

        layout(set = 0, binding = 0) uniform accelerationStructureNV topLevelAS;

        struct RayPayload {
            vec4 color;
            uint recursionDepth;
        };

        layout (location = 0) rayPayloadInNV RayPayload rayPayload;
        hitAttributeNV vec2 attribs;
        layout(location = 2) rayPayloadNV bool isHit;

        struct SceneConstantBuffer {
            mat4x4 projectionToWorld;
            vec4 cameraPosition;
            vec4 lightPosition;
            vec4 lightAmbientColor;
            vec4 lightDiffuseColor;
            float reflectance;
            float elapsedTime;
        };

        struct PrimitiveConstantBuffer {
            vec4 albedo;
            float reflectanceCoef;
            float diffuseCoef;
            float specularCoef;
            float specularPower;
            float stepScale;

            float padding[3];
        };

        layout(set = 0, binding = 2, std140) uniform appData {
            SceneConstantBuffer params;
        };

        const vec4 kBackgroundColor = vec4(0.8f, 0.9f, 1.0f, 1.0f);
        const float kInShadowRadiance = 0.35f;

        struct Ray {
            vec3 origin;
            vec3 direction;
        };

        Ray generateCameraRay(uvec2 index, in vec3 cameraPosition, in mat4x4 projectionToWorld) {

            vec2 xy = index + 0.5;
            vec2 screenPos = xy / gl_LaunchSizeNV.xy * 2.0 - 1.0;

            screenPos.y = -screenPos.y;

            vec4 world = projectionToWorld * vec4(screenPos, 0.0, 1.0);
            world.xyz /= world.w;

            Ray ray;
            ray.origin = cameraPosition;
            ray.direction = normalize(world.xyz - ray.origin);

            return ray;
        }

        layout(set = 0, binding = 3, std430) readonly buffer FacesBuffer {
            uvec4 faces[];
        } FacesArray[];

        layout(set = 0, binding = 4, std430) readonly buffer NormalBuffer {
            vec4 normals[];
        } NormalArray[];

        layout(shaderRecordNV) buffer InlineData {
            PrimitiveConstantBuffer material;
        };

        vec2 texCoords(in vec3 position) {
            return position.xz;
        }

        void calculateRayDifferentials(out vec2 ddx_uv, out vec2 ddy_uv, in vec2 uv, in vec3 hitPosition, in vec3 surfaceNormal, in vec3 cameraPosition, in mat4x4 projectionToWorld) {
            Ray ddx = generateCameraRay(gl_LaunchIDNV.xy + uvec2(1, 0), cameraPosition, projectionToWorld);
            Ray ddy = generateCameraRay(gl_LaunchIDNV.xy + uvec2(0, 1), cameraPosition, projectionToWorld);

            vec3 ddx_pos = ddx.origin - ddx.direction * dot(ddx.origin - hitPosition, surfaceNormal) / dot(ddx.direction, surfaceNormal);
            vec3 ddy_pos = ddy.origin - ddy.direction * dot(ddy.origin - hitPosition, surfaceNormal) / dot(ddy.direction, surfaceNormal);

            ddx_uv = texCoords(ddx_pos) - uv;
            ddy_uv = texCoords(ddy_pos) - uv;
        }

        float checkersTextureBoxFilter(in vec2 uv, in vec2 dpdx, in vec2 dpdy, in uint ratio) {
            vec2 w = max(abs(dpdx), abs(dpdy));
            vec2 a = uv + 0.5 * w;
            vec2 b = uv - 0.5 * w;

            vec2 i = (floor(a) + min(fract(a) * ratio, 1.0) - floor(b) - min(fract(b) * ratio, 1.0)) / (ratio * w);
            return (1.0 - i.x) * (1.0 - i.y);
        }

        float analyticalCheckersTexture(in vec3 hitPosition, in vec3 surfaceNormal, in vec3 cameraPosition, in mat4x4 projectionToWorld) {
            vec2 ddx_uv;
            vec2 ddy_uv;
            vec2 uv = texCoords(hitPosition);

            calculateRayDifferentials(ddx_uv, ddy_uv, uv, hitPosition, surfaceNormal, cameraPosition, projectionToWorld);
            return checkersTextureBoxFilter(uv, ddx_uv, ddy_uv, 50);
        }

        vec3 hitWorldPosition() {
            return gl_WorldRayOriginNV + gl_RayTmaxNV * gl_WorldRayDirectionNV;
        }

        float calculateDiffuseCoefficient(in vec3 hitPosition, in vec3 incidentLightRay, in vec3 normal) {
            float fNDotL = clamp(dot(-incidentLightRay, normal), 0.0, 1.0);
            return fNDotL;
        }

        float calculateSpecularCoefficient(in vec3 hitPosition, in vec3 incidentLightRay, in vec3 normal, in float specularPower) {
            vec3 reflectedLightRay = normalize(reflect(incidentLightRay, normal));
            return pow(clamp(dot(reflectedLightRay, normalize(-gl_WorldRayDirectionNV)), 0.0, 1.0), specularPower);
        }

        vec4 calculatePhongLighting(in vec4 albedo, in vec3 normal, in bool isInShadow, in float diffuseCoeff, in float specularCoef, in float specularPower) {
            vec3 hitPosition = hitWorldPosition();
            vec3 lightPosition = params.lightPosition.xyz;
            float shadowFactor = isInShadow ? kInShadowRadiance : 1.0;
            vec3 incidentLightRay = normalize(hitPosition - lightPosition);

            vec4 lightDiffuseColor = params.lightDiffuseColor;
            float kd = calculateDiffuseCoefficient(hitPosition, incidentLightRay, normal);
            vec4 diffuseColor = shadowFactor * diffuseCoeff * kd * lightDiffuseColor * albedo;

            vec4 specularColor = vec4(0.0, 0.0, 0.0, 0.0);
            if (!isInShadow) {
                vec4 lightSpecularColor = vec4(1.0, 1.0, 1.0, 1.0);
                float ks = calculateSpecularCoefficient(hitPosition, incidentLightRay, normal, specularPower);
                specularColor = specularCoef * ks * lightSpecularColor;
            }

            vec4 ambientColor = params.lightAmbientColor;
            vec4 ambientColorMin = params.lightAmbientColor - 0.1;
            vec4 ambientColorMax = params.lightAmbientColor;
            float a = 1.0 - clamp(dot(normal, vec3(0.0, -1.0, 0.0)), 0.0, 1.0);
            ambientColor = albedo * mix(ambientColorMin, ambientColorMax, a);

            return ambientColor + diffuseColor + specularColor;
        }

        vec4 calculatePhongLighting(in vec4 albedo, in vec3 normal, in bool isInShadow) {
            return calculatePhongLighting(albedo, normal, isInShadow, 1.0, 1.0, 50);
        }

        vec4 calculatePhongLighting(in vec4 albedo, in vec3 normal, in bool isInShadow, in float diffuseCoeff) {
            return calculatePhongLighting(albedo, normal, isInShadow, diffuseCoeff, 1.0, 50);
        }

        vec4 calculatePhongLighting(in vec4 albedo, in vec3 normal, in bool isInShadow, in float diffuseCoeff, in float specularCoef) {
            return calculatePhongLighting(albedo, normal, isInShadow, diffuseCoeff, specularCoef, 50);
        }

        vec3 fresnelReflectanceSchlick(in vec3 I, in vec3 N, in vec3 f0) {
            float cosi = clamp(dot(-I, N), 0.0, 1.0);
            return f0 + (1.0 - f0) * pow(1.0 - cosi, 5.0);
        }

        vec4 traceRadianceRay(in Ray ray, in uint currentRayRecursionDepth) {

            if (currentRayRecursionDepth > 3) {
                return vec4(0.0, 0.0, 0.0, 0.0);
            }

            vec3 origin = ray.origin;
            vec3 direction = ray.direction;

            float tmin = 0;
            float tmax = 10000;

            rayPayload.color = vec4(0.0, 0.0, 0.0, 0.0);
            rayPayload.recursionDepth = currentRayRecursionDepth + 1;
            traceNV(topLevelAS, gl_RayFlagsCullBackFacingTrianglesNV, 0xff, 0, 0, 0, origin, tmin, direction, tmax, 0);

            return rayPayload.color;
        }

        bool traceShadowRayAndReportIfHit(in Ray ray, in uint currentRayRecursionDepth) {

            if (currentRayRecursionDepth > 3) {
                return false;
            }

            vec3 origin = ray.origin;
            vec3 direction = ray.direction;
            float tmin = 0;
            float tmax = 10000;

            isHit = true;

            traceNV(topLevelAS, gl_RayFlagsCullBackFacingTrianglesNV | gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsOpaqueNV | gl_RayFlagsSkipClosestHitShaderNV, 0xff, 0, 0, 1, origin, tmin, direction, tmax, 2);

            return isHit;
        }

        void main() {
            uint indexSizeInBytes = 2;
            uint indicesPerTriangle = 3;
            uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
            uint baseIndex = gl_PrimitiveID * triangleIndexStride;

            const uvec4 face = FacesArray[nonuniformEXT(gl_InstanceCustomIndexNV)].faces[gl_PrimitiveID];

            const vec3 triangleNormal = NormalArray[nonuniformEXT(gl_InstanceCustomIndexNV)].normals[int(face.x)].xyz;

            vec3 hitPosition = hitWorldPosition();

            Ray shadowRay = { hitPosition, normalize(params.lightPosition.xyz - hitPosition) };
            bool shadowRayHit = traceShadowRayAndReportIfHit(shadowRay, rayPayload.recursionDepth);

            float checkers = analyticalCheckersTexture(hitPosition, triangleNormal, params.cameraPosition.xyz, params.projectionToWorld);

            vec4 reflectedColor = vec4(0.0, 0.0, 0.0, 0.0);
            if (material.reflectanceCoef > 0.001) {
                Ray reflectionRay = { hitPosition, reflect(gl_WorldRayDirectionNV, triangleNormal) };
                vec4 reflectionColor = traceRadianceRay(reflectionRay, rayPayload.recursionDepth);

                vec3 fresnelR = fresnelReflectanceSchlick(gl_WorldRayDirectionNV, triangleNormal, material.albedo.xyz);
                reflectedColor = material.reflectanceCoef * vec4(fresnelR, 1.0) * reflectionColor;
            }

            vec4 phongColor = calculatePhongLighting(material.albedo, triangleNormal, shadowRayHit, material.diffuseCoef, material.specularCoef, material.specularPower);
            vec4 color = checkers * (phongColor + reflectedColor);

            float t = gl_RayTmaxNV;
            color = mix(color, kBackgroundColor, 1.0 - exp(-0.000002 * t * t * t));

            rayPayload.color = color;
        }
        )";

const char kClosestHitAABBShaderSource[] =
        R"(#version 460 core
        #extension GL_NV_ray_tracing : require

        layout(set = 0, binding = 0) uniform accelerationStructureNV topLevelAS;

        struct RayPayload {
            vec4 color;
            uint recursionDepth;
        };

        layout (location = 0) rayPayloadInNV RayPayload rayPayload;
        hitAttributeNV vec3 normal;
        layout(location = 2) rayPayloadNV bool isHit;

        struct SceneConstantBuffer {
            mat4x4 projectionToWorld;
            vec4 cameraPosition;
            vec4 lightPosition;
            vec4 lightAmbientColor;
            vec4 lightDiffuseColor;
            float reflectance;
            float elapsedTime;
        };

        struct PrimitiveConstantBuffer {
            vec4 albedo;
            float reflectanceCoef;
            float diffuseCoef;
            float specularCoef;
            float specularPower;
            float stepScale;

            float padding[3];
        };

        layout(set = 0, binding = 2, std140) uniform appData {
            SceneConstantBuffer params;
        };

        const vec4 kBackgroundColor = vec4(0.8f, 0.9f, 1.0f, 1.0f);
        const float kInShadowRadiance = 0.35f;

        struct Ray {
            vec3 origin;
            vec3 direction;
        };

        Ray generateCameraRay(uvec2 index, in vec3 cameraPosition, in mat4x4 projectionToWorld) {

            vec2 xy = index + 0.5;
            vec2 screenPos = xy / gl_LaunchSizeNV.xy * 2.0 - 1.0;

            screenPos.y = -screenPos.y;

            vec4 world = projectionToWorld * vec4(screenPos, 0.0, 1.0);
            world.xyz /= world.w;

            Ray ray;
            ray.origin = cameraPosition;
            ray.direction = normalize(world.xyz - ray.origin);

            return ray;
        }

        layout(shaderRecordNV) buffer InlineData {
            PrimitiveConstantBuffer material;
        };

        vec2 texCoords(in vec3 position) {
            return position.xz;
        }

        void calculateRayDifferentials(out vec2 ddx_uv, out vec2 ddy_uv, in vec2 uv, in vec3 hitPosition, in vec3 surfaceNormal, in vec3 cameraPosition, in mat4x4 projectionToWorld) {
            Ray ddx = generateCameraRay(gl_LaunchIDNV.xy + uvec2(1, 0), cameraPosition, projectionToWorld);
            Ray ddy = generateCameraRay(gl_LaunchIDNV.xy + uvec2(0, 1), cameraPosition, projectionToWorld);

            vec3 ddx_pos = ddx.origin - ddx.direction * dot(ddx.origin - hitPosition, surfaceNormal) / dot(ddx.direction, surfaceNormal);
            vec3 ddy_pos = ddy.origin - ddy.direction * dot(ddy.origin - hitPosition, surfaceNormal) / dot(ddy.direction, surfaceNormal);

            ddx_uv = texCoords(ddx_pos) - uv;
            ddy_uv = texCoords(ddy_pos) - uv;
        }

        float checkersTextureBoxFilter(in vec2 uv, in vec2 dpdx, in vec2 dpdy, in uint ratio) {
            vec2 w = max(abs(dpdx), abs(dpdy));
            vec2 a = uv + 0.5 * w;
            vec2 b = uv - 0.5 * w;

            vec2 i = (floor(a) + min(fract(a) * ratio, 1.0) - floor(b) - min(fract(b) * ratio, 1.0)) / (ratio * w);
            return (1.0 - i.x) * (1.0 - i.y);
        }

        float analyticalCheckersTexture(in vec3 hitPosition, in vec3 surfaceNormal, in vec3 cameraPosition, in mat4x4 projectionToWorld) {
            vec2 ddx_uv;
            vec2 ddy_uv;
            vec2 uv = texCoords(hitPosition);

            calculateRayDifferentials(ddx_uv, ddy_uv, uv, hitPosition, surfaceNormal, cameraPosition, projectionToWorld);
            return checkersTextureBoxFilter(uv, ddx_uv, ddy_uv, 50);
        }

        vec3 hitWorldPosition() {
            return gl_WorldRayOriginNV + gl_RayTmaxNV * gl_WorldRayDirectionNV;
        }

        float calculateDiffuseCoefficient(in vec3 hitPosition, in vec3 incidentLightRay, in vec3 normal) {
            float fNDotL = clamp(dot(-incidentLightRay, normal), 0.0, 1.0);
            return fNDotL;
        }

        float calculateSpecularCoefficient(in vec3 hitPosition, in vec3 incidentLightRay, in vec3 normal, in float specularPower) {
            vec3 reflectedLightRay = normalize(reflect(incidentLightRay, normal));
            return pow(clamp(dot(reflectedLightRay, normalize(-gl_WorldRayDirectionNV)), 0.0, 1.0), specularPower);
        }

        vec4 calculatePhongLighting(in vec4 albedo, in vec3 normal, in bool isInShadow, in float diffuseCoeff, in float specularCoef, in float specularPower) {
            vec3 hitPosition = hitWorldPosition();
            vec3 lightPosition = params.lightPosition.xyz;
            float shadowFactor = isInShadow ? kInShadowRadiance : 1.0;
            vec3 incidentLightRay = normalize(hitPosition - lightPosition);

            vec4 lightDiffuseColor = params.lightDiffuseColor;
            float kd = calculateDiffuseCoefficient(hitPosition, incidentLightRay, normal);
            vec4 diffuseColor = shadowFactor * diffuseCoeff * kd * lightDiffuseColor * albedo;

            vec4 specularColor = vec4(0.0, 0.0, 0.0, 0.0);
            if (!isInShadow) {
                vec4 lightSpecularColor = vec4(1.0, 1.0, 1.0, 1.0);
                float ks = calculateSpecularCoefficient(hitPosition, incidentLightRay, normal, specularPower);
                specularColor = specularCoef * ks * lightSpecularColor;
            }

            vec4 ambientColor = params.lightAmbientColor;
            vec4 ambientColorMin = params.lightAmbientColor - 0.1;
            vec4 ambientColorMax = params.lightAmbientColor;
            float a = 1.0 - clamp(dot(normal, vec3(0.0, -1.0, 0.0)), 0.0, 1.0);
            ambientColor = albedo * mix(ambientColorMin, ambientColorMax, a);

            return ambientColor + diffuseColor + specularColor;
        }

        vec4 calculatePhongLighting(in vec4 albedo, in vec3 normal, in bool isInShadow) {
            return calculatePhongLighting(albedo, normal, isInShadow, 1.0, 1.0, 50);
        }

        vec4 calculatePhongLighting(in vec4 albedo, in vec3 normal, in bool isInShadow, in float diffuseCoeff) {
            return calculatePhongLighting(albedo, normal, isInShadow, diffuseCoeff, 1.0, 50);
        }

        vec4 calculatePhongLighting(in vec4 albedo, in vec3 normal, in bool isInShadow, in float diffuseCoeff, in float specularCoef) {
            return calculatePhongLighting(albedo, normal, isInShadow, diffuseCoeff, specularCoef, 50);
        }

        vec3 fresnelReflectanceSchlick(in vec3 I, in vec3 N, in vec3 f0) {
            float cosi = clamp(dot(-I, N), 0.0, 1.0);
            return f0 + (1.0 - f0) * pow(1.0 - cosi, 5.0);
        }

        vec4 traceRadianceRay(in Ray ray, in uint currentRayRecursionDepth) {

            if (currentRayRecursionDepth > 3) {
                return vec4(0.0, 0.0, 0.0, 0.0);
            }

            vec3 origin = ray.origin;
            vec3 direction = ray.direction;

            float tmin = 0;
            float tmax = 10000;

            rayPayload.color = vec4(0.0, 0.0, 0.0, 0.0);
            rayPayload.recursionDepth = currentRayRecursionDepth + 1;
            traceNV(topLevelAS, gl_RayFlagsCullBackFacingTrianglesNV, 0xff, 0, 0, 0, origin, tmin, direction, tmax, 0);

            return rayPayload.color;
        }

        bool traceShadowRayAndReportIfHit(in Ray ray, in uint currentRayRecursionDepth) {

            if (currentRayRecursionDepth > 3) {
                return false;
            }

            vec3 origin = ray.origin;
            vec3 direction = ray.direction;
            float tmin = 0;
            float tmax = 10000;

            isHit = true;

            traceNV(topLevelAS, gl_RayFlagsCullBackFacingTrianglesNV | gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsOpaqueNV | gl_RayFlagsSkipClosestHitShaderNV, 0xff, 0, 0, 1, origin, tmin, direction, tmax, 2);

            return isHit;
        }

        void main() {
            vec3 hitPosition = hitWorldPosition();

            Ray shadowRay = { hitPosition, normalize(params.lightPosition.xyz - hitPosition) };
            bool shadowRayHit = traceShadowRayAndReportIfHit(shadowRay, rayPayload.recursionDepth);

            vec4 reflectedColor = vec4(0.0, 0.0, 0.0, 0.0);
            if (material.reflectanceCoef > 0.001) {
                Ray reflectionRay = { hitPosition, reflect(gl_WorldRayDirectionNV, normal) };
                vec4 reflectionColor = traceRadianceRay(reflectionRay, rayPayload.recursionDepth);

                vec3 fresnelR = fresnelReflectanceSchlick(gl_WorldRayDirectionNV, normal, material.albedo.xyz);
                reflectedColor = material.reflectanceCoef * vec4(fresnelR, 1.0) * reflectionColor;
            }

            vec4 phongColor = calculatePhongLighting(material.albedo, normal, shadowRayHit, material.diffuseCoef, material.specularCoef, material.specularPower);
            vec4 color = phongColor + reflectedColor;

            float t = gl_RayTmaxNV;
            color = mix(color, kBackgroundColor, 1.0 - exp(-0.000002 * t * t * t));

            rayPayload.color = color;
        }
        )";

const char kMissShaderSource[] = R"(#version 460 core
#extension GL_NV_ray_tracing : require

struct RayPayload {
    vec4 color;
    uint recursionDepth;
};

layout(location = 0) rayPayloadInNV RayPayload rayPayload;

const vec4 kBackgroundColor = vec4(0.8f, 0.9f, 1.0f, 1.0f);

void main() {
    rayPayload.color = kBackgroundColor;
}
)";

const char kMissShadowRayShaderSource[] = R"(#version 460 core
#extension GL_NV_ray_tracing : require
layout(location = 2) rayPayloadInNV bool isHit;

void main() {
    isHit = false;
}
)";

const char kIntersectionAnalyticShaderSource[] = R"(#version 460 core
#extension GL_NV_ray_tracing : require

hitAttributeNV vec3 hitNormal;

struct PrimitiveInstancePerFrameBuffer {
    mat4x4 localSpaceToBottomLevelAS;
    mat4x4 bottomLevelASToLocalSpace;
};

struct PrimitiveConstantBuffer {
    vec4 albedo;
    float reflectanceCoef;
    float diffuseCoef;
    float specularCoef;
    float specularPower;
    float stepScale;

    float padding[3];
};

struct PrimitiveInstanceConstantBuffer
{
    uint instanceIndex;
    uint primitiveType; // Procedural primitive type

    float padding[2];
};

struct ProceduralPrimitiveAttributes
{
    vec3 normal;
};

layout(set = 0, binding = 5, std140) uniform instanceData {
    PrimitiveInstancePerFrameBuffer aabbPrimitiveAttribs[10];
};

layout(shaderRecordNV) buffer inlineData {
    PrimitiveConstantBuffer materialCB;
    PrimitiveInstanceConstantBuffer aabbCB;
};

struct Ray {
    vec3 origin;
    vec3 direction;
};

float calculateAnimationInterpolant(in float elapsedTime, in float cycleDuration) {
    float curLinearCycleTime = mod(elapsedTime, cycleDuration) / cycleDuration;
    curLinearCycleTime = (curLinearCycleTime <= 0.5) ? 2.0 * curLinearCycleTime : 1.0 - 2.0 * (curLinearCycleTime - 0.5);
    return smoothstep(0.0, 1.0, curLinearCycleTime);
}

bool isInRange(in float val, in float minVal, in float maxVal) {
    return (val >= minVal && val <= maxVal);
}

bool isCulled(in Ray ray, in vec3 hitSurfaceNormal) {
    float rayDirectionNormalDot = dot(ray.direction, hitSurfaceNormal);

    bool isCulled = (((gl_IncomingRayFlagsNV & gl_RayFlagsCullBackFacingTrianglesNV) != 0) && (rayDirectionNormalDot > 0))
                    ||
                    (((gl_IncomingRayFlagsNV & gl_RayFlagsCullFrontFacingTrianglesNV) != 0) && (rayDirectionNormalDot < 0));

    return isCulled;
}

bool isAValidHit(in Ray ray, in float thit, in vec3 hitSurfaceNormal) {
    return isInRange(thit, gl_RayTminNV, gl_RayTmaxNV) && !isCulled(ray, hitSurfaceNormal);
}

void swap(inout float val0, inout float val1) {
    float tmp = val0;
    val0 = val1;
    val1 = tmp;
}

vec3 hitWorldPosition() {
    return gl_WorldRayOriginNV + gl_RayTmaxNV * gl_WorldRayDirectionNV;
}

vec3 hitAttribute(vec3 vertexAttribute[3], vec3 barycentrics) {
    return vertexAttribute[0] +
        barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

Ray getRayInAABBPrimitiveLocalSpace() {
    PrimitiveInstancePerFrameBuffer attr = aabbPrimitiveAttribs[aabbCB.instanceIndex];

    Ray ray;
    ray.origin = (attr.bottomLevelASToLocalSpace * vec4(gl_ObjectRayOriginNV, 1.0)).xyz;
    ray.direction = (mat3x3(attr.bottomLevelASToLocalSpace) * gl_ObjectRayDirectionNV);
    return ray;
}

bool solveQuadraticEqn(float a, float b, float c, out float x0, out float x1) {
    float discr = b * b - 4.0 * a * c;
    if (discr < 0) return false;
    else if (discr == 0.0) x0 = x1 = -0.5 * b / a;
    else {
        float q = (b > 0.0) ?
            -0.5 * (b + sqrt(discr)) :
            -0.5 * (b - sqrt(discr));
        x0 = q / a;
        x1 = c / q;
    }

    if (x0 > x1) swap(x0, x1);

    return true;
}

vec3 calculateNormalForARaySphereHit(in Ray ray, in float thit, vec3 center) {
    vec3 hitPosition = ray.origin + thit * ray.direction;
    return normalize(hitPosition - center);
}

bool solveRaySphereIntersectionEquation(in Ray ray, out float tmin, out float tmax, in vec3 center, in float radius) {
    vec3 L = ray.origin - center;
    float a = dot(ray.direction, ray.direction);
    float b = 2.0 * dot(ray.direction, L);
    float c = dot(L, L) - radius * radius;
    return solveQuadraticEqn(a, b, c, tmin, tmax);
}

bool raySphereIntersectionTest(in Ray ray, out float thit, out float tmax, out ProceduralPrimitiveAttributes attr, in vec3 center, in float radius) {
    float t0, t1;

    if (!solveRaySphereIntersectionEquation(ray, t0, t1, center, radius)) return false;

    tmax = t1;

    if (t0 < gl_RayTminNV) {
        if (t1 < gl_RayTminNV) return false;

        attr.normal = calculateNormalForARaySphereHit(ray, t1, center);
        if (isAValidHit(ray, t1, attr.normal)) {
            thit = t1;
            return true;
        }
    }
    else {
        attr.normal = calculateNormalForARaySphereHit(ray, t0, center);
        if (isAValidHit(ray, t0, attr.normal)) {
            thit = t0;
            return true;
        }

        attr.normal = calculateNormalForARaySphereHit(ray, t1, center);
        if (isAValidHit(ray, t1, attr.normal)) {
            thit = t1;
            return true;
        }
    }

    return false;
}

bool raySpheresIntersectionTest(in Ray ray, out float thit, out ProceduralPrimitiveAttributes attr) {
    const int N = 3;
    vec3 centers[N] = {
        vec3(-0.3, -0.3, -0.3),
        vec3(0.1, 0.1, 0.4),
        vec3(0.35, 0.35, 0.0)
    };

    float radii[N] = { 0.6, 0.3, 0.15 };
    bool hitFound = false;

    thit = gl_RayTmaxNV;

    for (int i = 0; i < N; i++) {
        float _thit;
        float _tmax;

        ProceduralPrimitiveAttributes _attr;

        if (raySphereIntersectionTest(ray, _thit, _tmax, _attr, centers[i], radii[i])) {
            if (_thit < thit) {
                thit = _thit;
                attr = _attr;
                hitFound = true;
            }
        }
    }

    return hitFound;
}

bool rayAABBIntersectionTest(Ray ray, vec3 aabb[2], out float tmin, out float tmax) {

    vec3 tmin3, tmax3;
    ivec3 sign3 = ivec3(ray.direction.x > 0 ? 1 : 0, ray.direction.y > 0 ? 1 : 0, ray.direction.z > 0 ? 1 : 0);
    tmin3.x = (aabb[1 - sign3.x].x - ray.origin.x) / ray.direction.x;
    tmax3.x = (aabb[sign3.x].x - ray.origin.x) / ray.direction.x;

    tmin3.y = (aabb[1 - sign3.y].y - ray.origin.y) / ray.direction.y;
    tmax3.y = (aabb[sign3.y].y - ray.origin.y) / ray.direction.y;

    tmin3.z = (aabb[1 - sign3.z].z - ray.origin.z) / ray.direction.z;
    tmax3.z = (aabb[sign3.z].z - ray.origin.z) / ray.direction.z;

    tmin = max(max(tmin3.x, tmin3.y), tmin3.z);
    tmax = min(min(tmax3.x, tmax3.y), tmax3.z);

    return tmax > tmin && tmax >= gl_RayTminNV && tmin <= gl_RayTmaxNV;
}

bool rayAABBIntersectionTest(Ray ray, vec3 aabb[2], out float thit, out ProceduralPrimitiveAttributes attr) {

    float tmin, tmax;
    if (rayAABBIntersectionTest(ray, aabb, tmin, tmax)) {
        thit = tmin >= gl_RayTminNV ? tmin : tmax;

        vec3 hitPosition = ray.origin + thit * ray.direction;
        vec3 distanceToBounds[2] = {
            abs(aabb[0] - hitPosition),
            abs(aabb[1] - hitPosition)
        };

        const float eps = 0.0001;
        if (distanceToBounds[0].x < eps) attr.normal = vec3(-1.0, 0.0, 0.0);
        else if (distanceToBounds[0].y < eps) attr.normal = vec3(0.0, -1.0, 0.0);
        else if (distanceToBounds[0].z < eps) attr.normal = vec3(0.0, 0.0, -1.0);
        else if (distanceToBounds[1].x < eps) attr.normal = vec3(1.0, 0.0, 0.0);
        else if (distanceToBounds[1].y < eps) attr.normal = vec3(0.0, 1.0, 0.0);
        else if (distanceToBounds[1].z < eps) attr.normal = vec3(0.0, 0.0, 1.0);

        return isAValidHit(ray, thit, attr.normal);
    }

    return false;
}

bool rayAnalyticGeometryIntersectionTest(in Ray ray, in uint analyticPrimitiveType, out float thit, out ProceduralPrimitiveAttributes attr) {
    vec3 aabb[2] = {
        vec3(-1.0, -1.0, -1.0),
        vec3(1.0, 1.0, 1.0)
    };

    switch (analyticPrimitiveType) {
        case 0: return rayAABBIntersectionTest(ray, aabb, thit, attr);
        case 1: return raySpheresIntersectionTest(ray, thit, attr);
        default: return false;
    }

    return false;
}

void main() {

    Ray localRay = getRayInAABBPrimitiveLocalSpace();
    uint primitiveType = aabbCB.primitiveType;

    float thit;
    ProceduralPrimitiveAttributes attr;

    if (rayAnalyticGeometryIntersectionTest(localRay, primitiveType, thit, attr)) {
        PrimitiveInstancePerFrameBuffer aabbAttribute = aabbPrimitiveAttribs[aabbCB.instanceIndex];
        attr.normal = mat3x3(aabbAttribute.localSpaceToBottomLevelAS) * attr.normal;
        attr.normal = normalize( attr.normal * mat3x3(gl_ObjectToWorldNV) );

        hitNormal = attr.normal;

        reportIntersectionNV(thit, 0);
    }
}

)";

const char kIntersectionVolumetricShaderSource[] = R"(#version 460 core
#extension GL_NV_ray_tracing : require

hitAttributeNV vec3 hitNormal;

struct SceneConstantBuffer {
   mat4x4 projectionToWorld;
   vec4 cameraPosition;
   vec4 lightPosition;
   vec4 lightAmbientColor;
   vec4 lightDiffuseColor;
   float reflectance;
   float elapsedTime;
};

struct PrimitiveInstancePerFrameBuffer {
   mat4x4 localSpaceToBottomLevelAS;
   mat4x4 bottomLevelASToLocalSpace;
};

struct PrimitiveConstantBuffer {
   vec4 albedo;
   float reflectanceCoef;
   float diffuseCoef;
   float specularCoef;
   float specularPower;
   float stepScale;

   float padding[3];
};

struct PrimitiveInstanceConstantBuffer
{
   uint instanceIndex;
   uint primitiveType; // Procedural primitive type

   float padding[2];
};

struct ProceduralPrimitiveAttributes
{
   vec3 normal;
};

layout(set = 0, binding = 2, std140) uniform appData {
   SceneConstantBuffer params;
};

layout(set = 0, binding = 5, std140) uniform instanceData {
   PrimitiveInstancePerFrameBuffer aabbPrimitiveAttribs[10];
};

layout(shaderRecordNV) buffer inlineData {
    PrimitiveConstantBuffer materialCB;
    PrimitiveInstanceConstantBuffer aabbCB;
};

struct Ray {
    vec3 origin;
    vec3 direction;
};

struct Metaball {
    vec3 center;
    float radius;
};

float calculateAnimationInterpolant(in float elapsedTime, in float cycleDuration) {
    float curLinearCycleTime = mod(elapsedTime, cycleDuration) / cycleDuration;
    curLinearCycleTime = (curLinearCycleTime <= 0.5) ? 2.0 * curLinearCycleTime : 1.0 - 2.0 * (curLinearCycleTime - 0.5);
    return smoothstep(0.0, 1.0, curLinearCycleTime);
}

bool isInRange(in float val, in float minVal, in float maxVal) {
    return (val >= minVal && val <= maxVal);
}

bool isCulled(in Ray ray, in vec3 hitSurfaceNormal) {
   float rayDirectionNormalDot = dot(ray.direction, hitSurfaceNormal);

   bool isCulled = (((gl_IncomingRayFlagsNV & gl_RayFlagsCullBackFacingTrianglesNV) != 0) && (rayDirectionNormalDot > 0))
                   ||
                   (((gl_IncomingRayFlagsNV & gl_RayFlagsCullFrontFacingTrianglesNV) != 0) && (rayDirectionNormalDot < 0));

   return isCulled;
}

bool isAValidHit(in Ray ray, in float thit, in vec3 hitSurfaceNormal) {
   return isInRange(thit, gl_RayTminNV, gl_RayTmaxNV) && !isCulled(ray, hitSurfaceNormal);
}

void swap(inout float val0, inout float val1) {
    float tmp = val0;
    val0 = val1;
    val1 = tmp;
}

vec3 hitWorldPosition() {
    return gl_WorldRayOriginNV + gl_RayTmaxNV * gl_WorldRayDirectionNV;
}

Ray getRayInAABBPrimitiveLocalSpace() {
    PrimitiveInstancePerFrameBuffer attr = aabbPrimitiveAttribs[aabbCB.instanceIndex];

    Ray ray;
    ray.origin = (attr.bottomLevelASToLocalSpace * vec4(gl_ObjectRayOriginNV, 1.0)).xyz;
    ray.direction = (mat3x3(attr.bottomLevelASToLocalSpace) * gl_ObjectRayDirectionNV);
    return ray;
}

bool solveQuadraticEqn(float a, float b, float c, out float x0, out float x1) {
    float discr = b * b - 4.0 * a * c;
    if (discr < 0) return false;
    else if (discr == 0) x0 = x1 = -0.5 * b / a;
    else {
        float q = (b > 0.0) ?
            -0.5 * (b + sqrt(discr)) :
            -0.5 * (b - sqrt(discr));
        x0 = q / a;
        x1 = c / q;
    }

    if (x0 > x1) swap(x0, x1);

    return true;
}

bool solveRaySphereIntersectionEquation(in Ray ray, out float tmin, out float tmax, in vec3 center, in float radius) {
    vec3 L = ray.origin - center;
    float a = dot(ray.direction, ray.direction);
    float b = 2.0 * dot(ray.direction, L);
    float c = dot(L, L) - radius * radius;
    return solveQuadraticEqn(a, b, c, tmin, tmax);
}

bool raySolidSphereIntersectionTest(in Ray ray, out float thit, out float tmax, in vec3 center, in float radius) {
    float t0, t1;

    if (!solveRaySphereIntersectionEquation(ray, t0, t1, center, radius)) {
        return false;
    }

    thit = max(t0, gl_RayTminNV);
    tmax = min(t1, gl_RayTmaxNV);

    return true;
}

float calculateMetaballPotential(in vec3 position, in Metaball blob) {
    float dist = length(position - blob.center);

    if (dist <= blob.radius) {
        float d = dist;

        d = blob.radius - d;

        float r = blob.radius;

        return 6.0 * (d * d * d * d * d) / (r * r * r * r * r)
               - 15.0 * (d * d * d + d) / (r * r * r * r)
               + 10.0 * (d * d * d) / (r * r * r);
    }

    return 0.0;
}

float calculateMetaballsPotential(in vec3 position, in Metaball blobs[3], in uint activeMetaballs) {
    float sumFieldPotential = 0.0;

    for (uint j = 0; j < 3; j++) {
        sumFieldPotential += calculateMetaballPotential(position, blobs[j]);
    }

    return sumFieldPotential;
}

vec3 calculateMetaballsNormal(in vec3 position, in Metaball blobs[3], in uint activeMetaballs) {
    float e = 0.5773 * 0.00001;
    return normalize(vec3(
        calculateMetaballsPotential(position + vec3(-e, 0.0, 0.0), blobs, activeMetaballs) -
        calculateMetaballsPotential(position + vec3(e, 0.0, 0.0), blobs, activeMetaballs),
        calculateMetaballsPotential(position + vec3(0.0, -e, 0.0), blobs, activeMetaballs) -
        calculateMetaballsPotential(position + vec3(0.0, e, 0.0), blobs, activeMetaballs),
        calculateMetaballsPotential(position + vec3(0.0, 0.0, -e), blobs, activeMetaballs) -
        calculateMetaballsPotential(position + vec3(0.0, 0.0, e), blobs, activeMetaballs)));
}

void initializeAnimatedMetaballs(out Metaball blobs[3], in float elapsedTime, in float cycleDuration) {
    vec3 keyFrameCenters[3][2] = {
        { vec3(-0.3, -0.3, -0.4), vec3(0.3, -0.3, 0.0) },
        { vec3(0.0, -0.2, 0.5), vec3(0.0, 0.4, 0.5) },
        { vec3(0.4, 0.4, 0.4), vec3(-0.4, 0.2, -0.4) }
    };

    float radii[3] = { 0.45, 0.55, 0.45 };

    float tAnimate = calculateAnimationInterpolant(elapsedTime, cycleDuration);

    for (uint j = 0; j < 3; j++) {
        blobs[j].center = mix(keyFrameCenters[j][0], keyFrameCenters[j][1], tAnimate);
        blobs[j].radius = 3.8 * radii[j];
    }
}

void findIntersectingMetaballs(in Ray ray, out float tmin, out float tmax, inout Metaball blobs[3], out uint activeMetaballs) {
    tmin = (1.0 / 0.0);
    tmax = -(1.0 / 0.0);

    activeMetaballs = 0;

    for (uint i = 0; i < 3; i++) {
        float _thit, _tmax;

        if (raySolidSphereIntersectionTest(ray, _thit, _tmax, blobs[i].center, blobs[i].radius)) {
            tmin = min(_thit, tmin);
            tmax = max(_tmax, tmax);

            activeMetaballs = 3;
        }
    }

    tmin = max(tmin, gl_RayTminNV);
    tmax = min(tmax, gl_RayTmaxNV);
}

bool rayMetaballsIntersectionTest(in Ray ray, out float thit, out ProceduralPrimitiveAttributes attr, in float elapsedTime) {
    Metaball blobs[3];
    initializeAnimatedMetaballs(blobs, elapsedTime, 12.0);

    float tmin, tmax;
    uint activeMetaballs = 0;
    findIntersectingMetaballs(ray, tmin, tmax, blobs, activeMetaballs);

    uint maxSteps = 128;
    float t = tmin;
    float minTStep = (tmax - tmin) / maxSteps;
    uint iStep = 0;

    while (iStep++ < maxSteps) {
        vec3 position = ray.origin + t * ray.direction;
        float sumFieldPotential = 0;

        for (uint j = 0; j < 3; j++) {
            sumFieldPotential += calculateMetaballPotential(position, blobs[j]);
        }

        if (sumFieldPotential >= 0.25) {
            vec3 normal = calculateMetaballsNormal(position, blobs, activeMetaballs);
            if (isAValidHit(ray, t, normal)) {
                thit = t;
                attr.normal = normal;
                return true;
            }
        }

        t += minTStep;
    }

    return false;
}

bool rayVolumetricGeometryIntersectionTest(in Ray ray, in uint volumetricPrimitive, out float thit, out ProceduralPrimitiveAttributes attr, in float elapsedTime) {
    switch (volumetricPrimitive) {
        case 0: return rayMetaballsIntersectionTest(ray, thit, attr, elapsedTime);
        default: return false;
    }
}

void main() {

    Ray localRay = getRayInAABBPrimitiveLocalSpace();
    uint primitiveType = aabbCB.primitiveType;

    float thit;
    ProceduralPrimitiveAttributes attr;

    if (rayVolumetricGeometryIntersectionTest(localRay, primitiveType, thit, attr, params.elapsedTime)) {
        PrimitiveInstancePerFrameBuffer aabbAttribute = aabbPrimitiveAttribs[aabbCB.instanceIndex];
        attr.normal = mat3x3(aabbAttribute.localSpaceToBottomLevelAS) * attr.normal;
        attr.normal = normalize(attr.normal * mat3x3(gl_ObjectToWorldNV));

        hitNormal = attr.normal;
        reportIntersectionNV(thit, 0);
    }

   // else {
    //if (primitiveType == 0) {
    //    hitNormal = vec3(0.0,0.0,0.0);//attr.normal;
    //    reportIntersectionNV(0.0,0);
    //}
    //}
}

)";

const char kIntersectionSignedDistanceShaderSource[] = R"(#version 460 core
#extension GL_NV_ray_tracing : require

hitAttributeNV vec3 hitNormal;

struct PrimitiveInstancePerFrameBuffer {
  mat4x4 localSpaceToBottomLevelAS;
  mat4x4 bottomLevelASToLocalSpace;
};

struct PrimitiveConstantBuffer {
  vec4 albedo;
  float reflectanceCoef;
  float diffuseCoef;
  float specularCoef;
  float specularPower;
  float stepScale;

  float padding[3];
};

struct PrimitiveInstanceConstantBuffer
{
  uint instanceIndex;
  uint primitiveType; // Procedural primitive type

  float padding[2];
};

struct ProceduralPrimitiveAttributes
{
    vec3 normal;
};

layout(set = 0, binding = 5, std140) uniform instanceData {
  PrimitiveInstancePerFrameBuffer aabbPrimitiveAttribs[10];
};

layout(shaderRecordNV) buffer inlineData {
   PrimitiveConstantBuffer materialCB;
   PrimitiveInstanceConstantBuffer aabbCB;
};

struct Ray {
    vec3 origin;
    vec3 direction;
};

float calculateAnimationInterpolant(in float elapsedTime, in float cycleDuration) {
    float curLinearCycleTime = mod(elapsedTime, cycleDuration) / cycleDuration;
    curLinearCycleTime = (curLinearCycleTime <= 0.5) ? 2.0 * curLinearCycleTime : 1.0 - 2.0 * (curLinearCycleTime - 0.5);
    return smoothstep(0.0, 1.0, curLinearCycleTime);
}

bool isInRange(in float val, in float minVal, in float maxVal) {
   return (val >= minVal && val <= maxVal);
}

bool isCulled(in Ray ray, in vec3 hitSurfaceNormal) {
   float rayDirectionNormalDot = dot(ray.direction, hitSurfaceNormal);

   bool isCulled = (((gl_IncomingRayFlagsNV & gl_RayFlagsCullBackFacingTrianglesNV) != 0) && (rayDirectionNormalDot > 0))
                   ||
                   (((gl_IncomingRayFlagsNV & gl_RayFlagsCullFrontFacingTrianglesNV) != 0) && (rayDirectionNormalDot < 0));

   return isCulled;
}

bool isAValidHit(in Ray ray, in float thit, in vec3 hitSurfaceNormal) {
   return isInRange(thit, gl_RayTminNV, gl_RayTmaxNV) && !isCulled(ray, hitSurfaceNormal);
}

void swap(inout float val0, inout float val1) {
    float tmp = val0;
    val0 = val1;
    val1 = tmp;
}

vec3 hitWorldPosition() {
    return gl_WorldRayOriginNV + gl_RayTmaxNV * gl_WorldRayDirectionNV;
}

vec3 hitAttribute(vec3 vertexAttribute[3], vec3 barycentrics) {
    return vertexAttribute[0] +
        barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

Ray generateCameraRay(uvec2 index, in vec3 cameraPosition, in mat4x4 projectionToWorld) {
    vec2 xy = index + 0.5;
    vec2 screenPos = xy / gl_LaunchIDNV.xy * 2.0 - 1.0;

    screenPos.y = -screenPos.y;

    vec4 world = projectionToWorld * vec4(screenPos, 0.0, 1.0);

    Ray ray;
    ray.origin = vec3(cameraPosition);
    ray.direction = normalize(world.xyz - ray.origin);
    return ray;
}

Ray getRayInAABBPrimitiveLocalSpace() {
   PrimitiveInstancePerFrameBuffer attr = aabbPrimitiveAttribs[aabbCB.instanceIndex];

   Ray ray;
   ray.origin = (attr.bottomLevelASToLocalSpace * vec4(gl_ObjectRayOriginNV, 1.0)).xyz;
   ray.direction = (mat3x3(attr.bottomLevelASToLocalSpace) * gl_ObjectRayDirectionNV);
   return ray;
}

float opS(float d1, float d2) {
    return max(d1, -d2);
}

float opI(float d1, float d2) {
    return max(d1, d2);
}

vec3 fmod(vec3 x, vec3 y) {
    return x - y * trunc(x / y);
}

vec3 opRep(vec3 p, vec3 c) {
    return fmod(p, c) - 0.5 * c;
}

vec3 opTwist(vec3 p) {
    float c = cos(3.0 * p.y);
    float s = sin(3.0 * p.y);
    mat2x2 m = mat2x2(c, -s, s, c);
    return vec3(p.xz * m, p.y);
}

float sdSphere(vec3 p, float s) {
    return length(p) - s;
}

float sdBox(vec3 p, vec3 b) {
    vec3 d = abs(p) - b;
    return min(max(d.x, max(d.y, d.z)), 0.0) + length(max(d, 0.0));
}

float udRoundBox(vec3 p, vec3 b, float r) {
    return length(max(abs(p) - b, 0.0)) - r;
}

float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

float length_toPow2(vec3 p) {
    return dot(p, p);
}

float length_toPowNegative8(vec2 p) {
    p = p * p; p = p * p; p = p * p;
    return pow(p.x + p.y, 1.0 / 8.0);
}

float sdCylinder(vec3 p, vec2 h) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - h;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdOctahedron(vec3 p, vec3 h) {
    float d = 0.0;

    d = dot(vec2(max(abs(p.x), abs(p.z)), abs(p.y)),
            vec2(h.x, h.y));

    return d - h.y * h.z;
}

float sdPyramid(vec3 p, vec3 h) {
    float octa = sdOctahedron(p, h);

    return opS(octa, p.y);
}

float sdTorus82(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length_toPowNegative8(q) - t.y;
}

float sdFractalPyramid(in vec3 position, vec3 h, in float scale) {
    float a = h.z * h.y / h.x;
    vec3 v1 = vec3(0.0, h.z, 0.0);
    vec3 v2 = vec3(-a, 0.0, a);
    vec3 v3 = vec3(a, 0.0, -a);
    vec3 v4 = vec3(a, 0.0, a);
    vec3 v5 = vec3(-a, 0.0, -a);

    int n = 0;
    for (n = 0; n < 4; n++) {
        float dist, d;
        vec3 v;
        v = v1; dist = length_toPow2(position - v1);
        d = length_toPow2(position - v2); if (d < dist) { v = v2; dist = d; }
        d = length_toPow2(position - v3); if (d < dist) { v = v3; dist = d; }
        d = length_toPow2(position - v4); if (d < dist) { v = v4; dist = d; }
        d = length_toPow2(position - v5); if (d < dist) { v = v5; dist = d; }

        position = scale * position - v * (scale - 1.0);
    }

    float distance = sdPyramid(position, h);

    return distance * pow(scale, float(-n));
}

float sdFractalPyramid(in vec3 position, vec3 h) {
    return sdFractalPyramid(position, h, 2.0);
}

float getDistanceFromSignedDistancePrimitive(in vec3 position, in uint signedDistancePrimitive) {
    switch(signedDistancePrimitive) {
        case 0: return opI(sdSphere(opRep(position + 1.0, vec3(2.0 / 4.0)), 0.65 / 4.0), sdBox(position, vec3(1.0)));
        case 1: return opS(opS(udRoundBox(position, vec3(0.75), 0.2), sdSphere(position, 1.20)), -sdSphere(position, 1.32));
        case 2: return sdTorus82(position, vec2(0.75, 0.15));
        case 3: return sdTorus(opTwist(position), vec2(0.6, 0.2));
        case 4: return opS( sdTorus82(position, vec2(0.60, 0.3)),
                            sdCylinder(opRep(vec3(atan(position.x, position.z) / 6.2831,
                                                    1.0,
                                                    0.015 + 0.25 * length(position)) + 1.0,
                                             vec3(0.05, 1.0, 0.075)),
                                       vec2(0.02, 0.8)));
        case 5: return opI(sdCylinder(opRep(position + vec3(1.0, 1.0, 1.0), vec3(1.0, 2.0, 1.0)), vec2(0.3, 2.0)),
                           sdBox(position + vec3(1.0, 1.0, 1.0), vec3(2.0, 2.0, 2.0)));
        case 6: return sdFractalPyramid(position + vec3(0.0, 1.0, 0.0), vec3(0.894, 0.447, 2.0), 2.0);
        default: return 0.0;
    }
}

vec3 sdCalculateNormal(in vec3 pos, in uint sdPrimitive) {

    vec2 e = vec2(1.0, -1.0) * 0.5773 * 0.0001;
    return normalize(
        e.xyy * getDistanceFromSignedDistancePrimitive(pos + e.xyy, sdPrimitive) +
        e.yyx * getDistanceFromSignedDistancePrimitive(pos + e.yyx, sdPrimitive) +
        e.yxy * getDistanceFromSignedDistancePrimitive(pos + e.yxy, sdPrimitive) +
        e.xxx * getDistanceFromSignedDistancePrimitive(pos + e.xxx, sdPrimitive));
}

bool raySignedDistancePrimitiveTest(in Ray ray, uint sdPrimitive, out float thit, out ProceduralPrimitiveAttributes attr, in float stepScale) {

    const float threshold = 0.0001;
    float t = gl_RayTminNV;
    const uint maxSteps = 512;

    uint i = 0;

    while (i++ < maxSteps && t <= gl_RayTmaxNV) {
        vec3 position = ray.origin + t * ray.direction;
        float distance = getDistanceFromSignedDistancePrimitive(position, sdPrimitive);

        if (distance <= threshold * t) {
            vec3 hitSurfaceNormal = sdCalculateNormal(position, sdPrimitive);

            if (isAValidHit(ray, t, hitSurfaceNormal)) {
                thit = t;
                attr.normal = hitSurfaceNormal;
                return true;
            }
        }

        t += stepScale * distance;
    }

    return false;
}

void main() {

    Ray localRay = getRayInAABBPrimitiveLocalSpace();
    uint primitiveType = aabbCB.primitiveType;

    float thit;
    ProceduralPrimitiveAttributes attr;

    if (raySignedDistancePrimitiveTest(localRay, primitiveType, thit, attr, materialCB.stepScale)) {

        PrimitiveInstancePerFrameBuffer aabbAttribute = aabbPrimitiveAttribs[aabbCB.instanceIndex];
        attr.normal = mat3x3(aabbAttribute.localSpaceToBottomLevelAS) * attr.normal;
        attr.normal = normalize(attr.normal * mat3x3(gl_ObjectToWorldNV));

        hitNormal = attr.normal;

        reportIntersectionNV(thit, 0);
    }
}

)";

CRayTracing::CRayTracing(VkDevice device, VkPhysicalDevice gpu, VkQueue queue, VkCommandPool commandPool, VkPhysicalDeviceRayTracingPropertiesNV const& raytracingProperties)
    : m_device(device)
    , m_gpu(gpu)
    , m_queue(queue)
    , m_commandPool(commandPool)
    , m_helper(CVulkanHelper(device, gpu))
    , m_raytracingProperties(raytracingProperties)
{
}

void CRayTracing::init() {
    ACTIVATE_RAYTRACING(vkCreateAccelerationStructure);
    ACTIVATE_RAYTRACING(vkDestroyAccelerationStructure);
    ACTIVATE_RAYTRACING(vkGetAccelerationStructureMemoryRequirements);
    ACTIVATE_RAYTRACING(vkBindAccelerationStructureMemory);
    ACTIVATE_RAYTRACING(vkCmdBuildAccelerationStructure);
    ACTIVATE_RAYTRACING(vkCmdCopyAccelerationStructure);
    ACTIVATE_RAYTRACING(vkCmdTraceRays);
    ACTIVATE_RAYTRACING(vkCreateRayTracingPipelines);
    ACTIVATE_RAYTRACING(vkGetRayTracingShaderGroupHandles);
    ACTIVATE_RAYTRACING(vkGetAccelerationStructureHandle);
    ACTIVATE_RAYTRACING(vkCmdWriteAccelerationStructuresProperties);
    ACTIVATE_RAYTRACING(vkCompileDeferred);

    vkGetPhysicalDeviceMemoryProperties(m_gpu, &m_gpuMemProps);
}

void CRayTracing::initScene() {

    // Setup materials.
    {
        auto setAttributes = [&] (
            uint32_t primitiveIndex,
            glm::vec4 const& albedo,
            float reflectanceCoef = 0.0f,
            float diffuseCoef = 0.9f,
            float specularCoef = 0.7f,
            float specularPower = 50.0f,
            float stepScale = 1.0f)
        {
            PrimitiveConstantBuffer& attributes = m_aabbMaterialCB[primitiveIndex];
            attributes.albedo = albedo;
            attributes.reflectanceCoef = reflectanceCoef;
            attributes.diffuseCoef = diffuseCoef;
            attributes.specularCoef = specularCoef;
            attributes.specularPower = specularPower;
            attributes.stepScale = stepScale;
        };

        m_planeMaterialCB = { glm::vec4(0.9f, 0.9f, 0.9f, 1.0f), 0.25f, 1.0f, 0.4f, 50.0f, 1.0f, /*padding*/ glm::vec3(0.0f) };

        glm::vec4 green = glm::vec4(0.1f, 1.0f, 0.5f, 1.0f);
        glm::vec4 red = glm::vec4(1.0f, 0.5f, 0.5f, 1.0f);
        glm::vec4 yellow = glm::vec4(1.0f, 1.0f, 0.5f, 1.0f);

        uint32_t offset = 0;

        {
            setAttributes(offset + AnalyticPrimitive::AABB, red);
            setAttributes(offset + AnalyticPrimitive::Spheres, kChromiumReflectance, 1.0f);
            offset += AnalyticPrimitive::Count;
        }

        {
            setAttributes(offset + VolumetricPrimitive::Metaballs, kChromiumReflectance, 1.0f);
            offset += VolumetricPrimitive::Count;
        }

        {
            setAttributes(offset + SignedDistancePrimitive::MiniSpheres, green);
            setAttributes(offset + SignedDistancePrimitive::IntersectedRoundCube, green);
            setAttributes(offset + SignedDistancePrimitive::SquareTorus, kChromiumReflectance, 1.0f);
            setAttributes(offset + SignedDistancePrimitive::TwistedTorus, yellow, 0.0f, 1.0f, 0.7f, 50.0f, 0.5f);
            setAttributes(offset + SignedDistancePrimitive::Cog, yellow, 0.0f, 1.0f, 0.1f, 2.0f);
            setAttributes(offset + SignedDistancePrimitive::Cylinder, red);
            setAttributes(offset + SignedDistancePrimitive::FractalPyramid, green, 0.0f, 1.0f, 0.1f, 4.0f, 0.8f);
        }
    }

    // Setup camera.
    {
        m_eye = { 0.0f, 5.3f, -17.0f, 1.0f };
        m_at = { 0.0f, 0.0f, 0.0f, 1.0f };
        glm::vec4 right = { 1.0f, 0.0f, 0.0f, 0.0f };

        glm::vec4 direction = glm::normalize(m_at - m_eye);
        m_up = glm::vec4(glm::normalize(glm::cross(glm::vec3(direction), glm::vec3(right))), 0.0f);


        glm::mat4 rotate = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_eye = rotate * m_eye;
        m_up = rotate * m_up;

        updateCameraMatrices();
    }

    // Setup lights.
    {
        glm::vec4 lightPosition;
        glm::vec4 lightAmbientColor;
        glm::vec4 lightDiffuseColor;

        lightPosition = glm::vec4(0.0f, 18.0f, -20.0f, 0.0f);
        m_sceneCB.lightPosition = lightPosition;

        lightAmbientColor = glm::vec4(0.25f, 0.25f, 0.25f, 1.0f);
        m_sceneCB.lightAmbientColor = lightAmbientColor;

        float d = 0.6f;
        lightDiffuseColor = glm::vec4(d, d, d, 1.0f);
        m_sceneCB.lightDiffuseColor = lightDiffuseColor;
    }

    uint32_t instanceIndex = 0;

    for (uint32_t primitiveIndex = 0; primitiveIndex < AnalyticPrimitive::Count; ++primitiveIndex) {
        m_aabbInstanceCB[instanceIndex].instanceIndex = instanceIndex;
        m_aabbInstanceCB[instanceIndex].primitiveType = primitiveIndex;
        ++instanceIndex;
    }

    for (uint32_t primitiveIndex = 0; primitiveIndex < VolumetricPrimitive::Count; ++primitiveIndex) {
        m_aabbInstanceCB[instanceIndex].instanceIndex = instanceIndex;
        m_aabbInstanceCB[instanceIndex].primitiveType = primitiveIndex;
        ++instanceIndex;
    }

    for (uint32_t primitiveIndex = 0; primitiveIndex < SignedDistancePrimitive::Count; ++primitiveIndex) {
        m_aabbInstanceCB[instanceIndex].instanceIndex = instanceIndex;
        m_aabbInstanceCB[instanceIndex].primitiveType = primitiveIndex;
        ++instanceIndex;
    }
}

void CRayTracing::createSceneBuffer() {

    m_sceneBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, sizeof(SceneConstantBuffer), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void CRayTracing::updateSceneBuffer() {
    m_helper.copyToBuffer(m_sceneBuffer, &m_sceneCB, sizeof(SceneConstantBuffer));
}

void CRayTracing::createAABBPrimitiveBuffer() {
    m_aabbPrimitiveBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, sizeof(PrimitiveInstancePerFrameBuffer) * IntersectionShaderType::kTotalPrimitiveCount, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void CRayTracing::updateAABBPrimitiveBuffer() {
    m_helper.copyToBuffer(m_aabbPrimitiveBuffer, m_aabbPrimitiveAttributeBuffer, sizeof(PrimitiveInstancePerFrameBuffer) * IntersectionShaderType::kTotalPrimitiveCount);
}

VkShaderStageFlagBits CRayTracing::shaderType2ShaderStage(CShader::EShaderType type) {

    switch (type) {
    case CShader::SHADER_TYPE_MISS_SHADER:
        return VK_SHADER_STAGE_MISS_BIT_NV;
    case CShader::SHADER_TYPE_VERTEX_SHADER:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case CShader::SHADER_TYPE_ANY_HIT_SHADER:
        return VK_SHADER_STAGE_ANY_HIT_BIT_NV;
    case CShader::SHADER_TYPE_COMPUTE_SHADER:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    case CShader::SHADER_TYPE_FRAGMENT_SHADER:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case CShader::SHADER_TYPE_GEOMETRY_SHADER:
        return VK_SHADER_STAGE_GEOMETRY_BIT;
    case CShader::SHADER_TYPE_CLOSEST_HIT_SHADER:
        return VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;
    case CShader::SHADER_TYPE_INTERSECTION_SHADER:
        return VK_SHADER_STAGE_INTERSECTION_BIT_NV;
    case CShader::SHADER_TYPE_RAY_GENERATION_SHADER:
        return VK_SHADER_STAGE_RAYGEN_BIT_NV;
    case CShader::SHADER_TYPE_TESSELATION_EVAL_SHADER:
        return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    case CShader::SHADER_TYPE_TESSELATION_CONTROL_SHADER:
        return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    default:
        break;
    }

    return static_cast<VkShaderStageFlagBits>(0);
}

void CRayTracing::createShader(CShader::EShaderType type, std::string const& name, std::string const& shader_source) {

    CShader shader(name, type, shader_source);
    std::vector<uint32_t> spv = shader.compileFile();


    VkShaderModuleCreateInfo shaderInfo = {};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = spv.size() * 4;
    shaderInfo.pCode = reinterpret_cast<uint32_t*>(spv.data());

    VkShaderModule shaderModule;
    VkResult res = vkCreateShaderModule(m_device, &shaderInfo, nullptr, &shaderModule);
    if (res != VK_SUCCESS) {
        printf("could not create shader module\n");
    }

    VkPipelineShaderStageCreateInfo shaderStageInfo = {};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = shaderType2ShaderStage(type);
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";

    m_shaderStages.push_back(shaderStageInfo);
}

VkPipeline CRayTracing::createPipeline(VkPipelineLayout pipelineLayout) {

    m_shaderGroups.clear();

    createRayGenShaderGroups();

    for (size_t index = 0; index < m_rayGenShaderGroups.size(); ++index) {
        m_shaderGroups.push_back(m_rayGenShaderGroups[index]);
    }

    createMissShaderGroups();

    for (size_t index = 0; index < m_missShaderGroups.size(); ++index) {
        m_shaderGroups.push_back(m_missShaderGroups[index]);
    }

    createHitShaderGroups();

    for (size_t index = 0; index < m_hitShaderGroups.size(); ++index) {
        m_shaderGroups.push_back(m_hitShaderGroups[index]);
    }

    VkRayTracingPipelineCreateInfoNV raytracingPipelineInfo = {};
    raytracingPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV;
    raytracingPipelineInfo.maxRecursionDepth = 3;
    raytracingPipelineInfo.stageCount = static_cast<uint32_t>(m_shaderStages.size());
    raytracingPipelineInfo.pStages = m_shaderStages.data();
    raytracingPipelineInfo.groupCount = static_cast<uint32_t>(m_shaderGroups.size());
    raytracingPipelineInfo.pGroups = m_shaderGroups.data();
    raytracingPipelineInfo.layout = pipelineLayout;
    raytracingPipelineInfo.basePipelineIndex = 0;
    raytracingPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    vkCreateRayTracingPipelines(m_device, nullptr, 1, &raytracingPipelineInfo, nullptr, &m_raytracingPipeline);

    createRayGenShaderTable();
    //createMissShaderTable();
    //createHitShaderTable();

    return m_raytracingPipeline;
}

VulkanBuffer CRayTracing::getRayGenShaderGroups() {
    return m_raygenShaderGroupBuffer;
}

VulkanBuffer CRayTracing::getMissShaderGroups() {
    return m_missShaderGroupBuffer;
}

VulkanBuffer CRayTracing::getHitShaderGroups() {
    return m_hitShaderGroupBuffer;
}

void CRayTracing::createRayGenShaderTable() {
    VkDeviceSize bufferSize = m_raytracingProperties.shaderGroupHandleSize * m_rayGenShaderGroups.size() + m_raytracingProperties.shaderGroupHandleSize * m_missShaderGroups.size() + (m_raytracingProperties.shaderGroupHandleSize + sizeof(PrimitiveConstantBuffer) + sizeof(PrimitiveInstanceConstantBuffer)) * m_aabbs.size();
    m_raygenShaderGroupBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, bufferSize, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data = nullptr;
    vkMapMemory(m_device, m_raygenShaderGroupBuffer.memory, 0, bufferSize, 0, &data);
    uint8_t* mappedMemory = (uint8_t*)data;
    vkGetRayTracingShaderGroupHandles(m_device, m_raytracingPipeline, 0, m_rayGenShaderGroups.size(), m_raytracingProperties.shaderGroupHandleSize * m_rayGenShaderGroups.size(), mappedMemory);
    mappedMemory += m_raytracingProperties.shaderGroupHandleSize * m_rayGenShaderGroups.size();
    vkGetRayTracingShaderGroupHandles(m_device, m_raytracingPipeline, m_rayGenShaderGroups.size(), m_missShaderGroups.size(), m_raytracingProperties.shaderGroupHandleSize * m_missShaderGroups.size(), mappedMemory);
    mappedMemory += m_raytracingProperties.shaderGroupHandleSize * m_missShaderGroups.size();
    vkGetRayTracingShaderGroupHandles(m_device, m_raytracingPipeline, m_rayGenShaderGroups.size() + m_missShaderGroups.size(), 1, m_raytracingProperties.shaderGroupHandleSize, mappedMemory);
    mappedMemory += m_raytracingProperties.shaderGroupHandleSize;
    memcpy(mappedMemory, &m_planeMaterialCB, sizeof(PrimitiveConstantBuffer));
    mappedMemory += sizeof(PrimitiveConstantBuffer) + sizeof(PrimitiveInstanceConstantBuffer);

    uint32_t offset = 0;

    for (size_t index = 0; index < AnalyticPrimitive::Count; ++index) {
        vkGetRayTracingShaderGroupHandles(m_device, m_raytracingPipeline, m_rayGenShaderGroups.size() + m_missShaderGroups.size() + 1, 1, m_raytracingProperties.shaderGroupHandleSize, mappedMemory);
        mappedMemory += m_raytracingProperties.shaderGroupHandleSize;
        memcpy(mappedMemory, &m_aabbMaterialCB[index + offset], sizeof(PrimitiveConstantBuffer));
        mappedMemory += sizeof(m_aabbMaterialCB[0]);
        memcpy(mappedMemory, &m_aabbInstanceCB[index + offset], sizeof(PrimitiveInstanceConstantBuffer));
        mappedMemory += sizeof(m_aabbInstanceCB[0]);
    }

    offset += AnalyticPrimitive::Count;

    for (size_t index = 0; index < VolumetricPrimitive::Count; ++index) {
        vkGetRayTracingShaderGroupHandles(m_device, m_raytracingPipeline, m_rayGenShaderGroups.size() + m_missShaderGroups.size() + 2, 1, m_raytracingProperties.shaderGroupHandleSize, mappedMemory);
        mappedMemory += m_raytracingProperties.shaderGroupHandleSize;
        memcpy(mappedMemory, &m_aabbMaterialCB[index + offset], sizeof(PrimitiveConstantBuffer));
        mappedMemory += sizeof(m_aabbMaterialCB[0]);
        memcpy(mappedMemory, &m_aabbInstanceCB[index + offset], sizeof(PrimitiveInstanceConstantBuffer));
        mappedMemory += sizeof(m_aabbInstanceCB[0]);
    }

    offset += VolumetricPrimitive::Count;

    for (size_t index = 0; index < SignedDistancePrimitive::Count; ++index) {
        vkGetRayTracingShaderGroupHandles(m_device, m_raytracingPipeline, m_rayGenShaderGroups.size() + m_missShaderGroups.size() + 3, 1, m_raytracingProperties.shaderGroupHandleSize, mappedMemory);
        mappedMemory += m_raytracingProperties.shaderGroupHandleSize;
        memcpy(mappedMemory, &m_aabbMaterialCB[index + offset], sizeof(PrimitiveConstantBuffer));
        mappedMemory += sizeof(m_aabbMaterialCB[0]);
        memcpy(mappedMemory, &m_aabbInstanceCB[index + offset], sizeof(PrimitiveInstanceConstantBuffer));
        mappedMemory += sizeof(m_aabbInstanceCB[0]);
    }

    vkUnmapMemory(m_device, m_raygenShaderGroupBuffer.memory);
}

void CRayTracing::createMissShaderTable() {
    VkDeviceSize bufferSize = m_raytracingProperties.shaderGroupHandleSize * m_missShaderGroups.size();
    m_missShaderGroupBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, bufferSize, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    void* data = nullptr;
    vkMapMemory(m_device, m_missShaderGroupBuffer.memory, 0, bufferSize, 0, &data);
    uint8_t* mappedMemory = (uint8_t*)data;
    vkGetRayTracingShaderGroupHandles(m_device, m_raytracingPipeline, m_rayGenShaderGroups.size(), m_missShaderGroups.size(), bufferSize, mappedMemory);
    vkUnmapMemory(m_device, m_missShaderGroupBuffer.memory);
}

void CRayTracing::createHitShaderTable() {
    VkDeviceSize bufferSize = (m_raytracingProperties.shaderGroupHandleSize + sizeof(PrimitiveConstantBuffer) + sizeof(PrimitiveInstanceConstantBuffer)) * m_hitShaderGroups.size();
    m_hitShaderGroupBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, bufferSize, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    void* data = nullptr;
    vkMapMemory(m_device, m_hitShaderGroupBuffer.memory, 0, bufferSize, 0, &data);
    uint8_t* mappedMemory = (uint8_t*)data;

    vkGetRayTracingShaderGroupHandles(m_device, m_raytracingPipeline, m_rayGenShaderGroups.size() + m_missShaderGroups.size(), m_raytracingProperties.shaderGroupHandleSize, bufferSize, mappedMemory);
    mappedMemory += m_raytracingProperties.shaderGroupHandleSize;
    memcpy(mappedMemory, &m_planeMaterialCB, sizeof(PrimitiveConstantBuffer));
    mappedMemory += sizeof(PrimitiveConstantBuffer) + sizeof(PrimitiveInstanceConstantBuffer);

    //for (size_t index = 1; index < m_hitShaderGroups.size(); ++index) {
        vkGetRayTracingShaderGroupHandles(m_device, m_raytracingPipeline, m_rayGenShaderGroups.size() + m_missShaderGroups.size() /*+ index*/ + 1, m_raytracingProperties.shaderGroupHandleSize, bufferSize, mappedMemory);
        mappedMemory += m_raytracingProperties.shaderGroupHandleSize;
        memcpy(mappedMemory, &m_aabbMaterialCB[/*index - 1*/2], sizeof(PrimitiveConstantBuffer));
        mappedMemory += sizeof(PrimitiveConstantBuffer);
        memcpy(mappedMemory, &m_aabbInstanceCB[/*index - 1*/2], sizeof(PrimitiveInstanceConstantBuffer));
        mappedMemory += sizeof(PrimitiveInstanceConstantBuffer);

    //    vkGetRayTracingShaderGroupHandles(m_device, m_raytracingPipeline, m_rayGenShaderGroups.size() + m_missShaderGroups.size() /*+ index*/ + 2, m_raytracingProperties.shaderGroupHandleSize, bufferSize, mappedMemory);
    //    mappedMemory += m_raytracingProperties.shaderGroupHandleSize;
    //    memcpy(mappedMemory, &m_aabbMaterialCB[/*index - 1*/2], sizeof(PrimitiveConstantBuffer));
    //    mappedMemory += sizeof(PrimitiveConstantBuffer);
    //    memcpy(mappedMemory, &m_aabbInstanceCB[/*index - 1*/2], sizeof(PrimitiveInstanceConstantBuffer));
    //    mappedMemory += sizeof(PrimitiveInstanceConstantBuffer);

    //}
    vkUnmapMemory(m_device, m_hitShaderGroupBuffer.memory);
}

VulkanImage CRayTracing::createOffscreenImage(VkFormat format, uint32_t width, uint32_t height) {
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = { width, height, 1 };
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.queueFamilyIndexCount = 0;
    imageInfo.pQueueFamilyIndices = nullptr;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage offscreenImage;
    vkCreateImage(m_device, &imageInfo, nullptr, &offscreenImage);

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(m_device, offscreenImage, &memoryRequirements);

    uint32_t memoryType = m_helper.getMemoryType(memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo memoryAllocInfo = {};
    memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocInfo.allocationSize = memoryRequirements.size;
    memoryAllocInfo.memoryTypeIndex = memoryType;

    VkDeviceMemory offsreenImageMemory;
    vkAllocateMemory(m_device, &memoryAllocInfo, nullptr, &offsreenImageMemory);

    vkBindImageMemory(m_device, offscreenImage, offsreenImageMemory, 0);

    VulkanImage image;
    image.handle = offscreenImage;
    image.memory = offsreenImageMemory;
    image.size = memoryRequirements.size;
    image.format = format;
    image.width = width;
    image.height = height;

    m_offscreenImage = image;

    return image;
}

void CRayTracing::updateDescriptors(VkDescriptorSet descriptorSet) {

    VkImageViewCreateInfo offscreenImageViewInfo = {};
    offscreenImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    offscreenImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    offscreenImageViewInfo.format = m_offscreenImage.format;
    offscreenImageViewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    offscreenImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    offscreenImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    offscreenImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    offscreenImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    offscreenImageViewInfo.image = m_offscreenImage.handle;

    VkImageView offscreenImageView;
    vkCreateImageView(m_device, &offscreenImageViewInfo, nullptr, &offscreenImageView);

    VkWriteDescriptorSetAccelerationStructureNV descriptorAccelerationStructureInfo = {};
    descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    descriptorAccelerationStructureInfo.pAccelerationStructures = &m_topLevelAs;

    VkWriteDescriptorSet accelerationStructureWrite = {};
    accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
    accelerationStructureWrite.dstSet = descriptorSet;
    accelerationStructureWrite.descriptorCount = 1;
    accelerationStructureWrite.dstBinding = 0;
    accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;

    VkDescriptorImageInfo descriptorOutputImageInfo = {};
    descriptorOutputImageInfo.sampler = nullptr;
    descriptorOutputImageInfo.imageView = offscreenImageView;
    descriptorOutputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet outputImageWrite = {};
    outputImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    outputImageWrite.dstSet = descriptorSet;
    outputImageWrite.dstBinding = 1;
    outputImageWrite.dstArrayElement = 0;
    outputImageWrite.descriptorCount = 1;
    outputImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outputImageWrite.pImageInfo = &descriptorOutputImageInfo;

    VkDescriptorBufferInfo descriptorSceneBufferInfo = {};
    descriptorSceneBufferInfo.buffer = m_sceneBuffer.handle;
    descriptorSceneBufferInfo.range = m_sceneBuffer.size;

    VkWriteDescriptorSet sceneBufferWrite = {};
    sceneBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sceneBufferWrite.dstSet = descriptorSet;
    sceneBufferWrite.dstBinding = 2;
    sceneBufferWrite.dstArrayElement = 0;
    sceneBufferWrite.descriptorCount = 1;
    sceneBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sceneBufferWrite.pBufferInfo = &descriptorSceneBufferInfo;

    VkDescriptorBufferInfo descriptorFacesBufferInfo = {};
    descriptorFacesBufferInfo.buffer = m_facesBuffer.handle;
    descriptorFacesBufferInfo.range = m_facesBuffer.size;

    VkWriteDescriptorSet facesBufferWrite = {};
    facesBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    facesBufferWrite.dstSet = descriptorSet;
    facesBufferWrite.dstBinding = 3;
    facesBufferWrite.dstArrayElement = 0;
    facesBufferWrite.descriptorCount = 1;
    facesBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    facesBufferWrite.pBufferInfo = &descriptorFacesBufferInfo;

    VkDescriptorBufferInfo descriptorNormalBufferInfo = {};
    descriptorNormalBufferInfo.buffer = m_normalBuffer.handle;
    descriptorNormalBufferInfo.range = m_normalBuffer.size;

    VkWriteDescriptorSet normalBufferWrite = {};
    normalBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    normalBufferWrite.dstSet = descriptorSet;
    normalBufferWrite.dstBinding = 4;
    normalBufferWrite.dstArrayElement = 0;
    normalBufferWrite.descriptorCount = 1;
    normalBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    normalBufferWrite.pBufferInfo = &descriptorNormalBufferInfo;

    VkDescriptorBufferInfo descriptorAABBPrimitiveBufferInfo = {};
    descriptorAABBPrimitiveBufferInfo.buffer = m_aabbPrimitiveBuffer.handle;
    descriptorAABBPrimitiveBufferInfo.range = m_aabbPrimitiveBuffer.size;

    VkWriteDescriptorSet sceneAABBPrimitiveBufferWrite = {};
    sceneAABBPrimitiveBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sceneAABBPrimitiveBufferWrite.dstSet = descriptorSet;
    sceneAABBPrimitiveBufferWrite.dstBinding = 5;
    sceneAABBPrimitiveBufferWrite.dstArrayElement = 0;
    sceneAABBPrimitiveBufferWrite.descriptorCount = 1;
    sceneAABBPrimitiveBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sceneAABBPrimitiveBufferWrite.pBufferInfo = &descriptorAABBPrimitiveBufferInfo;

    std::vector<VkWriteDescriptorSet> descriptorWrites({accelerationStructureWrite, outputImageWrite, sceneBufferWrite, facesBufferWrite, normalBufferWrite, sceneAABBPrimitiveBufferWrite});
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void CRayTracing::createPrimitives() {

}

void CRayTracing::createShaderStages() {
    createShader(CShader::SHADER_TYPE_RAY_GENERATION_SHADER, "raygen", kRaygenShaderSource);
    createShader(CShader::SHADER_TYPE_CLOSEST_HIT_SHADER, "closest_hit_triangle", kClosestHitTriangleShaderSource);
    createShader(CShader::SHADER_TYPE_CLOSEST_HIT_SHADER, "closest_hit_aabb", kClosestHitAABBShaderSource);
    createShader(CShader::SHADER_TYPE_MISS_SHADER, "miss", kMissShaderSource);
    createShader(CShader::SHADER_TYPE_MISS_SHADER, "miss_shadowray", kMissShadowRayShaderSource);
    createShader(CShader::SHADER_TYPE_INTERSECTION_SHADER, "intersection_analytic", kIntersectionAnalyticShaderSource);
    createShader(CShader::SHADER_TYPE_INTERSECTION_SHADER, "intersection_volumetric", kIntersectionVolumetricShaderSource);
    createShader(CShader::SHADER_TYPE_INTERSECTION_SHADER, "intersection_signeddistance", kIntersectionSignedDistanceShaderSource);
}

/*
std::vector<VkRayTracingShaderGroupCreateInfoNV> const& CRayTracing::createShaderGroups() {
    VkRayTracingShaderGroupCreateInfoNV raygenShaderGroupInfo = {};
    raygenShaderGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
    raygenShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
    raygenShaderGroupInfo.generalShader = 0;
    raygenShaderGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
    raygenShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
    raygenShaderGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;

    m_shaderGroups.push_back(raygenShaderGroupInfo);

    VkRayTracingShaderGroupCreateInfoNV closestHitShaderGroupInfo = {};
    closestHitShaderGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
//    closestHitShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_NV;
    closestHitShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV;
    closestHitShaderGroupInfo.generalShader = VK_SHADER_UNUSED_NV;
    closestHitShaderGroupInfo.closestHitShader = 1;
    closestHitShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
    //closestHitShaderGroupInfo.intersectionShader = 3;
    closestHitShaderGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;

    m_shaderGroups.push_back(closestHitShaderGroupInfo);

    VkRayTracingShaderGroupCreateInfoNV missShaderGroupInfo = {};
    missShaderGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
    missShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
    missShaderGroupInfo.generalShader = 3;
    missShaderGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
    missShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
    missShaderGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;

    m_shaderGroups.push_back(missShaderGroupInfo);

    return m_shaderGroups;
}
*/

void CRayTracing::createRayGenShaderGroups() {

    VkRayTracingShaderGroupCreateInfoNV raygenShaderGroupInfo = {};
    raygenShaderGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
    raygenShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
    raygenShaderGroupInfo.generalShader = 0;
    raygenShaderGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
    raygenShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
    raygenShaderGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;

    m_rayGenShaderGroups.push_back(raygenShaderGroupInfo);
}

void CRayTracing::createMissShaderGroups() {

    VkRayTracingShaderGroupCreateInfoNV missShaderGroupInfo = {};
    missShaderGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
    missShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
    missShaderGroupInfo.generalShader = 3;
    missShaderGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
    missShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
    missShaderGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;

    m_missShaderGroups.push_back(missShaderGroupInfo);


    missShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
    missShaderGroupInfo.generalShader = 4;
    missShaderGroupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
    missShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
    missShaderGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;

    m_missShaderGroups.push_back(missShaderGroupInfo);

}

void CRayTracing::createHitShaderGroups() {

    VkRayTracingShaderGroupCreateInfoNV closestHitShaderGroupInfo = {};
    closestHitShaderGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
    closestHitShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV;
    closestHitShaderGroupInfo.generalShader = VK_SHADER_UNUSED_NV;
    closestHitShaderGroupInfo.closestHitShader = 1;
    closestHitShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
    closestHitShaderGroupInfo.intersectionShader = VK_SHADER_UNUSED_NV;

    m_hitShaderGroups.push_back(closestHitShaderGroupInfo);

    closestHitShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_NV;
    closestHitShaderGroupInfo.generalShader = VK_SHADER_UNUSED_NV;
    closestHitShaderGroupInfo.closestHitShader = 2;
    closestHitShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
    closestHitShaderGroupInfo.intersectionShader = 5;

    m_hitShaderGroups.push_back(closestHitShaderGroupInfo);

    closestHitShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_NV;
    closestHitShaderGroupInfo.generalShader = VK_SHADER_UNUSED_NV;
    closestHitShaderGroupInfo.closestHitShader = 2;
    closestHitShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
    closestHitShaderGroupInfo.intersectionShader = 6;

    m_hitShaderGroups.push_back(closestHitShaderGroupInfo);

    closestHitShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_NV;
    closestHitShaderGroupInfo.generalShader = VK_SHADER_UNUSED_NV;
    closestHitShaderGroupInfo.closestHitShader = 2;
    closestHitShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
    closestHitShaderGroupInfo.intersectionShader = 7;

    m_hitShaderGroups.push_back(closestHitShaderGroupInfo);
}

void CRayTracing::createCommandBuffers() {

}

void CRayTracing::buildAccelerationStructurePlane() {


}

BottomLevelAccelerationStructure CRayTracing::createBottomLevelAccelerationStructure(VkGeometryNV* geometry, uint32_t geometryCount, VkBuildAccelerationStructureFlagsNV buildFlags) {

    VkAccelerationStructureInfoNV accInfo = {};
    accInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
    accInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
    accInfo.instanceCount = 0;
    accInfo.geometryCount = geometryCount;
    accInfo.pGeometries = geometry;
    accInfo.flags = buildFlags;

    VkAccelerationStructureCreateInfoNV accelerationStructureInfo = {};
    accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
    accelerationStructureInfo.info = accInfo;
    accelerationStructureInfo.compactedSize = 0;

    VkAccelerationStructureNV accelerationStructure;
    vkCreateAccelerationStructure(m_device, &accelerationStructureInfo, nullptr, &accelerationStructure);

    VkAccelerationStructureMemoryRequirementsInfoNV bottomAccMemoryReqInfo = {};
    bottomAccMemoryReqInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
    bottomAccMemoryReqInfo.accelerationStructure = accelerationStructure;
    bottomAccMemoryReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;

    VkMemoryRequirements2 bottomAccMemoryReq = {};
    vkGetAccelerationStructureMemoryRequirements(m_device, &bottomAccMemoryReqInfo, &bottomAccMemoryReq);

    VkMemoryAllocateInfo bottomAccMemAlloc = {};
    bottomAccMemAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    bottomAccMemAlloc.allocationSize = bottomAccMemoryReq.memoryRequirements.size;
    bottomAccMemAlloc.memoryTypeIndex = m_helper.getMemoryType(bottomAccMemoryReq.memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory bottomAccMemory;
    vkAllocateMemory(m_device, &bottomAccMemAlloc, nullptr, &bottomAccMemory);

    VkBindAccelerationStructureMemoryInfoNV bottomAccBindMemoryInfo = {};
    bottomAccBindMemoryInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
    bottomAccBindMemoryInfo.accelerationStructure = accelerationStructure;
    bottomAccBindMemoryInfo.memory = bottomAccMemory;
    bottomAccBindMemoryInfo.memoryOffset = 0;
    bottomAccBindMemoryInfo.deviceIndexCount = 0;
    bottomAccBindMemoryInfo.pDeviceIndices = nullptr;

    vkBindAccelerationStructureMemory(m_device, 1, &bottomAccBindMemoryInfo);

    uint64_t accelerationStructureHandle;
    vkGetAccelerationStructureHandle(m_device, accelerationStructure, sizeof(uint64_t), &accelerationStructureHandle);

    BottomLevelAccelerationStructure accStruct = {};
    accStruct.handle = accelerationStructure;
    accStruct.memory = bottomAccMemory;
    accStruct.gpuHandle = accelerationStructureHandle;
    return accStruct;
}

void CRayTracing::buildTriangleAccelerationStructure() {

    buildPlaneGeometry();

    VkGeometryNV triangleGeometry = {};
    triangleGeometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
    triangleGeometry.pNext = nullptr;
    triangleGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
    triangleGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
    triangleGeometry.geometry.triangles.pNext = nullptr;
    triangleGeometry.geometry.triangles.vertexData = m_vertexBuffer.handle;
    triangleGeometry.geometry.triangles.vertexOffset = 0;
    triangleGeometry.geometry.triangles.vertexCount = 4;
    triangleGeometry.geometry.triangles.vertexStride = sizeof(Vertex);
    triangleGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangleGeometry.geometry.triangles.indexData = m_indexBuffer.handle;
    triangleGeometry.geometry.triangles.indexOffset = 0;
    triangleGeometry.geometry.triangles.indexCount = 6;
    triangleGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT16;
    triangleGeometry.geometry.triangles.transformData = VK_NULL_HANDLE;
    triangleGeometry.geometry.triangles.transformOffset = 0;
    triangleGeometry.geometry.aabbs = { };
    triangleGeometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
    triangleGeometry.flags = 0;

    BottomLevelAccelerationStructure triangleAccStruct = createBottomLevelAccelerationStructure(&triangleGeometry, 1);


    std::vector<VkGeometryNV> aabbGeometries;
    std::vector<BottomLevelAccelerationStructure> accStructs;

    for (size_t index = 0; index < m_aabbBuffers.size(); ++index) {
        VkGeometryNV aabbGeometry = {};
        aabbGeometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
        aabbGeometry.pNext = nullptr;
        aabbGeometry.geometryType = VK_GEOMETRY_TYPE_AABBS_NV;
        aabbGeometry.geometry.triangles = {};
        aabbGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
        aabbGeometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
        aabbGeometry.geometry.aabbs.numAABBs = 1;
        aabbGeometry.geometry.aabbs.stride = sizeof(AABB);
        aabbGeometry.geometry.aabbs.aabbData = m_aabbBuffers[index].handle;
        aabbGeometry.flags = 0;

        aabbGeometries.push_back(aabbGeometry);
        BottomLevelAccelerationStructure aabbAccStruct = createBottomLevelAccelerationStructure(&aabbGeometry, 1);
        accStructs.push_back(aabbAccStruct);
    }


    glm::uvec3 const kNumAabb = glm::uvec3(700, 1, 700);
    glm::vec3 const vWidth = glm::vec3(
        kNumAabb.x * kAabbWidth + (kNumAabb.x - 1) * kAabbDistance,
        kNumAabb.y * kAabbWidth + (kNumAabb.y - 1) * kAabbDistance,
        kNumAabb.z * kAabbWidth + (kNumAabb.z - 1) * kAabbDistance
    );

    glm::vec3 basePosition = vWidth * glm::vec3(-0.35f, 0.0f, -0.35f);

    //glm::mat4 scale = glm::scale(glm::mat4(1.0f), vWidth);
    //glm::mat4 translation = glm::translate(glm::mat4(1.0f), basePosition);
    //glm::mat3x4 transform =  translation;


    float triangleTransform[12] =
    {
        vWidth.x, 0.0f, 0.0f, basePosition.x,
        0.0f, vWidth.y, 0.0f, basePosition.y,
        0.0f, 0.0f, vWidth.z, basePosition.z,
    };

    VkGeometryInstanceNV triangleGeomInstance = {};
    memcpy(triangleGeomInstance.transform, &triangleTransform, sizeof(triangleTransform));
    triangleGeomInstance.mask = 1;
    triangleGeomInstance.instanceOffset = 0;
    triangleGeomInstance.accelerationStructureHandle = triangleAccStruct.gpuHandle;


    std::vector<VkGeometryInstanceNV> instances;
    instances.push_back(triangleGeomInstance);

    float aabbTransform[12] =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, kAabbWidth / 2.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
    };

    for (size_t index = 0; index < aabbGeometries.size(); ++index) {
        VkGeometryInstanceNV aabbGeomInstance = {};
        memcpy(aabbGeomInstance.transform, &aabbTransform, sizeof(aabbTransform));
        aabbGeomInstance.mask = 1;
        aabbGeomInstance.instanceOffset = 1 + index;
        aabbGeomInstance.accelerationStructureHandle = accStructs[index].gpuHandle;
        instances.push_back(aabbGeomInstance);
    }

    uint32_t instanceBufferSize = (uint32_t)sizeof(VkGeometryInstanceNV) * instances.size();
    VulkanBuffer instanceBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, instanceBufferSize, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_helper.copyToBuffer(instanceBuffer, instances.data(), instanceBufferSize);

    VkAccelerationStructureInfoNV topAccInfo = {};
    topAccInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
    topAccInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
    topAccInfo.instanceCount = instances.size();

    VkAccelerationStructureCreateInfoNV topAccelerationStructureInfo = {};
    topAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
    topAccelerationStructureInfo.info = topAccInfo;
    topAccelerationStructureInfo.compactedSize = 0;

    VkAccelerationStructureNV topAccelerationStructure;
    vkCreateAccelerationStructure(m_device, &topAccelerationStructureInfo, nullptr, &topAccelerationStructure);

    VkAccelerationStructureMemoryRequirementsInfoNV topAccMemReqInfo = {};
    topAccMemReqInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
    topAccMemReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
    topAccMemReqInfo.accelerationStructure = topAccelerationStructure;

    VkMemoryRequirements2 topAccMemReq;
    vkGetAccelerationStructureMemoryRequirements(m_device, &topAccMemReqInfo, &topAccMemReq);

    VkMemoryAllocateInfo topAccMemAllocInfo = {};
    topAccMemAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    topAccMemAllocInfo.allocationSize = topAccMemReq.memoryRequirements.size;
    topAccMemAllocInfo.memoryTypeIndex = m_helper.getMemoryType(topAccMemReq.memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory topAccMemory;
    vkAllocateMemory(m_device, &topAccMemAllocInfo, nullptr, &topAccMemory);

    VkBindAccelerationStructureMemoryInfoNV topAccBindInfo = {};
    topAccBindInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
    topAccBindInfo.accelerationStructure = topAccelerationStructure;
    topAccBindInfo.memory = topAccMemory;

    vkBindAccelerationStructureMemory(m_device, 1, &topAccBindInfo);

    VkAccelerationStructureMemoryRequirementsInfoNV accMemReqInfo = {};
    accMemReqInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
    accMemReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;

    accMemReqInfo.accelerationStructure = triangleAccStruct.handle;

    VkMemoryRequirements2 accMemReq;
    vkGetAccelerationStructureMemoryRequirements(m_device, &accMemReqInfo, &accMemReq);

    VkDeviceSize bottomTriangleAccelerationStructureBufferSize = accMemReq.memoryRequirements.size;

    accMemReqInfo.accelerationStructure = accStructs[0].handle;
    vkGetAccelerationStructureMemoryRequirements(m_device, &accMemReqInfo, &accMemReq);

    VkDeviceSize bottomAabbAccelerationStructureBufferSize = accMemReq.memoryRequirements.size;

    accMemReqInfo.accelerationStructure = topAccelerationStructure;
    vkGetAccelerationStructureMemoryRequirements(m_device, &accMemReqInfo, &accMemReq);
    VkDeviceSize topAccelerationStructureBufferSize = accMemReq.memoryRequirements.size;

    VkDeviceSize scratchBufferSize = std::max(std::max(bottomTriangleAccelerationStructureBufferSize, topAccelerationStructureBufferSize), bottomAabbAccelerationStructureBufferSize);
    VulkanBuffer scratchBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, scratchBufferSize, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkCommandBufferAllocateInfo commandBufferAllocInfo = {};
    commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocInfo.commandPool = m_commandPool;
    commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(m_device, &commandBufferAllocInfo, &cmdBuffer);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;


    {
        VkAccelerationStructureInfoNV asInfo = {};
        asInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
        asInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
        asInfo.geometryCount = 1;
        asInfo.pGeometries = &triangleGeometry;
        vkCmdBuildAccelerationStructure(cmdBuffer, &asInfo, VK_NULL_HANDLE, 0, VK_FALSE, triangleAccStruct.handle, VK_NULL_HANDLE, scratchBuffer.handle, 0);
    }

    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

    for (size_t index = 0; index < accStructs.size(); ++index)
    {
        VkAccelerationStructureInfoNV asInfo = {};
        asInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
        asInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
        asInfo.geometryCount = 1;//aabbGeometries.size();
        asInfo.pGeometries = &aabbGeometries[index];
        vkCmdBuildAccelerationStructure(cmdBuffer, &asInfo, VK_NULL_HANDLE, 0, VK_FALSE, accStructs[index].handle, VK_NULL_HANDLE, scratchBuffer.handle, 0);

        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
    }

    {
        VkAccelerationStructureInfoNV asInfo = {};
        asInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
        asInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
        asInfo.instanceCount = instances.size();

        vkCmdBuildAccelerationStructure(cmdBuffer, &asInfo, instanceBuffer.handle, 0, VK_FALSE, topAccelerationStructure, VK_NULL_HANDLE, scratchBuffer.handle, 0);
    }

    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

    vkEndCommandBuffer(cmdBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmdBuffer);

    m_topLevelAs = topAccelerationStructure;
}

void CRayTracing::updateCameraMatrices() {
    m_sceneCB.cameraPosition = m_eye;
    float fovAngleY = 45.0f;
    glm::mat4 view = glm::lookAtLH(glm::vec3(m_eye), glm::vec3(m_at), glm::vec3(m_up));
    glm::mat4 proj = glm::perspectiveLH(glm::radians(fovAngleY), m_aspectRatio, 0.01f, 125.0f);
    glm::mat4 viewProj = proj * view;
    m_sceneCB.projectionToWorld = glm::inverse(viewProj);
}

void CRayTracing::updateAABBPrimitivesAttributes(float animationTime) {
    glm::mat4 identity = glm::mat4(1.0f);

    glm::mat4 scale15y = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.5f, 1.0f));
    glm::mat4 scale15 = glm::scale(glm::mat4(1.0f), glm::vec3(1.5f, 1.5f, 1.5f));
    //glm::mat4 scale2 = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 2.0f, 2.0f));
    glm::mat4 scale3 = glm::scale(glm::mat4(1.0f), glm::vec3(3.0f, 3.0f, 3.0f));

    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), -2.0f * animationTime, glm::vec3(0.0f, 1.0f, 0.0f));

    auto setTransformAABB = [&](uint32_t primitiveIndex, glm::mat4& this_scale, glm::mat4& this_rotation) {
      glm::vec3 vTranslation =
              0.5f * (glm::vec3(m_aabbs[primitiveIndex].minX, m_aabbs[primitiveIndex].minY, m_aabbs[primitiveIndex].minZ)
                      + glm::vec3(m_aabbs[primitiveIndex].maxX, m_aabbs[primitiveIndex].maxY, m_aabbs[primitiveIndex].maxZ));
      glm::mat4 translation = glm::translate(glm::mat4(1.0f), vTranslation);

      glm::mat4 transform = translation * this_rotation * this_scale;
      m_aabbPrimitiveAttributeBuffer[primitiveIndex].localSpaceToBottomLevelAS = transform;
      m_aabbPrimitiveAttributeBuffer[primitiveIndex].bottomLevelASToLocalSpace = glm::inverse(transform);
    };

    uint32_t offset = 0;

    {
        setTransformAABB(offset + AnalyticPrimitive::AABB, scale15y, identity);
        setTransformAABB(offset + AnalyticPrimitive::Spheres, scale15, rotation);
        offset += AnalyticPrimitive::Count;
    }

    {
        setTransformAABB(offset + VolumetricPrimitive::Metaballs, scale15, rotation);
        offset += VolumetricPrimitive::Count;
    }

    {
        setTransformAABB(offset + SignedDistancePrimitive::MiniSpheres, identity, identity);
        setTransformAABB(offset + SignedDistancePrimitive::IntersectedRoundCube, identity, identity);
        setTransformAABB(offset + SignedDistancePrimitive::SquareTorus, scale15, identity);
        setTransformAABB(offset + SignedDistancePrimitive::TwistedTorus, identity, rotation);
        setTransformAABB(offset + SignedDistancePrimitive::Cog, identity, rotation);
        setTransformAABB(offset + SignedDistancePrimitive::Cylinder, scale15y, identity);
        setTransformAABB(offset + SignedDistancePrimitive::FractalPyramid, scale3, identity);
    }
}

void CRayTracing::buildProceduralGeometryAABBs() {

    {
        glm::ivec3 aabbGrid = glm::ivec3(4, 1, 4);
        glm::vec3 const basePosition = glm::vec3(
            -(aabbGrid.x * kAabbWidth + (aabbGrid.x - 1) * kAabbDistance) / 2.0f,
            -(aabbGrid.y * kAabbWidth + (aabbGrid.y - 1) * kAabbDistance) / 2.0f,
            -(aabbGrid.z * kAabbWidth + (aabbGrid.z - 1) * kAabbDistance) / 2.0f
        );

        glm::vec3 stride = glm::vec3(kAabbWidth + kAabbDistance, kAabbWidth + kAabbDistance, kAabbWidth + kAabbDistance);

        auto initializeAABB = [&](auto const& offsetIndex, auto const& size) {
            return AABB {
                basePosition.x + offsetIndex.x * stride.x,
                basePosition.y + offsetIndex.y * stride.y,
                basePosition.z + offsetIndex.z * stride.z,
                basePosition.x + offsetIndex.x * stride.x + size.x,
                basePosition.y + offsetIndex.y * stride.y + size.y,
                basePosition.z + offsetIndex.z * stride.z + size.z,
            };
        };

        m_aabbs.resize(IntersectionShaderType::kTotalPrimitiveCount);

        uint32_t offset = 0;

        {
            m_aabbs[offset + AnalyticPrimitive::AABB] = initializeAABB(glm::ivec3(3, 0, 0), glm::vec3(2.0f, 3.0f, 2.0f));
            m_aabbs[offset + AnalyticPrimitive::Spheres] = initializeAABB(glm::vec3(2.25f, 0.0f, 0.75f), glm::vec3(3.0f, 3.0f, 3.0f));
            offset += AnalyticPrimitive::Count;
        }

        {
            m_aabbs[offset + VolumetricPrimitive::Metaballs] = initializeAABB(glm::ivec3(0, 0, 0), glm::vec3(3.0f, 3.0f, 3.0f));
            offset += VolumetricPrimitive::Count;
        }

        {
            m_aabbs[offset + SignedDistancePrimitive::MiniSpheres] = initializeAABB(glm::ivec3(2, 0, 0), glm::vec3(2.0f, 2.0f, 2.0f));
            m_aabbs[offset + SignedDistancePrimitive::TwistedTorus] = initializeAABB(glm::ivec3(0, 0, 1), glm::vec3(2.0f, 2.0f, 2.0f));
            m_aabbs[offset + SignedDistancePrimitive::IntersectedRoundCube] = initializeAABB(glm::ivec3(0, 0, 2), glm::vec3(2.0f, 2.0f, 2.0f));
            m_aabbs[offset + SignedDistancePrimitive::SquareTorus] = initializeAABB(glm::vec3(0.75f, -0.1f, 2.25f), glm::vec3(3.0f, 3.0f, 3.0f));
            m_aabbs[offset + SignedDistancePrimitive::Cog] = initializeAABB(glm::ivec3(1, 0, 0), glm::vec3(2.0f, 2.0f, 2.0f));
            m_aabbs[offset + SignedDistancePrimitive::Cylinder] = initializeAABB(glm::ivec3(0, 0, 3), glm::vec3(2.0f, 3.0f, 2.0f));
            m_aabbs[offset + SignedDistancePrimitive::FractalPyramid] = initializeAABB(glm::ivec3(2, 0, 2), glm::vec3(6.0f, 6.0f, 6.0f));
        }

        for (size_t index = 0; index < m_aabbs.size(); ++index) {
            VulkanBuffer aabbBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(AABB), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            m_helper.copyToBuffer(aabbBuffer, &m_aabbs[index], sizeof(AABB));
            m_aabbBuffers.push_back(aabbBuffer);
        }
    }
}

void CRayTracing::buildPlaneGeometry() {
    Index indices[] = {
        3, 1, 0,
        2, 1, 3
    };

    Vertex vertices[] = {
        { glm::vec3(0.0f, 0.0f, 0.0f) },
        { glm::vec3(1.0f, 0.0f, 0.0f) },
        { glm::vec3(1.0f, 0.0f, 1.0f) },
        { glm::vec3(0.0f, 0.0f, 1.0f) },
    };

    m_indexBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, sizeof(indices), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_helper.copyToBuffer(m_indexBuffer, indices, sizeof(indices));
    m_vertexBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, sizeof(vertices), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_helper.copyToBuffer(m_vertexBuffer, vertices, sizeof(vertices));

    std::vector<uint32_t> faces;
    for(uint32_t index = 0; index < sizeof(indices) / sizeof(Index); index += 3) {
        faces.push_back(indices[index + 0]);
        faces.push_back(indices[index + 1]);
        faces.push_back(indices[index + 2]);
        faces.push_back(0);
    }

    m_facesBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, faces.size() * sizeof(uint32_t), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_helper.copyToBuffer(m_facesBuffer, faces.data(), faces.size() * sizeof(uint32_t));

    glm::vec4 normals[] = {
        glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
        glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
        glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
        glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
    };

    m_normalBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, sizeof(normals), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_helper.copyToBuffer(m_normalBuffer, normals, sizeof(normals));
    // TODO: create buffers
}

void CRayTracing::update() {

    float elapsedTime = 1.0f / 200.0f;

    // Rotate the camera around Y axis.
    {

        if (m_animateCamera)
        {
            float secondsToRotateAround = 48.0f;
            float angleToRotateBy = 360.0f * (elapsedTime / secondsToRotateAround);
            glm::mat4 rotate = glm::rotate(glm::mat4(1.0), glm::radians(angleToRotateBy), glm::vec3(0.0f, 1.0f, 0.0f));
            m_eye = rotate * m_eye;
            m_up = rotate * m_up;
            m_at = rotate * m_at;
            updateCameraMatrices();
        }


        if (m_animateLight)
        {
            float secondsToRotateAround = 8.0f;
            float angleToRotateBy = -360.0f * (elapsedTime / secondsToRotateAround);
            glm::mat4 rotate = glm::rotate(glm::mat4(1.0), glm::radians(angleToRotateBy), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::vec4 prevLightPosition = m_sceneCB.lightPosition;
            m_sceneCB.lightPosition = rotate * prevLightPosition;
        }

        static float animateGeometryTime = 0.0f;
        animateGeometryTime += elapsedTime;

        updateAABBPrimitivesAttributes(animateGeometryTime);

        m_sceneCB.elapsedTime = animateGeometryTime;

        updateSceneBuffer();

        updateAABBPrimitiveBuffer();

    }
}
