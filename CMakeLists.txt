cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
project(VulkanRendering)

set (CMAKE_CXX_STANDARD 11)

message(STATUS "Vulkan SDK Location $ENV{VULKAN_SDK}")

include_directories(external/glm)
include_directories(external/Vulkan-Headers/include)

add_executable(VulkanRendering
    raytracingscenedefines.hxx
    raytracingglsldefines.hxx
    raytracing.hxx
    raytracing.cxx
    #shader.hxx
    #shader.cxx
    vulkanhelper.hxx
    vulkanhelper.cxx
    main.cxx
    )

if (WIN32)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVK_USE_PLATFORM_WIN32_KHR")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_WIN32_KHR")	
else()
	target_link_libraries(VulkanRendering xcb)

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVK_USE_PLATFORM_XCB_KHR")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_XCB_KHR")	
endif()

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")