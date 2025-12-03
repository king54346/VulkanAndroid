#include <jni.h>
#include <android/log.h>
#include <vulkan/vulkan.h>
#include "VulkanTypes.h"

#define LOG_TAG "VulkanTexture"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// 外部函数声明
extern uint32_t findMemoryType(VkPhysicalDevice, uint32_t, VkMemoryPropertyFlags);

// 图像布局转换辅助函数
static void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                                  VkImageLayout oldLayout, VkImageLayout newLayout,
                                  VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                                  VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;

    vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);
}

// RAII辅助类：自动清理Vulkan资源
namespace {
    template<typename T, typename Deleter>
    class VulkanResource {
    private:
        T resource;
        Deleter deleter;
        bool released;

    public:
        VulkanResource(T res, Deleter del)
                : resource(res), deleter(del), released(false) {}

        ~VulkanResource() {
            if (!released && resource != VK_NULL_HANDLE) {
                deleter(resource);
            }
        }

        T get() const { return resource; }
        T* ptr() { return &resource; }

        T release() {
            released = true;
            return resource;
        }

        // 禁止拷贝
        VulkanResource(const VulkanResource&) = delete;
        VulkanResource& operator=(const VulkanResource&) = delete;
    };
}

// 创建测试纹理
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeCreateTestTexture(
        JNIEnv* env, jobject thiz, jlong deviceHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    if (!deviceInfo) {
        LOGE("Invalid device handle");
        return 0;
    }

    const uint32_t width = 1920;
    const uint32_t height = 1080;
    const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    const VkDeviceSize imageSize = width * height * 4;

    VkDevice device = deviceInfo->device;

    // 1. 创建图像
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage tempImage;
    if (vkCreateImage(device, &imageInfo, nullptr, &tempImage) != VK_SUCCESS) {
        LOGE("Failed to create image");
        return 0;
    }
    VulkanResource image(tempImage, [device](VkImage img) {
        vkDestroyImage(device, img, nullptr);
    });

    // 2. 分配图像内存
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image.get(), &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
            deviceInfo->physicalDevice,
            memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    VkDeviceMemory tempImageMemory;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &tempImageMemory) != VK_SUCCESS) {
        LOGE("Failed to allocate image memory");
        return 0;
    }
    VulkanResource imageMemory(tempImageMemory, [device](VkDeviceMemory mem) {
        vkFreeMemory(device, mem, nullptr);
    });

    vkBindImageMemory(device, image.get(), imageMemory.get(), 0);

    // 3. 创建staging buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer tempStagingBuffer;
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &tempStagingBuffer) != VK_SUCCESS) {
        LOGE("Failed to create staging buffer");
        return 0;
    }
    VulkanResource stagingBuffer(tempStagingBuffer, [device](VkBuffer buf) {
        vkDestroyBuffer(device, buf, nullptr);
    });

    // 4. 分配staging buffer内存
    VkMemoryRequirements bufferMemRequirements;
    vkGetBufferMemoryRequirements(device, stagingBuffer.get(), &bufferMemRequirements);

    VkMemoryAllocateInfo bufferAllocInfo{};
    bufferAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    bufferAllocInfo.allocationSize = bufferMemRequirements.size;
    bufferAllocInfo.memoryTypeIndex = findMemoryType(
            deviceInfo->physicalDevice,
            bufferMemRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VkDeviceMemory tempStagingMemory;
    if (vkAllocateMemory(device, &bufferAllocInfo, nullptr, &tempStagingMemory) != VK_SUCCESS) {
        LOGE("Failed to allocate staging buffer memory");
        return 0;
    }
    VulkanResource stagingBufferMemory(tempStagingMemory, [device](VkDeviceMemory mem) {
        vkFreeMemory(device, mem, nullptr);
    });

    vkBindBufferMemory(device, stagingBuffer.get(), stagingBufferMemory.get(), 0);

    // 5. 填充数据（青色）
    void* data;
    vkMapMemory(device, stagingBufferMemory.get(), 0, imageSize, 0, &data);

    uint8_t* pixels = static_cast<uint8_t*>(data);
    for (uint32_t i = 0; i < width * height; i++) {
        pixels[i * 4 + 0] = 122;   // R
        pixels[i * 4 + 1] = 255;   // G
        pixels[i * 4 + 2] = 255;   // B
        pixels[i * 4 + 3] = 255;   // A
    }

    vkUnmapMemory(device, stagingBufferMemory.get());

    // 6. 创建临时command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = deviceInfo->graphicsQueueFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    VkCommandPool tempCommandPool;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &tempCommandPool) != VK_SUCCESS) {
        LOGE("Failed to create temp command pool");
        return 0;
    }
    VulkanResource commandPool(tempCommandPool, [device](VkCommandPool pool) {
        vkDestroyCommandPool(device, pool, nullptr);
    });

    // 7. 分配command buffer
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandPool = commandPool.get();
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer);

    // 8. 记录命令
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // 转换图像布局 (UNDEFINED -> TRANSFER_DST_OPTIMAL)
    transitionImageLayout(commandBuffer, image.get(),
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          0, VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT);

    // 复制buffer到image
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.get(), image.get(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // 转换图像布局 (TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL)
    transitionImageLayout(commandBuffer, image.get(),
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    vkEndCommandBuffer(commandBuffer);

    // 9. 提交并等待
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(deviceInfo->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        LOGE("Failed to submit command buffer");
        return 0;
    }
    vkQueueWaitIdle(deviceInfo->graphicsQueue);

    // 临时资源会在作用域结束时自动清理（commandPool, stagingBuffer, stagingBufferMemory）

    // 10. 创建ImageView
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image.get();
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView tempImageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &tempImageView) != VK_SUCCESS) {
        LOGE("Failed to create image view");
        return 0;
    }
    VulkanResource imageView(tempImageView, [device](VkImageView view) {
        vkDestroyImageView(device, view, nullptr);
    });

    // 11. 创建Sampler
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

    VkSampler tempSampler;
    if (vkCreateSampler(device, &samplerInfo, nullptr, &tempSampler) != VK_SUCCESS) {
        LOGE("Failed to create sampler");
        return 0;
    }
    VulkanResource sampler(tempSampler, [device](VkSampler samp) {
        vkDestroySampler(device, samp, nullptr);
    });

    // 12. 创建并返回TextureInfo（转移所有权）
    TextureInfo* textureInfo = new TextureInfo();
    textureInfo->image = image.release();
    textureInfo->memory = imageMemory.release();
    textureInfo->imageView = imageView.release();
    textureInfo->sampler = sampler.release();
    textureInfo->width = width;
    textureInfo->height = height;

    LOGI("Texture created: %ux%u", width, height);
    return reinterpret_cast<jlong>(textureInfo);
}

// 获取纹理ImageView
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeGetTextureImageView(
        JNIEnv* env, jobject thiz, jlong textureHandle) {

    TextureInfo* textureInfo = reinterpret_cast<TextureInfo*>(textureHandle);
    if (textureInfo) {
        return reinterpret_cast<jlong>(textureInfo->imageView);
    }
    return 0;
}

// 获取纹理Sampler
extern "C" JNIEXPORT jlong JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeGetTextureSampler(
        JNIEnv* env, jobject thiz, jlong textureHandle) {

    TextureInfo* textureInfo = reinterpret_cast<TextureInfo*>(textureHandle);
    if (textureInfo) {
        return reinterpret_cast<jlong>(textureInfo->sampler);
    }
    return 0;
}

// 销毁纹理
extern "C" JNIEXPORT void JNICALL
Java_com_example_myapplication_VulkanRenderer_nativeDestroyTexture(
        JNIEnv* env, jobject thiz, jlong deviceHandle, jlong textureHandle) {

    DeviceInfo* deviceInfo = reinterpret_cast<DeviceInfo*>(deviceHandle);
    TextureInfo* textureInfo = reinterpret_cast<TextureInfo*>(textureHandle);

    if (deviceInfo && textureInfo) {
        if (textureInfo->sampler != VK_NULL_HANDLE) {
            vkDestroySampler(deviceInfo->device, textureInfo->sampler, nullptr);
        }
        if (textureInfo->imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(deviceInfo->device, textureInfo->imageView, nullptr);
        }
        if (textureInfo->image != VK_NULL_HANDLE) {
            vkDestroyImage(deviceInfo->device, textureInfo->image, nullptr);
        }
        if (textureInfo->memory != VK_NULL_HANDLE) {
            vkFreeMemory(deviceInfo->device, textureInfo->memory, nullptr);
        }
        delete textureInfo;

        LOGI("Texture destroyed");
    }
}