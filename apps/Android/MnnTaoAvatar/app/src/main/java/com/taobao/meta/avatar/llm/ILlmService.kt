package com.taobao.meta.avatar.llm

import kotlinx.coroutines.flow.Flow

/**
 * LLM 服务接口抽象
 * 用于支持真实实现和 mock 实现
 */
interface ILlmService {
    
    /**
     * 初始化 LLM 服务
     * @param modelDir 模型目录路径
     * @return 初始化是否成功
     */
    suspend fun init(modelDir: String?): Boolean
    
    /**
     * 开始新的会话
     */
    fun startNewSession()
    
    /**
     * 生成文本响应
     * @param text 输入文本
     * @return Flow<Pair<String?, String>> 第一个是增量文本，第二个是完整文本
     */
    fun generate(text: String): Flow<Pair<String?, String>>
    
    /**
     * 请求停止生成
     */
    fun requestStop()
    
    /**
     * 卸载服务
     */
    fun unload()
}
