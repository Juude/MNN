package com.taobao.meta.avatar.a2bs

import android.content.Context
import kotlinx.coroutines.CompletableDeferred

/**
 * A2BS 服务接口抽象
 * 用于支持真实实现和 mock 实现
 */
interface IA2BSService {
    
    /**
     * 等待初始化完成
     * @return 初始化是否成功
     */
    suspend fun waitForInitComplete(): Boolean
    
    /**
     * 初始化 A2BS 服务
     * @param modelDir 模型目录路径
     * @param context Android 上下文
     * @return 初始化是否成功
     */
    suspend fun init(modelDir: String?, context: Context): Boolean
    
    /**
     * 处理音频数据，生成混合形状数据
     * @param index 索引
     * @param audioData 音频数据
     * @param sampleRate 采样率
     * @return 音频到混合形状的数据
     */
    fun process(index: Int, audioData: ShortArray, sampleRate: Int): AudioToBlendShapeData
}
