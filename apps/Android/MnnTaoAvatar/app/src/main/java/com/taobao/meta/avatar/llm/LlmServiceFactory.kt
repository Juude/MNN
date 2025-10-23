package com.taobao.meta.avatar.llm

import android.content.Context
import android.content.SharedPreferences

/**
 * LLM 服务工厂
 * 用于创建和管理 LLM 服务实例
 */
object LlmServiceFactory {
    
    private const val PREFS_NAME = "llm_service_prefs"
    private const val KEY_USE_MOCK = "use_mock_service"
    
    /**
     * 创建 LLM 服务实例
     * @param context Android 上下文
     * @param forceMock 是否强制使用 mock 服务
     * @return LLM 服务实例
     */
    fun createLlmService(context: Context, forceMock: Boolean = false): ILlmService {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val useMock = forceMock || prefs.getBoolean(KEY_USE_MOCK, false)
        
        return if (useMock) {
            MockLlmService()
        } else {
            LlmService()
        }
    }
    
    /**
     * 设置是否使用 mock 服务
     * @param context Android 上下文
     * @param useMock 是否使用 mock 服务
     */
    fun setUseMockService(context: Context, useMock: Boolean) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().putBoolean(KEY_USE_MOCK, useMock).apply()
    }
    
    /**
     * 检查是否使用 mock 服务
     * @param context Android 上下文
     * @return 是否使用 mock 服务
     */
    fun isUsingMockService(context: Context): Boolean {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return prefs.getBoolean(KEY_USE_MOCK, false)
    }
}
