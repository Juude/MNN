package com.taobao.meta.avatar.a2bs

import android.content.Context
import android.content.SharedPreferences

/**
 * A2BS 服务工厂
 * 用于创建和管理 A2BS 服务实例
 */
object A2BSServiceFactory {
    
    private const val PREFS_NAME = "a2bs_service_prefs"
    private const val KEY_USE_MOCK = "use_mock_service"
    
    /**
     * 创建 A2BS 服务实例
     * @param context Android 上下文
     * @param forceMock 是否强制使用 mock 服务
     * @return A2BS 服务实例
     */
    fun createA2BSService(context: Context, forceMock: Boolean = false): IA2BSService {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val useMock = forceMock || prefs.getBoolean(KEY_USE_MOCK, false)
        
        return if (useMock) {
            MockA2BSService()
        } else {
            A2BSService()
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
