# Vulkan Raytracing Procedural Geometry Sample

Vulkan port of the D3D12 Procedural Geometry Demo [WIP]
[![Build Status](https://app.travis-ci.com/DerRM/VulkanRaytracingProceduralGeometry.svg?branch=master)](https://app.travis-ci.com/DerRM/VulkanRaytracingProceduralGeometry)

This sample uses the official VK_KHR_ray_tracing_pipeline and VK_KHR_acceleration_structure extensions. It was ported from the before adapted sample for [VK_KHR_ray_tracing](https://github.com/DerRM/VulkanRaytracingProceduralGeometry/tree/VK_KHR_ray_tracing).

![alt text](screenshot.png "Vulkan Raytracing Procedural Geometry Sample")

This sample demonstrates how to implement procedural geometry using intersection shaders. It utilizes multiple intersections shaders to create analytic and volumetric, signed distance and fractal geometry. In addition, it introduces:

* Extended shader table layouts and indexing covering multiple geometries and bottom-level acceleration structures (bottom-level AS, or BLAS for short).
* Use of trace ray recursion and two different ray types: radiance and shadow rays.
* It also serves as a cross reference between D3D12 DXR style API calls and Vulkan as it directly adapts the [D3D12 Raytracing Procedural Geometry sample](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12Raytracing/src/D3D12RaytracingProceduralGeometry/readme.md)