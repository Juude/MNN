//
//  model_runner.cpp
//
//  Created by MNN on 2024/01/01.
//  ModelRunner class implementation
//

#include "model_runner.hpp"
#include "../../../transformers/llm/engine/include/llm/llm.hpp"
#include <MNN/AutoTime.hpp>
#include <MNN/expr/ExecutorScope.hpp>
#include <MNN/MNNDefine.h>
#include <MNN/HalideRuntime.h>
#include <iomanip>
#include <algorithm>

using namespace MNN::Transformer;

ModelRunner::ModelRunner(Llm* llm) : llm_(llm) {
    if (!llm_) {
        throw std::invalid_argument("LLM pointer cannot be null");
    }
}

int ModelRunner::EvalPrompts(const std::vector<std::string>& prompts) {
    int prompt_len = 0;
    int decode_len = 0;
    int64_t vision_time = 0;
    int64_t audio_time = 0;
    int64_t prefill_time = 0;
    int64_t decode_time = 0;
    int64_t sample_time = 0;
    
    for (int i = 0; i < prompts.size(); i++) {
        const auto& prompt = prompts[i];
        if (prompt.substr(0, 1) == "#") {
            continue;
        }
        
        ProcessPrompt(prompt, &std::cout);
        auto context = llm_->getContext();
        prompt_len += context->prompt_len;
        decode_len += context->gen_seq_len;
        vision_time += context->vision_us;
        audio_time += context->audio_us;
        prefill_time += context->prefill_us;
        decode_time += context->decode_us;
        sample_time += context->sample_us;
    }
    
    ShowPerformanceMetrics(prompt_len, decode_len, vision_time, audio_time, 
                          prefill_time, decode_time, sample_time);
    
    return 0;
}

int ModelRunner::EvalFile(const std::string& prompt_file) {
    std::cout << "Reading prompts from: " << prompt_file << "\n";
    
    auto prompts = ReadPromptsFromFile(prompt_file);
    if (prompts.empty()) {
        std::cerr << "Error: Prompt file is empty or could not be read" << std::endl;
        return 1;
    }
    
    return EvalPrompts(prompts);
}

void ModelRunner::InteractiveChat() {
    std::cout << "🚀 Starting interactive chat mode...\n";
    std::cout << "Commands: /help, /reset, /config, /exit\n";
#ifdef LLM_SUPPORT_VISION
#ifndef OPENCV_NOT_AVAILABLE
    std::cout << "💡 You can also use video prompts: <video>path/to/video.mp4</video>\n";
#else
    std::cout << "⚠️  Video processing is disabled (OpenCV not available)\n";
#endif
#endif
    std::cout << "\n";
    
    while (true) {
        std::cout << "👤 User: ";
        std::string input;
        std::getline(std::cin, input);
        
        if (input == "/exit") break;
        if (input == "/help") ShowChatHelp();
        else if (input == "/reset") ResetConversation();
        else if (input == "/config") ShowConfig();
        else if (!input.empty()) {
            std::cout << "\n🤖 Assistant: " << std::flush;
            
            // Use ProcessPrompt to handle both text and video prompts
            ProcessPrompt(input, &std::cout);
            std::cout << "\n";
        }
    }
}

int ModelRunner::ProcessPrompt(const std::string& prompt, std::ostream* output, int max_new_tokens) {
    if (output == nullptr) {
        output = &std::cout;
    }
    
#ifdef LLM_SUPPORT_VISION
    // Check if prompt contains video tags
    std::regex video_regex("<video>(.*?)</video>");
    std::smatch match;
    if (std::regex_search(prompt, match, video_regex)) {
        return ProcessVideoPrompt(prompt, output);
    }
#endif

    // Regular text prompt processing
    MNN::AutoTime _t(0, "response");
    llm_->response(prompt, output, nullptr, max_new_tokens);
    return 0;
}

#ifdef LLM_SUPPORT_VISION
#ifndef OPENCV_NOT_AVAILABLE
MNN::Express::VARP ModelRunner::MatToVar(const cv::Mat& mat) {
    // Ensure the mat is not empty
    if (mat.empty()) {
        MNN_ERROR("Input cv::Mat is empty!\n");
        return nullptr;
    }
    // Only support CV_8UC3 for now
    if (mat.type() != CV_8UC3) {
        MNN_ERROR("Only support CV_8UC3 for MatToVar!\n");
        return nullptr;
    }
    auto var = MNN::Express::_Input({mat.rows, mat.cols, 3}, MNN::Express::NHWC, halide_type_of<uint8_t>());
    auto ptr = var->writeMap<uint8_t>();
    memcpy(ptr, mat.data, mat.total() * mat.elemSize());
    return var;
}

int ModelRunner::ProcessVideoPrompt(const std::string& prompt_str, std::ostream* output) {
    std::regex video_regex("<video>(.*?)</video>");
    std::smatch match;
    if (!std::regex_search(prompt_str, match, video_regex)) {
        return ProcessPrompt(prompt_str, output);
    }
    
    std::string video_path = match[1].str();
    std::string text_part = std::regex_replace(prompt_str, video_regex, "");
    
    cv::VideoCapture cap(video_path);
    if (!cap.isOpened()) {
        std::cerr << "Error: Failed to open video file: " << video_path << std::endl;
        return 1;
    }
    
    std::vector<MNN::Express::VARP> images;
    std::string final_prompt = text_part;
    double fps = cap.get(cv::CAP_PROP_FPS);
    int frame_count = cap.get(cv::CAP_PROP_FRAME_COUNT);
    int duration = frame_count / fps;
    int sample_rate = 2; // frames per second
    
    final_prompt += " The video has " + std::to_string(frame_count) + " frames, total " + std::to_string(duration) + " seconds. ";

    for (int i = 0; i < frame_count; ++i) {
        cv::Mat frame;
        cap.read(frame);
        if (frame.empty()) continue;

        if (i % static_cast<int>(fps / sample_rate) == 0) {
            int current_second = i / fps;
            char timestamp[16];
            snprintf(timestamp, sizeof(timestamp), "Frame at %02d:%02d: ", current_second / 60, current_second % 60);
            final_prompt += timestamp;
            final_prompt += "<img></img>";
            images.push_back(MatToVar(frame));
        }
    }
    cap.release();
    
    std::cout << "Final prompt: " << final_prompt << std::endl;
    std::cout << "Read " << images.size() << " frames from video." << std::endl;
    
    MNN::AutoTime _t(0, "responseWithImages");
    llm_->responseWithImages(final_prompt, images, output, nullptr, 9999);
    
    return 0;
}
#else
// Stub implementations when OpenCV is not available
MNN::Express::VARP ModelRunner::MatToVar(void* mat) {
    std::cerr << "Error: OpenCV not available, video processing disabled" << std::endl;
    return nullptr;
}

int ModelRunner::ProcessVideoPrompt(const std::string& prompt_str, std::ostream* output) {
    std::cerr << "Error: OpenCV not available, video processing disabled" << std::endl;
    return 1;
}
#endif
#endif

std::vector<std::string> ModelRunner::ReadPromptsFromFile(const std::string& prompt_file) {
    std::ifstream prompt_fs(prompt_file);
    if (!prompt_fs.is_open()) {
        std::cerr << "Error: Failed to open prompt file: " << prompt_file << std::endl;
        return {};
    }
    
    std::vector<std::string> prompts;
    std::string prompt;
    while (std::getline(prompt_fs, prompt)) {
        if (prompt.back() == '\r') {
            prompt.pop_back();
        }
        if (!prompt.empty()) {
            prompts.push_back(prompt);
        }
    }
    prompt_fs.close();
    
    return prompts;
}

void ModelRunner::ShowPerformanceMetrics(int prompt_len, int decode_len, 
                                       int64_t vision_time, int64_t audio_time,
                                       int64_t prefill_time, int64_t decode_time, 
                                       int64_t sample_time) {
    float vision_s = vision_time / 1e6;
    float audio_s = audio_time / 1e6;
    float prefill_s = (prefill_time / 1e6 + vision_s + audio_s);
    float decode_s = decode_time / 1e6;
    float sample_s = sample_time / 1e6;
    
    std::cout << "\n#################################\n";
    std::cout << "prompt tokens num = " << prompt_len << "\n";
    std::cout << "decode tokens num = " << decode_len << "\n";
    std::cout << " vision time = " << std::fixed << std::setprecision(2) << vision_s << " s\n";
    std::cout << "  audio time = " << std::fixed << std::setprecision(2) << audio_s << " s\n";
    std::cout << "prefill time = " << std::fixed << std::setprecision(2) << prefill_s << " s\n";
    std::cout << " decode time = " << std::fixed << std::setprecision(2) << decode_s << " s\n";
    std::cout << " sample time = " << std::fixed << std::setprecision(2) << sample_s << " s\n";
    std::cout << "prefill speed = " << std::fixed << std::setprecision(2) << (prompt_len / prefill_s) << " tok/s\n";
    std::cout << " decode speed = " << std::fixed << std::setprecision(2) << (decode_len / decode_s) << " tok/s\n";
    std::cout << "##################################\n";
}

void ModelRunner::ShowChatHelp() {
    std::cout << "\nAvailable commands:\n";
    std::cout << "  /help   - Show this help message\n";
    std::cout << "  /reset  - Reset conversation context\n";
    std::cout << "  /config - Show current configuration\n";
    std::cout << "  /exit   - Exit chat mode\n";
#ifdef LLM_SUPPORT_VISION
#ifndef OPENCV_NOT_AVAILABLE
    std::cout << "\nVideo prompts:\n";
    std::cout << "  Use <video>path/to/video.mp4</video> in your message to process video files\n";
    std::cout << "  Example: \"What's happening in this video? <video>demo.mp4</video>\"\n";
#endif
#endif
    std::cout << "\n";
}

void ModelRunner::ResetConversation() {
    llm_->reset();
    std::cout << "🔄 Conversation context reset.\n\n";
}

void ModelRunner::ShowConfig() {
    auto config = llm_->getConfig();
    if (config) {
        std::cout << "Current configuration:\n";
        // Add configuration display logic here
        std::cout << "Configuration loaded successfully.\n";
    } else {
        std::cout << "No configuration available.\n";
    }
    std::cout << "\n";
}
