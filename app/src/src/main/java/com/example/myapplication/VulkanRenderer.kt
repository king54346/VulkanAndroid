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

    // 🔥 优化: 多个命令缓冲区用于并行渲染
    private var vkCommandBuffers: LongArray = LongArray(0)

    // 🔥 优化: 添加同步对象
    private var imageAvailableSemaphores: LongArray = LongArray(0)
    private var renderFinishedSemaphores: LongArray = LongArray(0)
    private var inFlightFences: LongArray = LongArray(0)
    private var currentFrame = 0
    private val MAX_FRAMES_IN_FLIGHT = 2  // 双缓冲

    private var inputTexture: Long = 0
    private var filter: VulkanFilter? = null

    private val isInitialized = AtomicBoolean(false)
    private val isRendering = AtomicBoolean(false)
    private var renderThread: Thread? = null
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
        // 1-8. 原有的初始化代码保持不变
        vkInstance = nativeCreateInstance()
        if (!validateHandle(vkInstance, "Instance")) return false

        vkDevice = nativeCreateDevice(vkInstance, surface)
        if (!validateHandle(vkDevice, "Device")) {
            cleanupInternal()
            return false
        }

        vkRenderPass = nativeCreateRenderPass(vkDevice)
        if (!validateHandle(vkRenderPass, "RenderPass")) {
            cleanupInternal()
            return false
        }

        vkSwapchain = nativeCreateSwapchain(vkDevice, surface)
        if (!validateHandle(vkSwapchain, "Swapchain")) {
            cleanupInternal()
            return false
        }

        if (!nativeCreateFramebuffers(vkDevice, vkSwapchain, vkRenderPass)) {
            Log.e(TAG, "Failed to create framebuffers")
            cleanupInternal()
            return false
        }
        Log.d(TAG, "✓ Framebuffers created")

        vkCommandPool = nativeCreateCommandPool(vkDevice)
        if (!validateHandle(vkCommandPool, "CommandPool")) {
            cleanupInternal()
            return false
        }

        // 🔥 优化: 创建多个命令缓冲区
        val imageCount = nativeGetSwapchainImageCount(vkSwapchain)
        vkCommandBuffers = LongArray(imageCount)
        if (!nativeAllocateCommandBuffers(vkDevice, vkCommandPool, imageCount, vkCommandBuffers)) {
            Log.e(TAG, "Failed to allocate command buffers")
            cleanupInternal()
            return false
        }
        Log.d(TAG, "✓ Allocated $imageCount command buffers")

        // 🔥 优化: 创建同步对象
        imageAvailableSemaphores = LongArray(MAX_FRAMES_IN_FLIGHT)
        renderFinishedSemaphores = LongArray(MAX_FRAMES_IN_FLIGHT)
        inFlightFences = LongArray(MAX_FRAMES_IN_FLIGHT)

        if (!nativeCreateSyncObjects(
                vkDevice,
                MAX_FRAMES_IN_FLIGHT,
                imageAvailableSemaphores,
                renderFinishedSemaphores,
                inFlightFences
            )) {
            Log.e(TAG, "Failed to create sync objects")
            cleanupInternal()
            return false
        }
        Log.d(TAG, "✓ Sync objects created")

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
                // 🔥 优化: 不需要 sleep，GPU 会自然限制帧率
                // Thread.sleep(16) // 移除这行
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
        if (!isInitialized.get()) {
            Log.w(TAG, "renderFrame called but not initialized")
            return
        }

        try {
            // 🔥 优化: 等待前一帧完成（但不阻塞 CPU）
            nativeWaitForFence(vkDevice, inFlightFences[currentFrame])

            // 🔥 优化: 获取下一个图像（使用信号量）
            val result = nativeAcquireNextImageWithSemaphore(
                vkDevice,
                vkSwapchain,
                imageAvailableSemaphores[currentFrame]
            )

            val imageIndex = (result and 0xFFFFFFFF).toInt()
            val resultCode = (result shr 32).toInt()

            if (resultCode < 0) {
                Log.w(TAG, "Failed to acquire image: $resultCode")
                return
            }

            // 🔥 优化: 重置 fence
            nativeResetFence(vkDevice, inFlightFences[currentFrame])

            // 🔥 优化: 只在需要时录制命令缓冲区（可以预录制）
            recordCommandBuffer(imageIndex)

            // 🔥 优化: 提交时使用信号量同步
            nativeSubmitCommandBufferWithSync(
                vkDevice,
                vkCommandBuffers[imageIndex],
                imageAvailableSemaphores[currentFrame],
                renderFinishedSemaphores[currentFrame],
                inFlightFences[currentFrame]
            )

            // 🔥 优化: Present 时等待渲染完成信号量
            nativePresentImageWithSync(
                vkDevice,
                vkSwapchain,
                imageIndex,
                renderFinishedSemaphores[currentFrame]
            )

            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT

        } catch (e: Exception) {
            Log.e(TAG, "Error rendering frame", e)
        }
    }

    private fun recordCommandBuffer(imageIndex: Int) {
        val commandBuffer = vkCommandBuffers[imageIndex]

        // 🔥 优化: 重置命令缓冲区（不需要每次重新分配）
        nativeResetCommandBuffer(commandBuffer)

        nativeBeginCommandBuffer(commandBuffer)
        nativeBeginRenderPass(commandBuffer, vkRenderPass, imageIndex, vkSwapchain)

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

            vulkanFilter.draw(commandBuffer, textureImageView, identityMatrix)
        }

        nativeEndRenderPass(commandBuffer)
        nativeEndCommandBuffer(commandBuffer)
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

            // 🔥 优化: 等待所有帧完成
            nativeWaitForAllFences(vkDevice, inFlightFences)

            val success = nativeResizeSwapchain(
                vkDevice,
                vkSwapchain,
                vkRenderPass,
                width,
                height
            )

            if (success) {
                // 重新分配命令缓冲区
                val imageCount = nativeGetSwapchainImageCount(vkSwapchain)
                if (vkCommandBuffers.size != imageCount) {
                    // 释放旧的
                    nativeFreeCommandBuffers(vkDevice, vkCommandPool, vkCommandBuffers)

                    // 分配新的
                    vkCommandBuffers = LongArray(imageCount)
                    nativeAllocateCommandBuffers(vkDevice, vkCommandPool, imageCount, vkCommandBuffers)
                }

                if (wasRendering) {
                    startRendering()
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
            return
        }

        Log.d(TAG, "Cleaning up Vulkan resources")

        // 🔥 优化: 等待所有操作完成
        if (vkDevice != 0L) {
            nativeDeviceWaitIdle(vkDevice)
        }

        // 清理同步对象
        if (inFlightFences.isNotEmpty()) {
            nativeDestroySyncObjects(
                vkDevice,
                imageAvailableSemaphores,
                renderFinishedSemaphores,
                inFlightFences
            )
            imageAvailableSemaphores = LongArray(0)
            renderFinishedSemaphores = LongArray(0)
            inFlightFences = LongArray(0)
        }

        destroyResource(inputTexture, "Texture") { nativeDestroyTexture(vkDevice, it) }

        if (vkCommandBuffers.isNotEmpty()) {
            nativeFreeCommandBuffers(vkDevice, vkCommandPool, vkCommandBuffers)
            vkCommandBuffers = LongArray(0)
        }

        destroyResource(vkCommandPool, "CommandPool") { nativeDestroyCommandPool(vkDevice, it) }
        destroyResource(vkSwapchain, "Swapchain") { nativeDestroySwapchain(vkDevice, it) }
        destroyResource(vkRenderPass, "RenderPass") { nativeDestroyRenderPass(vkDevice, it) }
        destroyResource(vkDevice, "Device") { nativeDestroyDevice(it) }
        destroyResource(vkInstance, "Instance") { nativeDestroyInstance(it) }

        inputTexture = 0
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

    // ========== Native 方法 ==========

    // 原有方法
    private external fun nativeCreateInstance(): Long
    private external fun nativeCreateDevice(instance: Long, surface: Surface): Long
    private external fun nativeCreateRenderPass(device: Long): Long
    private external fun nativeCreateSwapchain(device: Long, surface: Surface): Long
    private external fun nativeCreateCommandPool(device: Long): Long
    private external fun nativeBeginCommandBuffer(commandBuffer: Long)
    private external fun nativeBeginRenderPass(
        commandBuffer: Long,
        renderPass: Long,
        imageIndex: Int,
        vkSwapchain: Long
    )
    private external fun nativeEndRenderPass(commandBuffer: Long)
    private external fun nativeEndCommandBuffer(commandBuffer: Long)
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
    private external fun nativeDestroyDevice(device: Long)
    private external fun nativeDestroyInstance(instance: Long)
    private external fun nativeCreateTestTexture(device: Long): Long
    private external fun nativeDestroyTexture(device: Long, texture: Long)
    private external fun nativeGetTextureImageView(texture: Long): Long
    private external fun nativeCreateFramebuffers(device: Long, swapchain: Long, renderPass: Long): Boolean

    // 🔥 新增的优化方法
    private external fun nativeGetSwapchainImageCount(swapchain: Long): Int
    private external fun nativeAllocateCommandBuffers(
        device: Long,
        commandPool: Long,
        count: Int,
        commandBuffers: LongArray
    ): Boolean
    private external fun nativeFreeCommandBuffers(
        device: Long,
        commandPool: Long,
        commandBuffers: LongArray
    )
    private external fun nativeResetCommandBuffer(commandBuffer: Long)

    // 同步对象
    private external fun nativeCreateSyncObjects(
        device: Long,
        count: Int,
        imageAvailableSemaphores: LongArray,
        renderFinishedSemaphores: LongArray,
        inFlightFences: LongArray
    ): Boolean
    private external fun nativeDestroySyncObjects(
        device: Long,
        imageAvailableSemaphores: LongArray,
        renderFinishedSemaphores: LongArray,
        inFlightFences: LongArray
    )
    private external fun nativeWaitForFence(device: Long, fence: Long)
    private external fun nativeResetFence(device: Long, fence: Long)
    private external fun nativeWaitForAllFences(device: Long, fences: LongArray)

    // 改进的渲染方法
    private external fun nativeAcquireNextImageWithSemaphore(
        device: Long,
        swapchain: Long,
        semaphore: Long
    ): Long  // 返回 (resultCode << 32) | imageIndex

    private external fun nativeSubmitCommandBufferWithSync(
        device: Long,
        commandBuffer: Long,
        waitSemaphore: Long,
        signalSemaphore: Long,
        fence: Long
    )

    private external fun nativePresentImageWithSync(
        device: Long,
        swapchain: Long,
        imageIndex: Int,
        waitSemaphore: Long
    )

    private external fun nativeDeviceWaitIdle(device: Long)

    companion object {
        private const val TAG = "VulkanRenderer"

        init {
            System.loadLibrary("myapplication")
        }
    }
}