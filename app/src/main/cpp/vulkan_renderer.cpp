//
// Created by 31483 on 2025/11/21.
//
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <vector>
#include <string>
#include <android/log.h>
#include <set>
#include <sstream>
#define LOG_TAG "VulkanRenderer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// 结构体定义
struct SwapchainInfo {
    VkSwapchainKHR swapchain;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;
    VkSurfaceFormatKHR format;
    VkExtent2D extent;
};

// 设备详情
struct DeviceInfo {
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    uint32_t graphicsQueueFamily;
    uint32_t presentQueueFamily;
    VkSurfaceKHR surface;
};

// 全局变量存储
static std::vector<const char*> instanceExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
};

static std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// 辅助函数：查找队列族
uint32_t findQueueFamily(VkPhysicalDevice physicalDevice, VkQueueFlags flags, VkSurfaceKHR surface = VK_NULL_HANDLE) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & flags) {
            if (surface != VK_NULL_HANDLE) {
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
                if (presentSupport) {
                    return i;
                }
            } else {
                return i;
            }
        }
    }
    return UINT32_MAX;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeCreateFramebuffers(
        JNIEnv* env, jobject thiz,
        jlong deviceHandle,
        jlong swapchainHandle,
        jlong renderPass) {

    LOGI("=== nativeCreateFramebuffers START ===");

    if (deviceHandle == 0 || swapchainHandle == 0 || renderPass == 0) {
        LOGE("Invalid handles: device=%llx, swapchain=%llx, renderPass=%llx",
             (unsigned long long)deviceHandle,
             (unsigned long long)swapchainHandle,
             (unsigned long long)renderPass);
        return JNI_FALSE;
    }

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    SwapchainInfo* swapchainInfo = reinterpret_cast<SwapchainInfo*>(swapchainHandle);
    VkRenderPass vkRenderPass = reinterpret_cast<VkRenderPass>(renderPass);

    LOGI("Creating %zu framebuffers...", swapchainInfo->imageViews.size());

    swapchainInfo->framebuffers.resize(swapchainInfo->imageViews.size());

    for (size_t i = 0; i < swapchainInfo->imageViews.size(); i++) {
        VkImageView attachments[] = {
                swapchainInfo->imageViews[i]
        };
        //  renderpass，宽高，imageViews(images的视图)
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = vkRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainInfo->extent.width;
        framebufferInfo.height = swapchainInfo->extent.height;
        framebufferInfo.layers = 1;
        //        创建帧缓冲
        VkResult result = vkCreateFramebuffer(deviceInfo->device, &framebufferInfo, nullptr,
                                              &swapchainInfo->framebuffers[i]);

        if (result != VK_SUCCESS) {
            LOGE("Failed to create framebuffer %zu: %d", i, result);

            // 清理已创建的 framebuffers
            for (size_t j = 0; j < i; j++) {
                vkDestroyFramebuffer(deviceInfo->device, swapchainInfo->framebuffers[j], nullptr);
            }
            swapchainInfo->framebuffers.clear();

            return JNI_FALSE;
        }

        LOGI("Created framebuffer[%zu]: %llu, extent=%ux%u",
             i, swapchainInfo->framebuffers[i],
             swapchainInfo->extent.width, swapchainInfo->extent.height);
    }

    LOGI("All %zu framebuffers created successfully", swapchainInfo->framebuffers.size());
    return JNI_TRUE;
}
// 1. 创建Vulkan实例
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeCreateInstance(
        JNIEnv* env, jobject /* this */) {

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VulkanFilter";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;
    const char* validationLayers[] = {
            "VK_LAYER_KHRONOS_validation"
    };
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = instanceExtensions.size();
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();
    createInfo.enabledLayerCount = 0;

    VkInstance instance;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

    if (result != VK_SUCCESS) {
        LOGE("Failed to create Vulkan instance: %d", result);
        return 0;
    }

    LOGI("Vulkan instance created successfully");
    return reinterpret_cast<jlong>(instance);
}

void LogPhysicalDevices(const std::vector<VkPhysicalDevice>& devices) {
    std::ostringstream oss;
    oss << "count=" << devices.size();
    for (const auto &d : devices) {
        oss << " 0x" << std::hex << reinterpret_cast<uintptr_t>(d);
    }
    LOGE("%s", oss.str().c_str());
}

// 2. 创建设备
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeCreateDevice(
        JNIEnv* env, jobject /* this */, jlong instanceHandle, jobject surface) {

    VkInstance instance = reinterpret_cast<VkInstance>(instanceHandle);

    // 创建Surface
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);

    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo{};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.window = window;

    VkSurfaceKHR vkSurface;
    vkCreateAndroidSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &vkSurface);

    // 选择物理设备
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        LOGE("Failed to find GPUs with Vulkan support");
        return 0;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    LogPhysicalDevices(devices);
    VkPhysicalDevice physicalDevice = devices[0]; // 使用第一个设备

    // 查找队列族
    uint32_t graphicsFamily = findQueueFamily(physicalDevice, VK_QUEUE_GRAPHICS_BIT);
    uint32_t presentFamily = findQueueFamily(physicalDevice, VK_QUEUE_GRAPHICS_BIT, vkSurface);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {graphicsFamily, presentFamily};

    float queuePriority = 1.0f;
    //    循环为每个唯一的队列家族创建配置信息
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;  // todo 先暂时填写队列大小为1，然后做可自定义设置，修改graphicsQueue为列表
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }
//    启用物理设备的功能特性
    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = queueCreateInfos.size();
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkDevice device;
    VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);

    if (result != VK_SUCCESS) {
        LOGE("Failed to create logical device: %d", result);
        return 0;
    }

    // 保存设备信息
    DeviceInfo* deviceInfo = new DeviceInfo();
    deviceInfo->device = device;
    deviceInfo->physicalDevice = physicalDevice;
    deviceInfo->graphicsQueueFamily = graphicsFamily;
    deviceInfo->presentQueueFamily = presentFamily;
    deviceInfo->surface = vkSurface;
    // 获取队列， 输入队列组索引
    vkGetDeviceQueue(device, graphicsFamily, 0, &deviceInfo->graphicsQueue);
    vkGetDeviceQueue(device, presentFamily, 0, &deviceInfo->presentQueue);

    ANativeWindow_release(window);

    LOGI("Vulkan device created successfully");
    return reinterpret_cast<jlong>(deviceInfo);
}


// 3. 创建RenderPass
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeCreateRenderPass(
        JNIEnv* env, jobject /* this */, jlong deviceHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);

    LOGI("=== Creating RenderPass ===");

    // Color attachment
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_B8G8R8A8_UNORM;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;  // 必须是 STORE！
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // 用于呈现

    LOGI("Color attachment: format=%d, samples=%d, loadOp=%d, storeOp=%d",
         colorAttachment.format, colorAttachment.samples,
         colorAttachment.loadOp, colorAttachment.storeOp);

    // Attachment reference
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    LOGI("Subpass: colorAttachments=%d", subpass.colorAttachmentCount);

    // Subpass dependency
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Create render pass
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass renderPass;
    VkResult result = vkCreateRenderPass(deviceInfo->device, &renderPassInfo, nullptr, &renderPass);

    if (result != VK_SUCCESS) {
        LOGE("Failed to create render pass: %d", result);
        return 0;
    }

    LOGI("✓ RenderPass created: %p", (void*)renderPass);
    return reinterpret_cast<jlong>(renderPass);
}

// 4. 创建Swapchain
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeCreateSwapchain(
        JNIEnv* env, jobject /* this */, jlong deviceHandle, jobject surface) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);

    // 查询surface能力
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(deviceInfo->physicalDevice, deviceInfo->surface, &capabilities);

    // 选择格式
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(deviceInfo->physicalDevice, deviceInfo->surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(deviceInfo->physicalDevice, deviceInfo->surface, &formatCount, formats.data());

    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = format;
            break;
        }
    }

    // 选择present mode
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

    // 选择extent
    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = 1920;
        extent.height = 1080;
    }
    //计算交换链中应该包含多少个图像
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = deviceInfo->surface;                    // 显示表面
    createInfo.minImageCount = imageCount;                       // 图像数量
    createInfo.imageFormat = surfaceFormat.format;               // 图像格式(RGBA等)
    createInfo.imageColorSpace = surfaceFormat.colorSpace;       // 颜色空间
    createInfo.imageExtent = extent;                             // 图像尺寸(宽高)
    createInfo.imageArrayLayers = 1;                             // 图层数(2D总是1)
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // 图像用途 作为颜色附件（可渲染）

    uint32_t queueFamilyIndices[] = {deviceInfo->graphicsQueueFamily, deviceInfo->presentQueueFamily};

    if (deviceInfo->graphicsQueueFamily != deviceInfo->presentQueueFamily) {
        //   图形队列和呈现队列是不同的族，图像可以被多个队列家族同时访问，适用于独立显卡
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        //    图形队列和呈现队列是同一个族， 图像一次只能被一个队列家族访问
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    //    显示变换和合成
    createInfo.preTransform = capabilities.currentTransform;     // 显示变换 应用显示变换（旋转、镜像等）
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Alpha合成
    createInfo.presentMode = presentMode;                        // 呈现模式 如垂直同步、立即模式等
    createInfo.clipped = VK_TRUE;                                // 裁剪
    createInfo.oldSwapchain = VK_NULL_HANDLE;                    // 旧交换链，大小缩放的时候用于交换链重建  todo

    VkSwapchainKHR swapchain;
    VkResult result = vkCreateSwapchainKHR(deviceInfo->device, &createInfo, nullptr, &swapchain);

    if (result != VK_SUCCESS) {
        LOGE("Failed to create swapchain: %d", result);
        return 0;
    }

    // 获取swapchain images
    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(deviceInfo->device, swapchain, &swapchainImageCount, nullptr);
    std::vector<VkImage> swapchainImages(swapchainImageCount);
    vkGetSwapchainImagesKHR(deviceInfo->device, swapchain, &swapchainImageCount, swapchainImages.data());

    //从images 创建image views
    std::vector<VkImageView> swapchainImageViews(swapchainImageCount);
    for (size_t i = 0; i < swapchainImageCount; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = surfaceFormat.format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        vkCreateImageView(deviceInfo->device, &viewInfo, nullptr, &swapchainImageViews[i]);
    }

    SwapchainInfo* swapchainInfo = new SwapchainInfo();
    swapchainInfo->swapchain = swapchain;
    swapchainInfo->images = swapchainImages;
    swapchainInfo->imageViews = swapchainImageViews;
    swapchainInfo->format = surfaceFormat;
    swapchainInfo->extent = extent;

    LOGI("Swapchain created successfully with %d images", swapchainImageCount);
    return reinterpret_cast<jlong>(swapchainInfo);
}

// 5. 创建CommandPool
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeCreateCommandPool(
        JNIEnv* env, jobject /* this */, jlong deviceHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = deviceInfo->graphicsQueueFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPool commandPool;
    VkResult result = vkCreateCommandPool(deviceInfo->device, &poolInfo, nullptr, &commandPool);

    if (result != VK_SUCCESS) {
        LOGE("Failed to create command pool: %d", result);
        return 0;
    }

    LOGI("Command pool created successfully");
    return reinterpret_cast<jlong>(commandPool);
}

// 6. 分配CommandBuffer
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeAllocateCommandBuffer(
        JNIEnv* env, jobject /* this */, jlong deviceHandle, jlong commandPoolHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    VkCommandPool commandPool = reinterpret_cast<VkCommandPool>(commandPoolHandle);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    VkResult result = vkAllocateCommandBuffers(deviceInfo->device, &allocInfo, &commandBuffer);

    if (result != VK_SUCCESS) {
        LOGE("Failed to allocate command buffer: %d", result);
        return 0;
    }

    return reinterpret_cast<jlong>(commandBuffer);
}

// 7. 获取下一个图像
extern "C" JNIEXPORT jint JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeAcquireNextImage(
        JNIEnv* env, jobject /* this */, jlong deviceHandle, jlong swapchainHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    SwapchainInfo* swapchainInfo = reinterpret_cast<SwapchainInfo*>(swapchainHandle);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
            deviceInfo->device,
            swapchainInfo->swapchain,
            UINT64_MAX,
            VK_NULL_HANDLE,
            VK_NULL_HANDLE,
            &imageIndex
    );

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOGE("Failed to acquire swapchain image: %d", result);
        return -1;
    }

    return imageIndex;
}

// 8. 开始CommandBuffer
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeBeginCommandBuffer(
        JNIEnv* env, jobject /* this */, jlong commandBufferHandle) {

    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(commandBufferHandle);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
}

// 9. 开始RenderPass
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeBeginRenderPass(
        JNIEnv* env, jobject /* this */,
        jlong commandBufferHandle,
        jlong renderPassHandle,
        jint imageIndex,
        jlong swapchainHandle) {

    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(commandBufferHandle);
    VkRenderPass renderPass = reinterpret_cast<VkRenderPass>(renderPassHandle);
    SwapchainInfo* swapchainInfo = reinterpret_cast<SwapchainInfo*>(swapchainHandle);

    LOGI("BeginRenderPass: imageIndex=%d, framebuffer count=%zu",
         imageIndex, swapchainInfo->framebuffers.size());

    if (imageIndex < 0 || imageIndex >= swapchainInfo->framebuffers.size()) {
        LOGE("Invalid image index: %d", imageIndex);
        return;
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapchainInfo->framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchainInfo->extent;

    // 设置清屏颜色为红色，方便看到是否有渲染
    VkClearValue clearColor = {{{1.0f, 0.0f, 0.0f, 1.0f}}};  // 红色背景
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    LOGI("✓ Render pass begun with red clear color");
    // 设置动态 viewport 和 scissor（重要！）
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchainInfo->extent.width);
    viewport.height = static_cast<float>(swapchainInfo->extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainInfo->extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    LOGI("✓ Render pass begun: %dx%d",
         swapchainInfo->extent.width, swapchainInfo->extent.height);
}

// 10. 结束RenderPass
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeEndRenderPass(
        JNIEnv* env, jobject /* this */, jlong commandBufferHandle) {

    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(commandBufferHandle);
    vkCmdEndRenderPass(commandBuffer);
}

// 11. 结束CommandBuffer
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeEndCommandBuffer(
        JNIEnv* env, jobject /* this */, jlong commandBufferHandle) {

    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(commandBufferHandle);
    vkEndCommandBuffer(commandBuffer);
}

// 12. 提交CommandBuffer
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeSubmitCommandBuffer(
        JNIEnv* env, jobject /* this */,
        jlong deviceHandle,
        jlong commandBufferHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(commandBufferHandle);

    LOGI("=== Submitting Command Buffer ===");
    LOGI("CommandBuffer: %p", (void*)commandBuffer);
    LOGI("Queue: %p", (void*)deviceInfo->graphicsQueue);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // === 关键：需要等待信号量！===
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 0;  // 暂时不用信号量测试
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;

    VkResult result = vkQueueSubmit(deviceInfo->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

    if (result != VK_SUCCESS) {
        LOGE("Failed to submit command buffer: %d", result);
        return;
    }

    LOGI("✓ Command buffer submitted");

    // 同步等待（测试用，生产环境应该用fence）
    result = vkQueueWaitIdle(deviceInfo->graphicsQueue);
    if (result != VK_SUCCESS) {
        LOGE("Failed to wait for queue idle: %d", result);
    } else {
        LOGI("✓ Queue wait idle completed");
    }
}
// 13. Present图像
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativePresentImage(
        JNIEnv* env, jobject /* this */, jlong deviceHandle, jlong swapchainHandle, jint imageIndex) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    SwapchainInfo* swapchainInfo = reinterpret_cast<SwapchainInfo*>(swapchainHandle);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchainInfo->swapchain;
    presentInfo.pImageIndices = reinterpret_cast<uint32_t*>(&imageIndex);

    vkQueuePresentKHR(deviceInfo->presentQueue, &presentInfo);
}

// 14. 获取Swapchain ImageView
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeGetSwapchainImageView(
        JNIEnv* env, jobject /* this */, jlong swapchainHandle, jint imageIndex) {

    SwapchainInfo* swapchainInfo = reinterpret_cast<SwapchainInfo*>(swapchainHandle);
    return reinterpret_cast<jlong>(swapchainInfo->imageViews[imageIndex]);
}

// 15. 调整Swapchain大小
extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeResizeSwapchain(
        JNIEnv* env, jobject thiz,
        jlong deviceHandle,
        jlong swapchainHandle,
        jlong renderPassHandle,
        jint width,
        jint height) {

    LOGI("=== nativeResizeSwapchain START ===");
    LOGI("  New size: %dx%d", width, height);

    if (width <= 0 || height <= 0) {
        LOGE("Invalid dimensions: %dx%d", width, height);
        return JNI_FALSE;
    }

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    SwapchainInfo* swapchainInfo = reinterpret_cast<SwapchainInfo*>(swapchainHandle);
    VkRenderPass renderPass = reinterpret_cast<VkRenderPass>(renderPassHandle);

    if (!deviceInfo || !swapchainInfo) {
        LOGE("Invalid handles");
        return JNI_FALSE;
    }

    // 1. 等待所有操作完成
    VkResult result = vkDeviceWaitIdle(deviceInfo->device);
    if (result != VK_SUCCESS) {
        LOGE("vkDeviceWaitIdle failed: %d", result);
        return JNI_FALSE;
    }

    // 2. 销毁旧的framebuffers
    for (auto framebuffer : swapchainInfo->framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(deviceInfo->device, framebuffer, nullptr);
        }
    }
    swapchainInfo->framebuffers.clear();

    // 3. 销毁旧的image views
    for (auto imageView : swapchainInfo->imageViews) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(deviceInfo->device, imageView, nullptr);
        }
    }
    swapchainInfo->imageViews.clear();

    // 4. 保存旧的swapchain用于重建
    VkSwapchainKHR oldSwapchain = swapchainInfo->swapchain;

    // 5. 获取surface能力
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            deviceInfo->physicalDevice,
            deviceInfo->surface,
            &surfaceCapabilities
    );

    if (result != VK_SUCCESS) {
        LOGE("Failed to get surface capabilities: %d", result);
        return JNI_FALSE;
    }

    // 6. 确定新的extent
    VkExtent2D newExtent;
    if (surfaceCapabilities.currentExtent.width != UINT32_MAX) {
        newExtent = surfaceCapabilities.currentExtent;
    } else {
        newExtent.width = std::clamp(
                static_cast<uint32_t>(width),
                surfaceCapabilities.minImageExtent.width,
                surfaceCapabilities.maxImageExtent.width
        );
        newExtent.height = std::clamp(
                static_cast<uint32_t>(height),
                surfaceCapabilities.minImageExtent.height,
                surfaceCapabilities.maxImageExtent.height
        );
    }

    LOGI("  Surface extent: %ux%u", newExtent.width, newExtent.height);

    // 7. 确定image数量
    uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0 &&
        imageCount > surfaceCapabilities.maxImageCount) {
        imageCount = surfaceCapabilities.maxImageCount;
    }

    // 8. 创建新的swapchain
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = deviceInfo->surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = swapchainInfo->format.format;
    createInfo.imageColorSpace = swapchainInfo->format.colorSpace;
    createInfo.imageExtent = newExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = surfaceCapabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // 防止撕裂
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain; // 重要:使用旧swapchain


    VkSwapchainKHR newSwapchain;
    result = vkCreateSwapchainKHR(
            deviceInfo->device,
            &createInfo,
            nullptr,
            &newSwapchain
    );

    if (result != VK_SUCCESS) {
        LOGE("Failed to create swapchain: %d", result);
        return JNI_FALSE;
    }

    // 9. 销毁旧的swapchain
    if (oldSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(deviceInfo->device, oldSwapchain, nullptr);
    }

    swapchainInfo->swapchain = newSwapchain;
    swapchainInfo->extent = newExtent;

    // 10. 获取swapchain images
    uint32_t actualImageCount;
    vkGetSwapchainImagesKHR(
            deviceInfo->device,
            newSwapchain,
            &actualImageCount,
            nullptr
    );

    std::vector<VkImage> images(actualImageCount);
    vkGetSwapchainImagesKHR(
            deviceInfo->device,
            newSwapchain,
            &actualImageCount,
            images.data()
    );

    LOGI("  Swapchain image count: %u", actualImageCount);

    // 11. 创建image views
    swapchainInfo->imageViews.resize(actualImageCount);
    for (size_t i = 0; i < actualImageCount; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainInfo->format.format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        result = vkCreateImageView(
                deviceInfo->device,
                &viewInfo,
                nullptr,
                &swapchainInfo->imageViews[i]
        );

        if (result != VK_SUCCESS) {
            LOGE("Failed to create image view %zu: %d", i, result);
            return JNI_FALSE;
        }
    }

    // 12. 创建framebuffers
    swapchainInfo->framebuffers.resize(actualImageCount);
    for (size_t i = 0; i < actualImageCount; i++) {
        VkImageView attachments[] = { swapchainInfo->imageViews[i] };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = newExtent.width;
        framebufferInfo.height = newExtent.height;
        framebufferInfo.layers = 1;

        result = vkCreateFramebuffer(
                deviceInfo->device,
                &framebufferInfo,
                nullptr,
                &swapchainInfo->framebuffers[i]
        );

        if (result != VK_SUCCESS) {
            LOGE("Failed to create framebuffer %zu: %d", i, result);
            return JNI_FALSE;
        }
    }

    LOGI("=== nativeResizeSwapchain SUCCESS ===");
    return JNI_TRUE;
}

// 清理函数
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeDestroySwapchain(
        JNIEnv* env, jobject /* this */, jlong deviceHandle, jlong swapchainHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    SwapchainInfo* swapchainInfo = reinterpret_cast<SwapchainInfo*>(swapchainHandle);

    for (auto imageView : swapchainInfo->imageViews) {
        vkDestroyImageView(deviceInfo->device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(deviceInfo->device, swapchainInfo->swapchain, nullptr);
    delete swapchainInfo;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeDestroyRenderPass(
        JNIEnv* env, jobject /* this */, jlong deviceHandle, jlong renderPassHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    VkRenderPass renderPass = reinterpret_cast<VkRenderPass>(renderPassHandle);
    vkDestroyRenderPass(deviceInfo->device, renderPass, nullptr);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeDestroyCommandPool(
        JNIEnv* env, jobject /* this */, jlong deviceHandle, jlong commandPoolHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    VkCommandPool commandPool = reinterpret_cast<VkCommandPool>(commandPoolHandle);
    vkDestroyCommandPool(deviceInfo->device, commandPool, nullptr);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeFreeCommandBuffer(
        JNIEnv* env, jobject /* this */, jlong deviceHandle, jlong commandPoolHandle, jlong commandBufferHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    VkCommandPool commandPool = reinterpret_cast<VkCommandPool>(commandPoolHandle);
    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(commandBufferHandle);

    vkFreeCommandBuffers(deviceInfo->device, commandPool, 1, &commandBuffer);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeDestroyDevice(
        JNIEnv* env, jobject /* this */, jlong deviceHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    vkDestroyDevice(deviceInfo->device, nullptr);
    delete deviceInfo;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeDestroyInstance(
        JNIEnv* env, jobject /* this */, jlong instanceHandle) {

    VkInstance instance = reinterpret_cast<VkInstance>(instanceHandle);
    vkDestroyInstance(instance, nullptr);
}


// 辅助结构体：保存纹理的完整信息
struct TextureInfo {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView imageView;
    VkSampler sampler;  // 添加这个
    uint32_t width;
    uint32_t height;
};

// 辅助函数：查找合适的内存类型
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    LOGE("Failed to find suitable memory type");
    return 0;
}

// 创建蓝色纹理
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeCreateTestTexture(
        JNIEnv* env, jobject /* this */, jlong deviceHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);

    const uint32_t width = 1920;
    const uint32_t height = 1080;
    const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
    VkCommandPool tempCommandPool = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;

    // 1. 创建图像
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result = vkCreateImage(deviceInfo->device, &imageInfo, nullptr, &image);
    if (result != VK_SUCCESS) {
        LOGE("Failed to create image: %d", result);
        return 0;
    }

    // 2. 分配内存
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(deviceInfo->device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
            deviceInfo->physicalDevice,
            memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    result = vkAllocateMemory(deviceInfo->device, &allocInfo, nullptr, &imageMemory);
    if (result != VK_SUCCESS) {
        LOGE("Failed to allocate image memory: %d", result);
    }

    vkBindImageMemory(deviceInfo->device, image, imageMemory, 0);

    // 3-9. 用作用域包裹所有临时变量
    {
        const VkDeviceSize imageSize = width * height * 4;

        // 创建staging buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = imageSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        result = vkCreateBuffer(deviceInfo->device, &bufferInfo, nullptr, &stagingBuffer);
        if (result != VK_SUCCESS) {
            LOGE("Failed to create staging buffer: %d", result);
            //todo 清理
        }

        VkMemoryRequirements bufferMemRequirements;
        vkGetBufferMemoryRequirements(deviceInfo->device, stagingBuffer, &bufferMemRequirements);

        VkMemoryAllocateInfo bufferAllocInfo{};
        bufferAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        bufferAllocInfo.allocationSize = bufferMemRequirements.size;
        bufferAllocInfo.memoryTypeIndex = findMemoryType(
                deviceInfo->physicalDevice,
                bufferMemRequirements.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        result = vkAllocateMemory(deviceInfo->device, &bufferAllocInfo, nullptr, &stagingBufferMemory);
        if (result != VK_SUCCESS) {
            LOGE("Failed to allocate staging buffer memory: %d", result);
        }

        vkBindBufferMemory(deviceInfo->device, stagingBuffer, stagingBufferMemory, 0);

        // 填充蓝色数据
        void* data;
        vkMapMemory(deviceInfo->device, stagingBufferMemory, 0, imageSize, 0, &data);

        uint8_t* pixels = static_cast<uint8_t*>(data);
        for (uint32_t i = 0; i < width * height; i++) {
            pixels[i * 4 + 0] = 122;    // R
            pixels[i * 4 + 1] = 255;    // G
            pixels[i * 4 + 2] = 0;  // B (纯蓝色)
            pixels[i * 4 + 3] = 255;  // A
        }

        vkUnmapMemory(deviceInfo->device, stagingBufferMemory);

        // 创建临时 command pool 和 buffer
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = deviceInfo->graphicsQueueFamily;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

        result = vkCreateCommandPool(deviceInfo->device, &poolInfo, nullptr, &tempCommandPool);
        if (result != VK_SUCCESS) {
            LOGE("Failed to create temp command pool: %d", result);
        }

        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandPool = tempCommandPool;
        cmdAllocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(deviceInfo->device, &cmdAllocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        // 转换图像布局 (UNDEFINED -> TRANSFER_DST_OPTIMAL)
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
        );

        // 复制buffer到image
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(
                commandBuffer,
                stagingBuffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &region
        );

        // 转换图像布局 (TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL)
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
        );

        vkEndCommandBuffer(commandBuffer);

        // 提交并等待
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        result = vkQueueSubmit(deviceInfo->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        if (result != VK_SUCCESS) {
            LOGE("Failed to submit command buffer: %d", result);
        }
        vkQueueWaitIdle(deviceInfo->graphicsQueue);

        // 清理临时资源
        vkFreeCommandBuffers(deviceInfo->device, tempCommandPool, 1, &commandBuffer);
    } // 作用域结束，自动销毁临时变量

    // 10. 创建ImageView
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    result = vkCreateImageView(deviceInfo->device, &viewInfo, nullptr, &imageView);
    if (result != VK_SUCCESS) {
        LOGE("Failed to create image view: %d", result);
    }

    // 11. 创建 Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    result = vkCreateSampler(deviceInfo->device, &samplerInfo, nullptr, &sampler);
    if (result != VK_SUCCESS) {
        LOGE("Failed to create sampler: %d", result);
        goto cleanup;
    }

    // 12. 清理staging资源
    if (stagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(deviceInfo->device, stagingBuffer, nullptr);
    }
    if (stagingBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(deviceInfo->device, stagingBufferMemory, nullptr);
    }
    if (tempCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(deviceInfo->device, tempCommandPool, nullptr);
    }

    // 13. 保存纹理信息
    {
        TextureInfo* textureInfo = new TextureInfo();
        textureInfo->image = image;
        textureInfo->memory = imageMemory;
        textureInfo->imageView = imageView;
        textureInfo->sampler = VK_NULL_HANDLE;  // 不需要 sampler
        textureInfo->width = width;
        textureInfo->height = height;

        LOGI("✓ Blue texture created: %dx%d", width, height);
        return reinterpret_cast<jlong>(textureInfo);
    }

    cleanup:
    // 清理所有可能已分配的资源
    if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(deviceInfo->device, sampler, nullptr);
    }
    if (imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(deviceInfo->device, imageView, nullptr);
    }
    if (stagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(deviceInfo->device, stagingBuffer, nullptr);
    }
    if (stagingBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(deviceInfo->device, stagingBufferMemory, nullptr);
    }
    if (tempCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(deviceInfo->device, tempCommandPool, nullptr);
    }
    if (image != VK_NULL_HANDLE) {
        vkDestroyImage(deviceInfo->device, image, nullptr);
    }
    if (imageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(deviceInfo->device, imageMemory, nullptr);
    }

    return 0;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeGetTextureSampler(
        JNIEnv* env, jobject /* this */, jlong textureHandle) {

    if (textureHandle == 0) return 0;

    TextureInfo* textureInfo = reinterpret_cast<TextureInfo*>(textureHandle);
    return reinterpret_cast<jlong>(textureInfo->sampler);
}
// 更新销毁纹理函数
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeDestroyTexture(
        JNIEnv* env, jobject /* this */, jlong deviceHandle, jlong textureHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    TextureInfo* textureInfo = reinterpret_cast<TextureInfo*>(textureHandle);

    if (textureInfo) {
        // 销毁 Sampler
//        if (textureInfo->sampler != VK_NULL_HANDLE) {
//            vkDestroySampler(deviceInfo->device, textureInfo->sampler, nullptr);
//        }
        if (textureInfo->imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(deviceInfo->device, textureInfo->imageView, nullptr);
        }
        if (textureInfo->image != VK_NULL_HANDLE) {
            vkDestroyImage(deviceInfo->device, textureInfo->image, nullptr);
        }
        if (textureInfo->memory != VK_NULL_HANDLE) {
            vkFreeMemory(deviceInfo->device, textureInfo->memory, nullptr);
        }
        delete textureInfo;
    }

    LOGI("Texture destroyed");
}


extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeGetTextureImageView(
        JNIEnv* env, jobject /* this */, jlong textureHandle) {

    TextureInfo* textureInfo = reinterpret_cast<TextureInfo*>(textureHandle);
    if (textureInfo) {
        return reinterpret_cast<jlong>(textureInfo->imageView);
    }
    return 0;
}
