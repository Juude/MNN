#pragma once
#include <string>
#include <filesystem>
#include "httplib.h"
#include "ms_api_client.hpp"
#include "hf_api_client.hpp"

namespace fs = std::filesystem;

namespace mls {

class MsRemoteModelDownloader {
public:
    explicit MsRemoteModelDownloader(std::string host, int max_attempts = 3, int retry_delay_seconds = 2);

    std::string DownloadFile(const fs::path& storage_folder,
                             const std::string& repo,
                             const MsFileInfo& file_info,
                             std::string& error_info);

    std::string DownloadWithRetries(const fs::path& storage_folder,
                                    const std::string& repo,
                                    const MsFileInfo& file_info,
                                    std::string& error_info,
                                    int max_retries);
private:
    void DownloadToTmpAndMove(const fs::path& incomplete_path,
                              const fs::path& destination_path,
                              const std::string& url_to_download,
                              httplib::Headers& headers,
                              size_t expected_size,
                              const std::string& file_name,
                              bool force_download,
                              std::string& error_info);

    void DownloadFileInner(const std::string& url,
                           const fs::path& temp_file,
                           const std::unordered_map<std::string, std::string>& proxies,
                           size_t resume_size,
                           const httplib::Headers& headers,
                           size_t expected_size,
                           const std::string& displayed_filename,
                           std::string& error_info);

    bool CheckDiskSpace(size_t required_size, const fs::path& path);
    void MoveWithPermissions(const fs::path& src, const fs::path& dest, std::string& error_info);

    int max_attempts_;
    int retry_delay_seconds_;
    std::string host_;
};

} // namespace mls
