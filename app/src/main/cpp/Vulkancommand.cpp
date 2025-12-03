//
// Created by 31483 on 2025/11/28.
//
#include <jni.h>
#include <android/log.h>
#include <vulkan/vulkan.h>
#include "VulkanTypes.h"

#define LOG_TAG "VulkanCommand"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// 创建CommandPool
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeCreateCommandPool(
        JNIEnv* env, jobject thiz, jlong deviceHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    if (!deviceInfo) {
        LOGE("Invalid device handle");
        return 0;
    }

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

// 分配CommandBuffer
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeAllocateCommandBuffer(
        JNIEnv* env, jobject thiz, jlong deviceHandle, jlong commandPoolHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    VkCommandPool commandPool = reinterpret_cast<VkCommandPool>(commandPoolHandle);

    if (!deviceInfo || !commandPool) {
        LOGE("Invalid handles");
        return 0;
    }

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

// 开始CommandBuffer
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeBeginCommandBuffer(
        JNIEnv* env, jobject thiz, jlong commandBufferHandle) {

    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(commandBufferHandle);
    if (!commandBuffer) {
        LOGE("Invalid command buffer handle");
        return;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
}

// 开始RenderPass
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeBeginRenderPass(
        JNIEnv* env, jobject thiz,
        jlong commandBufferHandle,
        jlong renderPassHandle,
        jint imageIndex,
        jlong swapchainHandle) {

    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(commandBufferHandle);
    VkRenderPass renderPass = reinterpret_cast<VkRenderPass>(renderPassHandle);
    SwapchainInfo* swapchainInfo = reinterpret_cast<SwapchainInfo*>(swapchainHandle);

    if (!commandBuffer || !renderPass || !swapchainInfo) {
        LOGE("Invalid handles");
        return;
    }

    if (imageIndex < 0 || imageIndex >= static_cast<jint>(swapchainInfo->framebuffers.size())) {
        LOGE("Invalid image index: %d", imageIndex);
        return;
    }

    // RenderPass配置
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapchainInfo->framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchainInfo->extent;

    // 设置清屏颜色（红色用于调试）
    VkClearValue clearColor = {{{1.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // 设置动态viewport和scissor
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

    LOGI("Render pass begun: %ux%u", swapchainInfo->extent.width, swapchainInfo->extent.height);
}

// 结束RenderPass
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeEndRenderPass(
        JNIEnv* env, jobject thiz, jlong commandBufferHandle) {

    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(commandBufferHandle);
    if (commandBuffer) {
        vkCmdEndRenderPass(commandBuffer);
    }
}

// 结束CommandBuffer
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeEndCommandBuffer(
        JNIEnv* env, jobject thiz, jlong commandBufferHandle) {

    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(commandBufferHandle);
    if (commandBuffer) {
        vkEndCommandBuffer(commandBuffer);
    }
}

// 提交CommandBuffer
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeSubmitCommandBuffer(
        JNIEnv* env, jobject thiz,
        jlong deviceHandle,
        jlong commandBufferHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(commandBufferHandle);

    if (!deviceInfo || !commandBuffer) {
        LOGE("Invalid handles");
        return;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkResult result = vkQueueSubmit(deviceInfo->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

    if (result != VK_SUCCESS) {
        LOGE("Failed to submit command buffer: %d", result);
        return;
    }

    // 等待队列完成（生产环境应该使用fence）
    result = vkQueueWaitIdle(deviceInfo->graphicsQueue);
    if (result != VK_SUCCESS) {
        LOGE("Failed to wait for queue idle: %d", result);
    }
}

// 释放CommandBuffer
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeFreeCommandBuffer(
        JNIEnv* env, jobject thiz, jlong deviceHandle, jlong commandPoolHandle,
        jlong commandBufferHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    VkCommandPool commandPool = reinterpret_cast<VkCommandPool>(commandPoolHandle);
    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(commandBufferHandle);

    if (deviceInfo && commandPool && commandBuffer) {
        vkFreeCommandBuffers(deviceInfo->device, commandPool, 1, &commandBuffer);
    }
}

// 销毁CommandPool
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeDestroyCommandPool(
        JNIEnv* env, jobject thiz, jlong deviceHandle, jlong commandPoolHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    VkCommandPool commandPool = reinterpret_cast<VkCommandPool>(commandPoolHandle);

    if (deviceInfo && commandPool) {
        vkDestroyCommandPool(deviceInfo->device, commandPool, nullptr);
    }
}