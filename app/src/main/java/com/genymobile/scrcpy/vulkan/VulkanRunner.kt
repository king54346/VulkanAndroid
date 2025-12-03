package com.genymobile.scrcpy.vulkan

import VulkanException
import android.graphics.SurfaceTexture
import android.os.Build
import android.os.Handler
import android.os.HandlerThread
import android.view.Surface
import androidx.annotation.RequiresApi
import com.genymobile.scrcpy.device.Size
import java.util.concurrent.Semaphore

class VulkanRunner @JvmOverloads constructor(
    private val filter: VulkanFilter,
    private val overrideTransformMatrix: FloatArray? = null
) {
    private var vkInstance: Long = 0
    private var vkPhysicalDevice: Long = 0
    private var vkDevice: Long = 0
    private var vkQueue: Long = 0
    private var vkCommandPool: Long = 0
    private var vkSwapchain: Long = 0

    private var vkRenderPass: Long = 0
    private var vkFramebuffers: LongArray? = null
    private var vkCommandBuffers: LongArray? = null

    private var vkImageAvailableSemaphore: Long = 0
    private var vkRenderFinishedSemaphore: Long = 0
    private var vkFence: Long = 0

    private var surfaceTexture: SurfaceTexture? = null
    private var inputSurface: Surface? = null
    private var outputSurface: Surface? = null

    private var vkSurface: Long = 0
    private var vkExternalTexture: Long = 0
    private var vkExternalImage: Long = 0
    private var vkExternalMemory: Long = 0

    private var stopped = false
    private var inputSize: Size? = null
    private var outputSize: Size? = null

    @Throws(VulkanException::class)
    fun start(inputSize: Size, outputSize: Size, outputSurface: Surface): Surface? {
        initOnce()

        this.inputSize = inputSize
        this.outputSize = outputSize
        this.outputSurface = outputSurface

        val sem = Semaphore(0)
        val throwableRef = arrayOfNulls<Throwable>(1)

        handler!!.post {
            try {
                run(inputSize, outputSize, outputSurface)
            } catch (throwable: Throwable) {
                throwableRef[0] = throwable
            } finally {
                sem.release()
            }
        }

        try {
            sem.acquire()
        } catch (e: InterruptedException) {
            Thread.currentThread().interrupt()
        }

        val throwable = throwableRef[0]
        if (throwable != null) {
            if (throwable is VulkanException) {
                throw throwable
            }
            throw VulkanException("Asynchronous Vulkan runner init failed", throwable)
        }

        return inputSurface
    }

    @Throws(VulkanException::class)
    private fun run(inputSize: Size, outputSize: Size, outputSurface: Surface) {
        // 1. Create Vulkan Instance
        createInstance()

        // 2. Create Android Surface
        createAndroidSurface(outputSurface)

        // 3. Pick Physical Device
        pickPhysicalDevice()

        // 4. Create Logical Device and Queue
        createLogicalDevice()

        // 5. Create Swapchain
        createSwapchain(outputSize)

        // 6. Create Render Pass
        createRenderPass()

        // 7. Create Framebuffers
        createFramebuffers()

        // 8. Create Command Pool and Buffers
        createCommandPool()
        createCommandBuffers()

        // 9. Create Synchronization Objects
        createSyncObjects()

        // 10. Create External Texture (for SurfaceTexture)
        createExternalTexture(inputSize)

        // 11. Initialize Filter
        filter.init(vkDevice, vkRenderPass)

        // 12. Setup SurfaceTexture
        setupSurfaceTexture(inputSize)
    }

    private fun createInstance() {
        val appInfo = VkApplicationInfo(
            applicationName = "ScrcpyVulkan",
            applicationVersion = 1,
            engineName = "ScrcpyEngine",
            engineVersion = 1,
            apiVersion = VK_API_VERSION_1_1
        )

        val extensions = arrayOf(
            "VK_KHR_surface",
            "VK_KHR_android_surface",
            "VK_ANDROID_external_memory_android_hardware_buffer"
        )

        vkInstance = nativeCreateInstance(appInfo, extensions)
        if (vkInstance == 0L) {
            throw VulkanException("Failed to create Vulkan instance")
        }
    }

    private fun createAndroidSurface(surface: Surface) {
        vkSurface = nativeCreateAndroidSurface(vkInstance, surface)
        if (vkSurface == 0L) {
            throw VulkanException("Failed to create Android surface")
        }
    }

    private fun pickPhysicalDevice() {
        val devices = nativeEnumeratePhysicalDevices(vkInstance)
        if (devices.isEmpty()) {
            throw VulkanException("No Vulkan physical devices found")
        }

        // Pick first suitable device (could add scoring logic)
        vkPhysicalDevice = devices[0]
    }

    private fun createLogicalDevice() {
        val queueFamilyIndex = nativeFindQueueFamily(vkPhysicalDevice, vkSurface)
        if (queueFamilyIndex < 0) {
            throw VulkanException("No suitable queue family found")
        }

        val deviceExtensions = arrayOf(
            "VK_KHR_swapchain",
            "VK_ANDROID_external_memory_android_hardware_buffer"
        )

        val result = nativeCreateLogicalDevice(
            vkPhysicalDevice,
            queueFamilyIndex,
            deviceExtensions
        )

        vkDevice = result.device
        vkQueue = result.queue

        if (vkDevice == 0L || vkQueue == 0L) {
            throw VulkanException("Failed to create logical device")
        }
    }

    private fun createSwapchain(size: Size) {
        vkSwapchain = nativeCreateSwapchain(
            vkDevice,
            vkPhysicalDevice,
            vkSurface,
            size.width,
            size.height
        )

        if (vkSwapchain == 0L) {
            throw VulkanException("Failed to create swapchain")
        }
    }

    private fun createRenderPass() {
        vkRenderPass = nativeCreateRenderPass(vkDevice)
        if (vkRenderPass == 0L) {
            throw VulkanException("Failed to create render pass")
        }
    }

    private fun createFramebuffers() {
        val swapchainImages = nativeGetSwapchainImages(vkDevice, vkSwapchain)
        vkFramebuffers = LongArray(swapchainImages.size)

        for (i in swapchainImages.indices) {
            vkFramebuffers!![i] = nativeCreateFramebuffer(
                vkDevice,
                vkRenderPass,
                swapchainImages[i],
                outputSize!!.width,
                outputSize!!.height
            )
        }
    }

    private fun createCommandPool() {
        vkCommandPool = nativeCreateCommandPool(vkDevice, 0) // queue family index
        if (vkCommandPool == 0L) {
            throw VulkanException("Failed to create command pool")
        }
    }

    private fun createCommandBuffers() {
        val count = vkFramebuffers?.size ?: 0
        vkCommandBuffers = nativeAllocateCommandBuffers(vkDevice, vkCommandPool, count)
    }

    private fun createSyncObjects() {
        vkImageAvailableSemaphore = nativeCreateSemaphore(vkDevice)
        vkRenderFinishedSemaphore = nativeCreateSemaphore(vkDevice)
        vkFence = nativeCreateFence(vkDevice)

        if (vkImageAvailableSemaphore == 0L ||
            vkRenderFinishedSemaphore == 0L ||
            vkFence == 0L) {
            throw VulkanException("Failed to create sync objects")
        }
    }

    private fun createExternalTexture(size: Size) {
        // Create external image backed by AHardwareBuffer
        val result = nativeCreateExternalImage(
            vkDevice,
            vkPhysicalDevice,
            size.width,
            size.height
        )

        vkExternalImage = result.image
        vkExternalMemory = result.memory
        vkExternalTexture = result.imageView

        if (vkExternalImage == 0L || vkExternalMemory == 0L) {
            throw VulkanException("Failed to create external texture")
        }
    }

    private fun setupSurfaceTexture(size: Size) {
        // Get AHardwareBuffer from Vulkan external memory
        val hardwareBuffer = nativeGetHardwareBuffer(vkExternalMemory)

        // Create SurfaceTexture from hardware buffer
        surfaceTexture = SurfaceTexture(false) // detached mode
        nativeAttachHardwareBufferToSurfaceTexture(surfaceTexture, hardwareBuffer)
        surfaceTexture!!.setDefaultBufferSize(size.width, size.height)

        inputSurface = Surface(surfaceTexture)

        surfaceTexture!!.setOnFrameAvailableListener({
            if (stopped) {
                return@setOnFrameAvailableListener
            }
            render()
        }, handler)
    }

    private fun render() {
        // Wait for previous frame
        nativeWaitForFence(vkDevice, vkFence)
        nativeResetFence(vkDevice, vkFence)

        // Acquire next image
        val imageIndex = nativeAcquireNextImage(
            vkDevice,
            vkSwapchain,
            vkImageAvailableSemaphore
        )

        if (imageIndex < 0) {
            return // Swapchain needs recreation
        }

        // Update external texture from SurfaceTexture
        surfaceTexture!!.updateTexImage()
        val timestamp = surfaceTexture!!.timestamp

        // Get transform matrix
        val matrix: FloatArray = if (overrideTransformMatrix != null) {
            overrideTransformMatrix
        } else {
            FloatArray(16).apply {
                surfaceTexture!!.getTransformMatrix(this)
            }
        }

        // Record command buffer
        val cmdBuffer = vkCommandBuffers!![imageIndex]
        nativeBeginCommandBuffer(cmdBuffer)

        nativeBeginRenderPass(
            cmdBuffer,
            vkRenderPass,
            vkFramebuffers!![imageIndex],
            outputSize!!.width,
            outputSize!!.height
        )

        // Draw using filter
        filter.draw(
            cmdBuffer,
            vkExternalTexture,
            matrix
        )

        nativeEndRenderPass(cmdBuffer)
        nativeEndCommandBuffer(cmdBuffer)

        // Submit
        nativeSubmitCommandBuffer(
            vkQueue,
            cmdBuffer,
            vkImageAvailableSemaphore,
            vkRenderFinishedSemaphore,
            vkFence
        )

        // Present with timestamp
        nativeQueuePresent(
            vkQueue,
            vkSwapchain,
            imageIndex,
            vkRenderFinishedSemaphore,
            timestamp
        )
    }

    fun stopAndRelease() {
        val sem = Semaphore(0)

        handler!!.post {
            stopped = true
            surfaceTexture?.setOnFrameAvailableListener(null, handler)

            filter.release()

            // Destroy Vulkan resources
            nativeDeviceWaitIdle(vkDevice)

            vkCommandBuffers?.let {
                nativeFreeCommandBuffers(vkDevice, vkCommandPool, it)
            }

            vkFramebuffers?.forEach { fb ->
                nativeDestroyFramebuffer(vkDevice, fb)
            }

            if (vkFence != 0L) nativeDestroyFence(vkDevice, vkFence)
            if (vkRenderFinishedSemaphore != 0L) nativeDestroySemaphore(vkDevice, vkRenderFinishedSemaphore)
            if (vkImageAvailableSemaphore != 0L) nativeDestroySemaphore(vkDevice, vkImageAvailableSemaphore)

            if (vkExternalTexture != 0L) nativeDestroyImageView(vkDevice, vkExternalTexture)
            if (vkExternalImage != 0L) nativeDestroyImage(vkDevice, vkExternalImage)
            if (vkExternalMemory != 0L) nativeFreeMemory(vkDevice, vkExternalMemory)

            if (vkCommandPool != 0L) nativeDestroyCommandPool(vkDevice, vkCommandPool)
            if (vkRenderPass != 0L) nativeDestroyRenderPass(vkDevice, vkRenderPass)
            if (vkSwapchain != 0L) nativeDestroySwapchain(vkDevice, vkSwapchain)
            if (vkSurface != 0L) nativeDestroySurface(vkInstance, vkSurface)
            if (vkDevice != 0L) nativeDestroyDevice(vkDevice)
            if (vkInstance != 0L) nativeDestroyInstance(vkInstance)

            surfaceTexture?.release()
            inputSurface?.release()

            sem.release()
        }

        try {
            sem.acquire()
        } catch (e: InterruptedException) {
            Thread.currentThread().interrupt()
        }
    }

    // Native method declarations (implemented in C++ via JNI)
    private external fun nativeCreateInstance(appInfo: VkApplicationInfo, extensions: Array<String>): Long
    private external fun nativeCreateAndroidSurface(instance: Long, surface: Surface): Long
    private external fun nativeEnumeratePhysicalDevices(instance: Long): LongArray
    private external fun nativeFindQueueFamily(physicalDevice: Long, surface: Long): Int
    private external fun nativeCreateLogicalDevice(physicalDevice: Long, queueFamilyIndex: Int, extensions: Array<String>): DeviceResult
    private external fun nativeCreateSwapchain(device: Long, physicalDevice: Long, surface: Long, width: Int, height: Int): Long
    private external fun nativeCreateRenderPass(device: Long): Long
    private external fun nativeGetSwapchainImages(device: Long, swapchain: Long): LongArray
    private external fun nativeCreateFramebuffer(device: Long, renderPass: Long, imageView: Long, width: Int, height: Int): Long
    private external fun nativeCreateCommandPool(device: Long, queueFamilyIndex: Int): Long
    private external fun nativeAllocateCommandBuffers(device: Long, commandPool: Long, count: Int): LongArray
    private external fun nativeCreateSemaphore(device: Long): Long
    private external fun nativeCreateFence(device: Long): Long
    private external fun nativeCreateExternalImage(device: Long, physicalDevice: Long, width: Int, height: Int): ExternalImageResult
    private external fun nativeGetHardwareBuffer(memory: Long): Any
    private external fun nativeAttachHardwareBufferToSurfaceTexture(surfaceTexture: SurfaceTexture?, hardwareBuffer: Any)
    private external fun nativeWaitForFence(device: Long, fence: Long)
    private external fun nativeResetFence(device: Long, fence: Long)
    private external fun nativeAcquireNextImage(device: Long, swapchain: Long, semaphore: Long): Int
    private external fun nativeBeginCommandBuffer(commandBuffer: Long)
    private external fun nativeBeginRenderPass(commandBuffer: Long, renderPass: Long, framebuffer: Long, width: Int, height: Int)
    private external fun nativeEndRenderPass(commandBuffer: Long)
    private external fun nativeEndCommandBuffer(commandBuffer: Long)
    private external fun nativeSubmitCommandBuffer(queue: Long, commandBuffer: Long, waitSemaphore: Long, signalSemaphore: Long, fence: Long)
    private external fun nativeQueuePresent(queue: Long, swapchain: Long, imageIndex: Int, waitSemaphore: Long, timestamp: Long)
    private external fun nativeDeviceWaitIdle(device: Long)
    private external fun nativeFreeCommandBuffers(device: Long, commandPool: Long, commandBuffers: LongArray)
    private external fun nativeDestroyFramebuffer(device: Long, framebuffer: Long)
    private external fun nativeDestroyFence(device: Long, fence: Long)
    private external fun nativeDestroySemaphore(device: Long, semaphore: Long)
    private external fun nativeDestroyImageView(device: Long, imageView: Long)
    private external fun nativeDestroyImage(device: Long, image: Long)
    private external fun nativeFreeMemory(device: Long, memory: Long)
    private external fun nativeDestroyCommandPool(device: Long, commandPool: Long)
    private external fun nativeDestroyRenderPass(device: Long, renderPass: Long)
    private external fun nativeDestroySwapchain(device: Long, swapchain: Long)
    private external fun nativeDestroySurface(instance: Long, surface: Long)
    private external fun nativeDestroyDevice(device: Long)
    private external fun nativeDestroyInstance(instance: Long)

    companion object {
        private const val VK_API_VERSION_1_1 = (1 shl 22) or (1 shl 12)

        private var handlerThread: HandlerThread? = null
        private var handler: Handler? = null
        private var quit = false

        init {
            System.loadLibrary("scrcpy-vulkan")
        }

        @Synchronized
        fun initOnce() {
            if (handlerThread == null) {
                check(!quit) { "Could not init VulkanRunner after it is quit" }
                handlerThread = HandlerThread("VulkanRunner")
                handlerThread!!.start()
                handler = Handler(handlerThread!!.looper)
            }
        }

        fun quit() {
            val thread: HandlerThread?
            synchronized(VulkanRunner::class.java) {
                thread = handlerThread
                quit = true
            }
            thread?.quitSafely()
        }

        @Throws(InterruptedException::class)
        fun join() {
            val thread: HandlerThread?
            synchronized(VulkanRunner::class.java) {
                thread = handlerThread
            }
            thread?.join()
        }
    }

    // Data classes for native results
    data class VkApplicationInfo(
        val applicationName: String,
        val applicationVersion: Int,
        val engineName: String,
        val engineVersion: Int,
        val apiVersion: Int
    )

    data class DeviceResult(
        val device: Long,
        val queue: Long
    )

    data class ExternalImageResult(
        val image: Long,
        val memory: Long,
        val imageView: Long
    )
}



