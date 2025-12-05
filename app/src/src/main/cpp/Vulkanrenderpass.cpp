//
// Created by 31483 on 2025/11/28.
//
#include <jni.h>
#include <android/log.h>
#include <vulkan/vulkan.h>
#include "VulkanTypes.h"

#define LOG_TAG "VulkanRenderPass"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// 创建RenderPass
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeCreateRenderPass(
        JNIEnv* env, jobject thiz, jlong deviceHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    if (!deviceInfo) {
        LOGE("Invalid device handle");
        return 0;
    }

    LOGI("Creating RenderPass");

    // Color attachment描述
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_B8G8R8A8_UNORM;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Attachment引用
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    // Subpass依赖
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // 创建RenderPass
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

    LOGI("RenderPass created successfully");
    return reinterpret_cast<jlong>(renderPass);
}

// 创建Framebuffers
extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeCreateFramebuffers(
        JNIEnv* env, jobject thiz,
        jlong deviceHandle,
        jlong swapchainHandle,
        jlong renderPass) {

    if (deviceHandle == 0 || swapchainHandle == 0 || renderPass == 0) {
        LOGE("Invalid handles");
        return JNI_FALSE;
    }

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    SwapchainInfo* swapchainInfo = reinterpret_cast<SwapchainInfo*>(swapchainHandle);
    VkRenderPass vkRenderPass = reinterpret_cast<VkRenderPass>(renderPass);

    LOGI("Creating %zu framebuffers", swapchainInfo->imageViews.size());

    swapchainInfo->framebuffers.resize(swapchainInfo->imageViews.size());

    for (size_t i = 0; i < swapchainInfo->imageViews.size(); i++) {
        VkImageView attachments[] = {swapchainInfo->imageViews[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = vkRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainInfo->extent.width;
        framebufferInfo.height = swapchainInfo->extent.height;
        framebufferInfo.layers = 1;

        VkResult result = vkCreateFramebuffer(deviceInfo->device, &framebufferInfo, nullptr,
                                              &swapchainInfo->framebuffers[i]);

        if (result != VK_SUCCESS) {
            LOGE("Failed to create framebuffer %zu: %d", i, result);

            // 清理已创建的framebuffers
            for (size_t j = 0; j < i; j++) {
                vkDestroyFramebuffer(deviceInfo->device, swapchainInfo->framebuffers[j], nullptr);
            }
            swapchainInfo->framebuffers.clear();
            return JNI_FALSE;
        }
    }

    LOGI("All %zu framebuffers created successfully", swapchainInfo->framebuffers.size());
    return JNI_TRUE;
}

// 销毁RenderPass
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeDestroyRenderPass(
        JNIEnv* env, jobject thiz, jlong deviceHandle, jlong renderPassHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    VkRenderPass renderPass = reinterpret_cast<VkRenderPass>(renderPassHandle);

    if (deviceInfo && renderPass) {
        vkDestroyRenderPass(deviceInfo->device, renderPass, nullptr);
    }
}