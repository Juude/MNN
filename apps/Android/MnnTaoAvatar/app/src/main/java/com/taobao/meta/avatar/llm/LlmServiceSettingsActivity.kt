package com.taobao.meta.avatar.llm

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.widget.Button
import android.widget.Switch
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.taobao.meta.avatar.R

/**
 * LLM 服务设置界面
 * 允许用户切换使用真实服务或 mock 服务
 */
class LlmServiceSettingsActivity : AppCompatActivity() {
    
    private lateinit var mockServiceSwitch: Switch
    private lateinit var statusText: TextView
    private lateinit var testButton: Button
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_llm_service_settings)
        
        initViews()
        setupListeners()
        updateStatus()
    }
    
    private fun initViews() {
        mockServiceSwitch = findViewById(R.id.switch_mock_service)
        statusText = findViewById(R.id.text_status)
        testButton = findViewById(R.id.button_test_service)
    }
    
    private fun setupListeners() {
        mockServiceSwitch.setOnCheckedChangeListener { _, isChecked ->
            LlmServiceFactory.setUseMockService(this, isChecked)
            updateStatus()
        }
        
        testButton.setOnClickListener {
            testLlmService()
        }
    }
    
    private fun updateStatus() {
        val isUsingMock = LlmServiceFactory.isUsingMockService(this)
        mockServiceSwitch.isChecked = isUsingMock
        
        val statusMessage = if (isUsingMock) {
            "当前使用 Mock LLM 服务\n" +
            "• 无需真实模型文件\n" +
            "• 快速响应测试\n" +
            "• 适合开发和调试"
        } else {
            "当前使用真实 LLM 服务\n" +
            "• 需要模型文件\n" +
            "• 真实 AI 响应\n" +
            "• 适合生产环境"
        }
        
        statusText.text = statusMessage
    }
    
    private fun testLlmService() {
        // 这里可以添加测试逻辑
        statusText.text = "测试功能待实现..."
    }
    
    companion object {
        fun start(context: Context) {
            val intent = Intent(context, LlmServiceSettingsActivity::class.java)
            context.startActivity(intent)
        }
    }
}
