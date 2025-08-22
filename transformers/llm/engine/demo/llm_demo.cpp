//
//  llm_demo.cpp
//
//  Created by MNN on 2023/03/24.
//  ZhaodeWang
//

#include "llm/llm.hpp"
#define MNN_OPEN_TIME_TRACE
#include <MNN/AutoTime.hpp>
#include <MNN/expr/ExecutorScope.hpp>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <initializer_list>
#include "llmconfig.hpp"
#include <regex>
#ifdef LLM_SUPPORT_VISION
#include <opencv2/opencv.hpp>
#endif
#ifdef LLM_SUPPORT_AUDIO
#include "audio/audio.hpp"
#endif

#ifdef LLM_SUPPORT_VISION
MNN::Express::VARP mat_to_var(const cv::Mat& mat) {
    // Ensure the mat is not empty
    if (mat.empty()) {
        MNN_ERROR("Input cv::Mat is empty!\n");
        return nullptr;
    }
    // Only support CV_8UC3 for now
    if (mat.type() != CV_8UC3) {
        MNN_ERROR("Only support CV_8UC3 for mat_to_var!\n");
        return nullptr;
    }
    auto var = MNN::Express::_Input({mat.rows, mat.cols, 3}, MNN::Express::NHWC, halide_type_of<uint8_t>());
    auto ptr = var->writeMap<uint8_t>();
    memcpy(ptr, mat.data, mat.total() * mat.elemSize());
return var;
}
#endif

using namespace MNN::Transformer;

static void tuning_prepare(Llm* llm) {
    MNN_PRINT("Prepare for tuning opt Begin\n");
    llm->tuning(OP_ENCODER_NUMBER, {1, 5, 10, 20, 30, 50, 100});
    MNN_PRINT("Prepare for tuning opt End\n");
}

std::vector<std::vector<std::string>> parse_csv(const std::vector<std::string>& lines) {
    std::vector<std::vector<std::string>> csv_data;
    std::string line;
    std::vector<std::string> row;
    std::string cell;
    bool insideQuotes = false;
    bool startCollecting = false;

    // content to stream
    std::string content = "";
    for (auto line : lines) {
        content = content + line + "\n";
    }
    std::istringstream stream(content);

    while (stream.peek() != EOF) {
        char c = stream.get();
        if (c == '"') {
            if (insideQuotes && stream.peek() == '"') { // quote
                cell += '"';
                stream.get(); // skip quote
            } else {
                insideQuotes = !insideQuotes; // start or end text in quote
            }
            startCollecting = true;
        } else if (c == ',' && !insideQuotes) { // end element, start new element
            row.push_back(cell);
            cell.clear();
            startCollecting = false;
        } else if ((c == '\n' || stream.peek() == EOF) && !insideQuotes) { // end line
            row.push_back(cell);
            csv_data.push_back(row);
            cell.clear();
            row.clear();
            startCollecting = false;
        } else {
            cell += c;
            startCollecting = true;
        }
    }
    return csv_data;
}

static int benchmark(Llm* llm, const std::vector<std::string>& prompts, int max_token_number) {
    int prompt_len = 0;
    int decode_len = 0;
    // llm->warmup();
    auto context = llm->getContext();
    if (max_token_number > 0) {
        llm->set_config("{\"max_new_tokens\":1}");
    }
#ifdef LLM_SUPPORT_AUDIO
    std::vector<float> waveform;
    llm->setWavformCallback([&](const float* ptr, size_t size, bool last_chunk) {
        waveform.reserve(waveform.size() + size);
        waveform.insert(waveform.end(), ptr, ptr + size);
        if (last_chunk) {
            auto waveform_var = MNN::Express::_Const(waveform.data(), {(int)waveform.size()}, MNN::Express::NCHW, halide_type_of<float>());
            MNN::AUDIO::save("output.wav", waveform_var, 24000);
            waveform.clear();
        }
        return true;
    });
#endif
    for (int i = 0; i < prompts.size(); i++) {
        auto prompt = prompts[i];
     // #define MIMO_NO_THINKING
     #ifdef MIMO_NO_THINKING
        // update config.json and llm_config.json if need. example:
        llm->set_config("{\"assistant_prompt_template\":\"<|im_start|>assistant\\n<think>\\n</think>\%s<|im_end|>\\n\"}");
        prompt = prompt + "<think>\n</think>";
     #endif

        // prompt start with '#' will be ignored
        if (prompt.substr(0, 1) == "#") {
            continue;
        }
        if (max_token_number >= 0) {
            llm->response(prompt, &std::cout, nullptr, 0);
            while (!llm->stoped() && context->gen_seq_len < max_token_number) {
                llm->generate(1);
            }
        } else {
            llm->response(prompt);
        }
        prompt_len += context->prompt_len;
        decode_len += context->gen_seq_len;
    }
    llm->generateWavform();
    return 0;
}

static int ceval(Llm* llm, const std::vector<std::string>& lines, std::string filename) {
    auto csv_data = parse_csv(lines);
    int right = 0, wrong = 0;
    std::vector<std::string> answers;
    for (int i = 1; i < csv_data.size(); i++) {
        const auto& elements = csv_data[i];
        std::string prompt = elements[1];
        prompt += "\n\nA. " + elements[2];
        prompt += "\nB. " + elements[3];
        prompt += "\nC. " + elements[4];
        prompt += "\nD. " + elements[5];
        prompt += "\n\n";
        printf("%s", prompt.c_str());
        printf("## 进度: %d / %lu\n", i, lines.size() - 1);
        std::ostringstream lineOs;
        llm->response(prompt.c_str(), &lineOs);
        auto line = lineOs.str();
        printf("%s", line.c_str());
        answers.push_back(line);
    }
    {
        auto position = filename.rfind("/");
        if (position != std::string::npos) {
            filename = filename.substr(position + 1, -1);
        }
        position = filename.find("_val");
        if (position != std::string::npos) {
            filename.replace(position, 4, "_res");
        }
        std::cout << "store to " << filename << std::endl;
    }
    std::ofstream ofp(filename);
    ofp << "id,answer" << std::endl;
    for (int i = 0; i < answers.size(); i++) {
        auto& answer = answers[i];
        ofp << i << ",\""<< answer << "\"" << std::endl;
    }
    ofp.close();
    return 0;
}

static int eval(Llm* llm, std::string prompt_file, int max_token_number) {
    std::cout << "prompt file is " << prompt_file << std::endl;
    std::string ext;
    if (prompt_file.find_last_of(".") != std::string::npos) {
        ext = prompt_file.substr(prompt_file.find_last_of(".") + 1);
    }
    if (ext == "mp4" || ext == "avi" || ext == "mov") {
#ifdef LLM_SUPPORT_VISION
        MNN_PRINT("Video file input via file argument is deprecated. Use `-p 'prompt:<video>/path/to/video.mp4</video>'` instead.\n");
        return 1;
#else
        MNN_PRINT("LLM_SUPPORT_VISION is not enabled, can't process video file\n");
        return 1;
#endif
    }
    std::ifstream prompt_fs(prompt_file);
    std::vector<std::string> prompts;
    std::string prompt;
//#define LLM_DEMO_ONELINE
#ifdef LLM_DEMO_ONELINE
    std::ostringstream tempOs;
    tempOs << prompt_fs.rdbuf();
    prompt = tempOs.str();
    prompts = {prompt};
#else
    while (std::getline(prompt_fs, prompt)) {
        if (prompt.empty()) {
            continue;
        }
        if (prompt.back() == '\r') {
            prompt.pop_back();
        }
        prompts.push_back(prompt);
    }
#endif
    prompt_fs.close();
    if (prompts.empty()) {
        return 1;
    }
    // ceval
    if (prompts[0] == "id,question,A,B,C,D,answer") {
        return ceval(llm, prompts, prompt_file);
    }
    return benchmark(llm, prompts, max_token_number);
}

void chat(Llm* llm) {
    auto context = llm->getContext();
    while (true) {
        std::cout << "\nUser: ";
        std::string user_str;
        std::getline(std::cin, user_str);
        if (user_str == "/exit") {
            return;
        }
        if (user_str == "/reset") {
            llm->reset();
            std::cout << "\nA: reset done." << std::endl;
            continue;
        }
        std::vector<std::pair<std::string, std::string>> messages = {{"user", user_str}};
        std::cout << "\nA: " << std::flush;
        llm->response(messages);
        std::cout << std::endl;
    }
}

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " config.json [prompt.txt] or -p \"prompt\"" << std::endl;
        return 0;
    }
    MNN::BackendConfig backendConfig;
    auto executor = MNN::Express::Executor::newExecutor(MNN_FORWARD_CPU, backendConfig, 1);
    MNN::Express::ExecutorScope s(executor);

    std::string config_path = argv[1];
    std::cout << "config path is " << config_path << std::endl;
    std::unique_ptr<Llm> llm(Llm::createLLM(config_path));
    llm->set_config("{\"tmp_path\":\"tmp\"}");
    {
        AUTOTIME;
        llm->load();
    }
    if (true) {
        AUTOTIME;
        tuning_prepare(llm.get());
    }
    int max_new_tokens = -1;
    // get max_new_tokens from config.json
    auto config = llm->getConfig();
    if (config) {
        max_new_tokens = config->config_.value("max_new_tokens", max_new_tokens);
    }
    if (argc > 3) {
        std::string prompt_arg = argv[2];
        if (prompt_arg != "-p") {
            std::istringstream os(argv[3]);
            os >> max_new_tokens;
        }
    }
    
    llm->set_config("{\"max_new_tokens\":1}");

    if (argc > 2) {
        std::string prompt_arg = argv[2];
        if (prompt_arg == "-p") {
            if (argc > 3) {
                std::string prompt_str = argv[3];
#ifdef LLM_SUPPORT_VISION
                std::regex video_regex("<video>(.*?)</video>");
                std::smatch match;
                if (std::regex_search(prompt_str, match, video_regex)) {
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
                            images.push_back(mat_to_var(frame));
                        }
                    }
                    cap.release();
                    std::cout << "Final prompt: " << final_prompt << std::endl;
                    std::cout << "Read " << images.size() << " frames from video." << std::endl;
                    MNN::AutoTime _t(0, "responseWithImages");
                    llm->responseWithImages(final_prompt, images, &std::cout, nullptr, 9999);

                } else {
                    MNN::AutoTime _t(0, "response");
                    llm->response(prompt_str, &std::cout, nullptr, max_new_tokens);
                }
#else
                MNN::AutoTime _t(0, "response");
                llm->response(prompt_str, &std::cout, nullptr, max_new_tokens);
#endif
                return 0;
            } else {
                MNN_PRINT("Error: -p flag requires a prompt string.\n");
                return 1;
            }
        }
        return eval(llm.get(), prompt_arg, max_new_tokens);
    }
    chat(llm.get());
    return 0;
}
