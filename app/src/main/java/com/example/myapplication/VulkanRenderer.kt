package com.example.myapplication

import android.util.Log
import android.view.Surface
import com.genymobile.scrcpy.vulkan.VulkanFilter
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.withLock

class VulkanRenderer(private val surface: Surface) {

    // Vulkan handles
    private var vkInstance: Long = 0
    private var vkDevice: Long = 0
    private var vkRenderPass: Long = 0
    private var vkSwapchain: Long = 0
    private var vkCommandPool: Long = 0
    private var vkCommandBuffer: Long = 0
    private var inputTexture: Long = 0

    private var filter: VulkanFilter? = null

    // 使用 AtomicBoolean 替代普通 boolean，提供更好的线程安全性
    private val isInitialized = AtomicBoolean(false)
    private val isRendering = AtomicBoolean(false)
    private var renderThread: Thread? = null

    // 使用 ReentrantLock 替代 Object，提供更灵活的锁机制
    private val lock = ReentrantLock()

    fun initialize(): Boolean = lock.withLock {
        if (isInitialized.get()) {
            Log.w(TAG, "Already initialized")
            return true
        }

        Log.d(TAG, "=== Initializing Vulkan ===")

        return try {
            initializeVulkanResources()
        } catch (e: Exception) {
            Log.e(TAG, "Exception during initialization", e)
            cleanupInternal()
            false
        }
    }

    private fun initializeVulkanResources(): Boolean {
        // 1. Instance
        vkInstance = nativeCreateInstance()
        if (!validateHandle(vkInstance, "Instance")) {
            return false
        }

        // 2. Device
        vkDevice = nativeCreateDevice(vkInstance, surface)
        if (!validateHandle(vkDevice, "Device")) {
            cleanupInternal()
            return false
        }

        // 3. RenderPass
        vkRenderPass = nativeCreateRenderPass(vkDevice)
        if (!validateHandle(vkRenderPass, "RenderPass")) {
            cleanupInternal()
            return false
        }

        // 4. Swapchain
        vkSwapchain = nativeCreateSwapchain(vkDevice, surface)
        if (!validateHandle(vkSwapchain, "Swapchain")) {
            cleanupInternal()
            return false
        }

        // 5. Framebuffers
        if (!nativeCreateFramebuffers(vkDevice, vkSwapchain, vkRenderPass)) {
            Log.e(TAG, "Failed to create framebuffers")
            cleanupInternal()
            return false
        }
        Log.d(TAG, "✓ Framebuffers created")

        // 6. CommandPool
        vkCommandPool = nativeCreateCommandPool(vkDevice)
        if (!validateHandle(vkCommandPool, "CommandPool")) {
            cleanupInternal()
            return false
        }

        // 7. CommandBuffer
        vkCommandBuffer = nativeAllocateCommandBuffer(vkDevice, vkCommandPool)
        if (!validateHandle(vkCommandBuffer, "CommandBuffer")) {
            cleanupInternal()
            return false
        }

        // 8. Test Texture
        inputTexture = nativeCreateTestTexture(vkDevice)
        if (!validateHandle(inputTexture, "TestTexture")) {
            cleanupInternal()
            return false
        }

        isInitialized.set(true)
        Log.i(TAG, "=== Vulkan initialized successfully ===")
        return true
    }

    private fun validateHandle(handle: Long, resourceName: String): Boolean {
        return if (handle == 0L) {
            Log.e(TAG, "Failed to create $resourceName")
            false
        } else {
            Log.d(TAG, "✓ $resourceName created: $handle")
            true
        }
    }

    fun getDevice(): Long {
        checkInitialized()
        return vkDevice
    }

    fun getRenderPass(): Long {
        checkInitialized()
        return vkRenderPass
    }

    private fun checkInitialized() {
        if (!isInitialized.get()) {
            throw IllegalStateException("VulkanRenderer not initialized")
        }
    }

    fun setFilter(filter: VulkanFilter) {
        this.filter = filter
    }

    fun startRendering() {
        if (!isInitialized.get()) {
            Log.e(TAG, "Cannot start rendering - not initialized")
            return
        }

        if (isRendering.compareAndSet(false, true)) {
            renderThread = Thread({
                android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_DISPLAY)
                Log.d(TAG, "Render thread started")
                runRenderLoop()
            }, "VulkanRenderThread").apply {
                start()
            }
        } else {
            Log.w(TAG, "Already rendering")
        }
    }

    private fun runRenderLoop() {
        while (isRendering.get()) {
            try {
                renderFrame()
                Thread.sleep(16) // ~60fps
            } catch (e: InterruptedException) {
                Log.d(TAG, "Render thread interrupted")
                break
            } catch (e: Exception) {
                Log.e(TAG, "Error in render loop", e)
            }
        }
        Log.d(TAG, "Render thread stopped")
    }

    fun stopRendering() {
        if (isRendering.compareAndSet(true, false)) {
            renderThread?.interrupt()
            renderThread?.join(1000)
            renderThread = null
            Log.d(TAG, "Rendering stopped")
        }
    }

    private fun renderFrame() {
        lock.withLock {
            if (!isInitialized.get()) {
                Log.w(TAG, "renderFrame called but not initialized")
                return
            }

            try {
                // 获取下一个图像
                val imageIndex = nativeAcquireNextImage(vkDevice, vkSwapchain)
                if (imageIndex < 0) {
                    Log.w(TAG, "Failed to acquire image: $imageIndex")
                    return
                }
                // Log.v(TAG, "Acquired image index: $imageIndex")  // 取消注释查看每帧

                recordCommandBuffer(imageIndex)
                submitAndPresent(imageIndex)

            } catch (e: Exception) {
                Log.e(TAG, "Error rendering frame", e)
            }
        }
    }

    private fun recordCommandBuffer(imageIndex: Int) {
        // Log.v(TAG, "Recording command buffer for image $imageIndex")  // 取消注释

        nativeBeginCommandBuffer(vkCommandBuffer)
        nativeBeginRenderPass(vkCommandBuffer, vkRenderPass, imageIndex, vkSwapchain)

        filter?.let { vulkanFilter ->
            val textureImageView = nativeGetTextureImageView(inputTexture)
            if (textureImageView == 0L) {
                Log.e(TAG, "Invalid texture image view!")
                return
            }

            val identityMatrix = floatArrayOf(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            )

            // Log.v(TAG, "Calling filter.draw() with textureView=$textureImageView")  // 取消注释
            vulkanFilter.draw(vkCommandBuffer, textureImageView, identityMatrix)
        } ?: run {
            Log.e(TAG, "Filter is null!")
        }

        nativeEndRenderPass(vkCommandBuffer)
        nativeEndCommandBuffer(vkCommandBuffer)
    }

    private fun submitAndPresent(imageIndex: Int) {
        // Log.v(TAG, "Submitting and presenting image $imageIndex")  // 取消注释
        nativeSubmitCommandBuffer(vkDevice, vkCommandBuffer)
        nativePresentImage(vkDevice, vkSwapchain, imageIndex)
    }

    fun resize(width: Int, height: Int) {
        if (width <= 0 || height <= 0) {
            Log.w(TAG, "Invalid size: ${width}x${height}")
            return
        }

        lock.withLock {
            if (!isInitialized.get()) return

            val wasRendering = isRendering.get()
            if (wasRendering) {
                stopRendering()
            }

            Log.d(TAG, "Resizing swapchain to ${width}x${height}")

            val success = nativeResizeSwapchain(
                vkDevice,
                vkSwapchain,
                vkRenderPass,
                width,
                height
            )

            if (success) {
                if (wasRendering) {
                    startRendering()
                } else {

                }
            } else {
                Log.e(TAG, "Failed to resize swapchain")
            }
        }
    }

    fun release() {
        lock.withLock {
            stopRendering()
            cleanupInternal()
        }
    }

    private fun cleanupInternal() {
        if (!isInitialized.compareAndSet(true, false)) {
            // 已经清理过了
            return
        }

        Log.d(TAG, "Cleaning up Vulkan resources")

        // 按创建的逆序销毁资源
        destroyResource(inputTexture, "Texture") { nativeDestroyTexture(vkDevice, it) }
        destroyResource(vkCommandBuffer, "CommandBuffer") {
            nativeFreeCommandBuffer(vkDevice, vkCommandPool, it)
        }
        destroyResource(vkCommandPool, "CommandPool") { nativeDestroyCommandPool(vkDevice, it) }
        destroyResource(vkSwapchain, "Swapchain") { nativeDestroySwapchain(vkDevice, it) }
        destroyResource(vkRenderPass, "RenderPass") { nativeDestroyRenderPass(vkDevice, it) }
        destroyResource(vkDevice, "Device") { nativeDestroyDevice(it) }
        destroyResource(vkInstance, "Instance") { nativeDestroyInstance(it) }

        // 清理句柄
        inputTexture = 0
        vkCommandBuffer = 0
        vkCommandPool = 0
        vkSwapchain = 0
        vkRenderPass = 0
        vkDevice = 0
        vkInstance = 0

        Log.d(TAG, "Cleanup completed")
    }

    private inline fun destroyResource(handle: Long, name: String, destroy: (Long) -> Unit) {
        if (handle != 0L) {
            try {
                destroy(handle)
                Log.d(TAG, "✓ Destroyed $name")
            } catch (e: Exception) {
                Log.e(TAG, "Error destroying $name", e)
            }
        }
    }

    // Native方法声明
    private external fun nativeCreateInstance(): Long
    private external fun nativeCreateDevice(instance: Long, surface: Surface): Long
    private external fun nativeCreateRenderPass(device: Long): Long
    private external fun nativeCreateSwapchain(device: Long, surface: Surface): Long
    private external fun nativeCreateCommandPool(device: Long): Long
    private external fun nativeAllocateCommandBuffer(device: Long, commandPool: Long): Long
    private external fun nativeAcquireNextImage(device: Long, swapchain: Long): Int
    private external fun nativeBeginCommandBuffer(commandBuffer: Long)
    private external fun nativeBeginRenderPass(
        commandBuffer: Long,
        renderPass: Long,
        imageIndex: Int,
        vkSwapchain: Long
    )
    private external fun nativeEndRenderPass(commandBuffer: Long)
    private external fun nativeEndCommandBuffer(commandBuffer: Long)
    private external fun nativeSubmitCommandBuffer(device: Long, commandBuffer: Long)
    private external fun nativePresentImage(device: Long, swapchain: Long, imageIndex: Int)
    private external fun nativeGetSwapchainImageView(swapchain: Long, imageIndex: Int): Long
    private external fun nativeResizeSwapchain(
        deviceHandle: Long,
        swapchainHandle: Long,
        renderPass: Long,
        width: Int,
        height: Int
    ): Boolean
    private external fun nativeDestroySwapchain(device: Long, swapchain: Long)
    private external fun nativeDestroyRenderPass(device: Long, renderPass: Long)
    private external fun nativeDestroyCommandPool(device: Long, commandPool: Long)
    private external fun nativeFreeCommandBuffer(device: Long, commandPool: Long, commandBuffer: Long)
    private external fun nativeDestroyDevice(device: Long)
    private external fun nativeDestroyInstance(instance: Long)
    private external fun nativeCreateTestTexture(device: Long): Long
    private external fun nativeDestroyTexture(device: Long, texture: Long)
    private external fun nativeGetTextureImageView(texture: Long): Long
    private external fun nativeCreateFramebuffers(device: Long, swapchain: Long, renderPass: Long): Boolean
    private external fun nativeGetTextureSampler(texture: Long): Long
    companion object {
        private const val TAG = "VulkanRenderer"

        init {
            System.loadLibrary("myapplication")
        }
    }
}