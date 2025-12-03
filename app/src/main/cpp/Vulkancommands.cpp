//
// Created by 31483 on 2025/11/29.
//
#include "VulkanJNI.h"

using namespace VulkanJNI;

// ============================================
// Pipeline Binding
// ============================================
extern "C" JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativeBindPipeline(
        JNIEnv* env, jobject /* this */,
        jlong commandBufferHandle,
        jlong pipelineHandle) {

    VkCommandBuffer commandBuffer = fromHandle<VkCommandBuffer>(commandBufferHandle);
    VkPipeline pipeline = fromHandle<VkPipeline>(pipelineHandle);

    if (!validateHandle(commandBuffer, "commandBuffer") || !validateHandle(pipeline, "pipeline")) {
        return;
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    LOGD("✓ Pipeline bound");
}

// ============================================
// Descriptor Sets Binding
// ============================================
extern "C" JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativeBindDescriptorSets(
        JNIEnv* env, jobject /* this */,
        jlong commandBufferHandle,
        jlong pipelineLayoutHandle,
        jlong descriptorSetHandle) {

    VkCommandBuffer commandBuffer = fromHandle<VkCommandBuffer>(commandBufferHandle);
    VkPipelineLayout pipelineLayout = fromHandle<VkPipelineLayout>(pipelineLayoutHandle);
    VkDescriptorSet descriptorSet = fromHandle<VkDescriptorSet>(descriptorSetHandle);

    if (!validateHandle(commandBuffer, "commandBuffer") ||
        !validateHandle(pipelineLayout, "pipelineLayout") ||
        !validateHandle(descriptorSet, "descriptorSet")) {
        return;
    }

    vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0,  // firstSet
            1,  // descriptorSetCount
            &descriptorSet,
            0,  // dynamicOffsetCount
            nullptr
    );

    LOGD("✓ Descriptor sets bound");
}

// ============================================
// Push Constants
// ============================================
extern "C" JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativePushConstants(
        JNIEnv* env, jobject /* this */,
        jlong commandBufferHandle,
        jlong pipelineLayoutHandle,
        jfloatArray dataArray) {

    VkCommandBuffer commandBuffer = fromHandle<VkCommandBuffer>(commandBufferHandle);
    VkPipelineLayout pipelineLayout = fromHandle<VkPipelineLayout>(pipelineLayoutHandle);

    if (!validateHandle(commandBuffer, "commandBuffer") || !validateHandle(pipelineLayout, "pipelineLayout")) {
        return;
    }

    if (dataArray == nullptr) {
        LOGE("Push constants data array is null");
        return;
    }

    jsize dataSize = env->GetArrayLength(dataArray);
    if (dataSize != 4) {
        LOGE("Invalid push constants data size: %d (expected 4)", dataSize);
        return;
    }

    jfloat* data = env->GetFloatArrayElements(dataArray, nullptr);
    if (data == nullptr) {
        LOGE("Failed to get float array elements");
        return;
    }

    // Push constants to GPU
    vkCmdPushConstants(
            commandBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,  // offset
            dataSize * sizeof(float),
            data
    );

    // Periodic logging for debugging
    static int logCounter = 0;
    if (logCounter++ % 100 == 0) {
        LOGD("Push constants: res=(%.0f, %.0f), time=%.2f", data[0], data[1], data[2]);
    }

    env->ReleaseFloatArrayElements(dataArray, data, JNI_ABORT);
}

// ============================================
// Draw Command
// ============================================
extern "C" JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativeDraw(
        JNIEnv* env, jobject /* this */,
        jlong commandBufferHandle,
        jint vertexCount,
        jint instanceCount,
        jint firstVertex,
        jint firstInstance) {

    VkCommandBuffer commandBuffer = fromHandle<VkCommandBuffer>(commandBufferHandle);

    if (!validateHandle(commandBuffer, "commandBuffer")) {
        return;
    }

    vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}