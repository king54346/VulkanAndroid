package com.example.myapplication
import android.graphics.PixelFormat
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.util.Size
import android.view.Surface
import android.view.SurfaceHolder
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.example.myapplication.databinding.ActivityMainBinding
import com.genymobile.scrcpy.util.AffineMatrix
import com.genymobile.scrcpy.vulkan.AffineVulkanFilter
import com.genymobile.scrcpy.vulkan.VulkanRunner
import com.genymobile.scrcpy.vulkan.VulkanFilter

class MainActivity : AppCompatActivity(), SurfaceHolder.Callback {
    private val animationHandler = Handler(Looper.getMainLooper())
    private lateinit var binding: ActivityMainBinding
    private var vulkanRunner: VulkanRunner? = null
    private var vulkanFilter: VulkanFilter? = null
    private var inputSurface: Surface? = null

    private var isVulkanInitialized = false
    private var outputSurface: Surface? = null

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
        outputSurface = holder.surface
        // Don't initialize yet - wait for surfaceChanged to get dimensions
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.d(TAG, "surfaceChanged: ${width}x${height}, format=$format")

        if (isVulkanInitialized) {
            Log.w(TAG, "Vulkan already initialized, ignoring surfaceChanged")
            return
        }

        outputSurface = holder.surface
        initializeVulkan(width, height)
    }

    private fun initializeVulkan(width: Int, height: Int) {
        if (isVulkanInitialized) {
            Log.w(TAG, "Vulkan already initialized")
            return
        }

        if (outputSurface == null) {
            Log.e(TAG, "Output surface is null")
            return
        }

        try {
            // 定义输入和输出尺寸
            val inputSize = Size(width, height)
            val outputSize = Size(width, height)

            // 创建 filter
            val filter = AffineVulkanFilter(this, AffineMatrix.rotate(30.0).fromCenter())
            vulkanFilter = filter

            // 创建 VulkanRunner
            val runner = VulkanRunner(filter)
            vulkanRunner = runner

            // 启动 runner，获取输入 surface（可能为 null）
            val surface = runner.start(inputSize, outputSize, outputSurface!!)

            // 🔥 修改：不管 surface 是否为 null 都继续
            if (surface == null) {
                Log.i(TAG, "VulkanRunner started with direct texture rendering (no input surface)")
                // 🔥 使用图案动画（不需要 input surface）
                startPatternAnimation(width, height)
            } else {
                Log.i(TAG, "VulkanRunner started with input surface")
                inputSurface = surface
                // 如果有 input surface，可以用其他方式写入数据
                // startFrameProducer(inputSurface!!)
            }

            isVulkanInitialized = true
            Log.i(TAG, "VulkanRunner initialization completed")

        } catch (e: Exception) {
            Log.e(TAG, "Error initializing VulkanRunner", e)
            showError("Initialization error: ${e.message}")
            cleanup()
        }
    }

    fun createGradientTexture(width: Int, height: Int): ByteArray {
        val data = ByteArray(width * height * 4) // RGBA

        for (y in 0 until height) {
            for (x in 0 until width) {
                val index = (y * width + x) * 4

                // 水平渐变：从左到右，红色到蓝色
                data[index + 0] = ((255.0 * x / width).toInt()).toByte()  // R
                data[index + 1] = 0                                         // G
                data[index + 2] = ((255.0 * (width - x) / width).toInt()).toByte() // B
                data[index + 3] = 255.toByte()                              // A
            }
        }

        return data
    }



    private fun startPatternAnimation(width: Int, height: Int) {
        textureWidth = width
        textureHeight = height
        Log.d(TAG, "Starting pattern animation: ${width}x${height}")
        animationHandler.postDelayed(patternAnimationRunnable, 33)
    }
    private fun stopPatternAnimation() {
        Log.d(TAG, "Stopping pattern animation")
        animationHandler.removeCallbacks(patternAnimationRunnable)
    }
    private val patternAnimationRunnable = object : Runnable {
        override fun run() {
            if (!isVulkanInitialized) return

            patternPhase += 0.05f

            // 选择不同的图案效果
            val pattern = when ((patternPhase / 10).toInt() % 3) {
                0 -> generateCheckerboardPattern(textureWidth, textureHeight, 50)
                1 -> generateConcentricCircles(textureWidth, textureHeight, patternPhase)
                else -> generatePlasmaEffect(textureWidth, textureHeight, patternPhase)
            }

            vulkanRunner?.updateInputTexture(pattern)

            animationHandler.postDelayed(this, 33) // ~30 FPS (图案生成较慢)
        }
    }
    private var patternPhase = 0f
    private var textureWidth = 0
    private var textureHeight = 0

    /**
     * 生成棋盘格图案
     */
    private fun generateCheckerboardPattern(
        width: Int,
        height: Int,
        cellSize: Int
    ): ByteArray {
        val data = ByteArray(width * height * 4)

        for (y in 0 until height) {
            for (x in 0 until width) {
                val index = (y * width + x) * 4

                val cellX = x / cellSize
                val cellY = y / cellSize
                val isWhite = (cellX + cellY) % 2 == 0

                val color = if (isWhite) 255.toByte() else 0.toByte()
                data[index + 0] = color
                data[index + 1] = color
                data[index + 2] = color
                data[index + 3] = 255.toByte()
            }
        }

        return data
    }

    /**
     * 生成同心圆图案
     */
    private fun generateConcentricCircles(
        width: Int,
        height: Int,
        phase: Float
    ): ByteArray {
        val data = ByteArray(width * height * 4)
        val centerX = width / 2f
        val centerY = height / 2f

        for (y in 0 until height) {
            for (x in 0 until width) {
                val index = (y * width + x) * 4

                val dx = x - centerX
                val dy = y - centerY
                val distance = Math.sqrt((dx * dx + dy * dy).toDouble()).toFloat()

                val wave = Math.sin((distance / 20.0 + phase).toDouble())
                val intensity = ((wave * 127 + 128).toInt()).toByte()

                data[index + 0] = intensity
                data[index + 1] = intensity
                data[index + 2] = intensity
                data[index + 3] = 255.toByte()
            }
        }

        return data
    }

    /**
     * 生成等离子效果
     */
    private fun generatePlasmaEffect(
        width: Int,
        height: Int,
        time: Float
    ): ByteArray {
        val data = ByteArray(width * height * 4)

        for (y in 0 until height) {
            for (x in 0 until width) {
                val index = (y * width + x) * 4

                val value = Math.sin(x / 16.0 + time) +
                        Math.sin(y / 8.0 - time) +
                        Math.sin((x + y) / 16.0) +
                        Math.sin(Math.sqrt((x * x + y * y).toDouble()) / 8.0 + time)

                val normalized = (value / 4.0 + 0.5).coerceIn(0.0, 1.0)

                // 彩色等离子
                val r = (Math.sin(normalized * Math.PI * 2) * 127 + 128).toInt()
                val g = (Math.sin(normalized * Math.PI * 2 + Math.PI * 2 / 3) * 127 + 128).toInt()
                val b = (Math.sin(normalized * Math.PI * 2 + Math.PI * 4 / 3) * 127 + 128).toInt()

                data[index + 0] = r.toByte()
                data[index + 1] = g.toByte()
                data[index + 2] = b.toByte()
                data[index + 3] = 255.toByte()
            }
        }

        return data
    }
    /**
     * 示例：启动帧生产者
     * 在实际应用中，这可能是：
     * - MediaCodec 解码器
     * - Camera2 预览
     * - MediaPlayer
     * - 或其他任何生成帧的组件
     */
    private fun startFrameProducer(surface: Surface) {
        // TODO: 实现帧生产逻辑
        // 例如：
        // mediaCodec.configure(format, surface, null, 0)
        // mediaCodec.start()

        // 或者：
        // cameraDevice.createCaptureSession(
        //     listOf(surface),
        //     sessionCallback,
        //     handler
        // )

        Log.d(TAG, "Frame producer should write to surface: $surface")

        // 临时测试：创建一个简单的测试渲染器写入测试帧
        startTestFrameProducer(surface)
    }

    /**
     * 测试用：生成测试帧
     */
    private fun startTestFrameProducer(surface: Surface) {
        // 这里可以使用 MediaCodec 解码一个测试视频
        // 或者使用 Canvas 绘制测试图案
        // 或者使用另一个 OpenGL/Vulkan 上下文生成纹理

        // 示例：使用 Canvas 绘制（需要在单独的线程）
        Thread {
            try {
                var frame = 0
                while (isVulkanInitialized && !Thread.currentThread().isInterrupted) {
                    try {
                        val canvas = surface.lockCanvas(null)
                        if (canvas != null) {
                            try {
                                // 绘制测试图案
                                canvas.drawColor(
                                    android.graphics.Color.rgb(
                                        (frame * 2) % 255,
                                        (frame * 3) % 255,
                                        (frame * 5) % 255
                                    )
                                )

                                // 绘制一些形状
                                val paint = android.graphics.Paint().apply {
                                    color = android.graphics.Color.WHITE
                                    textSize = 48f
                                }
                                canvas.drawText("Frame: $frame", 50f, 100f, paint)

                            } finally {
                                surface.unlockCanvasAndPost(canvas)
                            }
                        }
                        frame++
                        Thread.sleep(16) // ~60 FPS
                    } catch (e: Exception) {
                        Log.e(TAG, "Error drawing test frame", e)
                        break
                    }
                }
            } catch (e: InterruptedException) {
                Log.d(TAG, "Test frame producer interrupted")
            }
        }.start()
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
        isVulkanInitialized = false

        // 停止 runner（这会自动清理所有 Vulkan 资源和 filter）
        vulkanRunner?.stopAndRelease()

        // 注意：inputSurface 会被 runner 自动释放，不需要手动 release
        inputSurface = null
        outputSurface = null

        vulkanRunner = null
        vulkanFilter = null
        stopPatternAnimation()  // 停止图案动画
        Log.d(TAG, "Cleanup completed")
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