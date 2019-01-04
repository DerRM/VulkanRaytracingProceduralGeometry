cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
project(VulkanRendering)

find_package(Vulkan)

include_directories($ENV{VULKAN_SDK}/../source/shaderc/src/build/install/include)
include_directories(/home/derrm/Downloads/glm/build/install/include)

add_executable(VulkanRendering
    raytracingscenedefines.hxx
    raytracingglsldefines.hxx
    raytracing.hxx
    raytracing.cxx
    shader.hxx
    shader.cxx
    vulkanhelper.hxx
    vulkanhelper.cxx
    main.cxx
    )

target_include_directories(VulkanRendering PRIVATE Vulkan::Vulkan)
target_link_libraries(VulkanRendering Vulkan::Vulkan xcb $ENV{VULKAN_SDK}/../source/shaderc/src/build/install/lib64/libshaderc_shared.so)


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVK_USE_PLATFORM_XCB_KHR")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_XCB_KHR")
