//
// Created by 31483 on 2025/11/29.
//
#include "VulkanJNI.h"
#include <vector>

using namespace VulkanJNI;

// ============================================
// Descriptor Set Layout
// ============================================
extern "C" JNIEXPORT jlong JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativeCreateDescriptorSetLayout(
        JNIEnv* env, jobject /* this */, jlong deviceHandle) {

    VkDevice device = getDevice(deviceHandle);
    if (!validateHandle(device, "device")) return 0;

    // Binding 0: Combined image sampler (fragment shader)
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;

    VkDescriptorSetLayout descriptorSetLayout;
    VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);

    if (!validateResult(result, "vkCreateDescriptorSetLayout")) return 0;

    LOGI("✓ Descriptor set layout created: %p", (void*)descriptorSetLayout);
    return toHandle(descriptorSetLayout);
}

// ============================================
// Descriptor Pool
// ============================================
extern "C" JNIEXPORT jlong JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativeCreateDescriptorPool(
        JNIEnv* env, jobject /* this */, jlong deviceHandle) {

    VkDevice device = getDevice(deviceHandle);
    if (!validateHandle(device, "device")) return 0;

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    VkDescriptorPool descriptorPool;
    VkResult result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);

    if (!validateResult(result, "vkCreateDescriptorPool")) return 0;

    LOGI("✓ Descriptor pool created: %p", (void*)descriptorPool);
    return toHandle(descriptorPool);
}

// ============================================
// Descriptor Set Allocation
// ============================================
extern "C" JNIEXPORT jlong JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativeAllocateDescriptorSet(
        JNIEnv* env, jobject /* this */,
        jlong deviceHandle,
        jlong descriptorPoolHandle,
        jlong descriptorSetLayoutHandle) {

    VkDevice device = getDevice(deviceHandle);
    VkDescriptorPool descriptorPool = fromHandle<VkDescriptorPool>(descriptorPoolHandle);
    VkDescriptorSetLayout descriptorSetLayout = fromHandle<VkDescriptorSetLayout>(descriptorSetLayoutHandle);

    if (!validateHandle(device, "device") ||
        !validateHandle(descriptorPool, "descriptorPool") ||
        !validateHandle(descriptorSetLayout, "descriptorSetLayout")) {
        return 0;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorSet descriptorSet;
    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);

    if (!validateResult(result, "vkAllocateDescriptorSets")) return 0;

    LOGI("✓ Descriptor set allocated: %p", (void*)descriptorSet);
    return toHandle(descriptorSet);
}

// ============================================
// Descriptor Set Update
// ============================================
extern "C" JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativeUpdateDescriptorSet(
        JNIEnv* env, jobject /* this */,
        jlong deviceHandle,
        jlong descriptorSetHandle,
        jlong imageViewHandle,
        jlong samplerHandle) {

    VkDevice device = getDevice(deviceHandle);
    VkDescriptorSet descriptorSet = fromHandle<VkDescriptorSet>(descriptorSetHandle);
    VkImageView imageView = fromHandle<VkImageView>(imageViewHandle);
    VkSampler sampler = fromHandle<VkSampler>(samplerHandle);

    if (!validateHandle(device, "device") ||
        !validateHandle(descriptorSet, "descriptorSet") ||
        !validateHandle(imageView, "imageView") ||
        !validateHandle(sampler, "sampler")) {
        return;
    }

    LOGD("Updating descriptor set: view=%p, sampler=%p", (void*)imageView, (void*)sampler);

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

    LOGD("✓ Descriptor set updated");
}

// ============================================
// Cleanup Functions
// ============================================
extern "C" JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativeDestroyDescriptorPool(
        JNIEnv* env, jobject /* this */, jlong deviceHandle, jlong descriptorPoolHandle) {

    VkDevice device = getDevice(deviceHandle);
    VkDescriptorPool descriptorPool = fromHandle<VkDescriptorPool>(descriptorPoolHandle);

    if (validateHandle(device, "device") && validateHandle(descriptorPool, "descriptorPool")) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        LOGD("✓ Descriptor pool destroyed");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativeDestroyDescriptorSetLayout(
        JNIEnv* env, jobject /* this */, jlong deviceHandle, jlong descriptorSetLayoutHandle) {

    VkDevice device = getDevice(deviceHandle);
    VkDescriptorSetLayout descriptorSetLayout = fromHandle<VkDescriptorSetLayout>(descriptorSetLayoutHandle);

    if (validateHandle(device, "device") && validateHandle(descriptorSetLayout, "descriptorSetLayout")) {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        LOGD("✓ Descriptor set layout destroyed");
    }
}