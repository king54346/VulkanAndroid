//
// Created by 31483 on 2025/11/28.
//
#include <jni.h>
#include <android/log.h>
#include <vulkan/vulkan.h>
#include <algorithm>
#include "VulkanTypes.h"

#define LOG_TAG "VulkanSwapchain"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// 外部函数声明
extern VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>&);
extern uint32_t getSwapchainImageCount(const VkSurfaceCapabilitiesKHR&);
extern VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR&, uint32_t, uint32_t);

// 创建ImageViews
static bool createImageViews(DeviceInfo* deviceInfo, SwapchainInfo* swapchainInfo,
                             const std::vector<VkImage>& images) {
    swapchainInfo->imageViews.resize(images.size());

    for (size_t i = 0; i < images.size(); i++) {
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

        VkResult result = vkCreateImageView(deviceInfo->device, &viewInfo, nullptr,
                                            &swapchainInfo->imageViews[i]);
        if (result != VK_SUCCESS) {
            LOGE("Failed to create image view %zu: %d", i, result);

            // 清理已创建的image views
            for (size_t j = 0; j < i; j++) {
                vkDestroyImageView(deviceInfo->device, swapchainInfo->imageViews[j], nullptr);
            }
            swapchainInfo->imageViews.clear();
            return false;
        }
    }
    return true;
}

// 创建Swapchain
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeCreateSwapchain(
        JNIEnv* env, jobject thiz, jlong deviceHandle, jobject surface) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    if (!deviceInfo) {
        LOGE("Invalid device handle");
        return 0;
    }

    // 查询surface能力
    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            deviceInfo->physicalDevice, deviceInfo->surface, &capabilities);
    if (result != VK_SUCCESS) {
        LOGE("Failed to get surface capabilities: %d", result);
        return 0;
    }

    // 查询格式
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(deviceInfo->physicalDevice, deviceInfo->surface,
                                         &formatCount, nullptr);
    if (formatCount == 0) {
        LOGE("No surface formats available");
        return 0;
    }

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(deviceInfo->physicalDevice, deviceInfo->surface,
                                         &formatCount, formats.data());

    // 选择格式和配置
    VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(formats);
    uint32_t imageCount = getSwapchainImageCount(capabilities);
    VkExtent2D extent = chooseSwapExtent(capabilities,1920,1080);

    // 创建swapchain
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = deviceInfo->surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // 队列族配置
    uint32_t queueFamilyIndices[] = {
            deviceInfo->graphicsQueueFamily,
            deviceInfo->presentQueueFamily
    };

    if (deviceInfo->graphicsQueueFamily != deviceInfo->presentQueueFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain;
    result = vkCreateSwapchainKHR(deviceInfo->device, &createInfo, nullptr, &swapchain);

    if (result != VK_SUCCESS) {
        LOGE("Failed to create swapchain: %d", result);
        return 0;
    }

    // 获取swapchain images
    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(deviceInfo->device, swapchain, &swapchainImageCount, nullptr);
    std::vector<VkImage> swapchainImages(swapchainImageCount);
    vkGetSwapchainImagesKHR(deviceInfo->device, swapchain, &swapchainImageCount,
                            swapchainImages.data());

    // 保存swapchain信息
    SwapchainInfo* swapchainInfo = new SwapchainInfo();
    swapchainInfo->swapchain = swapchain;
    swapchainInfo->images = swapchainImages;
    swapchainInfo->format = surfaceFormat;
    swapchainInfo->extent = extent;

    // 创建image views
    if (!createImageViews(deviceInfo, swapchainInfo, swapchainImages)) {
        vkDestroySwapchainKHR(deviceInfo->device, swapchain, nullptr);
        delete swapchainInfo;
        return 0;
    }

    LOGI("Swapchain created: %d images, %ux%u", swapchainImageCount,
         extent.width, extent.height);
    return reinterpret_cast<jlong>(swapchainInfo);
}

// 调整Swapchain大小
extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeResizeSwapchain(
        JNIEnv* env, jobject thiz,
        jlong deviceHandle,
        jlong swapchainHandle,
        jlong renderPassHandle,
        jint width,
        jint height) {

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

    LOGI("Resizing swapchain to %dx%d", width, height);

    // 等待设备空闲
    vkDeviceWaitIdle(deviceInfo->device);

    // 清理旧的framebuffers和image views
    for (auto framebuffer : swapchainInfo->framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(deviceInfo->device, framebuffer, nullptr);
        }
    }
    swapchainInfo->framebuffers.clear();

    for (auto imageView : swapchainInfo->imageViews) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(deviceInfo->device, imageView, nullptr);
        }
    }
    swapchainInfo->imageViews.clear();

    // 获取surface能力
    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            deviceInfo->physicalDevice, deviceInfo->surface, &capabilities);
    if (result != VK_SUCCESS) {
        LOGE("Failed to get surface capabilities: %d", result);
        return JNI_FALSE;
    }

    // 确定新的extent
    VkExtent2D newExtent = chooseSwapExtent(capabilities, width, height);
    uint32_t imageCount = getSwapchainImageCount(capabilities);

    // 创建新的swapchain
    VkSwapchainKHR oldSwapchain = swapchainInfo->swapchain;

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
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    VkSwapchainKHR newSwapchain;
    result = vkCreateSwapchainKHR(deviceInfo->device, &createInfo, nullptr, &newSwapchain);
    if (result != VK_SUCCESS) {
        LOGE("Failed to create swapchain: %d", result);
        return JNI_FALSE;
    }

    // 销毁旧的swapchain
    if (oldSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(deviceInfo->device, oldSwapchain, nullptr);
    }

    swapchainInfo->swapchain = newSwapchain;
    swapchainInfo->extent = newExtent;

    // 获取新的images
    uint32_t actualImageCount;
    vkGetSwapchainImagesKHR(deviceInfo->device, newSwapchain, &actualImageCount, nullptr);
    std::vector<VkImage> images(actualImageCount);
    vkGetSwapchainImagesKHR(deviceInfo->device, newSwapchain, &actualImageCount, images.data());

    // 创建image views
    if (!createImageViews(deviceInfo, swapchainInfo, images)) {
        return JNI_FALSE;
    }

    // 创建framebuffers
    swapchainInfo->framebuffers.resize(actualImageCount);
    for (size_t i = 0; i < actualImageCount; i++) {
        VkImageView attachments[] = {swapchainInfo->imageViews[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = newExtent.width;
        framebufferInfo.height = newExtent.height;
        framebufferInfo.layers = 1;

        result = vkCreateFramebuffer(deviceInfo->device, &framebufferInfo, nullptr,
                                     &swapchainInfo->framebuffers[i]);
        if (result != VK_SUCCESS) {
            LOGE("Failed to create framebuffer %zu: %d", i, result);
            return JNI_FALSE;
        }
    }

    LOGI("Swapchain resized successfully to %ux%u", newExtent.width, newExtent.height);
    return JNI_TRUE;
}

// 获取下一个图像
extern "C" JNIEXPORT jint JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeAcquireNextImage(
        JNIEnv* env, jobject thiz, jlong deviceHandle, jlong swapchainHandle) {

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

// Present图像
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativePresentImage(
        JNIEnv* env, jobject thiz, jlong deviceHandle, jlong swapchainHandle, jint imageIndex) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    SwapchainInfo* swapchainInfo = reinterpret_cast<SwapchainInfo*>(swapchainHandle);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchainInfo->swapchain;
    presentInfo.pImageIndices = reinterpret_cast<uint32_t*>(&imageIndex);

    vkQueuePresentKHR(deviceInfo->presentQueue, &presentInfo);
}

// 获取ImageView
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeGetSwapchainImageView(
        JNIEnv* env, jobject thiz, jlong swapchainHandle, jint imageIndex) {

    SwapchainInfo* swapchainInfo = reinterpret_cast<SwapchainInfo*>(swapchainHandle);
    if (swapchainInfo && imageIndex >= 0 &&
        imageIndex < static_cast<jint>(swapchainInfo->imageViews.size())) {
        return reinterpret_cast<jlong>(swapchainInfo->imageViews[imageIndex]);
    }
    return 0;
}

// 销毁Swapchain
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeDestroySwapchain(
        JNIEnv* env, jobject thiz, jlong deviceHandle, jlong swapchainHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    SwapchainInfo* swapchainInfo = reinterpret_cast<SwapchainInfo*>(swapchainHandle);

    if (deviceInfo && swapchainInfo) {
        for (auto imageView : swapchainInfo->imageViews) {
            vkDestroyImageView(deviceInfo->device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(deviceInfo->device, swapchainInfo->swapchain, nullptr);
        delete swapchainInfo;
    }
}


// ========== 新增：获取交换链图像数量 ==========
extern "C" JNIEXPORT jint JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeGetSwapchainImageCount(
        JNIEnv* env, jobject /* this */, jlong swapchainHandle) {

    SwapchainInfo* swapchainInfo = reinterpret_cast<SwapchainInfo*>(swapchainHandle);
    return static_cast<jint>(swapchainInfo->images.size());
}

// ========== 新增：批量分配命令缓冲区 ==========
extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeAllocateCommandBuffers(
        JNIEnv* env, jobject /* this */,
        jlong deviceHandle,
        jlong commandPoolHandle,
        jint count,
        jlongArray commandBuffersArray) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    VkCommandPool commandPool = reinterpret_cast<VkCommandPool>(commandPoolHandle);

    std::vector<VkCommandBuffer> commandBuffers(count);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(count);

    VkResult result = vkAllocateCommandBuffers(deviceInfo->device, &allocInfo, commandBuffers.data());

    if (result != VK_SUCCESS) {
        LOGE("Failed to allocate command buffers: %d", result);
        return JNI_FALSE;
    }

    // 转换为 jlong 数组
    jlong* buffers = env->GetLongArrayElements(commandBuffersArray, nullptr);
    for (int i = 0; i < count; i++) {
        buffers[i] = reinterpret_cast<jlong>(commandBuffers[i]);
    }
    env->ReleaseLongArrayElements(commandBuffersArray, buffers, 0);

    LOGI("✓ Allocated %d command buffers", count);
    return JNI_TRUE;
}

// ========== 新增：批量释放命令缓冲区 ==========
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeFreeCommandBuffers(
        JNIEnv* env, jobject /* this */,
        jlong deviceHandle,
        jlong commandPoolHandle,
        jlongArray commandBuffersArray) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    VkCommandPool commandPool = reinterpret_cast<VkCommandPool>(commandPoolHandle);

    jsize count = env->GetArrayLength(commandBuffersArray);
    jlong* buffers = env->GetLongArrayElements(commandBuffersArray, nullptr);

    std::vector<VkCommandBuffer> commandBuffers(count);
    for (int i = 0; i < count; i++) {
        commandBuffers[i] = reinterpret_cast<VkCommandBuffer>(buffers[i]);
    }

    vkFreeCommandBuffers(deviceInfo->device, commandPool, count, commandBuffers.data());

    env->ReleaseLongArrayElements(commandBuffersArray, buffers, JNI_ABORT);

    LOGI("✓ Freed %d command buffers", count);
}

// ========== 新增：重置命令缓冲区 ==========
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeResetCommandBuffer(
        JNIEnv* env, jobject /* this */, jlong commandBufferHandle) {

    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(commandBufferHandle);
    vkResetCommandBuffer(commandBuffer, 0);
}

// ========== 新增：创建同步对象 ==========
extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeCreateSyncObjects(
        JNIEnv* env, jobject /* this */,
        jlong deviceHandle,
        jint count,
        jlongArray imageAvailableSemaphoresArray,
        jlongArray renderFinishedSemaphoresArray,
        jlongArray inFlightFencesArray) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);

    jlong* imageAvailableSems = env->GetLongArrayElements(imageAvailableSemaphoresArray, nullptr);
    jlong* renderFinishedSems = env->GetLongArrayElements(renderFinishedSemaphoresArray, nullptr);
    jlong* fences = env->GetLongArrayElements(inFlightFencesArray, nullptr);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // 初始为已触发状态

    for (int i = 0; i < count; i++) {
        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkFence fence;

        if (vkCreateSemaphore(deviceInfo->device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(deviceInfo->device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
            vkCreateFence(deviceInfo->device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {

            LOGE("Failed to create sync objects for frame %d", i);

            // 清理已创建的对象
            for (int j = 0; j < i; j++) {
                vkDestroySemaphore(deviceInfo->device, reinterpret_cast<VkSemaphore>(imageAvailableSems[j]), nullptr);
                vkDestroySemaphore(deviceInfo->device, reinterpret_cast<VkSemaphore>(renderFinishedSems[j]), nullptr);
                vkDestroyFence(deviceInfo->device, reinterpret_cast<VkFence>(fences[j]), nullptr);
            }

            env->ReleaseLongArrayElements(imageAvailableSemaphoresArray, imageAvailableSems, JNI_ABORT);
            env->ReleaseLongArrayElements(renderFinishedSemaphoresArray, renderFinishedSems, JNI_ABORT);
            env->ReleaseLongArrayElements(inFlightFencesArray, fences, JNI_ABORT);

            return JNI_FALSE;
        }

        imageAvailableSems[i] = reinterpret_cast<jlong>(imageAvailableSemaphore);
        renderFinishedSems[i] = reinterpret_cast<jlong>(renderFinishedSemaphore);
        fences[i] = reinterpret_cast<jlong>(fence);
    }

    env->ReleaseLongArrayElements(imageAvailableSemaphoresArray, imageAvailableSems, 0);
    env->ReleaseLongArrayElements(renderFinishedSemaphoresArray, renderFinishedSems, 0);
    env->ReleaseLongArrayElements(inFlightFencesArray, fences, 0);

    LOGI("✓ Created %d sets of sync objects", count);
    return JNI_TRUE;
}

// ========== 新增：销毁同步对象 ==========
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeDestroySyncObjects(
        JNIEnv* env, jobject /* this */,
        jlong deviceHandle,
        jlongArray imageAvailableSemaphoresArray,
        jlongArray renderFinishedSemaphoresArray,
        jlongArray inFlightFencesArray) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);

    jsize count = env->GetArrayLength(imageAvailableSemaphoresArray);
    jlong* imageAvailableSems = env->GetLongArrayElements(imageAvailableSemaphoresArray, nullptr);
    jlong* renderFinishedSems = env->GetLongArrayElements(renderFinishedSemaphoresArray, nullptr);
    jlong* fences = env->GetLongArrayElements(inFlightFencesArray, nullptr);

    for (int i = 0; i < count; i++) {
        vkDestroySemaphore(deviceInfo->device, reinterpret_cast<VkSemaphore>(imageAvailableSems[i]), nullptr);
        vkDestroySemaphore(deviceInfo->device, reinterpret_cast<VkSemaphore>(renderFinishedSems[i]), nullptr);
        vkDestroyFence(deviceInfo->device, reinterpret_cast<VkFence>(fences[i]), nullptr);
    }

    env->ReleaseLongArrayElements(imageAvailableSemaphoresArray, imageAvailableSems, JNI_ABORT);
    env->ReleaseLongArrayElements(renderFinishedSemaphoresArray, renderFinishedSems, JNI_ABORT);
    env->ReleaseLongArrayElements(inFlightFencesArray, fences, JNI_ABORT);

    LOGI("✓ Destroyed %d sets of sync objects", count);
}

// ========== 新增：等待 Fence ==========
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeWaitForFence(
        JNIEnv* env, jobject /* this */,
        jlong deviceHandle,
        jlong fenceHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    VkFence fence = reinterpret_cast<VkFence>(fenceHandle);

    vkWaitForFences(deviceInfo->device, 1, &fence, VK_TRUE, UINT64_MAX);
}

// ========== 新增：重置 Fence ==========
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeResetFence(
        JNIEnv* env, jobject /* this */,
        jlong deviceHandle,
        jlong fenceHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    VkFence fence = reinterpret_cast<VkFence>(fenceHandle);

    vkResetFences(deviceInfo->device, 1, &fence);
}

// ========== 新增：等待所有 Fences ==========
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeWaitForAllFences(
        JNIEnv* env, jobject /* this */,
        jlong deviceHandle,
        jlongArray fencesArray) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);

    jsize count = env->GetArrayLength(fencesArray);
    jlong* fencesLong = env->GetLongArrayElements(fencesArray, nullptr);

    std::vector<VkFence> fences(count);
    for (int i = 0; i < count; i++) {
        fences[i] = reinterpret_cast<VkFence>(fencesLong[i]);
    }

    vkWaitForFences(deviceInfo->device, count, fences.data(), VK_TRUE, UINT64_MAX);

    env->ReleaseLongArrayElements(fencesArray, fencesLong, JNI_ABORT);
}

// ========== 新增：带信号量的获取图像 ==========
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeAcquireNextImageWithSemaphore(
        JNIEnv* env, jobject /* this */,
        jlong deviceHandle,
        jlong swapchainHandle,
        jlong semaphoreHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    SwapchainInfo* swapchainInfo = reinterpret_cast<SwapchainInfo*>(swapchainHandle);
    VkSemaphore semaphore = reinterpret_cast<VkSemaphore>(semaphoreHandle);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
            deviceInfo->device,
            swapchainInfo->swapchain,
            UINT64_MAX,
            semaphore,  // 🔥 使用信号量
            VK_NULL_HANDLE,
            &imageIndex
    );

    // 返回 (resultCode << 32) | imageIndex
    jlong returnValue = (static_cast<jlong>(result) << 32) | static_cast<jlong>(imageIndex);
    return returnValue;
}

// ========== 新增：带同步的提交命令缓冲区 ==========
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeSubmitCommandBufferWithSync(
        JNIEnv* env, jobject /* this */,
        jlong deviceHandle,
        jlong commandBufferHandle,
        jlong waitSemaphoreHandle,
        jlong signalSemaphoreHandle,
        jlong fenceHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(commandBufferHandle);
    VkSemaphore waitSemaphore = reinterpret_cast<VkSemaphore>(waitSemaphoreHandle);
    VkSemaphore signalSemaphore = reinterpret_cast<VkSemaphore>(signalSemaphoreHandle);
    VkFence fence = reinterpret_cast<VkFence>(fenceHandle);

    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &waitSemaphore;  // 🔥 等待图像可用
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &signalSemaphore;  // 🔥 通知渲染完成

    VkResult result = vkQueueSubmit(deviceInfo->graphicsQueue, 1, &submitInfo, fence);  // 🔥 使用 fence

    if (result != VK_SUCCESS) {
        LOGE("Failed to submit command buffer with sync: %d", result);
    }

    // 🔥 关键：不再等待 Queue Idle！
    // vkQueueWaitIdle(deviceInfo->graphicsQueue);  // 删除这行
}

// ========== 新增：带同步的 Present ==========
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativePresentImageWithSync(
        JNIEnv* env, jobject /* this */,
        jlong deviceHandle,
        jlong swapchainHandle,
        jint imageIndex,
        jlong waitSemaphoreHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    SwapchainInfo* swapchainInfo = reinterpret_cast<SwapchainInfo*>(swapchainHandle);
    VkSemaphore waitSemaphore = reinterpret_cast<VkSemaphore>(waitSemaphoreHandle);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &waitSemaphore;  // 🔥 等待渲染完成
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchainInfo->swapchain;
    presentInfo.pImageIndices = reinterpret_cast<uint32_t*>(&imageIndex);

    VkResult result = vkQueuePresentKHR(deviceInfo->presentQueue, &presentInfo);

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOGE("Failed to present image with sync: %d", result);
    }
}

// ========== 新增：等待设备空闲 ==========
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeDeviceWaitIdle(
        JNIEnv* env, jobject /* this */, jlong deviceHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    vkDeviceWaitIdle(deviceInfo->device);
}
