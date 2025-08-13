//
// Created by ruoyi.sjd on 2024/12/18.
// Copyright (c) 2024 Alibaba Group Holding Limited All rights reserved.
//

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include "ms_api_client.hpp"
#include "file_utils.hpp"

namespace mnncli {

// ModelScope model downloader class
class MsModelDownloader {
public:
    explicit MsModelDownloader(const std::string& cache_root_path);
    
    // Download a model repository
    bool DownloadModel(const std::string& model_id, std::string& error_info);
    
    // Get the download path for a model
    std::filesystem::path GetDownloadPath(const std::string& model_id);
    
    // Delete a model repository
    bool DeleteRepo(const std::string& model_id);
    
    // Get repository size
    int64_t GetRepoSize(const std::string& model_id, std::string& error_info);
    
    // Check for updates
    bool CheckUpdate(const std::string& model_id, std::string& error_info);

private:
    // Download the ModelScope repository
    bool DownloadMsRepo(const std::string& model_id, std::string& error_info);
    
    // Inner download implementation
    bool DownloadMsRepoInner(const std::string& model_id, const std::string& model_scope_id, 
                            const MsRepoInfo& ms_repo_info, std::string& error_info);
    
    // Collect download tasks for ModelScope files
    std::vector<std::pair<std::string, std::filesystem::path>> CollectMsTaskList(
        const std::string& model_scope_id,
        const std::filesystem::path& storage_folder,
        const std::filesystem::path& parent_pointer_path,
        const MsRepoInfo& ms_repo_info,
        int64_t& total_size,
        int64_t& downloaded_size);
    
    // Download a single file
    bool DownloadFile(const std::string& url, const std::filesystem::path& destination_path, 
                     int64_t expected_size, const std::string& file_name, std::string& error_info);
    
    // Get cache path root for ModelScope
    static std::string GetCachePathRoot(const std::string& model_download_path_root);
    
    // Get model path
    static std::filesystem::path GetModelPath(const std::string& models_download_path_root, const std::string& model_id);

private:
    std::string cache_root_path_;
    std::unique_ptr<MsApiClient> ms_api_client_;
    std::vector<std::string> paused_models_;
};

} // namespace mnncli
