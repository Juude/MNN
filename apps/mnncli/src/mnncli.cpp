//
//  mnncli.cpp
//
//  Created by MNN on 2023/03/24.
//  Jinde.Song
//  LLM command line tool, based on llm_demo.cpp
//
#include "../../../transformers/llm/engine/include/llm/llm.hpp"
#define MNN_OPEN_TIME_TRACE
#include <MNN/AutoTime.hpp>
#include <fstream>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include "file_utils.hpp"
#include "remote_model_downloader.hpp"
#include "llm_benchmark.hpp"
#include "mnncli_server.hpp"

using namespace MNN::Transformer;
namespace fs = std::filesystem;

// Forward declarations
class CommandLineInterface;
class UserInterface;
class ConfigManager;
class LLMManager;
class ModelManager;

// User interface utilities
class UserInterface {
public:
    static void ShowWelcome() {
        std::cout << "🚀 MNN CLI - AI Model Command Line Interface\n";
        std::cout << "Type 'mnncli --help' for available commands\n\n";
    }
    
    static void ShowProgress(const std::string& message, float progress) {
        int bar_width = 50;
        int pos = bar_width * progress;
        
        std::cout << "\r" << message << " [";
        for (int i = 0; i < bar_width; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << int(progress * 100.0) << "%" << std::flush;
        
        if (progress >= 1.0) std::cout << std::endl;
    }
    
    static void ShowError(const std::string& error, const std::string& suggestion = "") {
        std::cerr << "❌ Error: " << error << std::endl;
        if (!suggestion.empty()) {
            std::cerr << "💡 Suggestion: " << suggestion << std::endl;
        }
    }
    
    static void ShowSuccess(const std::string& message) {
        std::cout << "✅ " << message << std::endl;
    }
    
    static void ShowInfo(const std::string& message) {
        std::cout << "ℹ️  " << message << std::endl;
    }
};

// Configuration management
class ConfigManager {
public:
    struct Config {
        std::string default_model;
        std::string cache_dir;
        std::string log_level;
        int default_max_tokens;
        float default_temperature;
        std::string api_host;
        int api_port;
        int api_workers;
    };
    
    static Config LoadDefaultConfig() {
        return {
            .default_model = "",
            .cache_dir = "~/.cache/mnncli",
            .log_level = "info",
            .default_max_tokens = 1000,
            .default_temperature = 0.7f,
            .api_host = "127.0.0.1",
            .api_port = 8000,
            .api_workers = 4
        };
    }
    
    static void ShowConfig(const Config& config) {
        std::cout << "Configuration:\n";
        std::cout << "  Default Model: " << (config.default_model.empty() ? "Not set" : config.default_model) << "\n";
        std::cout << "  Cache Directory: " << config.cache_dir << "\n";
        std::cout << "  Log Level: " << config.log_level << "\n";
        std::cout << "  Default Max Tokens: " << config.default_max_tokens << "\n";
        std::cout << "  Default Temperature: " << config.default_temperature << "\n";
        std::cout << "  API Host: " << config.api_host << "\n";
        std::cout << "  API Port: " << config.api_port << "\n";
        std::cout << "  API Workers: " << config.api_workers << "\n";
    }
};

// LLM management
class LLMManager {
public:
    static std::unique_ptr<Llm> CreateLLM(const std::string& config_path, bool use_template) {
        std::unique_ptr<Llm> llm(Llm::createLLM(config_path));
        if (use_template) {
            llm->set_config("{\"tmp_path\":\"tmp\"}");
        } else {
            llm->set_config("{\"tmp_path\":\"tmp\",\"use_template\":false}");
        }
        {
            AUTOTIME;
            llm->load();
        }
        if (true) {
            AUTOTIME;
            TuningPrepare(llm.get());
        }
        return llm;
    }
    
private:
    static void TuningPrepare(Llm* llm) {
        MNN_PRINT("Prepare for tuning opt Begin\n");
        llm->tuning(OP_ENCODER_NUMBER, {1, 5, 10, 20, 30, 50, 100});
        MNN_PRINT("Prepare for tuning opt End\n");
    }
};

// Model management
class ModelManager {
public:
    static int ListLocalModels() {
        std::vector<std::string> model_names;
        int result = list_local_models(mnncli::FileUtils::GetBaseCacheDir(), model_names);
        
        if (result != 0) {
            UserInterface::ShowError("Failed to list local models");
            return result;
        }
        
        if (!model_names.empty()) {
            std::cout << "Local models:\n";
            for (auto& name : model_names) {
                std::cout << "  📁 " << name << "\n";
            }
        } else {
            std::cout << "No local models found.\n";
            std::cout << "Use 'mnncli model search <keyword>' to search remote models\n";
            std::cout << "Use 'mnncli model download <name>' to download models\n";
        }
        return 0;
    }
    
    static int SearchRemoteModels(const std::string& keyword) {
        if (keyword.empty()) {
            UserInterface::ShowError("Search keyword is required", "Usage: mnncli model search <keyword>");
            return 1;
        }
        
        try {
            mnncli::HfApiClient client;
            auto repos = client.SearchRepos(keyword);
            
            if (repos.empty()) {
                std::cout << "No models found for keyword: " << keyword << "\n";
                return 0;
            }
            
            std::cout << "Found " << repos.size() << " models:\n";
            for (auto& repo : repos) {
                auto pos = repo.model_id.rfind('/');
                std::string name = (pos != std::string::npos) ? repo.model_id.substr(pos + 1) : repo.model_id;
                std::cout << "  🔍 " << name << " (downloads: " << repo.downloads << ")\n";
            }
        } catch (const std::exception& e) {
            UserInterface::ShowError("Failed to search models: " + std::string(e.what()));
            return 1;
        }
        return 0;
    }
    
    static int DownloadModel(const std::string& model_name) {
        if (model_name.empty()) {
            UserInterface::ShowError("Model name is required", "Usage: mnncli model download <name>");
            return 1;
        }
        
        std::cout << "Downloading model: " << model_name << "\n";
        try {
            mnncli::HfApiClient api_client;
            std::string error_info;
            std::string repo_name = model_name;
            
            if (repo_name.find("taobao-mnn/") != 0) {
                repo_name = "taobao-mnn/" + repo_name;
            }
            
            const auto repo_info = api_client.GetRepoInfo(repo_name, "main", error_info);
            if (!error_info.empty()) {
                UserInterface::ShowError("Failed to get repo info: " + error_info);
                return 1;
            }
            
            UserInterface::ShowProgress("Downloading model", 0.0f);
            api_client.DownloadRepo(repo_info);
            UserInterface::ShowProgress("Downloading model", 1.0f);
            UserInterface::ShowSuccess("Model downloaded successfully: " + model_name);
            
        } catch (const std::exception& e) {
            UserInterface::ShowError("Failed to download model: " + std::string(e.what()));
            return 1;
        }
        return 0;
    }
    
    static int DeleteModel(const std::string& model_name) {
        if (model_name.empty()) {
            UserInterface::ShowError("Model name is required", "Usage: mnncli model delete <name>");
            return 1;
        }
        
        std::cout << "Deleting model: " << model_name << "\n";
        try {
            std::string linker_path = mnncli::FileUtils::GetFolderLinkerPath(model_name);
            mnncli::FileUtils::RemoveFileIfExists(linker_path);
            
            std::string full_name = model_name;
            if (model_name.find("taobao-mnn") != 0) {
                full_name = "taobao-mnn/" + model_name;
            }
            
            std::string storage_path = mnncli::FileUtils::GetStorageFolderPath(full_name);
            mnncli::FileUtils::RemoveFileIfExists(storage_path);
            
            UserInterface::ShowSuccess("Model deleted successfully: " + model_name);
        } catch (const std::exception& e) {
            UserInterface::ShowError("Failed to delete model: " + std::string(e.what()));
            return 1;
        }
        return 0;
    }
    
private:
    static int list_local_models(const std::string& directory_path, std::vector<std::string>& model_names) {
        std::error_code ec;
        if (!fs::exists(directory_path, ec)) {
            return 1;
        }
        if (!fs::is_directory(directory_path, ec)) {
            return 1;
        }
        for (const auto& entry : fs::directory_iterator(directory_path, ec)) {
            if (ec) {
                return 1;
            }
            if (fs::is_symlink(entry, ec)) {
                if (ec) {
                    return 1;
                }
                std::string file_name = entry.path().filename().string();
                model_names.emplace_back(file_name);
            }
        }
        std::sort(model_names.begin(), model_names.end());
        return 0;
    }
};

// Interactive chat session
class InteractiveChat {
public:
    InteractiveChat(Llm* llm) : llm_(llm) {}
    
    void Start() {
        std::cout << "🚀 Starting interactive chat mode...\n";
        std::cout << "Commands: /help, /reset, /config, /exit\n\n";
        
        while (true) {
            std::cout << "👤 User: ";
            std::string input;
            std::getline(std::cin, input);
            
            if (input == "/exit") break;
            if (input == "/help") ShowHelp();
            else if (input == "/reset") Reset();
            else if (input == "/config") ShowConfig();
            else if (!input.empty()) ProcessInput(input);
        }
    }
    
private:
    void ProcessInput(const std::string& input) {
        ChatMessages messages;
        messages.emplace_back("system", "You are a helpful assistant.");
        messages.emplace_back("user", input);
        
        std::cout << "\n🤖 Assistant: " << std::flush;
        llm_->response(messages);
        auto context = llm_->getContext();
        auto assistant_str = context->generate_str;
        std::cout << assistant_str << "\n";
        
        // Keep only last 100 messages to prevent memory issues
        if (messages.size() > 100) {
            messages.erase(messages.begin() + 1, messages.begin() + 2);
        }
    }
    
    void ShowHelp() {
        std::cout << "\nAvailable commands:\n";
        std::cout << "  /help   - Show this help message\n";
        std::cout << "  /reset  - Reset conversation context\n";
        std::cout << "  /config - Show current configuration\n";
        std::cout << "  /exit   - Exit chat mode\n\n";
    }
    
    void Reset() {
        llm_->reset();
        std::cout << "🔄 Conversation context reset.\n\n";
    }
    
    void ShowConfig() {
        auto config = ConfigManager::LoadDefaultConfig();
        ConfigManager::ShowConfig(config);
        std::cout << "\n";
    }
    
    Llm* llm_;
};

// Performance evaluation
class PerformanceEvaluator {
public:
    static int EvalPrompts(Llm* llm, const std::vector<std::string>& prompts) {
        int prompt_len = 0;
        int decode_len = 0;
        int64_t vision_time = 0;
        int64_t audio_time = 0;
        int64_t prefill_time = 0;
        int64_t decode_time = 0;
        
        for (int i = 0; i < prompts.size(); i++) {
            const auto& prompt = prompts[i];
            if (prompt.substr(0, 1) == "#") {
                continue;
            }
            
            llm->response(prompt);
            auto context = llm->getContext();
            prompt_len += context->prompt_len;
            decode_len += context->gen_seq_len;
            prefill_time += context->prefill_us;
            decode_time += context->decode_us;
        }
        
        float vision_s = vision_time / 1e6;
        float audio_s = audio_time / 1e6;
        float prefill_s = prefill_time / 1e6;
        float decode_s = decode_time / 1e6;
        
        std::cout << "\n📊 Performance Report\n";
        std::cout << "=====================\n";
        std::cout << "Prompt tokens: " << prompt_len << "\n";
        std::cout << "Decode tokens: " << decode_len << "\n";
        std::cout << "Vision time: " << std::fixed << std::setprecision(2) << vision_s << "s\n";
        std::cout << "Audio time: " << std::fixed << std::setprecision(2) << audio_s << "s\n";
        std::cout << "Prefill time: " << std::fixed << std::setprecision(2) << prefill_s << "s\n";
        std::cout << "Decode time: " << std::fixed << std::setprecision(2) << decode_s << "s\n";
        std::cout << "Prefill speed: " << std::fixed << std::setprecision(2) << (prompt_len / prefill_s) << " tok/s\n";
        std::cout << "Decode speed: " << std::fixed << std::setprecision(2) << (decode_len / decode_s) << " tok/s\n";
        std::cout << "=====================\n";
        
        return 0;
    }
    
    static int EvalFile(Llm* llm, const std::string& prompt_file) {
        std::cout << "Reading prompts from: " << prompt_file << "\n";
        std::ifstream prompt_fs(prompt_file);
        if (!prompt_fs.is_open()) {
            UserInterface::ShowError("Failed to open prompt file: " + prompt_file);
            return 1;
        }
        
        std::vector<std::string> prompts;
        std::string prompt;
        while (std::getline(prompt_fs, prompt)) {
            if (prompt.back() == '\r') {
                prompt.pop_back();
            }
            prompts.push_back(prompt);
        }
        prompt_fs.close();
        
        if (prompts.empty()) {
            UserInterface::ShowError("Prompt file is empty");
            return 1;
        }
        
        return EvalPrompts(llm, prompts);
    }
};

// Main command line interface
class CommandLineInterface {
public:
    CommandLineInterface() : verbose_(false) {}
    
    int Run(int argc, const char* argv[]) {
        if (argc < 2) {
            PrintUsage();
            return 0;
        }
        
        std::string cmd = argv[1];
        
        try {
            if (cmd == "model") {
                return HandleModelCommand(argc, argv);
            } else if (cmd == "run") {
                return HandleRunCommand(argc, argv);
            } else if (cmd == "serve") {
                return HandleServeCommand(argc, argv);
            } else if (cmd == "benchmark") {
                return HandleBenchmarkCommand(argc, argv);
            } else if (cmd == "config") {
                return HandleConfigCommand(argc, argv);
            } else if (cmd == "info") {
                return HandleInfoCommand(argc, argv);
            } else if (cmd == "--help" || cmd == "-h") {
                PrintUsage();
                return 0;
            } else if (cmd == "--version" || cmd == "-v") {
                PrintVersion();
                return 0;
            } else {
                // Legacy command support for backward compatibility
                return HandleLegacyCommand(argc, argv);
            }
        } catch (const std::exception& e) {
            UserInterface::ShowError("Unexpected error: " + std::string(e.what()));
            return 1;
        }
        
        return 0;
    }
    
private:
    int HandleModelCommand(int argc, const char* argv[]) {
        if (argc < 3) {
            PrintModelUsage();
            return 1;
        }
        
        std::string subcmd = argv[2];
        
        if (subcmd == "list") {
            return ModelManager::ListLocalModels();
        } else if (subcmd == "search") {
            if (argc < 4) {
                UserInterface::ShowError("Search keyword required", "Usage: mnncli model search <keyword>");
                return 1;
            }
            return ModelManager::SearchRemoteModels(argv[3]);
        } else if (subcmd == "download") {
            if (argc < 4) {
                UserInterface::ShowError("Model name required", "Usage: mnncli model download <name>");
                return 1;
            }
            return ModelManager::DownloadModel(argv[3]);
        } else if (subcmd == "delete") {
            if (argc < 4) {
                UserInterface::ShowError("Model name required", "Usage: mnncli model delete <name>");
                return 1;
            }
            return ModelManager::DeleteModel(argv[3]);
        } else {
            PrintModelUsage();
            return 1;
        }
    }
    
    int HandleRunCommand(int argc, const char* argv[]) {
        if (argc < 3) {
            UserInterface::ShowError("Model name required", "Usage: mnncli run <model_name> [options]");
            return 1;
        }
        
        std::string model_name = argv[2];
        std::string config_path = mnncli::FileUtils::GetConfigPath(model_name);
        std::string prompt;
        std::string prompt_file;
        
        // Parse options
        for (int i = 3; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "-p" || arg == "--prompt") {
                if (++i >= argc) {
                    UserInterface::ShowError("Missing prompt text", "Usage: -p <prompt_text>");
                    return 1;
                }
                prompt = argv[i];
            } else if (arg == "-f" || arg == "--file") {
                if (++i >= argc) {
                    UserInterface::ShowError("Missing prompt file", "Usage: -f <prompt_file>");
                    return 1;
                }
                prompt_file = argv[i];
            } else if (arg == "-c" || arg == "--config") {
                if (++i >= argc) {
                    UserInterface::ShowError("Missing config path", "Usage: -c <config_path>");
                    return 1;
                }
                config_path = mnncli::FileUtils::ExpandTilde(argv[i]);
            }
        }
        
        if (config_path.empty()) {
            UserInterface::ShowError("Config path is empty", "Use -c to specify config path");
            return 1;
        }
        
        std::cout << "Starting model: " << model_name << "\n";
        std::cout << "Config path: " << config_path << "\n";
        
        auto llm = LLMManager::CreateLLM(config_path, true);
        
        if (prompt.empty() && prompt_file.empty()) {
            InteractiveChat chat(llm.get());
            chat.Start();
        } else if (!prompt.empty()) {
            PerformanceEvaluator::EvalPrompts(llm.get(), {prompt});
        } else {
            PerformanceEvaluator::EvalFile(llm.get(), prompt_file);
        }
        
        return 0;
    }
    
    int HandleServeCommand(int argc, const char* argv[]) {
        if (argc < 3) {
            UserInterface::ShowError("Model name required", "Usage: mnncli serve <model_name> [options]");
            return 1;
        }
        
        std::string model_name = argv[2];
        std::string config_path = (fs::path(mnncli::FileUtils::GetBaseCacheDir()) / model_name / "config.json").string();
        std::string host = "127.0.0.1";
        int port = 8000;
        
        // Parse options
        for (int i = 3; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "-c" || arg == "--config") {
                if (++i >= argc) {
                    UserInterface::ShowError("Missing config path", "Usage: -c <config_path>");
                    return 1;
                }
                config_path = mnncli::FileUtils::ExpandTilde(argv[i]);
            } else if (arg == "--host") {
                if (++i >= argc) {
                    UserInterface::ShowError("Missing host", "Usage: --host <host>");
                    return 1;
                }
                host = argv[i];
            } else if (arg == "--port") {
                if (++i >= argc) {
                    UserInterface::ShowError("Missing port", "Usage: --port <port>");
                    return 1;
                }
                port = std::stoi(argv[i]);
            }
        }
        
        std::cout << "Starting API server for model: " << model_name << "\n";
        std::cout << "Host: " << host << ":" << port << "\n";
        
        mnncli::MnncliServer server;
        bool is_r1 = IsR1(config_path);
        auto llm = LLMManager::CreateLLM(config_path, !is_r1);
        server.Start(llm.get(), is_r1);
        
        return 0;
    }
    
    int HandleBenchmarkCommand(int argc, const char* argv[]) {
        if (argc < 3) {
            UserInterface::ShowError("Model name required", "Usage: mnncli benchmark <model_name> [options]");
            return 1;
        }
        
        std::string model_name = argv[2];
        std::string config_path = mnncli::FileUtils::GetConfigPath(model_name);
        
        // Parse options
        for (int i = 3; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "-c" || arg == "--config") {
                if (++i >= argc) {
                    UserInterface::ShowError("Missing config path", "Usage: -c <config_path>");
                    return 1;
                }
                config_path = argv[i];
            }
        }
        
        if (config_path.empty()) {
            UserInterface::ShowError("Config path is empty", "Use -c to specify config path");
            return 1;
        }
        
        std::cout << "Starting benchmark for model: " << model_name << "\n";
        
        auto llm = LLMManager::CreateLLM(config_path, true);
        mnncli::LLMBenchmark benchmark;
        benchmark.Start(llm.get(), {});
        
        return 0;
    }
    
    int HandleConfigCommand(int argc, const char* argv[]) {
        if (argc < 3) {
            ConfigManager::ShowConfig(ConfigManager::LoadDefaultConfig());
            return 0;
        }
        
        std::string subcmd = argv[2];
        if (subcmd == "show") {
            ConfigManager::ShowConfig(ConfigManager::LoadDefaultConfig());
        } else if (subcmd == "set") {
            if (argc < 5) {
                UserInterface::ShowError("Missing key or value", "Usage: mnncli config set <key> <value>");
                return 1;
            }
            UserInterface::ShowInfo("Config set command not implemented yet");
        } else if (subcmd == "reset") {
            UserInterface::ShowInfo("Config reset command not implemented yet");
        } else {
            UserInterface::ShowError("Unknown config subcommand", "Use: show, set, or reset");
            return 1;
        }
        
        return 0;
    }
    
    int HandleInfoCommand(int argc, const char* argv[]) {
        std::cout << "MNN CLI Information:\n";
        std::cout << "====================\n";
        std::cout << "Version: 1.0.0\n";
        std::cout << "Cache Directory: " << mnncli::FileUtils::GetBaseCacheDir() << "\n";
        std::cout << "Available Models: ";
        
        std::vector<std::string> model_names;
        if (list_local_models(mnncli::FileUtils::GetBaseCacheDir(), model_names) == 0) {
            std::cout << model_names.size() << "\n";
        } else {
            std::cout << "Unknown\n";
        }
        
        return 0;
    }
    
    int HandleLegacyCommand(int argc, const char* argv[]) {
        std::string cmd = argv[1];
        
        if (cmd == "list") {
            return ModelManager::ListLocalModels();
        } else if (cmd == "search") {
            if (argc < 3) {
                UserInterface::ShowError("Search keyword required", "Usage: mnncli search <keyword>");
                return 1;
            }
            return ModelManager::SearchRemoteModels(argv[2]);
        } else if (cmd == "download") {
            if (argc < 3) {
                UserInterface::ShowError("Model name required", "Usage: mnncli download <name>");
                return 1;
            }
            return ModelManager::DownloadModel(argv[2]);
        } else if (cmd == "delete") {
            if (argc < 3) {
                UserInterface::ShowError("Model name required", "Usage: mnncli delete <name>");
                return 1;
            }
            return ModelManager::DeleteModel(argv[2]);
        } else if (cmd == "run") {
            return HandleRunCommand(argc, argv);
        } else if (cmd == "serve") {
            return HandleServeCommand(argc, argv);
        } else if (cmd == "benchmark") {
            return HandleBenchmarkCommand(argc, argv);
        } else {
            PrintUsage();
            return 1;
        }
    }
    
    void PrintUsage() {
        std::cout << "MNN CLI - AI Model Command Line Interface\n\n";
        std::cout << "Usage: mnncli <command> [options]\n\n";
        std::cout << "Commands:\n";
        std::cout << "  model     Manage models (list, search, download, delete)\n";
        std::cout << "  run       Run model inference\n";
        std::cout << "  serve     Start API server\n";
        std::cout << "  benchmark Run performance benchmarks\n";
        std::cout << "  config    Manage configuration\n";
        std::cout << "  info      Show system information\n";
        std::cout << "\nGlobal Options:\n";
        std::cout << "  --help    Show this help message\n";
        std::cout << "  --version Show version information\n";
        std::cout << "\nExamples:\n";
        std::cout << "  mnncli model list                    # List local models\n";
        std::cout << "  mnncli model search qwen             # Search for Qwen models\n";
        std::cout << "  mnncli run qwen-7b                  # Run Qwen-7B model\n";
        std::cout << "  mnncli serve qwen-7b --port 8000    # Start API server\n";
        std::cout << "  mnncli benchmark qwen-7b            # Run benchmark\n";
    }
    
    void PrintModelUsage() {
        std::cout << "Model Management Commands:\n";
        std::cout << "  mnncli model list                    # List local models\n";
        std::cout << "  mnncli model search <keyword>        # Search remote models\n";
        std::cout << "  mnncli model download <name>         # Download model\n";
        std::cout << "  mnncli model delete <name>           # Delete model\n";
    }
    
    void PrintVersion() {
        std::cout << "MNN CLI version 1.0.0\n";
        std::cout << "Built with MNN framework\n";
    }
    
    static bool IsR1(const std::string& path) {
        std::string lowerModelName = path;
        std::transform(lowerModelName.begin(), lowerModelName.end(), lowerModelName.begin(), ::tolower);
        return lowerModelName.find("deepseek-r1") != std::string::npos;
    }
    
    static int list_local_models(const std::string& directory_path, std::vector<std::string>& model_names) {
        std::error_code ec;
        if (!fs::exists(directory_path, ec)) {
            return 1;
        }
        if (!fs::is_directory(directory_path, ec)) {
            return 1;
        }
        for (const auto& entry : fs::directory_iterator(directory_path, ec)) {
            if (ec) {
                return 1;
            }
            if (fs::is_symlink(entry, ec)) {
                if (ec) {
                    return 1;
                }
                std::string file_name = entry.path().filename().string();
                model_names.emplace_back(file_name);
            }
        }
        std::sort(model_names.begin(), model_names.end());
        return 0;
    }
    
    bool verbose_;
};

int main(int argc, const char* argv[]) {
    UserInterface::ShowWelcome();
    
    CommandLineInterface cli;
    return cli.Run(argc, argv);
}
