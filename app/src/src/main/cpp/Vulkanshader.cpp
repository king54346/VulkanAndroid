//
// Created by 31483 on 2025/11/29.
//
#include "VulkanJNI.h"
#include <vector>
#include <cstring>

using namespace VulkanJNI;

// ============================================
// Shader Module Creation
// ============================================
namespace {

    bool validateSPIRV(const std::vector<uint32_t>& code) {
        if (code.empty()) {
            LOGE("Shader code is empty");
            return false;
        }

        if (code.size() * sizeof(uint32_t) < 16) {
            LOGE("Shader code too small: %zu bytes", code.size() * sizeof(uint32_t));
            return false;
        }

        // Validate SPIR-V magic number
        const uint32_t SPIRV_MAGIC = 0x07230203;
        if (code[0] != SPIRV_MAGIC) {
            LOGE("Invalid SPIR-V magic: 0x%08x (expected 0x%08x)", code[0], SPIRV_MAGIC);
            return false;
        }

        LOGD("SPIR-V magic verified: 0x%08x", code[0]);
        return true;
    }

} // anonymous namespace

extern "C" JNIEXPORT jlong JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativeCreateShaderModule(
        JNIEnv* env, jobject /* this */, jlong deviceHandle, jbyteArray codeArray) {

    LOGI("=== Creating Shader Module ===");

    VkDevice device = getDevice(deviceHandle);
    if (!validateHandle(device, "device")) return 0;

    if (codeArray == nullptr) {
        LOGE("Code array is null!");
        return 0;
    }

    jsize codeSize = env->GetArrayLength(codeArray);
    LOGI("Shader code size: %d bytes", codeSize);

    if (codeSize % 4 != 0) {
        LOGE("Shader code size must be multiple of 4, got %d", codeSize);
        return 0;
    }

    // Create aligned buffer for SPIR-V code
    std::vector<uint32_t> alignedCode(codeSize / 4);

    jbyte* code = env->GetByteArrayElements(codeArray, nullptr);
    if (code == nullptr) {
        LOGE("Failed to get byte array elements");
        return 0;
    }

    // Copy to aligned buffer
    std::memcpy(alignedCode.data(), code, codeSize);
    env->ReleaseByteArrayElements(codeArray, code, JNI_ABORT);

    // Validate SPIR-V
    if (!validateSPIRV(alignedCode)) {
        return 0;
    }

    // Create shader module
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = codeSize;
    createInfo.pCode = alignedCode.data();

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);

    if (!validateResult(result, "vkCreateShaderModule")) return 0;

    LOGI("✓ Shader module created: %p", (void*)shaderModule);
    return toHandle(shaderModule);
}

// ============================================
// Sampler Creation
// ============================================
extern "C" JNIEXPORT jlong JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativeCreateSampler(
        JNIEnv* env, jobject /* this */, jlong deviceHandle) {

    VkDevice device = getDevice(deviceHandle);
    if (!validateHandle(device, "device")) return 0;

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

    VkSampler sampler;
    VkResult result = vkCreateSampler(device, &samplerInfo, nullptr, &sampler);

    if (!validateResult(result, "vkCreateSampler")) return 0;

    LOGI("✓ Sampler created: %p", (void*)sampler);
    return toHandle(sampler);
}

// ============================================
// Cleanup Functions
// ============================================
extern "C" JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativeDestroyShaderModule(
        JNIEnv* env, jobject /* this */, jlong deviceHandle, jlong shaderModuleHandle) {

    VkDevice device = getDevice(deviceHandle);
    VkShaderModule shaderModule = fromHandle<VkShaderModule>(shaderModuleHandle);

    if (validateHandle(device, "device") && validateHandle(shaderModule, "shaderModule")) {
        vkDestroyShaderModule(device, shaderModule, nullptr);
        LOGD("✓ Shader module destroyed");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativeDestroySampler(
        JNIEnv* env, jobject /* this */, jlong deviceHandle, jlong samplerHandle) {

    VkDevice device = getDevice(deviceHandle);
    VkSampler sampler = fromHandle<VkSampler>(samplerHandle);

    if (validateHandle(device, "device") && validateHandle(sampler, "sampler")) {
        vkDestroySampler(device, sampler, nullptr);
        LOGD("✓ Sampler destroyed");
    }
}