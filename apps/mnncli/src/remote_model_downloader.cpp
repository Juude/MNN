//
// Created by ruoyi.sjd on 2024/12/18.
// Copyright (c) 2024 Alibaba Group Holding Limited All rights reserved.
//

#include "remote_model_downloader.hpp"
#include "httplib.h"
#include <cstdio>
#include <fstream>
#include <functional>
#include "file_utils.hpp"
#include <rapidjson/document.h>

static const std::string HUGGINGFACE_HEADER_X_REPO_COMMIT = "x-repo-commit";
static const std::string HUGGINGFACE_HEADER_X_LINKED_ETAG = "x-linked-etag";
static const std::string HUGGINGFACE_HEADER_X_LINKED_SIZE = "x-linked-size";

namespace mnncli {

    size_t ParseContentLength(const std::string& content_length) {
        if (!content_length.empty()) {
            return std::stoul(content_length);
        }
        return 0;
    }

    std::string NormalizeETag(const std::string& etag) {
        if (!etag.empty() && etag[0] == '"' && etag[etag.size() - 1] == '"') {
            return etag.substr(1, etag.size() - 2);
        }
        return etag;
    }

    //from get_hf_file_metadata
    HfFileMetadata RemoteModelDownloader::GetFileMetadata(const std::string& url, std::string& error_info) {
        httplib::SSLClient cli(this->host_, 443);
        httplib::Headers headers;
        HfFileMetadata metadata{};
        headers.emplace("Accept-Encoding", "identity");
        auto res = cli.Head(url, headers);
        if (!res || (res->status != 200 && res->status != 302)) {
            error_info = "GetFileMetadata Failed to fetch metadata status " + std::to_string(res ? res->status : -1);
            return metadata;
        }
        metadata.location = url;
        if (res->status == 302) {
            metadata.location = res->get_header_value("Location");
        }
        std::string linked_etag = res->get_header_value(HUGGINGFACE_HEADER_X_LINKED_ETAG);
        std::string etag = res->get_header_value("ETag");
        metadata.etag = NormalizeETag(!linked_etag.empty() ? linked_etag : etag);
        std::string linked_size = res->get_header_value(HUGGINGFACE_HEADER_X_LINKED_SIZE);
        std::string content_length = res->get_header_value("Content-Length");
        metadata.size = ParseContentLength(!linked_size.empty() ? linked_size : content_length);
        metadata.commit_hash = res->get_header_value(HUGGINGFACE_HEADER_X_REPO_COMMIT);
        return metadata;
    }

    RemoteModelDownloader::RemoteModelDownloader(std::string host, int max_attempts, int retry_delay_seconds)
        : max_attempts_(max_attempts),
          retry_delay_seconds_(retry_delay_seconds),
        host_(std::move(host)){
    }

    std::string RemoteModelDownloader::DownloadWithRetries(
                      const fs::path& storage_folder,
                      const std::string& repo,
                      const std::string& revision,
                      const std::string& relative_path,
                      std::string& error_info,
                      int max_retries) {
        printf("Starting download from %s provider (max retries: %d)\n", GetProviderName().c_str(), max_retries);
        
        int attempt = 0;
        bool success = false;
        while (attempt < max_retries) {
            // Reset error message for each attempt
            error_info.clear();
            auto result = this->DownloadFile(storage_folder, repo, revision, relative_path, error_info);
            if (error_info.empty()) {
                success = true;
                printf("Download completed successfully from %s provider\n", GetProviderName().c_str());
                return result;
            } else {
                attempt++;
                fprintf(stderr, "DownloadFile error at file: %s error message: %s, attempt: %d/%d\n", 
                        relative_path.c_str(), error_info.c_str(), attempt, max_retries);
                if (attempt < max_retries) {
                    printf("Retrying in 1 second...\n");
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }
        }
        
        fprintf(stderr, "Download failed after %d attempts from %s provider\n", max_retries, GetProviderName().c_str());
        return "";
    }

    std::string RemoteModelDownloader::DownloadFile(
                                    const fs::path& storage_folder,
                                    const std::string& repo,
                                    const std::string& revision,
                                    const std::string& relative_path,
                                    std::string& error_info) {
        // Show which download provider is being used
        printf("Downloading from %s provider\n", GetProviderName().c_str());
        
        std::string url = "https://" + this->host_ + "/";
        url += repo;
        url += "/resolve/" + revision + "/" + relative_path;
        
        printf("Download URL: %s\n", url.c_str());
        
        auto metadata = GetFileMetadata(url, error_info);
        if (!error_info.empty()) {
            printf("DownloadFile GetFileMetadata failed: %s\n", error_info.c_str());
            return "";
        }
        
        auto repo_folder_name = mnncli::FileUtils::RepoFolderName(repo, "model");
        fs::path blob_path = storage_folder / "blobs" / metadata.etag;
        std::filesystem::path blob_path_incomplete = storage_folder / "blobs" / (metadata.etag + ".incomplete");
        fs::path pointer_path = mnncli::FileUtils::GetPointerPath(
            storage_folder, metadata.commit_hash, relative_path);
        fs::create_directories(blob_path.parent_path());
        fs::create_directories(pointer_path.parent_path());

        if (fs::exists(pointer_path)) {
            printf("File %s already exists, skipping download\n", relative_path.c_str());
            return pointer_path.string();
        } else if (fs::exists(blob_path)) {
            std::error_code ec;
            mnncli::FileUtils::CreateSymlink(blob_path, pointer_path, ec);
            if (ec) {
                fprintf(stderr, "DownloadFile create symlink error for pointer_path: %s\n", pointer_path.string().c_str());
                error_info = ec.message();
                return "";
            }
            printf("DownloadFile %s already exists just create symlink\n", relative_path.c_str());
            return pointer_path.string();
        }

        std::mutex lock;
        {
            std::lock_guard<std::mutex> guard(lock);
            httplib::Headers headers;
            DownloadToTmpAndMove(blob_path_incomplete, blob_path, metadata.location, headers, metadata.size,
                                 relative_path, false, error_info);
            if (error_info.empty()) {
                std::error_code ec;
                mnncli::FileUtils::CreateSymlink(blob_path, pointer_path, ec);
                if (ec) {
                    error_info = "create link error: " + ec.message();
                }
            }
        }
        return pointer_path.string();
    }

    void RemoteModelDownloader::DownloadToTmpAndMove(
        const fs::path& incomplete_path,
        const fs::path& destination_path,
        const std::string& url_to_download,
        httplib::Headers& headers,
        size_t expected_size,
        const std::string& file_name,
        bool force_download,
        std::string& error_info) {
        if (fs::exists(destination_path) && !force_download) {
            return;
        }
        if (std::filesystem::exists(incomplete_path) && force_download) {
            std::filesystem::remove(incomplete_path);
        }
        size_t resume_size = std::filesystem::exists(incomplete_path) ? std::filesystem::file_size(incomplete_path) : 0;
        DownloadFileInner(url_to_download, incomplete_path, {}, resume_size, headers, expected_size, file_name, error_info);
        if (error_info.empty()) {
            printf("DownloadFile  %s success\n", file_name.c_str());
            MoveWithPermissions(incomplete_path, destination_path, error_info);
        } else {
            printf("DownloadFile  %s failed\n", file_name.c_str());
        }
    }

    void RemoteModelDownloader::DownloadFileInner(
        const std::string& url,
        const std::filesystem::path& temp_file,
        const std::unordered_map<std::string, std::string>& proxies,
        size_t resume_size,
        const httplib::Headers& headers,
        const size_t expected_size,
        const std::string& displayed_filename,
        std::string& error_info
    ) {
        auto [host, path] = HfApiClient::ParseUrl(url);
        httplib::SSLClient client(host, 443);
        httplib::Headers request_headers(headers.begin(), headers.end());
        if (resume_size > 0) {
            printf("DownloadFile %s resume size %zu", displayed_filename.c_str(), resume_size);
            request_headers.emplace("Range", "bytes=" + std::to_string(resume_size) + "-");
        }
        std::ofstream output(temp_file, std::ios::binary | std::ios::app);
        DownloadProgress progress;
        progress.downloaded = resume_size;
        auto res = client.Get(path, request_headers,
              [&](const httplib::Response& response) {
                  auto content_length_str = response.get_header_value("Content-Length");
                  if (!content_length_str.empty()) {
                      progress.content_length = std::stoull(content_length_str) + resume_size;
                  }
                  return true;
              },
              [&](const char* data, size_t data_length) {
                  output.write(data, data_length);
                  progress.downloaded += data_length;
                  if (expected_size > 0) {
                      double percentage = (static_cast<double>(progress.downloaded) / progress.content_length) * 100.0;
                      printf("\rDownloadFile %s progress: %.2f%%", displayed_filename.c_str(), percentage);
                      fflush(stdout);
                  }
                  return true;
              }
        );
        output.flush();
        output.close();
        if (res) {
            if (res->status >= 200 && res->status < 300 || res->status == 416) {
                progress.success = true;
                printf("\n");
            } else {
                error_info = "HTTP error: " + std::to_string(res->status);
            }
        } else {
            error_info  = "Connection error: " + std::string(httplib::to_string(res.error()));
        }
        if (!error_info.empty()) {
            printf("HTTP Get Error: %s\n", error_info.c_str());
        }
    }

    bool RemoteModelDownloader::CheckDiskSpace(size_t required_size, const std::filesystem::path& path) {
        auto space = std::filesystem::space(path);
        if (space.available < required_size) {
            return false;
        }
        return true;
    }

    void RemoteModelDownloader::MoveWithPermissions(const std::filesystem::path& src,
                                                    const std::filesystem::path& dest,
                                                    std::string& error_info) {
        if (FileUtils::Move(src, dest, error_info)) {
            std::filesystem::permissions(dest, std::filesystem::perms::owner_all);
        }
    }

}

// SimpleModelSearcher implementation
namespace mnncli {

std::vector<RepoItem> SimpleModelSearcher::SearchModelsFromHf(const std::string& keyword) {
    try {
        HfApiClient hfClient;
        return hfClient.SearchRepos(keyword);
    } catch (const std::exception& e) {
        printf("Failed to search models from HuggingFace: %s\n", e.what());
        return {};
    }
}

std::vector<ModelMarketItem> SimpleModelSearcher::SearchModelsFromMarket(const std::string& keyword, const std::string& preferredSource) {
    // Fetch data from the actual model market API
    auto marketData = FetchModelMarketData();
    if (marketData.empty()) {
        printf("Failed to fetch model market data, falling back to mock data\n");
        return CreateMockMarketData();
    }
    
    // Filter the models based on keyword and preferred source
    return FilterModels(marketData, keyword, preferredSource);
}

std::vector<ModelMarketItem> SimpleModelSearcher::SearchModels(const std::string& keyword, const std::string& preferredSource) {
    // Try market first
    auto marketResults = SearchModelsFromMarket(keyword, preferredSource);
    if (!marketResults.empty()) {
        return marketResults;
    }
    
    // Fallback to HuggingFace
    printf("Market search returned no results, trying HuggingFace...\n");
    auto hfResults = SearchModelsFromHf(keyword);
    return ConvertHfResultsToMarketItems(hfResults);
}

std::vector<ModelMarketItem> SimpleModelSearcher::CreateMockMarketData() {
    std::vector<ModelMarketItem> mockModels;
    
    // Create some mock models for demonstration
    ModelMarketItem model1;
    model1.modelName = "Qwen-1.8B-Chat";
    model1.vendor = "MNN";
    model1.sizeB = 1.8;
    model1.tags = {"chat", "qwen", "1.8b", "int4"};
    model1.categories = {"LLM"};
    model1.sources = {{"ModelScope", "MNN/Qwen-1.8B-Chat-Int4"}, {"HuggingFace", "MNN/Qwen-1.8B-Chat-Int4"}};
    model1.description = "Qwen 1.8B Chat model optimized by MNN";
    model1.fileSize = 1024 * 1024 * 1024; // 1GB
    model1.currentSource = "ModelScope";
    model1.currentRepoPath = "MNN/Qwen-1.8B-Chat-Int4";
    model1.modelId = "ModelScope/MNN/Qwen-1.8B-Chat-Int4";
    mockModels.push_back(model1);
    
    ModelMarketItem model2;
    model2.modelName = "Qwen-7B-Chat";
    model2.vendor = "MNN";
    model2.sizeB = 7.0;
    model2.tags = {"chat", "qwen", "7b", "int4"};
    model2.categories = {"LLM"};
    model2.sources = {{"ModelScope", "MNN/Qwen-7B-Chat-Int4"}, {"HuggingFace", "MNN/Qwen-7B-Chat-Int4"}};
    model2.description = "Qwen 7B Chat model optimized by MNN";
    model2.fileSize = 2LL * 1024 * 1024 * 1024; // 2GB
    model2.currentSource = "ModelScope";
    model2.currentRepoPath = "MNN/Qwen-7B-Chat-Int4";
    model2.modelId = "ModelScope/MNN/Qwen-7B-Chat-Int4";
    mockModels.push_back(model2);
    
    return mockModels;
}

std::vector<ModelMarketItem> SimpleModelSearcher::FilterModels(const std::vector<ModelMarketItem>& models, 
                                                              const std::string& keyword, 
                                                              const std::string& preferredSource) {
    std::vector<ModelMarketItem> filtered;
    std::string lowerKeyword = keyword;
    std::transform(lowerKeyword.begin(), lowerKeyword.end(), lowerKeyword.begin(), ::tolower);
    
    for (const auto& model : models) {
        bool matchesKeyword = false;
        bool matchesSource = preferredSource.empty();
        
        // Check if model matches keyword
        std::string lowerModelName = model.modelName;
        std::transform(lowerModelName.begin(), lowerModelName.end(), lowerModelName.begin(), ::tolower);
        
        if (lowerModelName.find(lowerKeyword) != std::string::npos) {
            matchesKeyword = true;
        } else {
            // Check tags
            for (const auto& tag : model.tags) {
                std::string lowerTag = tag;
                std::transform(lowerTag.begin(), lowerTag.end(), lowerTag.begin(), ::tolower);
                if (lowerTag.find(lowerKeyword) != std::string::npos) {
                    matchesKeyword = true;
                    break;
                }
            }
        }
        
        // Check if model matches preferred source
        if (!preferredSource.empty()) {
            matchesSource = model.sources.find(preferredSource) != model.sources.end();
        }
        
        if (matchesKeyword && matchesSource) {
            filtered.push_back(model);
        }
    }
    
    return filtered;
}

std::vector<ModelMarketItem> SimpleModelSearcher::ConvertHfResultsToMarketItems(const std::vector<RepoItem>& hfResults) {
    std::vector<ModelMarketItem> marketItems;
    
    for (const auto& hfItem : hfResults) {
        ModelMarketItem marketItem;
        marketItem.modelName = hfItem.model_id;
        marketItem.vendor = "HuggingFace";
        marketItem.sizeB = 0.0; // Unknown from HF search
        marketItem.tags = hfItem.tags;
        marketItem.categories = {};
        marketItem.sources = {{"HuggingFace", hfItem.model_id}};
        marketItem.description = "";
        marketItem.fileSize = 0;
        marketItem.currentSource = "HuggingFace";
        marketItem.currentRepoPath = hfItem.model_id;
        marketItem.modelId = "HuggingFace/" + hfItem.model_id;
        
        marketItems.push_back(marketItem);
    }
    
    return marketItems;
}

std::vector<ModelMarketItem> SimpleModelSearcher::FetchModelMarketData() {
    std::vector<ModelMarketItem> models;
    
    // Use the same URL as ModelRepository.kt
    const std::string MARKET_API_URL = "https://meta.alicdn.com/data/mnn/apis/model_market.json";
    
    // Parse the URL to get host and path
    auto [host, path] = HfApiClient::ParseUrl(MARKET_API_URL);
    if (host.empty() || path.empty()) {
        printf("Failed to parse market API URL: %s\n", MARKET_API_URL.c_str());
        return {};
    }
    
    httplib::SSLClient cli(host, 443);
    httplib::Headers headers;
    
    auto res = cli.Get(path, headers);
    if (!res) {
        printf("No response received from model market API\n");
        return {};
    }
    
    if (res->status != 200) {
        printf("Failed to fetch model market data. HTTP Status: %d\n", res->status);
        return {};
    }
    
    // Parse the JSON response
    rapidjson::Document doc;
    if (doc.Parse(res->body.c_str()).HasParseError()) {
        printf("Failed to parse model market JSON response\n");
        return {};
    }
    
    // Check if the response has the expected structure
    if (!doc.HasMember("models") || !doc["models"].IsArray()) {
        printf("Unexpected JSON format: 'models' array not found\n");
        return {};
    }
    
    // Parse models array
    const auto& modelsArray = doc["models"].GetArray();
    for (const auto& modelItem : modelsArray) {
        if (!modelItem.IsObject()) continue;
        
        ModelMarketItem model;
        
        // Extract model name
        if (modelItem.HasMember("modelName") && modelItem["modelName"].IsString()) {
            model.modelName = modelItem["modelName"].GetString();
        }
        
        // Extract vendor
        if (modelItem.HasMember("vendor") && modelItem["vendor"].IsString()) {
            model.vendor = modelItem["vendor"].GetString();
        }
        
        // Extract size in GB
        if (modelItem.HasMember("size_gb") && modelItem["size_gb"].IsNumber()) {
            if (modelItem["size_gb"].IsInt()) {
                model.sizeB = static_cast<double>(modelItem["size_gb"].GetInt());
            } else if (modelItem["size_gb"].IsDouble()) {
                model.sizeB = modelItem["size_gb"].GetDouble();
            }
        }
        
        // Extract tags
        if (modelItem.HasMember("tags") && modelItem["tags"].IsArray()) {
            const auto& tagsArray = modelItem["tags"].GetArray();
            for (const auto& tag : tagsArray) {
                if (tag.IsString()) {
                    model.tags.emplace_back(tag.GetString());
                }
            }
        }
        
        // Extract categories
        if (modelItem.HasMember("categories") && modelItem["categories"].IsArray()) {
            const auto& categoriesArray = modelItem["categories"].GetArray();
            for (const auto& category : categoriesArray) {
                if (category.IsString()) {
                    model.categories.emplace_back(category.GetString());
                }
            }
        }
        
        // Extract sources
        if (modelItem.HasMember("sources") && modelItem["sources"].IsObject()) {
            const auto& sourcesObj = modelItem["sources"].GetObject();
            for (const auto& source : sourcesObj) {
                if (source.name.IsString() && source.value.IsString()) {
                    model.sources[source.name.GetString()] = source.value.GetString();
                }
            }
        }
        
        // Extract file size
        if (modelItem.HasMember("file_size") && modelItem["file_size"].IsNumber()) {
            if (modelItem["file_size"].IsInt64()) {
                model.fileSize = static_cast<size_t>(modelItem["file_size"].GetInt64());
            } else if (modelItem["file_size"].IsUint64()) {
                model.fileSize = static_cast<size_t>(modelItem["file_size"].GetUint64());
            }
        }
        
        // Set default values for fields not in the API response
        model.description = "Model from MNN Model Market";
        model.currentSource = "ModelScope"; // Default source
        if (!model.sources.empty()) {
            model.currentRepoPath = model.sources.begin()->second;
            model.modelId = model.sources.begin()->first + "/" + model.sources.begin()->second;
        }
        
        models.push_back(model);
    }
    
    printf("Successfully fetched %zu models from model market API\n", models.size());
    return models;
}

}
