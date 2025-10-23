package com.taobao.meta.avatar.llm

import android.content.Context
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.flow.first

/**
 * LLM 服务测试工具
 * 用于验证 mock 实现和真实实现的功能
 */
object LlmServiceTester {
    
    /**
     * 测试 Mock LLM 服务
     */
    fun testMockService(): Boolean {
        return try {
            val mockService = MockLlmService()
            
            runBlocking {
                // 测试初始化
                val initResult = mockService.init(null)
                if (!initResult) {
                    println("❌ Mock service initialization failed")
                    return@runBlocking false
                }
                println("✅ Mock service initialized successfully")
                
                // 测试生成
                mockService.startNewSession()
                val flow = mockService.generate("测试输入")
                val result = flow.first()
                
                if (result.first != null && result.second.isNotEmpty()) {
                    println("✅ Mock service generated response: ${result.second}")
                    true
                } else {
                    println("❌ Mock service failed to generate response")
                    false
                }
            }
        } catch (e: Exception) {
            println("❌ Mock service test failed with exception: ${e.message}")
            false
        }
    }
    
    /**
     * 测试服务工厂
     */
    fun testServiceFactory(context: Context): Boolean {
        return try {
            // 测试创建 mock 服务
            val mockService = LlmServiceFactory.createLlmService(context, forceMock = true)
            if (mockService !is MockLlmService) {
                println("❌ Factory failed to create mock service")
                return false
            }
            println("✅ Factory created mock service successfully")
            
            // 测试创建真实服务
            val realService = LlmServiceFactory.createLlmService(context, forceMock = false)
            if (realService !is LlmService) {
                println("❌ Factory failed to create real service")
                return false
            }
            println("✅ Factory created real service successfully")
            
            // 测试配置持久化
            LlmServiceFactory.setUseMockService(context, true)
            val isUsingMock = LlmServiceFactory.isUsingMockService(context)
            if (!isUsingMock) {
                println("❌ Mock service configuration not persisted")
                return false
            }
            println("✅ Mock service configuration persisted successfully")
            
            true
        } catch (e: Exception) {
            println("❌ Service factory test failed with exception: ${e.message}")
            false
        }
    }
    
    /**
     * 运行所有测试
     */
    fun runAllTests(context: Context): Boolean {
        println("🧪 Starting LLM Service Tests...")
        println("=".repeat(50))
        
        val mockTestResult = testMockService()
        val factoryTestResult = testServiceFactory(context)
        
        println("=".repeat(50))
        println("📊 Test Results:")
        println("Mock Service Test: ${if (mockTestResult) "✅ PASSED" else "❌ FAILED"}")
        println("Factory Test: ${if (factoryTestResult) "✅ PASSED" else "❌ FAILED"}")
        
        val allPassed = mockTestResult && factoryTestResult
        println("Overall: ${if (allPassed) "✅ ALL TESTS PASSED" else "❌ SOME TESTS FAILED"}")
        
        return allPassed
    }
}
