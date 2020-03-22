#version 460 core
#extension GL_NV_ray_tracing : require
layout(location = 2) rayPayloadInNV bool isHit;

void main() {
    isHit = false;
}
