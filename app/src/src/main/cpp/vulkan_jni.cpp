#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/hardware_buffer.h>
#include <android/hardware_buffer_jni.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <vector>
#include <string>
#include <cstring>
#include <android/log.h>
// Vulkan上下文辅助结构体
// 封装渲染所需的核心Vulkan对象
struct VulkanContext {
    VkInstance instance = VK_NULL_HANDLE;              // Vulkan实例 - 与Vulkan库的连接
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;  // GPU物理设备
    VkDevice device = VK_NULL_HANDLE;                  // 逻辑设备，用于提交命令
    VkQueue queue = VK_NULL_HANDLE;                    // 命令队列，用于向GPU提交工作
    uint32_t queueFamilyIndex = 0;                     // 支持图形操作的队列族索引
};

/**
 * 将Java字符串数组转换为C++字符串向量
 * 用于向Vulkan传递扩展名和层名称
 *
 * @param env JNI环境
 * @param javaArray Java String[]数组
 * @return C字符串指针向量(调用者必须调用releaseStringArray来释放)
 */
std::vector<const char*> getStringArray(JNIEnv* env, jobjectArray javaArray) {
    std::vector<const char*> result;
    jsize count = env->GetArrayLength(javaArray);

    for (jsize i = 0; i < count; i++) {
        jstring jstr = (jstring)env->GetObjectArrayElement(javaArray, i);
        const char* str = env->GetStringUTFChars(jstr, nullptr);
        result.push_back(str);
    }

    return result;
}

/**
 * 释放getStringArray获取的字符串
 * 必须调用以避免JNI内存泄漏
 */
void releaseStringArray(JNIEnv* env, jobjectArray javaArray, const std::vector<const char*>& cArray) {
    jsize count = env->GetArrayLength(javaArray);

    for (jsize i = 0; i < count; i++) {
        jstring jstr = (jstring)env->GetObjectArrayElement(javaArray, i);
        env->ReleaseStringUTFChars(jstr, cArray[i]);
    }
}

extern "C" {

/**
 * 创建Vulkan实例
 * Vulkan实例是与Vulkan库的连接，必须在其他Vulkan操作之前创建
 *
 * @param appInfo 包含应用程序信息的Java对象(名称、版本、API版本)
 * @param extensions 要启用的实例扩展数组(androidwindow的扩展)
 * @return Vulkan实例句柄，失败返回0
 */
JNIEXPORT jlong JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeCreateInstance(
        JNIEnv *env, jobject thiz, jobject appInfo, jobjectArray extensions) {

    // 提取应用程序信息
    jclass appInfoClass = env->GetObjectClass(appInfo);
    jfieldID nameField = env->GetFieldID(appInfoClass, "applicationName", "Ljava/lang/String;");
    jfieldID versionField = env->GetFieldID(appInfoClass, "applicationVersion", "I");
    jfieldID apiVersionField = env->GetFieldID(appInfoClass, "apiVersion", "I");

    auto appName = (jstring) env->GetObjectField(appInfo, nameField);
    const char *appNameStr = env->GetStringUTFChars(appName, nullptr);
    jint appVersion = env->GetIntField(appInfo, versionField);
    jint apiVersion = env->GetIntField(appInfo, apiVersionField);

    // 填充Vulkan应用程序信息
    VkApplicationInfo vkAppInfo = {};
    vkAppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    vkAppInfo.pApplicationName = appNameStr;
    vkAppInfo.applicationVersion = appVersion;
    vkAppInfo.pEngineName = "ScrcpyEngine";
    vkAppInfo.engineVersion = 1;
    vkAppInfo.apiVersion = apiVersion;
    // 2. 获取扩展名
    auto extensionNames = getStringArray(env, extensions);

    // 配置实例创建信息
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &vkAppInfo;
    createInfo.enabledExtensionCount = extensionNames.size();
    createInfo.ppEnabledExtensionNames = extensionNames.data();

    // 3. 创建Vulkan实例
    VkInstance instance;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

    // 清理临时字符串
    env->ReleaseStringUTFChars(appName, appNameStr);
    releaseStringArray(env, extensions, extensionNames);

    if (result != VK_SUCCESS) {
        return 0;
    }

    return static_cast<jlong>(reinterpret_cast<uintptr_t>(instance));
}

/**
 * 创建Android Surface
 * Surface是Vulkan渲染目标，对应Android的Surface对象
 *
 * @param instanceHandle Vulkan实例句柄
 * @param surface Android Surface对象
 * @return Vulkan surface句柄，失败返回0
 */
JNIEXPORT jlong JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeCreateAndroidSurface(
        JNIEnv *env, jobject thiz, jlong instanceHandle, jobject surface) {

    VkInstance instance = reinterpret_cast<VkInstance>(static_cast<uintptr_t>(instanceHandle));
    // 从Java Surface对象获取ANativeWindow
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);

    // 配置Android surface创建信息
    VkAndroidSurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    createInfo.window = window;

    VkSurfaceKHR vkSurface;
    // vulkaninstance， vulkansurface
    VkResult result = vkCreateAndroidSurfaceKHR(instance, &createInfo, nullptr, &vkSurface);

    if (result != VK_SUCCESS) {
        ANativeWindow_release(window);
        return 0;
    }

    return static_cast<jlong>(reinterpret_cast<uintptr_t>(vkSurface));
}

/**
 * 枚举物理设备(GPU)
 * 列出系统中所有可用的Vulkan兼容GPU
 *
 * @param instanceHandle Vulkan实例句柄
 * @return 物理设备句柄数组
 */
JNIEXPORT jlongArray JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeEnumeratePhysicalDevices(
        JNIEnv *env, jobject thiz, jlong instanceHandle) {

    VkInstance instance = reinterpret_cast<VkInstance>(static_cast<uintptr_t>(instanceHandle));

    // 首先获取设备数量
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        return env->NewLongArray(0);
    }

    // 获取所有设备
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // 转换为Java long数组
    jlongArray result = env->NewLongArray(deviceCount);
    jlong *elements = new jlong[deviceCount];

    for (uint32_t i = 0; i < deviceCount; i++) {
        elements[i] = static_cast<jlong>(reinterpret_cast<uintptr_t>(devices[i]));
    }

    env->SetLongArrayRegion(result, 0, deviceCount, elements);
    delete[] elements;

    return result;
}

/**
 * 查找支持图形和显示的队列族
 * 队列族定义了GPU可以执行的操作类型(图形、计算、传输等)
 *
 * @param physicalDeviceHandle 物理设备句柄
 * @param surfaceHandle Surface句柄
 * @return 队列族索引，未找到返回-1
 */
JNIEXPORT jint JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeFindQueueFamily(
        JNIEnv *env, jobject thiz, jlong physicalDeviceHandle, jlong surfaceHandle) {

    VkPhysicalDevice physicalDevice = reinterpret_cast<VkPhysicalDevice>(static_cast<uintptr_t>(physicalDeviceHandle));
    VkSurfaceKHR surface = reinterpret_cast<VkSurfaceKHR>(static_cast<uintptr_t>(surfaceHandle));

    // 获取队列族数量
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    // 获取所有队列族属性
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                             queueFamilies.data());

    // 查找同时支持图形和显示的队列族
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);

            if (presentSupport) {
                return static_cast<jint>(i);
            }
        }
    }

    return -1;
}

/**
 * 创建逻辑设备和队列
 * 逻辑设备是应用程序与物理GPU交互的接口
 *
 * @param physicalDeviceHandle 物理设备句柄
 * @param queueFamilyIndex 队列族索引
 * @param extensions 要启用的设备扩展数组
 * @return 包含设备和队列句柄的DeviceResult对象
 */
JNIEXPORT jobject JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeCreateLogicalDevice(
        JNIEnv *env, jobject thiz, jlong physicalDeviceHandle, jint queueFamilyIndex,
        jobjectArray extensions) {

    VkPhysicalDevice physicalDevice = reinterpret_cast<VkPhysicalDevice>(static_cast<uintptr_t>(physicalDeviceHandle));

    // 配置队列创建信息
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    auto extensionNames = getStringArray(env, extensions);

    VkPhysicalDeviceFeatures deviceFeatures = {};

    // 配置设备创建信息
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = extensionNames.size();
    createInfo.ppEnabledExtensionNames = extensionNames.data();
    createInfo.pEnabledFeatures = &deviceFeatures;

    // 创建逻辑设备
    VkDevice device;
    VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);

    releaseStringArray(env, extensions, extensionNames);

    if (result != VK_SUCCESS) {
        return nullptr;
    }

    // 获取队列句柄
    VkQueue queue;
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

    // 创建DeviceResult对象返回给Java
    jclass deviceResultClass = env->FindClass(
            "com/genymobile/scrcpy/vulkan/VulkanRunner$DeviceResult");
    jmethodID constructor = env->GetMethodID(deviceResultClass, "<init>", "(JJ)V");

    return env->NewObject(deviceResultClass, constructor,
                          static_cast<jlong>(reinterpret_cast<uintptr_t>(device)),
                          static_cast<jlong>(reinterpret_cast<uintptr_t>(queue)));
}

/**
 * 创建交换链(Swapchain)
 * 交换链管理用于显示的图像缓冲区，实现双缓冲或三缓冲
 *
 * @param deviceHandle 逻辑设备句柄
 * @param physicalDeviceHandle 物理设备句柄
 * @param surfaceHandle Surface句柄
 * @param width 交换链宽度
 * @param height 交换链高度
 * @return 交换链句柄，失败返回0
 */
JNIEXPORT jlong JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeCreateSwapchain(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong physicalDeviceHandle,
        jlong surfaceHandle, jint width, jint height) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkPhysicalDevice physicalDevice = reinterpret_cast<VkPhysicalDevice>(static_cast<uintptr_t>(physicalDeviceHandle));
    VkSurfaceKHR surface = reinterpret_cast<VkSurfaceKHR>(static_cast<uintptr_t>(surfaceHandle));

    // 查询surface能力
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

    // 查询surface格式
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

    // 选择格式(优先使用SRGB)
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto &format: formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = format;
            break;
        }
    }

    // 查询显示模式
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount,
                                              presentModes.data());

    // 选择显示模式(优先使用MAILBOX以降低延迟)
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto &mode: presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = mode;
            break;
        }
    }

    // 设置交换链范围(分辨率)
    VkExtent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent.width = width;
        extent.height = height;
    }

    // 计算图像数量(最小数量+1以实现三缓冲)
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    // 配置交换链创建信息
    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain;
    VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain);

    if (result != VK_SUCCESS) {
        return 0;
    }

    return static_cast<jlong>(reinterpret_cast<uintptr_t>(swapchain));
}

/**
 * 创建渲染通道(Render Pass)
 * 渲染通道定义渲染操作的框架，包括附件(颜色、深度等)和子通道
 *
 * @param deviceHandle 逻辑设备句柄
 * @return 渲染通道句柄，失败返回0
 */
JNIEXPORT jlong JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeCreateRenderPass(
        JNIEnv *env, jobject thiz, jlong deviceHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));

    // 配置颜色附件
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = VK_FORMAT_B8G8R8A8_SRGB;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;      // 渲染前清除
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;    // 渲染后保存
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // 用于显示

    // 配置附件引用
    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // 配置子通道
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    // 配置子通道依赖(同步)
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // 配置渲染通道创建信息
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass renderPass;
    VkResult result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);

    if (result != VK_SUCCESS) {
        return 0;
    }

    return static_cast<jlong>(reinterpret_cast<uintptr_t>(renderPass));
}

/**
 * 获取交换链图像并创建图像视图
 * 图像视图定义如何访问图像(格式、范围等)
 *
 * @param deviceHandle 逻辑设备句柄
 * @param swapchainHandle 交换链句柄
 * @return 图像视图句柄数组
 */
JNIEXPORT jlongArray JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeGetSwapchainImages(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong swapchainHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkSwapchainKHR swapchain = reinterpret_cast<VkSwapchainKHR>(static_cast<uintptr_t>(swapchainHandle));

    // 获取图像数量
    uint32_t imageCount;
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);

    // 获取所有图像
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());

    // 为每个图像创建图像视图
    std::vector<VkImageView> imageViews(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        vkCreateImageView(device, &createInfo, nullptr, &imageViews[i]);
    }

    // 转换为Java long数组
    jlongArray result = env->NewLongArray(imageCount);
    jlong *elements = new jlong[imageCount];

    for (uint32_t i = 0; i < imageCount; i++) {
        elements[i] = static_cast<jlong>(reinterpret_cast<uintptr_t>(imageViews[i]));
    }

    env->SetLongArrayRegion(result, 0, imageCount, elements);
    delete[] elements;

    return result;
}

/**
 * 创建帧缓冲(Framebuffer)
 * 帧缓冲将图像视图绑定到渲染通道的附件
 *
 * @param deviceHandle 逻辑设备句柄
 * @param renderPassHandle 渲染通道句柄
 * @param imageViewHandle 图像视图句柄
 * @param width 帧缓冲宽度
 * @param height 帧缓冲高度
 * @return 帧缓冲句柄，失败返回0
 */
JNIEXPORT jlong JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeCreateFramebuffer(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong renderPassHandle,
        jlong imageViewHandle, jint width, jint height) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkRenderPass renderPass = reinterpret_cast<VkRenderPass>(static_cast<uintptr_t>(renderPassHandle));
    VkImageView imageView = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(imageViewHandle));

    VkImageView attachments[] = {imageView};

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;

    VkFramebuffer framebuffer;
    VkResult result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer);

    if (result != VK_SUCCESS) {
        return 0;
    }

    return static_cast<jlong>(reinterpret_cast<uintptr_t>(framebuffer));
}

/**
 * 创建命令池(Command Pool)
 * 命令池管理命令缓冲区的内存分配
 *
 * @param deviceHandle 逻辑设备句柄
 * @param queueFamilyIndex 队列族索引
 * @return 命令池句柄，失败返回0
 */
JNIEXPORT jlong JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeCreateCommandPool(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jint queueFamilyIndex) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;  // 允许重置单个命令缓冲区
    poolInfo.queueFamilyIndex = queueFamilyIndex;

    VkCommandPool commandPool;
    VkResult result = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);

    if (result != VK_SUCCESS) {
        return 0;
    }

    return static_cast<jlong>(reinterpret_cast<uintptr_t>(commandPool));
}

/**
 * 分配命令缓冲区(Command Buffers)
 * 命令缓冲区用于记录和提交GPU命令
 *
 * @param deviceHandle 逻辑设备句柄
 * @param commandPoolHandle 命令池句柄
 * @param count 要分配的命令缓冲区数量
 * @return 命令缓冲区句柄数组
 */
JNIEXPORT jlongArray JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeAllocateCommandBuffers(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong commandPoolHandle, jint count) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkCommandPool commandPool = reinterpret_cast<VkCommandPool>(static_cast<uintptr_t>(commandPoolHandle));

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // 主命令缓冲区(可直接提交到队列)
    allocInfo.commandBufferCount = count;

    std::vector<VkCommandBuffer> commandBuffers(count);
    VkResult result = vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data());

    if (result != VK_SUCCESS) {
        return env->NewLongArray(0);
    }

    jlongArray jArray = env->NewLongArray(count);
    jlong *elements = new jlong[count];

    for (int i = 0; i < count; i++) {
        elements[i] = static_cast<jlong>(reinterpret_cast<uintptr_t>(commandBuffers[i]));
    }

    env->SetLongArrayRegion(jArray, 0, count, elements);
    delete[] elements;

    return jArray;
}

/**
 * 创建信号量(Semaphore)
 * 信号量用于GPU内部的同步(队列之间或提交之间)
 *
 * @param deviceHandle 逻辑设备句柄
 * @return 信号量句柄，失败返回0
 */
JNIEXPORT jlong JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeCreateSemaphore(
        JNIEnv *env, jobject thiz, jlong deviceHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore semaphore;
    VkResult result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore);

    if (result != VK_SUCCESS) {
        return 0;
    }

    return static_cast<jlong>(reinterpret_cast<uintptr_t>(semaphore));
}

/**
 * 创建围栏(Fence)
 * 围栏用于CPU-GPU同步，CPU可以等待GPU操作完成
 *
 * @param deviceHandle 逻辑设备句柄
 * @return 围栏句柄，失败返回0
 */
JNIEXPORT jlong JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeCreateFence(
        JNIEnv *env, jobject thiz, jlong deviceHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // 初始为已触发状态

    VkFence fence;
    VkResult result = vkCreateFence(device, &fenceInfo, nullptr, &fence);

    if (result != VK_SUCCESS) {
        return 0;
    }

    return static_cast<jlong>(reinterpret_cast<uintptr_t>(fence));
}

/**
 * 创建外部图像(使用AHardwareBuffer)
 * 用于与Android系统共享图像数据，支持零拷贝
 *
 * @param deviceHandle 逻辑设备句柄
 * @param physicalDeviceHandle 物理设备句柄
 * @param width 图像宽度
 * @param height 图像高度
 * @return 包含图像、内存和视图句柄的ExternalImageResult对象
 */
JNIEXPORT jobject JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeCreateExternalImage(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong physicalDeviceHandle,
        jint width, jint height) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkPhysicalDevice physicalDevice = reinterpret_cast<VkPhysicalDevice>(static_cast<uintptr_t>(physicalDeviceHandle));

    // 创建AHardwareBuffer(Android硬件缓冲区)
    AHardwareBuffer_Desc bufferDesc = {};
    bufferDesc.width = width;
    bufferDesc.height = height;
    bufferDesc.layers = 1;
    bufferDesc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    bufferDesc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                       AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;

    AHardwareBuffer *hardwareBuffer;
    if (AHardwareBuffer_allocate(&bufferDesc, &hardwareBuffer) != 0) {
        return nullptr;
    }

    // 获取外部内存的内存要求
    VkAndroidHardwareBufferFormatPropertiesANDROID formatInfo = {};
    formatInfo.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID;

    VkAndroidHardwareBufferPropertiesANDROID bufferProperties = {};
    bufferProperties.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
    bufferProperties.pNext = &formatInfo;

    // 获取vkGetAndroidHardwareBufferPropertiesANDROID函数指针
    PFN_vkGetAndroidHardwareBufferPropertiesANDROID vkGetAHBProps =
            (PFN_vkGetAndroidHardwareBufferPropertiesANDROID) vkGetDeviceProcAddr(
                    device, "vkGetAndroidHardwareBufferPropertiesANDROID");

    if (vkGetAHBProps(device, hardwareBuffer, &bufferProperties) != VK_SUCCESS) {
        AHardwareBuffer_release(hardwareBuffer);
        return nullptr;
    }

    // 创建外部图像
    VkExternalMemoryImageCreateInfo externalMemoryInfo = {};
    externalMemoryInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalMemoryInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = &externalMemoryInfo;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = formatInfo.format;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image;
    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        AHardwareBuffer_release(hardwareBuffer);
        return nullptr;
    }

    // 从AHardwareBuffer导入内存
    VkImportAndroidHardwareBufferInfoANDROID importInfo = {};
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
    importInfo.buffer = hardwareBuffer;

    VkMemoryDedicatedAllocateInfo dedicatedInfo = {};
    dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedInfo.pNext = &importInfo;
    dedicatedInfo.image = image;

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &dedicatedInfo;
    allocInfo.allocationSize = bufferProperties.allocationSize;
    allocInfo.memoryTypeIndex = 0; // 需要找到合适的内存类型

    VkDeviceMemory memory;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyImage(device, image, nullptr);
        AHardwareBuffer_release(hardwareBuffer);
        return nullptr;
    }

    // 绑定图像内存
    vkBindImageMemory(device, image, memory, 0);

    // 创建图像视图
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = formatInfo.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        vkFreeMemory(device, memory, nullptr);
        vkDestroyImage(device, image, nullptr);
        AHardwareBuffer_release(hardwareBuffer);
        return nullptr;
    }

    // 创建ExternalImageResult对象返回给Java
    jclass resultClass = env->FindClass(
            "com/genymobile/scrcpy/vulkan/VulkanRunner$ExternalImageResult");
    jmethodID constructor = env->GetMethodID(resultClass, "<init>", "(JJJ)V");

    return env->NewObject(resultClass, constructor,
                          static_cast<jlong>(reinterpret_cast<uintptr_t>(image)),
                          static_cast<jlong>(reinterpret_cast<uintptr_t>(memory)),
                          static_cast<jlong>(reinterpret_cast<uintptr_t>(imageView)));
}

/**
 * 获取硬件缓冲区
 * (占位符实现，需要在创建时存储AHardwareBuffer指针)
 */
JNIEXPORT jobject JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeGetHardwareBuffer(
        JNIEnv *env, jobject thiz, jlong memoryHandle) {

    VkDeviceMemory memory = reinterpret_cast<VkDeviceMemory>(static_cast<uintptr_t>(memoryHandle));

    // 注意：这需要在创建时存储AHardwareBuffer指针
    // 为简化起见，返回占位符
    return nullptr;
}

/**
 * 将硬件缓冲区附加到SurfaceTexture
 * (依赖于Android API级别的实现)
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeAttachHardwareBufferToSurfaceTexture(
        JNIEnv *env, jobject thiz, jobject surfaceTexture, jobject hardwareBuffer) {

    // 这将使用SurfaceTexture::attachToGLContext与硬件缓冲区支持
    // 实现取决于Android API级别
}

/**
 * 等待围栏
 * CPU阻塞直到GPU完成操作
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeWaitForFence(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong fenceHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkFence fence = reinterpret_cast<VkFence>(static_cast<uintptr_t>(fenceHandle));

    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
}

/**
 * 重置围栏
 * 将围栏状态重置为未触发，以便重复使用
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeResetFence(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong fenceHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkFence fence = reinterpret_cast<VkFence>(static_cast<uintptr_t>(fenceHandle));

    vkResetFences(device, 1, &fence);
}

/**
 * 获取下一个可用图像
 * 从交换链获取下一个可用于渲染的图像索引
 *
 * @return 图像索引，失败或需要重建交换链返回-1
 */
JNIEXPORT jint JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeAcquireNextImage(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong swapchainHandle,
        jlong semaphoreHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkSwapchainKHR swapchain = reinterpret_cast<VkSwapchainKHR>(static_cast<uintptr_t>(swapchainHandle));
    VkSemaphore semaphore = reinterpret_cast<VkSemaphore>(static_cast<uintptr_t>(semaphoreHandle));

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                            semaphore, VK_NULL_HANDLE, &imageIndex);

    // 交换链过期或次优，需要重建
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return -1;
    }

    if (result != VK_SUCCESS) {
        return -1;
    }

    return static_cast<jint>(imageIndex);
}

/**
 * 开始记录命令缓冲区
 * 准备命令缓冲区以记录GPU命令
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeBeginCommandBuffer(
        JNIEnv *env, jobject thiz, jlong commandBufferHandle) {

    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(static_cast<uintptr_t>(commandBufferHandle));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // 一次性提交

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
}

/**
 * 开始渲染通道
 * 开始渲染操作，设置渲染目标和清除值
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeBeginRenderPass(
        JNIEnv *env, jobject thiz, jlong commandBufferHandle, jlong renderPassHandle,
        jlong framebufferHandle, jint width, jint height) {

    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(static_cast<uintptr_t>(commandBufferHandle));
    VkRenderPass renderPass = reinterpret_cast<VkRenderPass>(static_cast<uintptr_t>(renderPassHandle));
    VkFramebuffer framebuffer = reinterpret_cast<VkFramebuffer>(static_cast<uintptr_t>(framebufferHandle));

    // 设置清除颜色(黑色)
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {static_cast<uint32_t>(width),
                                        static_cast<uint32_t>(height)};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

/**
 * 结束渲染通道
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeEndRenderPass(
        JNIEnv *env, jobject thiz, jlong commandBufferHandle) {

    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(static_cast<uintptr_t>(commandBufferHandle));
    vkCmdEndRenderPass(commandBuffer);
}

/**
 * 结束命令缓冲区记录
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeEndCommandBuffer(
        JNIEnv *env, jobject thiz, jlong commandBufferHandle) {

    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(static_cast<uintptr_t>(commandBufferHandle));
    vkEndCommandBuffer(commandBuffer);
}

/**
 * 提交命令缓冲区到队列
 * 将记录的命令提交到GPU执行
 *
 * @param waitSemaphoreHandle 等待的信号量(图像可用)
 * @param signalSemaphoreHandle 完成时触发的信号量(渲染完成)
 * @param fenceHandle 完成时触发的围栏(CPU同步)
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeSubmitCommandBuffer(
        JNIEnv *env, jobject thiz, jlong queueHandle, jlong commandBufferHandle,
        jlong waitSemaphoreHandle, jlong signalSemaphoreHandle, jlong fenceHandle) {

    VkQueue queue = reinterpret_cast<VkQueue>(static_cast<uintptr_t>(queueHandle));
    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(static_cast<uintptr_t>(commandBufferHandle));
    VkSemaphore waitSemaphore = reinterpret_cast<VkSemaphore>(static_cast<uintptr_t>(waitSemaphoreHandle));
    VkSemaphore signalSemaphore = reinterpret_cast<VkSemaphore>(static_cast<uintptr_t>(signalSemaphoreHandle));
    VkFence fence = reinterpret_cast<VkFence>(static_cast<uintptr_t>(fenceHandle));

    // 在颜色附件输出阶段等待
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &waitSemaphore;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &signalSemaphore;

    vkQueueSubmit(queue, 1, &submitInfo, fence);
}

/**
 * 队列显示
 * 将渲染的图像提交到显示队列
 *
 * @param imageIndex 要显示的图像索引
 * @param waitSemaphoreHandle 等待的信号量(渲染完成)
 * @param timestamp 显示时间戳
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeQueuePresent(
        JNIEnv *env, jobject thiz, jlong queueHandle, jlong swapchainHandle,
        jint imageIndex, jlong waitSemaphoreHandle, jlong timestamp) {

    VkQueue queue = reinterpret_cast<VkQueue>(static_cast<uintptr_t>(queueHandle));
    VkSwapchainKHR swapchain = reinterpret_cast<VkSwapchainKHR>(static_cast<uintptr_t>(swapchainHandle));
    VkSemaphore waitSemaphore = reinterpret_cast<VkSemaphore>(static_cast<uintptr_t>(waitSemaphoreHandle));
    uint32_t index = static_cast<uint32_t>(imageIndex);

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &waitSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &index;

    vkQueuePresentKHR(queue, &presentInfo);
}

/**
 * 等待设备空闲
 * 阻塞直到设备完成所有待处理的操作
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeDeviceWaitIdle(
        JNIEnv *env, jobject thiz, jlong deviceHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    vkDeviceWaitIdle(device);
}

// ==================== 清理函数 ====================

/**
 * 释放命令缓冲区
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeFreeCommandBuffers(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong commandPoolHandle,
        jlongArray commandBuffersArray) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkCommandPool commandPool = reinterpret_cast<VkCommandPool>(static_cast<uintptr_t>(commandPoolHandle));

    jsize count = env->GetArrayLength(commandBuffersArray);
    jlong *elements = env->GetLongArrayElements(commandBuffersArray, nullptr);

    std::vector<VkCommandBuffer> commandBuffers(count);
    for (jsize i = 0; i < count; i++) {
        commandBuffers[i] = reinterpret_cast<VkCommandBuffer>(static_cast<uintptr_t>(elements[i]));
    }

    vkFreeCommandBuffers(device, commandPool, count, commandBuffers.data());

    env->ReleaseLongArrayElements(commandBuffersArray, elements, 0);
}

/**
 * 销毁帧缓冲
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeDestroyFramebuffer(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong framebufferHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkFramebuffer framebuffer = reinterpret_cast<VkFramebuffer>(static_cast<uintptr_t>(framebufferHandle));
    vkDestroyFramebuffer(device, framebuffer, nullptr);
}

/**
 * 销毁围栏
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeDestroyFence(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong fenceHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkFence fence = reinterpret_cast<VkFence>(static_cast<uintptr_t>(fenceHandle));
    vkDestroyFence(device, fence, nullptr);
}

/**
 * 销毁信号量
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeDestroySemaphore(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong semaphoreHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkSemaphore semaphore = reinterpret_cast<VkSemaphore>(static_cast<uintptr_t>(semaphoreHandle));
    vkDestroySemaphore(device, semaphore, nullptr);
}

/**
 * 销毁图像视图
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeDestroyImageView(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong imageViewHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkImageView imageView = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(imageViewHandle));
    vkDestroyImageView(device, imageView, nullptr);
}

/**
 * 销毁图像
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeDestroyImage(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong imageHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkImage image = reinterpret_cast<VkImage>(static_cast<uintptr_t>(imageHandle));
    vkDestroyImage(device, image, nullptr);
}

/**
 * 释放内存
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeFreeMemory(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong memoryHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkDeviceMemory memory = reinterpret_cast<VkDeviceMemory>(static_cast<uintptr_t>(memoryHandle));
    vkFreeMemory(device, memory, nullptr);
}

/**
 * 销毁命令池
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeDestroyCommandPool(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong commandPoolHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkCommandPool commandPool = reinterpret_cast<VkCommandPool>(static_cast<uintptr_t>(commandPoolHandle));
    vkDestroyCommandPool(device, commandPool, nullptr);
}

/**
 * 销毁渲染通道
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeDestroyRenderPass(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong renderPassHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkRenderPass renderPass = reinterpret_cast<VkRenderPass>(static_cast<uintptr_t>(renderPassHandle));
    vkDestroyRenderPass(device, renderPass, nullptr);
}

/**
 * 销毁交换链
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeDestroySwapchain(
        JNIEnv *env, jobject thiz, jlong deviceHandle, jlong swapchainHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    VkSwapchainKHR swapchain = reinterpret_cast<VkSwapchainKHR>(static_cast<uintptr_t>(swapchainHandle));
    vkDestroySwapchainKHR(device, swapchain, nullptr);
}

/**
 * 销毁Surface
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeDestroySurface(
        JNIEnv *env, jobject thiz, jlong instanceHandle, jlong surfaceHandle) {

    VkInstance instance = reinterpret_cast<VkInstance>(static_cast<uintptr_t>(instanceHandle));
    VkSurfaceKHR surface = reinterpret_cast<VkSurfaceKHR>(static_cast<uintptr_t>(surfaceHandle));
    vkDestroySurfaceKHR(instance, surface, nullptr);
}

/**
 * 销毁逻辑设备
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeDestroyDevice(
        JNIEnv *env, jobject thiz, jlong deviceHandle) {

    VkDevice device = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(deviceHandle));
    vkDestroyDevice(device, nullptr);
}

/**
 * 销毁Vulkan实例
 */
JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_VulkanRunner_nativeDestroyInstance(
        JNIEnv *env, jobject thiz, jlong instanceHandle) {

    VkInstance instance = reinterpret_cast<VkInstance>(static_cast<uintptr_t>(instanceHandle));
    vkDestroyInstance(instance, nullptr);
}
}