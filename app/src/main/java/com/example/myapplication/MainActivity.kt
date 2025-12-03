package com.example.myapplication

import android.graphics.PixelFormat
import android.os.Bundle
import android.util.Log
import android.view.SurfaceHolder
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.example.myapplication.databinding.ActivityMainBinding
import com.genymobile.scrcpy.vulkan.SimpleVulkanFilter

class MainActivity : AppCompatActivity(), SurfaceHolder.Callback {

    private lateinit var binding: ActivityMainBinding
    private var vulkanRenderer: VulkanRenderer? = null
    private var vulkanFilter: SimpleVulkanFilter? = null

    private var isVulkanInitialized = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupSurfaceView()
    }

    private fun setupSurfaceView() {
        with(binding.surfaceView) {
            holder.addCallback(this@MainActivity)
            holder.setFormat(PixelFormat.RGBA_8888)
        }
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.d(TAG, "surfaceCreated")
        initializeVulkan(holder)
    }

    private fun initializeVulkan(holder: SurfaceHolder) {
        if (isVulkanInitialized) {
            Log.w(TAG, "Vulkan already initialized")
            return
        }

        try {
            // 1. 创建 renderer
            val renderer = VulkanRenderer(holder.surface)
            vulkanRenderer = renderer

            // 2. 初始化 Vulkan
            if (!renderer.initialize()) {
                showError("Failed to initialize Vulkan")
                cleanup()
                return
            }

            // 3. 创建并初始化 filter
            val filter = SimpleVulkanFilter(this).apply {
                init(renderer.getDevice(), renderer.getRenderPass())
            }
            vulkanFilter = filter

            // 4. 连接组件
            renderer.setFilter(filter)

            // 5. 开始渲染
            renderer.startRendering()

            isVulkanInitialized = true
            Log.i(TAG, "Vulkan rendering started successfully")

        } catch (e: Exception) {
            Log.e(TAG, "Error initializing Vulkan", e)
            showError("Initialization error: ${e.message}")
            cleanup()
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.d(TAG, "surfaceChanged: ${width}x${height}, format=$format")

        // 如果需要动态调整大小，取消注释以下行
        // vulkanRenderer?.resize(width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.d(TAG, "surfaceDestroyed")
        cleanup()
    }

    override fun onDestroy() {
        super.onDestroy()
        Log.d(TAG, "onDestroy")
        cleanup()
    }

    private fun cleanup() {
        if (!isVulkanInitialized) return

        Log.d(TAG, "Cleaning up resources")

        vulkanRenderer?.stopRendering()
        vulkanFilter?.release()
        vulkanRenderer?.release()

        vulkanFilter = null
        vulkanRenderer = null
        isVulkanInitialized = false
    }

    private fun showError(message: String) {
        runOnUiThread {
            Toast.makeText(this, message, Toast.LENGTH_LONG).show()
            Log.e(TAG, message)
        }
    }

    external fun stringFromJNI(): String

    companion object {
        private const val TAG = "MainActivity"

        init {
            System.loadLibrary("myapplication")
        }
    }
}