//
//  model_runner.hpp
//
//  Created by MNN on 2024/01/01.
//  ModelRunner class for handling LLM model execution
//

#ifndef MODEL_RUNNER_HPP
#define MODEL_RUNNER_HPP

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <fstream>
#include <regex>
#include <sstream>

#ifdef LLM_SUPPORT_VISION
// Try to include OpenCV, but make it optional
#if __has_include(<opencv2/opencv.hpp>)
#include <opencv2/opencv.hpp>
#elif __has_include(<opencv4/opencv2/opencv.hpp>)
#include <opencv4/opencv2/opencv.hpp>
#else
#error "OpenCV headers not found, video processing will be disabled"
// Don't redefine LLM_SUPPORT_VISION, just disable the methods
#define OPENCV_NOT_AVAILABLE
#endif
#endif

// Forward declarations
namespace MNN::Transformer {
    class Llm;
}

namespace MNN::Express {
    class VARP;
}

// ModelRunner class for handling LLM model execution
class ModelRunner {
public:
    ModelRunner(MNN::Transformer::Llm* llm);
    ~ModelRunner() = default;
    
    // Main evaluation methods
    int EvalPrompts(const std::vector<std::string>& prompts);
    int EvalFile(const std::string& prompt_file);
    void InteractiveChat();
    
    // Common prompt processing method with video support
    int ProcessPrompt(const std::string& prompt, std::ostream* output = nullptr, 
                     int max_new_tokens = -1);
    
    // Video processing helper methods
#ifdef LLM_SUPPORT_VISION
#ifndef OPENCV_NOT_AVAILABLE
    static MNN::Express::VARP MatToVar(const cv::Mat& mat);
    int ProcessVideoPrompt(const std::string& prompt_str, std::ostream* output = nullptr);
#else
    // Stub declarations when OpenCV is not available
    static MNN::Express::VARP MatToVar(void* mat);
    int ProcessVideoPrompt(const std::string& prompt_str, std::ostream* output = nullptr);
#endif
#endif

private:
    MNN::Transformer::Llm* llm_;
    
    // Helper methods
    std::vector<std::string> ReadPromptsFromFile(const std::string& prompt_file);
    void ShowPerformanceMetrics(int prompt_len, int decode_len, 
                               int64_t vision_time, int64_t audio_time,
                               int64_t prefill_time, int64_t decode_time, 
                               int64_t sample_time);
    void ShowChatHelp();
    void ResetConversation();
    void ShowConfig();
};

#endif // MODEL_RUNNER_HPP
