#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>
#include <malloc.h>
#include <iostream>
#include <sstream>
#include <assert.h>
#include <algorithm>
#include <fstream>

#ifdef WIN32
#include <Windows.h>
#elif defined(__linux__)
#include <xcb/xcb.h>
#endif

//#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "shader.hxx"
#include "raytracing.hxx"


#define WIDTH 1280
#define HEIGHT 720

struct VkGeometryInstanceNV {
    float          transform[12];
    uint32_t       instanceCustomIndex : 24;
    uint32_t       mask : 8;
    uint32_t       instanceOffset : 24;
    uint32_t       flags : 8;
    uint64_t       accelerationStructureHandle;
};

uint32_t getMemoryType(VkPhysicalDeviceMemoryProperties& gpuMemProps, VkMemoryRequirements& memoryRequirements, VkMemoryPropertyFlags memoryProperties) {
    uint32_t memoryType = 0;
    for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < VK_MAX_MEMORY_TYPES; ++memoryTypeIndex) {
        if (memoryRequirements.memoryTypeBits & (1 << memoryTypeIndex)) {
            if ((gpuMemProps.memoryTypes[memoryTypeIndex].propertyFlags & memoryProperties) == memoryProperties) {
                memoryType = memoryTypeIndex;
                break;
            }
        }
    }

    return memoryType;
}

#ifdef WIN32
LRESULT CALLBACK wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	//Window* window = (Window*)(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	switch (message)
	{
	case WM_SIZE:
	{
		//openglRenderer.reshapeWindow(LOWORD(lParam), HIWORD(lParam));

		int width = LOWORD(lParam);
		int height = HIWORD(lParam);

		//if (window->m_reshapeFunc) {
		//	window->m_reshapeFunc(width, height);
		//}

		//window->setWidth(width);
		//window->setHeight(height);

	} break;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case 'D':
			//wasDPressed = true;
			break;

		default:
			break;
		}
		break;
	case WM_KEYUP:
		switch (wParam)
		{
		case 'D':
			//wasDPressed = !wasDPressed;
			break;
		default:
			break;
		}
		break;
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
#endif

#ifdef _DEBUG
VkBool32 VKAPI_PTR debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT                  /*messageTypes*/,
        const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
        void*                                            /*pUserData*/)
{
    std::string prefix;

    if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        prefix += "WARNING";
    }
    else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        prefix += "ERROR";
    }

    if (pCallbackData) {
        printf("[%s] %s, %s\n", prefix.c_str(), pCallbackData->pMessageIdName, pCallbackData->pMessage);
    }

    return VK_FALSE;
}
#endif

int main() {

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_1;
    appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.pApplicationName = "Something";
    appInfo.engineVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.pEngineName = "Cool Engine";

    uint32_t instanceLayerCount;
    vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
    std::vector<VkLayerProperties> instanceLayers(instanceLayerCount);
    vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayers.data());

    uint32_t instanceExtensionCount;
    vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);
    std::vector<VkExtensionProperties> instanceExtensions(instanceExtensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, instanceExtensions.data());

    std::vector<const char*> enabledExtensions;
    enabledExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#ifdef WIN32
	enabledExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
    enabledExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif

#ifdef _DEBUG
   enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    //enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    enabledExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    std::vector<const char*> enableValidationLayers;
#ifdef _DEBUG
    enableValidationLayers.push_back("VK_LAYER_LUNARG_standard_validation");
#endif
	enableValidationLayers.push_back("VK_LAYER_LUNARG_monitor");

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    instanceInfo.ppEnabledExtensionNames = enabledExtensions.data();
    instanceInfo.enabledLayerCount = static_cast<uint32_t>(enableValidationLayers.size());
    instanceInfo.ppEnabledLayerNames = enableValidationLayers.data();


    VkInstance instance;
    VkResult res = vkCreateInstance(&instanceInfo, nullptr, &instance);
	if (res != VK_SUCCESS) {
		printf("vkCreateInstance failed\n");
	}

#ifdef _DEBUG
    VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo = {};
    debugMessengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugMessengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    debugMessengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debugMessengerInfo.pfnUserCallback = debug_callback;

    PFN_vkCreateDebugUtilsMessengerEXT pfnCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

    VkDebugUtilsMessengerEXT debugMessenger;
    pfnCreateDebugUtilsMessengerEXT(instance, &debugMessengerInfo, nullptr, &debugMessenger);
#endif

    VkPhysicalDevice gpu;

    uint32_t gpuCount;
    vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
    std::vector<VkPhysicalDevice> gpus(gpuCount);
    vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data());

    gpu = gpus[0];

    uint32_t deviceLayerCount;
    vkEnumerateDeviceLayerProperties(gpu, &deviceLayerCount, nullptr);
    std::vector<VkLayerProperties> deviceLayers(deviceLayerCount);
    vkEnumerateDeviceLayerProperties(gpu, &deviceLayerCount, deviceLayers.data());

    uint32_t deviceExtensionCount;
    vkEnumerateDeviceExtensionProperties(gpu, nullptr, &deviceExtensionCount, nullptr);
    std::vector<VkExtensionProperties> deviceExtensions(deviceExtensionCount);
    vkEnumerateDeviceExtensionProperties(gpu, nullptr, &deviceExtensionCount, deviceExtensions.data());

    uint32_t queuePropertyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queuePropertyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueProperties(queuePropertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queuePropertyCount, queueProperties.data());

    const float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = 0;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    std::vector<const char*> activatedDeviceExtensions;
    activatedDeviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    activatedDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    activatedDeviceExtensions.push_back(VK_NV_RAY_TRACING_EXTENSION_NAME);
    activatedDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

    VkDevice device;


    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    vkGetPhysicalDeviceFeatures2(gpu, &features2);

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &features2;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(activatedDeviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = activatedDeviceExtensions.data();

    res = vkCreateDevice(gpu, &deviceCreateInfo, nullptr, &device);
	if (res != VK_SUCCESS) {
		printf("vkCreateDevice failed\n");
	}

    CVulkanHelper helper(device, gpu);

    VkQueue queue;
    vkGetDeviceQueue(device, 0, 0, &queue);

    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = 0;

    VkCommandPool commandPool;
    vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &commandPool);

    VkPhysicalDeviceMemoryProperties gpuMemProps;
    vkGetPhysicalDeviceMemoryProperties(gpu, &gpuMemProps);

    std::vector<VkDescriptorPoolSize> poolSizes;

    VkDescriptorPoolSize poolSize0 = {};
    poolSize0.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
    poolSize0.descriptorCount = 2;

    poolSizes.push_back(poolSize0);

    VkDescriptorPoolSize poolSize1 = {};
    poolSize1.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize1.descriptorCount = 2;

    poolSizes.push_back(poolSize1);

    VkDescriptorPoolSize poolSize2 = {};
    poolSize2.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize2.descriptorCount = 2;

    poolSizes.push_back(poolSize2);

    VkDescriptorPoolSize poolSize3 = {};
    poolSize3.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize3.descriptorCount = 2;

    poolSizes.push_back(poolSize3);

    VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.maxSets = 3;
    descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolInfo.pPoolSizes = poolSizes.data();

    VkDescriptorPool descriptorPool;
    vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool);

    VkPhysicalDeviceRayTracingPropertiesNV raytracingProperties = {};
    raytracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;

    VkPhysicalDeviceProperties2 props = {};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props.pNext = &raytracingProperties;

    vkGetPhysicalDeviceProperties2(gpu, &props);

    CRayTracing rayTracing(device, gpu, queue, commandPool, raytracingProperties);
    rayTracing.init();
    rayTracing.initScene();

    rayTracing.buildProceduralGeometryAABBs();
    rayTracing.buildTriangleAccelerationStructure();

    rayTracing.updateAABBPrimitivesAttributes(0.0f);

    std::vector<VkDescriptorSetLayoutBinding> layoutbindings;

    VkDescriptorSetLayoutBinding layoutbindingAccelerationStructure = {};
    layoutbindingAccelerationStructure.binding = 0;
    layoutbindingAccelerationStructure.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
    layoutbindingAccelerationStructure.descriptorCount = 1;
    layoutbindingAccelerationStructure.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;

    layoutbindings.push_back(layoutbindingAccelerationStructure);

    VkDescriptorSetLayoutBinding layoutbindingOuputImage = {};
    layoutbindingOuputImage.binding = 1;
    layoutbindingOuputImage.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    layoutbindingOuputImage.descriptorCount = 1;
    layoutbindingOuputImage.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

    layoutbindings.push_back(layoutbindingOuputImage);

    VkDescriptorSetLayoutBinding layoutbindingSceneBuffer = {};
    layoutbindingSceneBuffer.binding = 2;
    layoutbindingSceneBuffer.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutbindingSceneBuffer.descriptorCount = 1;
    layoutbindingSceneBuffer.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_INTERSECTION_BIT_NV;

    layoutbindings.push_back(layoutbindingSceneBuffer);

    VkDescriptorSetLayoutBinding layoutbindingFacesBuffer = {};
    layoutbindingFacesBuffer.binding = 3;
    layoutbindingFacesBuffer.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutbindingFacesBuffer.descriptorCount = 1;
    layoutbindingFacesBuffer.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;

    layoutbindings.push_back(layoutbindingFacesBuffer);

    VkDescriptorSetLayoutBinding layoutbindingNormalBuffer = {};
    layoutbindingNormalBuffer.binding = 4;
    layoutbindingNormalBuffer.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutbindingNormalBuffer.descriptorCount = 1;
    layoutbindingNormalBuffer.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;

    layoutbindings.push_back(layoutbindingNormalBuffer);

    VkDescriptorSetLayoutBinding layoutbindingAABBPrimitiveBuffer = {};
    layoutbindingAABBPrimitiveBuffer.binding = 5;
    layoutbindingAABBPrimitiveBuffer.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutbindingAABBPrimitiveBuffer.descriptorCount = 1;
    layoutbindingAABBPrimitiveBuffer.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_INTERSECTION_BIT_NV;

    layoutbindings.push_back(layoutbindingAABBPrimitiveBuffer);

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(layoutbindings.size());
    layoutInfo.pBindings = layoutbindings.data();

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

    VkDescriptorSetLayout descriptorSetLayout;
    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);

    descriptorSetLayouts.push_back(descriptorSetLayout);

    rayTracing.createShaderStages();

    //std::vector<VkRayTracingShaderGroupCreateInfoNV> const& shaderGroups = rayTracing.createShaderGroups();

    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {};
    descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocInfo.descriptorSetCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    descriptorSetAllocInfo.pSetLayouts = descriptorSetLayouts.data();
    descriptorSetAllocInfo.descriptorPool = descriptorPool;

    VkDescriptorSet descriptorSet;
    vkAllocateDescriptorSets(device, &descriptorSetAllocInfo, &descriptorSet);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo ={};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    VkPipelineLayout pipelineLayout;
    res = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
    if (res != VK_SUCCESS) {
        printf("vkCreatePipelineLayout failed\n");
    }

    VkPipeline raytracingPipeline = rayTracing.createPipeline(pipelineLayout);

    /*
    VkDeviceSize hitShaderAndDataSize = raytracingProperties.shaderGroupHandleSize + sizeof(PrimitiveConstantBuffer);
    VkDeviceSize raygenAndMissSize = raytracingProperties.shaderGroupHandleSize * 2;

    VulkanBuffer groupBuffer = helper.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, hitShaderAndDataSize + raygenAndMissSize, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    void* data = nullptr;
    vkMapMemory(device, groupBuffer.memory, 0, raytracingProperties.shaderGroupHandleSize * shaderGroups.size(), 0, &data);
    uint8_t* mappedMemory = (uint8_t*)data;
    vkGetRayTracingShaderGroupHandles(device, raytracingPipeline, 0, 1, raytracingProperties.shaderGroupHandleSize, mappedMemory);
    mappedMemory += raytracingProperties.shaderGroupHandleSize;
    vkGetRayTracingShaderGroupHandles(device, raytracingPipeline, 1, 1, hitShaderAndDataSize, mappedMemory);
    mappedMemory += raytracingProperties.shaderGroupHandleSize;
    memcpy(mappedMemory, &rayTracing.getPlaneMaterialBuffer(), sizeof(PrimitiveConstantBuffer));
    mappedMemory += sizeof(PrimitiveConstantBuffer);
    vkGetRayTracingShaderGroupHandles(device, raytracingPipeline, 2, 1, raytracingProperties.shaderGroupHandleSize, mappedMemory);
    vkUnmapMemory(device, groupBuffer.memory);
*/

	const char* applicationName = "VulkanRendering";

	VkSurfaceKHR surface;
#ifdef WIN32


	DWORD dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;

	HINSTANCE hInstance = GetModuleHandle(NULL);

	WNDCLASSEX windowClass;
	windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	windowClass.lpfnWndProc = (WNDPROC)wndProc;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = hInstance;
	windowClass.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	windowClass.hIconSm = windowClass.hIcon;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.hbrBackground = NULL;
	windowClass.lpszMenuName = NULL;
	windowClass.lpszClassName = applicationName;
	windowClass.cbSize = sizeof(WNDCLASSEX);

	RegisterClassEx(&windowClass);

	RECT winRect = { 0, 0, WIDTH, HEIGHT };
	AdjustWindowRect(&winRect, WS_OVERLAPPEDWINDOW, FALSE);

	HWND hWnd = CreateWindowEx(dwExStyle, applicationName, applicationName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, winRect.right - winRect.left, winRect.bottom - winRect.top, NULL, NULL, hInstance, NULL);
	//SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);
	ShowWindow(hWnd, SW_SHOW);
	SetForegroundWindow(hWnd);
	SetFocus(hWnd);

	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.hinstance = hInstance;
	surfaceCreateInfo.hwnd = hWnd;
	vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface);
#elif defined(__linux__)
    int screenp = 0;
    xcb_connection_t* connection = xcb_connect(nullptr, &screenp);
    if (xcb_connection_has_error(connection)) {
        printf("Failed to connect to X server using XCB.\n");
    }

    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(connection));

    for (int s = screenp; s > 0; --s) {
        xcb_screen_next(&iter);
    }

    xcb_screen_t* screen = iter.data;

    xcb_window_t window = xcb_generate_id(connection);

    uint32_t eventMask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t valueList[] = { screen->black_pixel, 0 };

    xcb_create_window(connection, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, WIDTH, HEIGHT, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, eventMask, valueList);

    VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.window = window;
    surfaceCreateInfo.connection = connection;

    vkCreateXcbSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface);
#endif

    VkBool32 queueSupported;
    vkGetPhysicalDeviceSurfaceSupportKHR(gpu, 0, surface, &queueSupported);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, surfaceFormats.data());

    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(gpu, 0, surface, &presentSupport);

    VkQueue presentQueue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, 0, 0, &presentQueue);

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &capabilities);

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentModeCount, presentModes.data());

    VkSurfaceFormatKHR surfaceFormat;

    if (surfaceFormats.size() == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
        surfaceFormat = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    }
    else {
        for (size_t formatIndex = 0; formatIndex < surfaceFormats.size(); ++formatIndex) {
            if (surfaceFormats[formatIndex].format == VK_FORMAT_B8G8R8A8_UNORM && surfaceFormats[formatIndex].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                surfaceFormat = surfaceFormats[formatIndex];
                break;
            }
        }
    }

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;;
	
    for (size_t presentModeIndex = 0; presentModeIndex < presentModes.size(); ++presentModeIndex) {
        if (presentModes[presentModeIndex] == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = presentModes[presentModeIndex];
            break;
        }
        else if (presentModes[presentModeIndex] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            presentMode = presentModes[presentModeIndex];
        }
    }
	
    VkExtent2D swapExtent;

    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        swapExtent = capabilities.currentExtent;
    }
    else {
        swapExtent = { WIDTH, HEIGHT };
        swapExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, swapExtent.width));
        swapExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, swapExtent.height));
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapChainInfo = {};
    swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainInfo.surface = surface;
    swapChainInfo.minImageCount = imageCount;
    swapChainInfo.imageFormat = surfaceFormat.format;
    swapChainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainInfo.imageExtent = swapExtent;
    swapChainInfo.imageArrayLayers = 1;
    swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainInfo.queueFamilyIndexCount = 0;
    swapChainInfo.pQueueFamilyIndices = nullptr;
    swapChainInfo.preTransform = capabilities.currentTransform;
    swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainInfo.presentMode = presentMode;
    swapChainInfo.clipped = VK_TRUE;
    swapChainInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain;
    vkCreateSwapchainKHR(device, &swapChainInfo, nullptr, &swapchain);

    uint32_t swapImageCount;
    vkGetSwapchainImagesKHR(device, swapchain, &swapImageCount, nullptr);
    std::vector<VkImage> swapImages(swapImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &swapImageCount, swapImages.data());

    std::vector<VkImageView> swapImageViews(swapImageCount);

    for (size_t swapImageIndex = 0; swapImageIndex < swapImages.size(); ++swapImageIndex) {
        VkImageViewCreateInfo imageViewInfo = {};
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.image = swapImages[swapImageIndex];
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.format = surfaceFormat.format;
        imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = 1;

        vkCreateImageView(device, &imageViewInfo, nullptr, &swapImageViews[swapImageIndex]);
    }

    rayTracing.createSceneBuffer();
    rayTracing.updateSceneBuffer();
    rayTracing.createAABBPrimitiveBuffer();
    rayTracing.updateAABBPrimitiveBuffer();

    VulkanImage offscreenImage = rayTracing.createOffscreenImage(surfaceFormat.format, swapExtent.width, swapExtent.height);

    rayTracing.updateDescriptors(descriptorSet);

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = surfaceFormat.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderpassInfo = {};
    renderpassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassInfo.attachmentCount = 1;
    renderpassInfo.pAttachments = &colorAttachment;
    renderpassInfo.subpassCount = 1;
    renderpassInfo.pSubpasses = &subpass;

    VkRenderPass renderpass;
    vkCreateRenderPass(device, &renderpassInfo, nullptr, &renderpass);

    std::vector<VkFramebuffer> swapFramebuffers(swapImageCount);

    for (size_t frameBufferIndex = 0; frameBufferIndex < swapFramebuffers.size(); ++frameBufferIndex) {
        VkImageView attachments[] = {
          swapImageViews[frameBufferIndex]
        };

        VkFramebufferCreateInfo frameBufferInfo = {};
        frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferInfo.renderPass = renderpass;
        frameBufferInfo.attachmentCount = 1;
        frameBufferInfo.pAttachments = attachments;
        frameBufferInfo.width = swapExtent.width;
        frameBufferInfo.height = swapExtent.height;
        frameBufferInfo.layers = 1;

        vkCreateFramebuffer(device, &frameBufferInfo, nullptr, &swapFramebuffers[frameBufferIndex]);
    }

    std::vector<VkCommandBuffer> commandBuffers(swapFramebuffers.size());

    VkCommandBufferAllocateInfo cmdBufferAllocInfo = {};
    cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufferAllocInfo.commandPool = commandPool;
    cmdBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufferAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
    vkAllocateCommandBuffers(device, &cmdBufferAllocInfo, commandBuffers.data());

    VulkanBuffer raygenShaderGroup = rayTracing.getRayGenShaderGroups();
    //VulkanBuffer missShaderGroup = rayTracing.getMissShaderGroups();
    //VulkanBuffer hitShaderGroup = rayTracing.getHitShaderGroups();

    for (size_t commandBufferIndex = 0; commandBufferIndex < commandBuffers.size(); ++commandBufferIndex) {

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

        vkBeginCommandBuffer(commandBuffers[commandBufferIndex], &beginInfo);

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = 1;

        VkImageMemoryBarrier imageMemoryBarrier {};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.srcAccessMask = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.image = offscreenImage.handle;
        imageMemoryBarrier.subresourceRange = subresourceRange;

        vkCmdPipelineBarrier(commandBuffers[commandBufferIndex], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

        vkCmdBindPipeline(commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, raytracingPipeline);
        vkCmdBindDescriptorSets(commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        vkCmdTraceRays(commandBuffers[commandBufferIndex],
                       raygenShaderGroup.handle, 0,
                       raygenShaderGroup.handle, 1 * raytracingProperties.shaderGroupHandleSize, raytracingProperties.shaderGroupHandleSize,
                       raygenShaderGroup.handle, 3 * raytracingProperties.shaderGroupHandleSize, (raytracingProperties.shaderGroupHandleSize + sizeof(PrimitiveConstantBuffer) + sizeof(PrimitiveInstanceConstantBuffer)),
                       VK_NULL_HANDLE, 0, 0,
                       WIDTH, HEIGHT, 1);

        imageMemoryBarrier.srcAccessMask = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.image = swapImages[commandBufferIndex];
        imageMemoryBarrier.subresourceRange = subresourceRange;
        vkCmdPipelineBarrier(commandBuffers[commandBufferIndex], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

        imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.image = offscreenImage.handle;
        imageMemoryBarrier.subresourceRange = subresourceRange;
        vkCmdPipelineBarrier(commandBuffers[commandBufferIndex], VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

        VkImageCopy copyRegion;
        copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.srcOffset = { 0, 0, 0 };
        copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.dstOffset = { 0, 0, 0 };
        copyRegion.extent = {swapExtent.width, swapExtent.height, 1};
        vkCmdCopyImage(commandBuffers[commandBufferIndex], offscreenImage.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapImages[commandBufferIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = 0;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.image = swapImages[commandBufferIndex];
        imageMemoryBarrier.subresourceRange = subresourceRange;
        vkCmdPipelineBarrier(commandBuffers[commandBufferIndex], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

        vkEndCommandBuffer(commandBuffers[commandBufferIndex]);
    }

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    std::vector<VkSemaphore> imageAvailableSemaphores(swapImageCount);
	for (size_t index = 0; index < imageAvailableSemaphores.size(); ++index) {
		vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[index]);
	}

    std::vector<VkSemaphore> renderFinishedSemaphores(swapImageCount);
	for (size_t index = 0; index < renderFinishedSemaphores.size(); ++index) {
		vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[index]);
	}

    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    std::vector<VkFence> fences(swapImageCount);
    for (uint32_t fenceIndex = 0; fenceIndex < fences.size(); ++fenceIndex) {
        vkCreateFence(device, &fenceCreateInfo, nullptr, &fences[fenceIndex]);
    }

	bool running = true;

#ifdef WIN32
#elif defined(__linux__)
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, static_cast<uint32_t>(strlen(applicationName)), applicationName);

    xcb_intern_atom_cookie_t wmDeleteCookie = xcb_intern_atom(connection, 0, strlen("WM_DELETE_WINDOW"), "WM_DELETE_WINDOW");
    xcb_intern_atom_cookie_t wmProtocolsCookie = xcb_intern_atom(connection, 0, strlen("WM_PROTOCOLS"), "WM_PROTOCOLS");
    xcb_intern_atom_reply_t *wmDeleteReply = xcb_intern_atom_reply(connection, wmDeleteCookie, nullptr);
    xcb_intern_atom_reply_t *wmProtocolReply = xcb_intern_atom_reply(connection, wmProtocolsCookie, nullptr);

    xcb_atom_t wmDeleteWin = wmDeleteReply->atom;
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, wmProtocolReply->atom, 4, 32, 1, &wmDeleteReply->atom);

    xcb_map_window(connection, window);
    xcb_flush(connection);

    xcb_generic_event_t *event;
    xcb_client_message_event_t *cm;
#endif

	uint32_t frameIndex = 0;

    while (running) {
#ifdef WIN32
		MSG msg;
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT) {
			running = false;
		}
#elif defined(__linux__)
        while ((event = xcb_poll_for_event(connection)) != nullptr) {
            switch (event->response_type & ~0x80) {
                case XCB_CLIENT_MESSAGE: {
                    cm = reinterpret_cast<xcb_client_message_event_t*>(event);
                    if (cm->data.data32[0] == wmDeleteWin) {
                        running = false;
                    }
                } break;
            }
            free(event);
        }
#endif
        rayTracing.update();

        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swapchain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphores[frameIndex], VK_NULL_HANDLE, &imageIndex);

        VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailableSemaphores[frameIndex];
        submitInfo.pWaitDstStageMask = &waitStageMask;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[frameIndex];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinishedSemaphores[frameIndex];

		VkFence fence = fences[frameIndex];
		vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &fence);

        vkQueueSubmit(queue, 1, &submitInfo, fence);

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinishedSemaphores[frameIndex];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(queue, &presentInfo);

		frameIndex = (frameIndex + 1) % swapImageCount;
    }

#ifdef WIN32
#elif defined(__linux__)
    xcb_destroy_window(connection, window);
#endif

    return 0;
}
