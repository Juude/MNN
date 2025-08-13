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
#include <thread>
#include <chrono>
#include "file_utils.hpp"
#include "remote_model_downloader.hpp"
#include "hf_api_client.hpp"
#include "ms_api_client.hpp"
#include "ms_model_downloader.hpp"
#include "ml_model_downloader.hpp"
#include "ml_api_client.hpp"
#include "llm_benchmark.hpp"
#include "mnncli_server.hpp"
#include "nlohmann/json.hpp"

using namespace MNN::Transformer;
namespace fs = std::filesystem;

// Forward declarations
class CommandLineInterface;
class UserInterface;
class LLMManager;
class ModelManager;

// User interface utilities
class UserInterface {
public:
    static void ShowWelcome() {
        std::cout << "🚀 MNN CLI - MNN Command Line Interface\n";
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
namespace ConfigManager {
    struct Config {
        std::string default_model;
        std::string cache_dir;
        std::string log_level;
        int default_max_tokens;
        float default_temperature;
        std::string api_host;
        int api_port;
        int api_workers;
        std::string download_provider;  // "huggingface", "modelscope", or "modelers"
    };
    
    static std::string GetConfigFilePath() {
        std::string config_dir = mnncli::FileUtils::GetBaseCacheDir();
        return (fs::path(config_dir) / "mnncli_config.json").string();
    }
    
    static bool SaveConfig(const Config& config) {
        try {
            std::string config_path = GetConfigFilePath();
            std::string config_dir = fs::path(config_path).parent_path().string();
            
            // Create config directory if it doesn't exist
            if (!fs::exists(config_dir)) {
                fs::create_directories(config_dir);
            }
            
            nlohmann::json j;
            j["default_model"] = config.default_model;
            j["cache_dir"] = config.cache_dir;
            j["log_level"] = config.log_level;
            j["default_max_tokens"] = config.default_max_tokens;
            j["default_temperature"] = config.default_temperature;
            j["api_host"] = config.api_host;
            j["api_port"] = config.api_port;
            j["api_workers"] = config.api_workers;
            j["download_provider"] = config.download_provider;
            
            std::ofstream file(config_path);
            if (file.is_open()) {
                file << j.dump(2);
                file.close();
                return true;
            }
        } catch (...) {
            // Silently fail if we can't save config
        }
        return false;
    }
    
    static Config LoadDefaultConfig() {
        Config config;
        
        // Try to load from config file first
        std::string config_path = GetConfigFilePath();
        if (fs::exists(config_path)) {
            try {
                std::ifstream file(config_path);
                if (file.is_open()) {
                    nlohmann::json j;
                    file >> j;
                    
                    config.default_model = j.value("default_model", "");
                    config.cache_dir = j.value("cache_dir", "~/.cache/mnncli");
                    config.log_level = j.value("log_level", "info");
                    config.default_max_tokens = j.value("default_max_tokens", 1000);
                    config.default_temperature = j.value("default_temperature", 0.7f);
                    config.api_host = j.value("api_host", "127.0.0.1");
                    config.api_port = j.value("api_port", 8000);
                    config.api_workers = j.value("api_workers", 4);
                    config.download_provider = j.value("download_provider", "huggingface");
                    
                    file.close();
                    
                    // Check environment variables (they take precedence over file)
                    if (const char* env_provider = std::getenv("MNN_DOWNLOAD_PROVIDER")) {
                        config.download_provider = env_provider;
                    }
                    if (const char* env_cache = std::getenv("MNN_CACHE_DIR")) {
                        config.cache_dir = env_cache;
                    }
                    if (const char* env_host = std::getenv("MNN_API_HOST")) {
                        config.api_host = env_host;
                    }
                    if (const char* env_port = std::getenv("MNN_API_PORT")) {
                        try {
                            config.api_port = std::stoi(env_port);
                        } catch (...) {
                            // Keep file value if invalid
                        }
                    }
                    
                    return config;
                }
            } catch (...) {
                // If file loading fails, fall back to defaults
            }
        }
        
        // Fall back to defaults and environment variables
        std::string download_provider = "huggingface"; // default
        if (const char* env_provider = std::getenv("MNN_DOWNLOAD_PROVIDER")) {
            download_provider = env_provider;
        }
        
        std::string cache_dir = "~/.cache/mnncli";
        if (const char* env_cache = std::getenv("MNN_CACHE_DIR")) {
            cache_dir = env_cache;
        }
        
        std::string api_host = "127.0.0.1";
        if (const char* env_host = std::getenv("MNN_API_HOST")) {
            api_host = env_host;
        }
        
        int api_port = 8000;
        if (const char* env_port = std::getenv("MNN_API_PORT")) {
            try {
                api_port = std::stoi(env_port);
            } catch (...) {
                // Keep default if invalid
            }
        }
        
        return {
            .default_model = "",
            .cache_dir = cache_dir,
            .log_level = "info",
            .default_max_tokens = 1000,
            .default_temperature = 0.7f,
            .api_host = api_host,
            .api_port = api_port,
            .api_workers = 4,
            .download_provider = download_provider
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
        std::cout << "  Download Provider: " << config.download_provider << "\n";
    }
    
    static bool SetConfigValue(Config& config, const std::string& key, const std::string& value) {
        if (key == "download_provider") {
            std::string lower_value = value;
            std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);
            if (lower_value == "huggingface" || lower_value == "hf" || 
                lower_value == "modelscope" || lower_value == "ms" || 
                lower_value == "modelers") {
                config.download_provider = lower_value;
                return true;
            } else {
                return false;
            }
        } else if (key == "cache_dir") {
            config.cache_dir = value;
            return true;
        } else if (key == "log_level") {
            config.log_level = value;
            return true;
        } else if (key == "api_host") {
            config.api_host = value;
            return true;
        } else if (key == "default_max_tokens") {
            try {
                config.default_max_tokens = std::stoi(value);
                return true;
            } catch (...) {
                return false;
            }
        } else if (key == "default_temperature") {
            try {
                config.default_temperature = std::stof(value);
                return true;
            } catch (...) {
                return false;
            }
        } else if (key == "api_port") {
            try {
                config.api_port = std::stoi(value);
                return true;
            } catch (...) {
                return false;
            }
        } else if (key == "api_workers") {
            try {
                config.api_workers = std::stoi(value);
                return true;
            } catch (...) {
                return false;
            }
        }
        return false;
    }
    
    static std::string GetConfigHelp() {
        return R"(
Available configuration keys:
  download_provider  - Set default download provider (huggingface, modelscope, modelers)
  cache_dir         - Set cache directory path
  log_level         - Set log level (debug, info, warn, error)
  api_host          - Set API server host
  api_port          - Set API server port
  default_max_tokens - Set default maximum tokens for generation
  default_temperature - Set default temperature for generation
  api_workers       - Set number of API worker threads

Environment Variables (take precedence over config):
  MNN_DOWNLOAD_PROVIDER - Set default download provider
  MNN_CACHE_DIR        - Set cache directory path
  MNN_API_HOST         - Set API server host
  MNN_API_PORT         - Set API server port

Examples:
  mnncli config set download_provider modelscope
  mnncli config set cache_dir ~/.mnncli/cache
  mnncli config set api_port 8080
  
  # Using environment variables
  export MNN_DOWNLOAD_PROVIDER=modelscope
  export MNN_CACHE_DIR=~/.mnncli/cache
  mnncli config show
)";
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
            // Use SimpleModelSearcher for enhanced search capabilities
            mnncli::SimpleModelSearcher searcher;
            
            // Get preferred source from environment or use default
            std::string preferredSource = "";
            if (const char* source = std::getenv("MNN_DOWNLOAD_SOURCE")) {
                preferredSource = source;
            }
            
            auto models = searcher.SearchModels(keyword, preferredSource);
            
            if (models.empty()) {
                std::cout << "No models found for keyword: " << keyword << "\n";
                return 0;
            }
            
            std::cout << "Found " << models.size() << " models:\n";
            for (auto& model : models) {
                std::cout << "  🔍 " << model.modelName;
                if (!model.vendor.empty()) {
                    std::cout << " (" << model.vendor << ")";
                }
                if (model.sizeB > 0) {
                    std::cout << " [" << model.sizeB << "B]";
                }
                if (!model.currentSource.empty()) {
                    std::cout << " - " << model.currentSource;
                }
                if (model.fileSize > 0) {
                    std::cout << " - " << (model.fileSize / (1024*1024*1024.0)) << "GB";
                }
                std::cout << "\n";
                
                // Show tags if available
                if (!model.tags.empty()) {
                    std::cout << "     Tags: ";
                    for (size_t i = 0; i < std::min(model.tags.size(), size_t(5)); ++i) {
                        if (i > 0) std::cout << ", ";
                        std::cout << model.tags[i];
                    }
                    if (model.tags.size() > 5) std::cout << "...";
                    std::cout << "\n";
                }
            }
            
            std::cout << "\nTo download a model, use: mnncli model download <model_name>\n";
            
        } catch (const std::exception& e) {
            UserInterface::ShowError("Failed to search models: " + std::string(e.what()));
            return 1;
        }
        return 0;
    }
    
    static int DownloadModel(const std::string& model_name, bool verbose = false) {
        if (model_name.empty()) {
            UserInterface::ShowError("Model name is required", "Usage: mnncli model download <name>");
            return 1;
        }
        
        std::cout << "Downloading model: " << model_name << "\n";
        
        // Get current configuration
        auto config = ConfigManager::LoadDefaultConfig();
        
        // Show which download provider will be used
        std::cout << "Using download provider: " << config.download_provider << "\n";
        
        try {
            // Create downloader with appropriate provider
            std::string host;
            std::string provider_name;
            mnncli::DownloadProvider provider_enum;
            
            if (config.download_provider == "modelscope" || config.download_provider == "ms") {
                host = "modelscope.cn";
                provider_name = "ModelScope";
                provider_enum = mnncli::DownloadProvider::MODELSCOPE;
                std::cout << "🌐 Downloading from ModelScope (modelscope.cn)\n";
                std::cout << "   ModelScope is Alibaba's AI model platform\n";
            } else if (config.download_provider == "modelers") {
                host = "modelers.cn";
                provider_name = "Modelers";
                provider_enum = mnncli::DownloadProvider::MODELERS;
                std::cout << "🌐 Downloading from Modelers (modelers.cn)\n";
                std::cout << "   Modelers is a community-driven model platform\n";
            } else {
                host = "hf-mirror.com";
                provider_name = "HuggingFace";
                provider_enum = mnncli::DownloadProvider::HUGGINGFACE;
                std::cout << "🌐 Downloading from HuggingFace (hf-mirror.com)\n";
                std::cout << "   HuggingFace is the leading AI model platform\n";
            }
            
            mnncli::RemoteModelDownloader downloader(host);
            
            // Set the download provider in the downloader
            downloader.SetDownloadProvider(provider_enum);
            
            std::cout << "🔧 Downloader configured for " << provider_name << "\n";
            std::cout << "📡 Target host: " << host << "\n";
            
            std::string error_info;
            std::string repo_name = model_name;
            
            // 首先尝试从 model market 中查找模型信息，获取正确的 repo 路径
            mnncli::SimpleModelSearcher searcher;
            auto search_results = searcher.SearchModels(model_name);
            
            if (!search_results.empty()) {
                // 找到了模型，根据选择的下载源从 sources 中获取正确的 repo 路径
                const auto& model = search_results[0];
                if (!model.sources.empty()) {
                    std::string source_key;
                    switch (provider_enum) {
                        case mnncli::DownloadProvider::HUGGINGFACE:
                            source_key = "HuggingFace";
                            break;
                        case mnncli::DownloadProvider::MODELSCOPE:
                            source_key = "ModelScope";
                            break;
                        case mnncli::DownloadProvider::MODELERS:
                            source_key = "Modelers";
                            break;
                    }
                    
                    auto it = model.sources.find(source_key);
                    if (it != model.sources.end()) {
                        repo_name = it->second;
                        std::cout << "📋 Found model in market: " << model.modelName << "\n";
                        std::cout << "🔗 Using repo path for " << source_key << ": " << repo_name << "\n";
                    }
                }
            } else {
                // 没找到模型，使用默认逻辑
                if (repo_name.find('/') == std::string::npos) {
                    if (provider_enum == mnncli::DownloadProvider::MODELSCOPE || 
                        provider_enum == mnncli::DownloadProvider::MODELERS) {
                        repo_name = "MNN/" + repo_name;
                    } else {
                        repo_name = "taobao-mnn/" + repo_name;
                    }
                }
            }
            
            // Parse owner/repo from the repo_name
            size_t slash_pos = repo_name.find('/');
            if (slash_pos == std::string::npos) {
                UserInterface::ShowError("Invalid model format", "Model name should be in format: owner/repo");
                return 1;
            }
            
            std::string owner = repo_name.substr(0, slash_pos);
            std::string repo = repo_name.substr(slash_pos + 1);
            
            if (verbose) {
                std::cout << "🔍 Parsed model info:\n";
                std::cout << "   Owner: " << owner << "\n";
                std::cout << "   Repository: " << repo << "\n";
                std::cout << "   Full name: " << repo_name << "\n";
            }
            
            // For ModelScope, we need to use the ModelScope API directly
            if (provider_enum == mnncli::DownloadProvider::MODELSCOPE) {
                std::cout << "📡 Fetching repository info from ModelScope API...\n";
                
                if (verbose) {
                    std::cout << "   Owner: " << owner << "\n";
                    std::cout << "   Repository: " << repo << "\n";
                    std::cout << "   Full name: " << repo_name << "\n";
                }
                
                // Use the ModelScope API client
                mnncli::MsApiClient ms_api_client;
                const auto ms_repo_info = ms_api_client.GetRepoInfo(repo_name, "main", error_info);
                if (!error_info.empty()) {
                    UserInterface::ShowError("Failed to get repo info: " + error_info);
                    return 1;
                }
                
                std::cout << "📦 Repository info retrieved successfully from ModelScope\n";
                std::cout << "   Model ID: " << ms_repo_info.model_id << "\n";
                std::cout << "   Revision: " << ms_repo_info.revision << "\n";
                std::cout << "   Files to download: " << ms_repo_info.files.size() << "\n";
                
                if (verbose) {
                    std::cout << "   Files:\n";
                    for (const auto& file : ms_repo_info.files) {
                        std::cout << "     - " << file.path << " (" << file.size << " bytes, SHA256: " << file.sha256.substr(0, 8) << "...)\n";
                    }
                }
                
                // Download the model using ModelScope downloader
                std::cout << "🚀 Starting ModelScope download...\n";
                mnncli::MsModelDownloader ms_downloader(mnncli::FileUtils::GetBaseCacheDir());
                std::string download_error;
                if (!ms_downloader.DownloadModel(repo_name, download_error)) {
                    UserInterface::ShowError("ModelScope download failed: " + download_error);
                    return 1;
                }
                
            } else if (provider_enum == mnncli::DownloadProvider::MODELERS) {
                std::cout << "📡 Fetching repository info from Modelers API...\n";
                
                if (verbose) {
                    std::cout << "   Model Group: " << owner << "\n";
                    std::cout << "   Model Path: " << repo << "\n";
                    std::cout << "   Full name: " << repo_name << "\n";
                }
                
                // Use the Modelers API client
                mnncli::MlApiClient ml_api_client;
                const auto ml_repo_info = ml_api_client.GetRepoInfo(repo_name, "main", error_info);
                if (!error_info.empty()) {
                    UserInterface::ShowError("Failed to get repo info: " + error_info);
                    return 1;
                }
                
                std::cout << "📦 Repository info retrieved successfully from Modelers\n";
                std::cout << "   Model ID: " << ml_repo_info.model_id << "\n";
                std::cout << "   Revision: " << ml_repo_info.revision << "\n";
                std::cout << "   Files to download: " << ml_repo_info.files.size() << "\n";
                
                if (verbose) {
                    std::cout << "   Files:\n";
                    for (const auto& file : ml_repo_info.files) {
                        std::cout << "     - " << file.path << " (" << file.size << " bytes, SHA256: " << file.sha256.substr(0, 8) << "...)\n";
                    }
                }
                
                // Download the model using Modelers downloader
                std::cout << "🚀 Starting Modelers download...\n";
                mnncli::MlModelDownloader ml_downloader(mnncli::FileUtils::GetBaseCacheDir());
                std::string download_error;
                if (!ml_downloader.DownloadModel(repo_name, download_error)) {
                    UserInterface::ShowError("Modelers download failed: " + download_error);
                    return 1;
                }
                
            } else {
                // For HuggingFace and other providers, use the existing HfApiClient
                std::cout << "📡 Fetching repository info from HuggingFace API...\n";
                
                mnncli::HfApiClient api_client;
                const auto repo_info = api_client.GetRepoInfo(repo_name, "main", error_info);
                if (!error_info.empty()) {
                    UserInterface::ShowError("Failed to get repo info: " + error_info);
                    return 1;
                }
                
                std::cout << "📦 Repository info retrieved successfully\n";
                std::cout << "   Model ID: " << repo_info.model_id << "\n";
                std::cout << "   Revision: " << repo_info.revision << "\n";
                std::cout << "   Commit SHA: " << repo_info.sha << "\n";
                std::cout << "   Files to download: " << repo_info.siblings.size() << "\n";
                
                UserInterface::ShowProgress("Downloading model", 0.0f);
                api_client.DownloadRepo(repo_info);
                UserInterface::ShowProgress("Downloading model", 1.0f);
            }
            
            UserInterface::ShowSuccess("Model downloaded successfully: " + model_name);
            std::cout << "✅ Download completed using " << provider_name << " provider\n";
            
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
        
        // Parse global options first
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "-v" || arg == "--verbose") {
                verbose_ = true;
                // Remove the verbose flag from argv by shifting remaining args
                for (int j = i; j < argc - 1; j++) {
                    argv[j] = argv[j + 1];
                }
                argc--;
                break;
            }
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
            return ModelManager::DownloadModel(argv[3], verbose_);
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
            
            std::string key = argv[3];
            std::string value = argv[4];
            
            auto config = ConfigManager::LoadDefaultConfig();
            if (ConfigManager::SetConfigValue(config, key, value)) {
                if (ConfigManager::SaveConfig(config)) {
                    UserInterface::ShowSuccess("Configuration updated and saved: " + key + " = " + value);
                } else {
                    UserInterface::ShowSuccess("Configuration updated: " + key + " = " + value);
                    std::cout << "Warning: Configuration could not be saved to file.\n";
                }
            } else {
                UserInterface::ShowError("Invalid configuration key or value", 
                    "Use 'mnncli config help' to see available options");
                return 1;
            }
        } else if (subcmd == "reset") {
            UserInterface::ShowInfo("Config reset command not implemented yet");
        } else if (subcmd == "help") {
            std::cout << ConfigManager::GetConfigHelp();
        } else {
            UserInterface::ShowError("Unknown config subcommand", "Use: show, set, reset, or help");
            return 1;
        }
        
        return 0;
    }
    
    int HandleInfoCommand(int argc, const char* argv[]) {
        std::cout << "MNN CLI Information:\n";
        std::cout << "====================\n";
        std::cout << "Version: 1.0.0\n";
        
        auto config = ConfigManager::LoadDefaultConfig();
        std::cout << "Cache Directory: " << config.cache_dir << "\n";
        std::cout << "Download Provider: " << config.download_provider << "\n";
        std::cout << "API Server: " << config.api_host << ":" << config.api_port << "\n";
        
        std::cout << "Available Models: ";
        
        std::vector<std::string> model_names;
        if (list_local_models(config.cache_dir, model_names) == 0) {
            std::cout << model_names.size() << "\n";
        } else {
            std::cout << "Unknown\n";
        }
        
        std::cout << "\nEnvironment Variables:\n";
        if (const char* env_provider = std::getenv("MNN_DOWNLOAD_PROVIDER")) {
            std::cout << "  MNN_DOWNLOAD_PROVIDER: " << env_provider << "\n";
        }
        if (const char* env_cache = std::getenv("MNN_CACHE_DIR")) {
            std::cout << "  MNN_CACHE_DIR: " << env_cache << "\n";
        }
        if (const char* env_host = std::getenv("MNN_API_HOST")) {
            std::cout << "  MNN_API_HOST: " << env_host << "\n";
        }
        if (const char* env_port = std::getenv("MNN_API_PORT")) {
            std::cout << "  MNN_API_PORT: " << env_port << "\n";
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
            return ModelManager::DownloadModel(argv[2], verbose_);
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
        std::cout << "  config    Manage configuration (show, set, reset, help)\n";
        std::cout << "  info      Show system information\n";
        std::cout << "\nGlobal Options:\n";
        std::cout << "  -v, --verbose  Enable verbose output for detailed debugging\n";
        std::cout << "  --help    Show this help message\n";
        std::cout << "  --version Show version information\n";
        std::cout << "\nExamples:\n";
        std::cout << "  mnncli model list                    # List local models\n";
        std::cout << "  mnncli model search qwen             # Search for Qwen models\n";
        std::cout << "  mnncli model download qwen-7b        # Download Qwen-7B model\n";
        std::cout << "  mnncli download qwen-7b -v           # Download with verbose output\n";
        std::cout << "  mnncli config set download_provider modelscope  # Set default provider\n";
        std::cout << "  mnncli config show                   # Show current configuration\n";
        std::cout << "  mnncli config help                   # Show configuration help\n";
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
