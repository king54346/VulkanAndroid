//
// Created by 31483 on 2025/11/28.
//
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <vector>
#include <set>
#include <sstream>
#include "VulkanTypes.h"

#define LOG_TAG "VulkanInstance"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// 外部函数声明
extern uint32_t findQueueFamily(VkPhysicalDevice, VkQueueFlags, VkSurfaceKHR);

// 全局扩展列表
static const std::vector<const char*> INSTANCE_EXTENSIONS = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
};

static const std::vector<const char*> DEVICE_EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static void logPhysicalDevices(const std::vector<VkPhysicalDevice>& devices) {
    std::ostringstream oss;
    oss << "Physical devices count=" << devices.size();
    for (const auto& d : devices) {
        oss << " 0x" << std::hex << reinterpret_cast<uintptr_t>(d);
    }
    LOGI("%s", oss.str().c_str());
}

// 创建Vulkan实例
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeCreateInstance(
        JNIEnv* env, jobject thiz) {

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VulkanFilter";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = INSTANCE_EXTENSIONS.size();
    createInfo.ppEnabledExtensionNames = INSTANCE_EXTENSIONS.data();
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

// 创建设备
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeCreateDevice(
        JNIEnv* env, jobject thiz, jlong instanceHandle, jobject surface) {

    VkInstance instance = reinterpret_cast<VkInstance>(instanceHandle);

    // 创建Surface
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        LOGE("Failed to get native window from surface");
        return 0;
    }

    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo{};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.window = window;

    VkSurfaceKHR vkSurface;
    VkResult result = vkCreateAndroidSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &vkSurface);
    if (result != VK_SUCCESS) {
        LOGE("Failed to create Android surface: %d", result);
        ANativeWindow_release(window);
        return 0;
    }

    // 枚举物理设备
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        LOGE("Failed to find GPUs with Vulkan support");
        ANativeWindow_release(window);
        return 0;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    logPhysicalDevices(devices);

    VkPhysicalDevice physicalDevice = devices[0];

    // 查找队列族
    uint32_t graphicsFamily = findQueueFamily(physicalDevice, VK_QUEUE_GRAPHICS_BIT, VK_NULL_HANDLE);
    uint32_t presentFamily = findQueueFamily(physicalDevice, VK_QUEUE_GRAPHICS_BIT, vkSurface);

    if (graphicsFamily == UINT32_MAX || presentFamily == UINT32_MAX) {
        LOGE("Failed to find required queue families");
        ANativeWindow_release(window);
        return 0;
    }

    // 创建队列信息
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {graphicsFamily, presentFamily};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // 设备特性
    VkPhysicalDeviceFeatures deviceFeatures{};

    // 创建逻辑设备
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = queueCreateInfos.size();
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    deviceCreateInfo.enabledExtensionCount = DEVICE_EXTENSIONS.size();
    deviceCreateInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();

    VkDevice device;
    result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);

    if (result != VK_SUCCESS) {
        LOGE("Failed to create logical device: %d", result);
        ANativeWindow_release(window);
        return 0;
    }

    // 保存设备信息
    DeviceInfo* deviceInfo = new DeviceInfo();
    deviceInfo->device = device;
    deviceInfo->physicalDevice = physicalDevice;
    deviceInfo->graphicsQueueFamily = graphicsFamily;
    deviceInfo->presentQueueFamily = presentFamily;
    deviceInfo->surface = vkSurface;

    vkGetDeviceQueue(device, graphicsFamily, 0, &deviceInfo->graphicsQueue);
    vkGetDeviceQueue(device, presentFamily, 0, &deviceInfo->presentQueue);

    ANativeWindow_release(window);

    LOGI("Vulkan device created successfully");
    return reinterpret_cast<jlong>(deviceInfo);
}

// 销毁设备
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeDestroyDevice(
        JNIEnv* env, jobject thiz, jlong deviceHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    if (deviceInfo) {
        vkDestroyDevice(deviceInfo->device, nullptr);
        delete deviceInfo;
    }
}

// 销毁实例
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeDestroyInstance(
        JNIEnv* env, jobject thiz, jlong instanceHandle) {

    VkInstance instance = reinterpret_cast<VkInstance>(instanceHandle);
    if (instance) {
        vkDestroyInstance(instance, nullptr);
    }
}