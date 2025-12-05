package com.genymobile.scrcpy.vulkan

import VulkanException
import android.content.Context
import android.util.Log
import java.io.IOException

object ShaderLoader {
    @Throws(IOException::class)
    fun loadShader(context: Context, assetPath: String): ByteArray {
        return context.assets.open(assetPath).use { inputStream ->
            inputStream.readBytes()
        }
    }

    @Throws(IOException::class)
    fun loadVertexShader(context: Context): ByteArray {
        return loadShader(context, "shaders/b_vert.spv")  // 使用全屏三角形顶点着色器
    }

    @Throws(IOException::class)
    fun loadFragmentShader(context: Context): ByteArray {
        return loadShader(context, "shaders/b_frag.spv")  // 使用Push Constants测试着色器
    }
}

class SimpleVulkanFilter(private val context: Context) : VulkanFilter {
    private var vkDevice: Long = 0
    private var vkPipeline: Long = 0
    private var vkPipelineLayout: Long = 0
    private var vkDescriptorSetLayout: Long = 0
    private var vkDescriptorPool: Long = 0
    private var vkDescriptorSet: Long = 0
    private var vkSampler: Long = 0

    private var vertexShaderModule: Long = 0
    private var fragmentShaderModule: Long = 0

    private var isInitialized = false
    private var currentTextureView: Long = 0

    // 添加：时间跟踪
    private val startTimeNanos = System.nanoTime()

    // 添加：表面尺寸
    private var surfaceWidth: Int = 1920
    private var surfaceHeight: Int = 1080

    override fun init(device: Long, renderPass: Long) {
        if (isInitialized) {
            Log.w(TAG, "Already initialized")
            return
        }

        this.vkDevice = device
        Log.d(TAG, "=== Initializing SimpleVulkanFilter ===")

        try {
            // 1. Load shaders
            val vertexShaderCode = ShaderLoader.loadVertexShader(context)
            val fragmentShaderCode = ShaderLoader.loadFragmentShader(context)
            Log.d(TAG, "✓ Shaders loaded: vert=${vertexShaderCode.size} bytes, frag=${fragmentShaderCode.size} bytes")

            // 2. Create shader modules
            vertexShaderModule = nativeCreateShaderModule(device, vertexShaderCode)
            fragmentShaderModule = nativeCreateShaderModule(device, fragmentShaderCode)

            if (vertexShaderModule == 0L || fragmentShaderModule == 0L) {
                throw VulkanException("Failed to create shader modules")
            }
            Log.d(TAG, "✓ Shader modules created")

            // 3. Create descriptor set layout
            vkDescriptorSetLayout = nativeCreateDescriptorSetLayout(device)
            if (vkDescriptorSetLayout == 0L) {
                throw VulkanException("Failed to create descriptor set layout")
            }
            Log.d(TAG, "✓ Descriptor set layout created")

            // 4. Create pipeline layout (修改：包含Push Constants配置)
            vkPipelineLayout =  nativeCreatePipelineLayout(device, vkDescriptorSetLayout)
            if (vkPipelineLayout == 0L) {
                throw VulkanException("Failed to create pipeline layout")
            }
            Log.d(TAG, "✓ Pipeline layout created with Push Constants support")

            // 5. Create graphics pipeline
            vkPipeline = nativeCreateGraphicsPipeline(
                device,
                renderPass,
                vkPipelineLayout,
                vertexShaderModule,
                fragmentShaderModule
            )
            if (vkPipeline == 0L) {
                throw VulkanException("Failed to create graphics pipeline")
            }
            Log.d(TAG, "✓ Graphics pipeline created")

            // 6. Create descriptor pool
            vkDescriptorPool = nativeCreateDescriptorPool(device)
            if (vkDescriptorPool == 0L) {
                throw VulkanException("Failed to create descriptor pool")
            }
            Log.d(TAG, "✓ Descriptor pool created")

            // 7. Create sampler
            vkSampler = nativeCreateSampler(device)
            if (vkSampler == 0L) {
                throw VulkanException("Failed to create sampler")
            }
            Log.d(TAG, "✓ Sampler created")

            // 8. Allocate descriptor set
            vkDescriptorSet = nativeAllocateDescriptorSet(
                vkDevice,
                vkDescriptorPool,
                vkDescriptorSetLayout
            )

            if (vkDescriptorSet == 0L) {
                throw VulkanException("Failed to allocate descriptor set")
            }
            Log.d(TAG, "✓ Descriptor set allocated")

            isInitialized = true
            Log.i(TAG, "=== SimpleVulkanFilter initialized successfully ===")

        } catch (e: Exception) {
            Log.e(TAG, "Failed to initialize filter", e)
            release()
            throw e
        }
    }

    // 添加：设置表面尺寸
    fun setSurfaceSize(width: Int, height: Int) {
        surfaceWidth = width
        surfaceHeight = height
        Log.d(TAG, "Surface size set: ${width}x${height}")
    }

    override fun draw(commandBuffer: Long, inputTexture: Long, transformMatrix: FloatArray) {
        if (!isInitialized) {
            Log.e(TAG, "Filter not initialized!")
            return
        }

        if (inputTexture == 0L) {
            Log.e(TAG, "Invalid input texture: 0")
            return
        }

        // Update descriptor set if texture changed
        if (currentTextureView != inputTexture) {
            Log.d(TAG, "Updating descriptor set with texture: $inputTexture")
            nativeUpdateDescriptorSet(vkDevice, vkDescriptorSet, inputTexture, vkSampler)
            currentTextureView = inputTexture
        }

        // Bind pipeline
        nativeBindPipeline(commandBuffer, vkPipeline)

        // Bind descriptor sets
        nativeBindDescriptorSets(commandBuffer, vkPipelineLayout, vkDescriptorSet)

        // 添加：Push Constants (resolution + time)
        if (surfaceWidth > 0 && surfaceHeight > 0) {
            val currentTime = (System.nanoTime() - startTimeNanos) / 1_000_000_000.0f
            val pushConstantsData = floatArrayOf(
                surfaceWidth.toFloat(),   // resolution.x
                surfaceHeight.toFloat(),  // resolution.y
                currentTime,              // time
                0.0f                      // padding
            )

            // 每100帧打印一次日志
            if (frameCount++ % 100 == 0) {
                Log.d(TAG, "Push Constants: ${surfaceWidth}x${surfaceHeight}, time=${"%.2f".format(currentTime)}")
            }

            nativePushConstants(commandBuffer, vkPipelineLayout, pushConstantsData)
        } else {
            Log.w(TAG, "Surface size not set, skipping Push Constants")
        }

        // Draw fullscreen triangle (3 vertices, not 6)
        // 全屏三角形只需要3个顶点
        nativeDraw(commandBuffer, 3, 1, 0, 0)
    }

    override fun release() {
        if (!isInitialized) return

        Log.d(TAG, "Releasing filter resources")

        if (vkDescriptorPool != 0L) {
            nativeDestroyDescriptorPool(vkDevice, vkDescriptorPool)
            vkDescriptorPool = 0L
        }
        if (vkSampler != 0L) {
            nativeDestroySampler(vkDevice, vkSampler)
            vkSampler = 0L
        }
        if (vkPipeline != 0L) {
            nativeDestroyPipeline(vkDevice, vkPipeline)
            vkPipeline = 0L
        }
        if (vkPipelineLayout != 0L) {
            nativeDestroyPipelineLayout(vkDevice, vkPipelineLayout)
            vkPipelineLayout = 0L
        }
        if (vkDescriptorSetLayout != 0L) {
            nativeDestroyDescriptorSetLayout(vkDevice, vkDescriptorSetLayout)
            vkDescriptorSetLayout = 0L
        }
        if (vertexShaderModule != 0L) {
            nativeDestroyShaderModule(vkDevice, vertexShaderModule)
            vertexShaderModule = 0L
        }
        if (fragmentShaderModule != 0L) {
            nativeDestroyShaderModule(vkDevice, fragmentShaderModule)
            fragmentShaderModule = 0L
        }

        isInitialized = false
        currentTextureView = 0
        Log.d(TAG, "Filter resources released")
    }

    // Native methods
    private external fun nativeCreateDescriptorSetLayout(device: Long): Long


    private external fun nativeCreateGraphicsPipeline(
        device: Long,
        renderPass: Long,
        pipelineLayout: Long,
        vertShaderModule: Long,
        fragShaderModule: Long
    ): Long
    private external fun nativeCreateDescriptorPool(device: Long): Long
    private external fun nativeCreateSampler(device: Long): Long
    private external fun nativeCreateShaderModule(device: Long, code: ByteArray): Long
    private external fun nativeAllocateDescriptorSet(
        device: Long,
        descriptorPool: Long,
        descriptorSetLayout: Long
    ): Long
    private external fun nativeUpdateDescriptorSet(
        device: Long,
        descriptorSet: Long,
        imageView: Long,
        sampler: Long
    )
    private external fun nativeCreatePipelineLayout(device: Long, descriptorSetLayout: Long): Long
    private external fun nativeBindPipeline(commandBuffer: Long, pipeline: Long)
    private external fun nativeBindDescriptorSets(
        commandBuffer: Long,
        pipelineLayout: Long,
        descriptorSet: Long
    )

    // 添加：Push Constants方法
    private external fun nativePushConstants(
        commandBuffer: Long,
        pipelineLayout: Long,
        data: FloatArray
    )

    private external fun nativeDraw(
        commandBuffer: Long,
        vertexCount: Int,
        instanceCount: Int,
        firstVertex: Int,
        firstInstance: Int
    )
    private external fun nativeDestroyDescriptorPool(device: Long, descriptorPool: Long)
    private external fun nativeDestroySampler(device: Long, sampler: Long)
    private external fun nativeDestroyPipeline(device: Long, pipeline: Long)
    private external fun nativeDestroyPipelineLayout(device: Long, pipelineLayout: Long)
    private external fun nativeDestroyDescriptorSetLayout(device: Long, descriptorSetLayout: Long)
    private external fun nativeDestroyShaderModule(device: Long, shaderModule: Long)

    companion object {
        const val TAG = "SimpleVulkanFilter"
        private var frameCount = 0  // 添加：帧计数器，用于日志控制

        init {
            System.loadLibrary("myapplication")
        }
    }
}