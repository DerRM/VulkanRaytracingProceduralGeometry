#version 460 core
#extension GL_EXT_ray_tracing : require

struct RayPayload {
    vec4 color;
    uint recursionDepth;
};

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;

const vec4 kBackgroundColor = vec4(0.8f, 0.9f, 1.0f, 1.0f);

void main() {
    rayPayload.color = kBackgroundColor;
}
