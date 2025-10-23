package com.taobao.meta.avatar.llm

import android.util.Log
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.cancellable
import kotlinx.coroutines.flow.channelFlow
import kotlinx.coroutines.flow.flowOf

/**
 * Mock LLM 服务实现
 * 用于测试和开发环境，不依赖真实的 LLM 模型
 */
class MockLlmService : ILlmService {
    
    private var stopRequested = false
    private var isInitialized = false
    
    // Mock 响应模板
    private val mockResponses = listOf(
        "这是一个模拟的 AI 回复。",
        "你好！我是 TaoAvatar 的 AI 助手。",
        "很高兴为您服务！有什么可以帮助您的吗？",
        "这是一个测试回复，用于验证系统功能。",
        "感谢您的提问，我正在模拟处理中...",
        "基于您的问题，我的建议是...",
        "让我为您分析一下这个问题。",
        "这是一个很有趣的话题，让我来回答您。"
    )
    
    override suspend fun init(modelDir: String?): Boolean {
        Log.d(TAG, "MockLlmService init called with modelDir: $modelDir")
        // 模拟初始化延迟
        delay(1000)
        isInitialized = true
        Log.d(TAG, "MockLlmService initialized successfully")
        return true
    }
    
    override fun startNewSession() {
        Log.d(TAG, "MockLlmService startNewSession called")
        stopRequested = false
    }
    
    override fun generate(text: String): Flow<Pair<String?, String>> = channelFlow {
        Log.d(TAG, "MockLlmService generate called with text: $text")
        
        if (!isInitialized) {
            Log.w(TAG, "MockLlmService not initialized, returning empty response")
            send(Pair(null, ""))
            return@channelFlow
        }
        
        stopRequested = false
        
        // 随机选择一个响应模板
        val selectedResponse = mockResponses.random()
        
        // 模拟流式输出，逐字符发送
        val words = selectedResponse.split("")
        var fullText = ""
        
        for (i in words.indices) {
            if (stopRequested) {
                Log.d(TAG, "MockLlmService generation stopped by user")
                break
            }
            
            val word = words[i]
            if (word.isNotEmpty()) {
                fullText += word
                send(Pair(word, fullText))
                
                // 模拟网络延迟
                delay(50 + (Math.random() * 100).toLong())
            }
        }
        
        // 发送完成信号
        send(Pair(null, fullText))
        Log.d(TAG, "MockLlmService generation completed")
        
    }.cancellable()
    
    override fun requestStop() {
        Log.d(TAG, "MockLlmService requestStop called")
        stopRequested = true
    }
    
    override fun unload() {
        Log.d(TAG, "MockLlmService unload called")
        isInitialized = false
        stopRequested = false
    }
    
    companion object {
        private const val TAG = "MockLlmService"
    }
}
