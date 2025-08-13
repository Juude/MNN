//
// Created by ruoyi.sjd on 2024/12/18.
// Copyright (c) 2024 Alibaba Group Holding Limited All rights reserved.
//
#pragma once
#include <string>
#include <functional>
#include <algorithm>
#include "httplib.h"
#include <filesystem>
#include "hf_api_client.hpp"

namespace fs = std::filesystem;
namespace mnncli {

// Download provider types
enum class DownloadProvider {
    HUGGINGFACE,
    MODELSCOPE,
    MODELERS
};

// Convert provider enum to string
inline std::string ProviderToString(DownloadProvider provider) {
    switch (provider) {
        case DownloadProvider::HUGGINGFACE: return "HuggingFace";
        case DownloadProvider::MODELSCOPE: return "ModelScope";
        case DownloadProvider::MODELERS: return "Modelers";
        default: return "Unknown";
    }
}

// Convert string to provider enum
inline DownloadProvider StringToProvider(const std::string& provider_str) {
    std::string lower = provider_str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "huggingface" || lower == "hf") return DownloadProvider::HUGGINGFACE;
    if (lower == "modelscope" || lower == "ms") return DownloadProvider::MODELSCOPE;
    if (lower == "modelers") return DownloadProvider::MODELERS;
    
    return DownloadProvider::HUGGINGFACE; // default
}

struct DownloadProgress {
    size_t content_length = 0;
    size_t downloaded = 0;
    bool success = false;
    std::string error_message;
};

class RemoteModelDownloader {
public:
    explicit RemoteModelDownloader(std::string host, int max_attempts = 3, int retry_delay_seconds = 2);
    
    // Set the download provider
    void SetDownloadProvider(DownloadProvider provider) { download_provider_ = provider; }
    
    // Get the current download provider
    DownloadProvider GetDownloadProvider() const { return download_provider_; }
    
    // Get provider name as string
    std::string GetProviderName() const { return ProviderToString(download_provider_); }

    std::string DownloadFile(
                      const std::filesystem::path& storage_folder,
                      const std::string& repo,
                      const std::string& revision,
                      const std::string& relative_path,
                      std::string& error_info);
    std::string DownloadWithRetries(
                      const fs::path& storage_folder,
                      const std::string& repo,
                      const std::string& revision,
                      const std::string& relative_path,
                      std::string& error_info,
                      int max_retries);
private:
    void DownloadToTmpAndMove(
        const fs::path& incomplete_path,
        const fs::path& destination_path,
        const std::string& url_to_download,
        httplib::Headers& headers,
        size_t expected_size,
        const std::string& file_name,
        bool force_download,
        std::string& error_info);

    void DownloadFileInner(
        const std::string& url,
        const std::filesystem::path& temp_file,
        const std::unordered_map<std::string, std::string>& proxies,
        size_t resume_size,
        const httplib::Headers& headers,
        const size_t expected_size,
        const std::string& displayed_filename,
        std::string& error_info);

    bool CheckDiskSpace(size_t required_size, const std::filesystem::path& path);

    void MoveWithPermissions(const std::filesystem::path& src, const std::filesystem::path& dest, std::string& error_info);

    HfFileMetadata GetFileMetadata(const std::string& url, std::string& error_info);

private:
    int max_attempts_;
    int retry_delay_seconds_;
    std::string host_;
    DownloadProvider download_provider_{DownloadProvider::HUGGINGFACE};
};

// Model market data structures (matching Kotlin ModelMarketItem)
struct ModelMarketItem {
    std::string modelName;
    std::string vendor;
    double sizeB;  // Model parameters in billions
    std::vector<std::string> tags;
    std::vector<std::string> categories;
    std::map<std::string, std::string> sources;  // source -> repoPath mapping
    std::string description;
    size_t fileSize;
    std::string currentSource;
    std::string currentRepoPath;
    std::string modelId;
};

struct ModelMarketData {
    std::string version;
    std::map<std::string, std::string> tagTranslations;
    std::vector<std::string> quickFilterTags;
    std::vector<std::string> vendorOrder;
    std::vector<ModelMarketItem> models;
    std::vector<ModelMarketItem> ttsModels;
    std::vector<ModelMarketItem> asrModels;
};

// Simple model searcher that can work with existing HfApiClient
class SimpleModelSearcher {
public:
    // Search using existing HfApiClient (keeping original functionality)
    std::vector<RepoItem> SearchModelsFromHf(const std::string& keyword);
    
    // Simple search that returns mock data for demonstration
    std::vector<ModelMarketItem> SearchModelsFromMarket(const std::string& keyword, const std::string& preferredSource = "");
    
    // Combined search that tries market first, then falls back to HF
    std::vector<ModelMarketItem> SearchModels(const std::string& keyword, const std::string& preferredSource = "");

private:
    // Fetch data from the actual model market API
    std::vector<ModelMarketItem> FetchModelMarketData();
    
    // Create mock data for demonstration
    std::vector<ModelMarketItem> CreateMockMarketData();
    
    // Filter models by keyword and source
    std::vector<ModelMarketItem> FilterModels(const std::vector<ModelMarketItem>& models, 
                                             const std::string& keyword, 
                                             const std::string& preferredSource);
    
    // Convert HF search results to ModelMarketItem format
    std::vector<ModelMarketItem> ConvertHfResultsToMarketItems(const std::vector<RepoItem>& hfResults);
};

}