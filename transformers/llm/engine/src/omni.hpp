//
//  omni.hpp
//
//  Created by MNN on 2025/04/08.
//  Copyright © 2018, Alibaba Group Holding Limited
//

#ifndef OMNI_hpp
#define OMNI_hpp

#include "llm/llm.hpp"
#include <chrono>
#include <functional>
#include <unordered_map>

namespace MNN {
using namespace Express;
namespace Transformer {

// 图像缓存条目
struct VisionCacheEntry {
    std::string image_hash;  // 图像内容的hash
    VARP vision_embedding;   // 缓存的视觉特征
    std::vector<int> token_ids;  // 对应的token IDs
    int64_t last_used_time;  // 最后使用时间（用于LRU淘汰）
    int reference_count = 0;  // 引用计数
    
    VisionCacheEntry(const std::string& hash, VARP embedding, const std::vector<int>& tokens)
        : image_hash(hash), vision_embedding(embedding), token_ids(tokens), 
          last_used_time(std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch()).count()) {}
};

// 视觉缓存管理器
class VisionCache {
private:
    std::unordered_map<std::string, std::unique_ptr<VisionCacheEntry>> mCache;
    size_t mMaxCacheSize = 100;  // 最大缓存条目数
    std::function<std::string(Express::VARP)> mImageHasher;  // 图像hash函数
    
public:
    VisionCache(size_t max_size = 100) : mMaxCacheSize(max_size) {
        // 默认图像hash函数：基于图像数据内容
        mImageHasher = [](Express::VARP image) -> std::string {
            if (image.get() == nullptr) return "";
            auto data = image->readMap<float>();
            auto size = image->getInfo()->size;
            std::hash<std::string> hasher;
            std::string data_str(reinterpret_cast<const char*>(data), size * sizeof(float));
            return std::to_string(hasher(data_str));
        };
    }
    
    // 获取缓存的视觉特征
    VisionCacheEntry* get(const std::string& image_hash);
    
    // 添加到缓存
    void put(const std::string& image_hash, VARP embedding, const std::vector<int>& token_ids);
    
    // 检查是否存在
    bool contains(const std::string& image_hash) const;
    
    // 生成图像hash
    std::string generateImageHash(Express::VARP image) { return mImageHasher(image); }
    
    // 清理缓存（LRU策略）
    void cleanup();
    
    // 清空所有缓存
    void clear() { mCache.clear(); }
    
    // 获取缓存统计信息
    size_t size() const { return mCache.size(); }
};

class MropeInfo {
public:
    MropeInfo() {}
    MropeInfo(const MropeInfo& info) {
        mT = info.mT;
        mH = info.mH;
        mW = info.mW;
    }
    int back() {
        if (mW.empty()) {
            return 0;
        }
        return mW.back();
    }
    int currentIdx() {
        if (mW.empty()) {
            return 0;
        }
        return back() + 1;
    }
    void push_back(int t, int h, int w) {
        mT.push_back(t);
        mH.push_back(h);
        mW.push_back(w);
    }
    void push_back(int t) {
        push_back(t, t, t);
    }
    void push_back() {
        int cur_idx = currentIdx();
        push_back(cur_idx, cur_idx, cur_idx);
    }
    void clear() {
        mT.clear();
        mH.clear();
        mW.clear();
    }
    std::vector<int> mT, mH, mW;
};

class Talker : public Llm {
public:
    Talker(std::shared_ptr<LlmConfig> config) : Llm(config), mThinker(nullptr) {}
    Talker(std::shared_ptr<LlmConfig> config, Llm* thinker) : Llm(config), mThinker(thinker) {}
    ~Talker() {}
    virtual void load() override;
    virtual void generate_init(std::ostream* os = nullptr, const char* end_with = nullptr) override;
    virtual Express::VARP embedding(const std::vector<int>& input_ids) override;
    virtual Express::VARP gen_position_ids(int seq_len) override;
    virtual int sample(Express::VARP logits, int offset = 0, int size = 0) override;
    virtual void setWavformCallback(std::function<bool(const float*, size_t, bool)> callback) override;
    VARP ditForward(const int codec_size, const int* codec_tokens, const float* initial_noise = nullptr);
    VARP bigvganForward(VARP mel);
    VARP token2wav(const std::vector<int>& codec_tokens);
    void token2wav(bool talker_done = false);
    void generate();
    void setPostionIds(const MropeInfo& positionIds);
    void addTalkerEmbeds(VARP talker_embeds);
    // is generate
    bool doGenerate() { return mWavformCallback != nullptr; }
    // is decode with token2wav
    bool mStreamWithDecode = false;
private:
    int mMaxNewTokens = 2048, mTextBosToken = 151872, mTextEosToken = 151861,
        mTextPadToken = 151859, mCodecBosToken = 8293, mCodecPadToken = 8292;
    VARP mTextBos, mTextEos, mTextPad, mCodecBos, mCodecPad, mSpk, mCond;
    MropeInfo mPositionIds;
    std::vector<VARP> mTalkerEmbeds;
    std::shared_ptr<Module> mPreDit, mDit, mBigvgan;
    Llm* mThinker;
    // stream generate
    std::vector<float> mInitialNoise, mWaveformBuffer;
    VARP mMelBuffer = nullptr;
    const int dit_chunk_size = 60, dit_left_context = 24,
        dit_right_context = 12, dit_right_padding = dit_right_context,
        vocoder_left_context = 8, vocoder_right_context = 8,
        vocoder_right_pad = vocoder_right_context, vocoder_upsample_rate = 240;
    int dit_left_padding = 0, dit_start_index = 0, vocoder_left_pad = 0;
    std::function<bool(const float*, size_t, bool)> mWavformCallback = nullptr;
};

class Omni : public Llm {
public:
    Omni(std::shared_ptr<LlmConfig> config);
    ~Omni() {
        mVisionModule.reset();
        mAudioModule.reset();
    }
    virtual void load() override;
    virtual std::vector<int> tokenizer_encode_with_images(const std::string& user_content, const std::vector<MNN::Express::VARP>& images) override;
    virtual void responseWithImages(const std::string& user_content, const std::vector<MNN::Express::VARP>& images,
                           std::ostream* os = &std::cout, const char* end_with = nullptr, int max_new_tokens = -1) override;
    virtual std::vector<Express::VARP> forwardRaw(Express::VARP hiddenState, Express::VARP mask, Express::VARP inputPos) override;
    virtual std::vector<int> tokenizer_encode(const std::string& query) override;
    virtual std::vector<int> tokenizer_encode(const MultimodalPrompt& multimodal_input) override;
    virtual Express::VARP embedding(const std::vector<int>& input_ids) override;
    virtual Express::VARP gen_position_ids(int seq_len) override;
    virtual void response(const std::vector<int>& input_ids, std::ostream* os = &std::cout, const char* end_with = nullptr, int max_new_tokens = -1) override;
    virtual void setWavformCallback(std::function<bool(const float*, size_t, bool)> callback) override;
    virtual void generateWavform() override;
    
    // 多轮会话接口
    void startConversation();
    void addConversationImage(VARP image, const std::string& placeholder = "");
    void addConversationMessage(const std::string& role, const std::string& content);
    void responseConversation(const std::string& user_input, std::ostream* os = &std::cout, 
                             const char* end_with = nullptr, int max_new_tokens = -1);
    void clearConversation();
    // some models preprocess function
    std::vector<int> visionProcess(VARP image);
    std::vector<int> defaultVisionProcess(VARP image);
    std::vector<int> qwen2VisionProcess(VARP image);
    std::vector<int> smolvlmVisionProcess(VARP image);
    std::vector<int> minicpmVisionProcess(VARP image);
private:
    int mVisionHeight = 448, mVisionWidth = 448, mVisionStart = 151857,
        mVisionEnd = 151858, mVisionPad = 151859, mAudioPad = 151646;
    int mVisionGlobal = 49152;
    int mVisionSizeUnit = 1, mVisionMaxSize = 2048;
    int mVisionNum = 0;
    std::vector<float> mVisionMean{122.7709383, 116.7460125, 104.09373615};
    std::vector<float> mVisionNorm{0.01459843, 0.01500777, 0.01422007};
    std::vector<int> multimodeProcess(const std::string& mode, std::string info);
    std::vector<int> visionProcess(const std::string& file);
    std::vector<int> visionProcess(Express::VARP image);
    std::vector<int> audioProcess(const std::string& file);
    std::vector<int> audioProcess(MNN::Express::VARP waveform);
    std::vector<int> processImageContent(const std::string& content, const std::map<std::string, PromptImagePart>& images);
    std::vector<int> processAudioContent(const std::string& content, const std::map<std::string, PromptAudioPart>& audios);
    std::shared_ptr<Module> mVisionModule, mAudioModule;
    std::vector<VARP> mVisionEmbeddings, mAudioEmbeddings;
    std::shared_ptr<Talker> mTalker;
    
    // 视觉特征缓存
    std::unique_ptr<VisionCache> mVisionCache;
    bool mEnableVisionCache = true;  // 是否启用视觉缓存
    
    // m_rope position ids
    void addPositionIds(int t, int h = -1, int w = -1);
    MropeInfo mPositionIds;
    
    // 新增缓存相关方法
    std::vector<int> visionProcessWithCache(VARP image);
    std::string computeImageHash(VARP image);
};

}
}
#endif // OMNI_hpp
