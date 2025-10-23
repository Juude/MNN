package com.taobao.meta.avatar.a2bs

import android.content.Context
import android.util.Log
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.delay
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlin.random.Random

/**
 * Mock A2BS 服务实现
 * 用于测试和开发环境，不依赖真实的 A2BS 模型
 */
class MockA2BSService : IA2BSService {
    
    @Volatile
    private var isLoaded = false
    private var initDeferred: CompletableDeferred<Boolean>? = null
    private val mutex = Mutex()
    
    // Mock 数据生成器
    private val mockDataGenerator = MockBlendShapeDataGenerator()
    
    override suspend fun waitForInitComplete(): Boolean {
        if (isLoaded) return true
        initDeferred?.let {
            return it.await()
        }
        return isLoaded
    }
    
    override suspend fun init(modelDir: String?, context: Context): Boolean = mutex.withLock {
        Log.d(TAG, "MockA2BSService init called with modelDir: $modelDir")
        
        if (isLoaded) return true
        
        // 模拟初始化延迟
        delay(500)
        
        // 模拟初始化过程
        Log.d(TAG, "MockA2BSService initializing...")
        delay(200)
        
        isLoaded = true
        initDeferred?.complete(true)
        
        Log.d(TAG, "MockA2BSService initialized successfully")
        return true
    }
    
    override fun process(index: Int, audioData: ShortArray, sampleRate: Int): AudioToBlendShapeData {
        Log.d(TAG, "MockA2BSService process called - index: $index, audioData length: ${audioData.size}, sampleRate: $sampleRate")
        
        // 生成模拟的混合形状数据
        val mockData = mockDataGenerator.generateMockData(audioData, sampleRate)
        
        Log.d(TAG, "MockA2BSService generated mock data with ${mockData.frame_num} frames")
        return mockData
    }
    
    companion object {
        private const val TAG = "MockA2BSService"
    }
}

/**
 * Mock 混合形状数据生成器
 */
class MockBlendShapeDataGenerator {
    
    // 模拟的表情参数数量（FLAME 模型通常有 50 个表情参数）
    private val expressionParams = 50
    // 模拟的姿态参数数量
    private val poseParams = 6
    // 模拟的关节变换参数数量
    private val jointTransformParams = 24 * 4 // 24个关节，每个关节4个参数（位置+旋转）
    
    /**
     * 生成模拟的混合形状数据
     */
    fun generateMockData(audioData: ShortArray, sampleRate: Int): AudioToBlendShapeData {
        val data = AudioToBlendShapeData()
        
        // 根据音频长度计算帧数（假设每帧 16ms）
        val frameDurationMs = 16
        val frameCount = maxOf(1, (audioData.size * 1000) / (sampleRate * frameDurationMs))
        data.frame_num = frameCount
        
        Log.d(TAG, "Generating mock data for $frameCount frames")
        
        // 生成模拟的表情数据
        data.expr = generateExpressionData(frameCount)
        
        // 生成模拟的姿态数据
        data.pose = generatePoseData(frameCount)
        data.pose_z = generatePoseZData(frameCount)
        data.app_pose_z = generateAppPoseZData(frameCount)
        
        // 生成模拟的关节变换数据
        data.joints_transform = generateJointTransformData(frameCount)
        
        // 生成模拟的下颌姿态数据
        data.jaw_pose = generateJawPoseData(frameCount)
        
        return data
    }
    
    /**
     * 生成模拟的表情数据
     */
    private fun generateExpressionData(frameCount: Int): List<FloatArray> {
        return (0 until frameCount).map { frameIndex ->
            FloatArray(expressionParams) { paramIndex ->
                // 基于音频特征生成表情参数
                val baseValue = Math.sin(frameIndex * 0.1 + paramIndex * 0.2).toFloat() * 0.3f
                val variation = Random.nextFloat() * 0.1f - 0.05f
                (baseValue + variation).coerceIn(-1f, 1f)
            }
        }
    }
    
    /**
     * 生成模拟的姿态数据
     */
    private fun generatePoseData(frameCount: Int): List<FloatArray> {
        return (0 until frameCount).map { frameIndex ->
            FloatArray(poseParams) { paramIndex ->
                // 生成小幅度的姿态变化
                val baseValue = Math.sin(frameIndex * 0.05 + paramIndex * 0.3).toFloat() * 0.1f
                val variation = Random.nextFloat() * 0.02f - 0.01f
                (baseValue + variation).coerceIn(-0.2f, 0.2f)
            }
        }
    }
    
    /**
     * 生成模拟的 Z 轴姿态数据
     */
    private fun generatePoseZData(frameCount: Int): List<FloatArray> {
        return (0 until frameCount).map { frameIndex ->
            FloatArray(poseParams) { paramIndex ->
                // Z 轴姿态变化更小
                val baseValue = Math.cos(frameIndex * 0.03 + paramIndex * 0.4).toFloat() * 0.05f
                val variation = Random.nextFloat() * 0.01f - 0.005f
                (baseValue + variation).coerceIn(-0.1f, 0.1f)
            }
        }
    }
    
    /**
     * 生成模拟的应用姿态 Z 数据
     */
    private fun generateAppPoseZData(frameCount: Int): List<FloatArray> {
        return (0 until frameCount).map { frameIndex ->
            FloatArray(poseParams) { paramIndex ->
                // 应用姿态数据
                val baseValue = Math.sin(frameIndex * 0.02 + paramIndex * 0.5).toFloat() * 0.03f
                val variation = Random.nextFloat() * 0.005f - 0.0025f
                (baseValue + variation).coerceIn(-0.05f, 0.05f)
            }
        }
    }
    
    /**
     * 生成模拟的关节变换数据
     */
    private fun generateJointTransformData(frameCount: Int): List<FloatArray> {
        return (0 until frameCount).map { frameIndex ->
            FloatArray(jointTransformParams) { paramIndex ->
                // 关节变换数据（位置和旋转）
                val jointIndex = paramIndex / 4
                val paramType = paramIndex % 4
                
                when (paramType) {
                    0, 1, 2 -> { // 位置参数
                        val baseValue = Math.sin(frameIndex * 0.01 + jointIndex * 0.1).toFloat() * 0.02f
                        val variation = Random.nextFloat() * 0.005f - 0.0025f
                        (baseValue + variation).coerceIn(-0.05f, 0.05f)
                    }
                    3 -> { // 旋转参数
                        val baseValue = Math.cos(frameIndex * 0.02 + jointIndex * 0.15).toFloat() * 0.1f
                        val variation = Random.nextFloat() * 0.02f - 0.01f
                        (baseValue + variation).coerceIn(-0.2f, 0.2f)
                    }
                    else -> 0f
                }
            }
        }
    }
    
    /**
     * 生成模拟的下颌姿态数据
     */
    private fun generateJawPoseData(frameCount: Int): List<FloatArray> {
        return (0 until frameCount).map { frameIndex ->
            FloatArray(poseParams) { paramIndex ->
                // 下颌姿态数据，主要影响说话时的下颌运动
                val baseValue = Math.sin(frameIndex * 0.2 + paramIndex * 0.3).toFloat() * 0.15f
                val variation = Random.nextFloat() * 0.03f - 0.015f
                (baseValue + variation).coerceIn(-0.3f, 0.3f)
            }
        }
    }
    
    companion object {
        private const val TAG = "MockBlendShapeDataGenerator"
    }
}
