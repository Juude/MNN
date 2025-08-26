

#ifndef PROMPT_Hpp
#define PROMPT_Hpp

#include "llm/llm.hpp"
#include "llmconfig.hpp"
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <unordered_map>


namespace MNN {
namespace Transformer {

// 多轮会话中的单个消息
struct ConversationMessage {
    std::string role;  // "system", "user", "assistant"
    std::string content;
    std::vector<std::string> image_placeholders;  // 该消息中包含的图片占位符
    int message_id = -1;  // 消息ID，用于缓存管理
};

// 会话状态管理
struct ConversationState {
    std::vector<ConversationMessage> history;
    std::unordered_map<std::string, int> image_cache_refs;  // 图片hash -> 缓存索引
    bool has_cached_images = false;
    int last_processed_message_id = -1;
    
    void clear() {
        history.clear();
        image_cache_refs.clear();
        has_cached_images = false;
        last_processed_message_id = -1;
    }
};

class Prompt {
private:
    class JinjaTemplate;
    std::shared_ptr<LlmContext> mContext;
    std::string mPromptTemplate; // for compatibility
    std::string mSystemPrompt;
    std::string mBos, mSystemTemplate, mUserTemplate, mAssistantTemplate;
    std::string mAssistantPrefix, mAssistantSuffix;
    std::string mSystemName = "system",
                mUserName = "user",
                mAssistantName = "assistant";
    std::shared_ptr<JinjaTemplate> mCommonTemplate;
    
    // 多轮会话支持
    ConversationState mConversationState;
public:
    static Prompt* createPrompt(std::shared_ptr<LlmContext> context, std::shared_ptr<LlmConfig> config);
    Prompt(std::shared_ptr<LlmContext> context, std::shared_ptr<LlmConfig> config);
    std::string getAssistantSuffix() const;
    void setParams(std::shared_ptr<LlmConfig> config);
    std::string applyTemplate(std::string user_content, bool add_system_prompt = false, bool add_generation_prompt = true);
    std::string applyTemplate(const std::vector<ChatMessage>& inputs, bool add_generation_prompt = true);
    
    // 多轮会话支持的新接口
    void addUserMessage(const std::string& content, const std::vector<std::string>& image_placeholders = {});
    void addAssistantMessage(const std::string& content);
    void addSystemMessage(const std::string& content);
    std::string applyConversationTemplate(bool add_generation_prompt = true);
    void clearConversation();
    const ConversationState& getConversationState() const { return mConversationState; }
    bool hasImagesInConversation() const { return mConversationState.has_cached_images; }
};

}
}


#endif
