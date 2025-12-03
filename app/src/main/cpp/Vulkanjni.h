#ifndef VULKAN_JNI_H
#define VULKAN_JNI_H

#include <jni.h>
#include <vulkan/vulkan.h>
#include <android/log.h>
#include "Vulkantypes.h"

// ============================================
// Logging Utilities
// ============================================
#define LOG_TAG "VulkanRenderer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// ============================================
// Type Conversion Helpers
// ============================================
namespace VulkanJNI {

    template<typename T>
    inline T fromHandle(jlong handle) {
        return reinterpret_cast<T>(static_cast<uintptr_t>(handle));
    }

    template<typename T>
    inline jlong toHandle(T ptr) {
        return static_cast<jlong>(reinterpret_cast<uintptr_t>(ptr));
    }

    inline DeviceInfo* getDeviceInfo(jlong deviceHandle) {
        return reinterpret_cast<DeviceInfo*>(deviceHandle);
    }

    inline VkDevice getDevice(jlong deviceHandle) {
        return getDeviceInfo(deviceHandle)->device;
    }

// ============================================
// Validation Helpers
// ============================================
    inline bool validateHandle(const void* handle, const char* name) {
        if (handle == VK_NULL_HANDLE || handle == nullptr) {
            LOGE("Invalid %s handle!", name);
            return false;
        }
        return true;
    }

    inline bool validateResult(VkResult result, const char* operation) {
        if (result != VK_SUCCESS) {
            LOGE("%s failed with result: %d", operation, result);
            return false;
        }
        return true;
    }

} // namespace VulkanJNI

#endif // VULKAN_JNI_H