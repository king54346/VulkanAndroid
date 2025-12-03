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