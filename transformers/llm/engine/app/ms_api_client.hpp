#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace mls {

struct MsFileInfo {
    std::string path;
    std::string sha256;
    size_t size{0};
};

struct MsRepoInfo {
    std::string model_id;
    std::vector<MsFileInfo> files;
};

class MsApiClient {
public:
    MsApiClient();

    MsRepoInfo GetRepoInfo(const std::string& repo_name, std::string& error_info);

    void DownloadRepo(const MsRepoInfo& repo_info);

private:
    int max_attempts_{3};
    int retry_delay_seconds_{1};
    std::string host_{"modelscope.cn"};
    std::string cache_path_{};
};

} // namespace mls
