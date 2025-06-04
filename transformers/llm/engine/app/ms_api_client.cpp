#include "ms_api_client.hpp"
#include "ms_remote_model_downloader.hpp"
#include "file_utils.hpp"
#include "httplib.h"
#include <rapidjson/document.h>
#include <string>
#include <cmath>
#include <thread>

namespace mls {

MsApiClient::MsApiClient() {
    cache_path_ = FileUtils::GetBaseCacheDir();
}

MsRepoInfo MsApiClient::GetRepoInfo(const std::string& repo_name, std::string& error_info) {
    auto pos = repo_name.find('/');
    if (pos == std::string::npos) {
        error_info = "invalid repo name";
        return {};
    }
    std::string group = repo_name.substr(0, pos);
    std::string name = repo_name.substr(pos + 1);
    std::string path = "/api/v1/models/" + group + "/" + name + "/repo/files";
    httplib::SSLClient cli(this->host_, 443);
    httplib::Headers headers;
    auto res = cli.Get(path, headers);
    if (!res || res->status != 200) {
        error_info = "failed to fetch repo info";
        return {};
    }
    rapidjson::Document doc;
    if (doc.Parse(res->body.c_str()).HasParseError()) {
        error_info = "parse json failed";
        return {};
    }
    MsRepoInfo info;
    info.model_id = repo_name;
    if (doc.HasMember("Data") && doc["Data"].IsObject()) {
        const auto& data = doc["Data"];
        if (data.HasMember("Files") && data["Files"].IsArray()) {
            for (auto& v : data["Files"].GetArray()) {
                if (!v.IsObject()) continue;
                MsFileInfo f;
                if (v.HasMember("Path") && v["Path"].IsString()) f.path = v["Path"].GetString();
                if (v.HasMember("Sha256") && v["Sha256"].IsString()) f.sha256 = v["Sha256"].GetString();
                if (v.HasMember("Size") && v["Size"].IsNumber()) f.size = v["Size"].GetUint64();
                info.files.emplace_back(std::move(f));
            }
        }
    }
    return info;
}

void MsApiClient::DownloadRepo(const MsRepoInfo& repo_info) {
    MsRemoteModelDownloader downloader{this->host_, this->max_attempts_, this->retry_delay_seconds_};
    std::string error_info;
    bool has_error = false;
    auto repo_folder_name = FileUtils::RepoFolderName(repo_info.model_id, "model");
    namespace fs = std::filesystem;
    fs::path storage_folder = fs::path(this->cache_path_) / repo_folder_name;
    const auto parent_pointer_path = FileUtils::GetPointerPathParent(storage_folder, "_no_sha_");
    const auto folder_link_path = fs::path(this->cache_path_) / FileUtils::GetFileName(repo_info.model_id);
    std::error_code ec;
    bool downloaded = is_symlink(folder_link_path, ec);
    if (downloaded) {
        printf("already donwnloaded at %s\n", folder_link_path.string().c_str());
        return;
    }
    for (const auto& file : repo_info.files) {
        downloader.DownloadWithRetries(storage_folder, repo_info.model_id, file, error_info, 3);
        has_error = has_error || !error_info.empty();
        if (has_error) {
            fprintf(stderr, "DownloadFile error at file: %s error message: %s\n", file.path.c_str(), error_info.c_str());
            break;
        }
    }
    if (!has_error) {
        std::error_code ec2;
        FileUtils::CreateSymlink(parent_pointer_path, folder_link_path, ec2);
        if (ec2) {
            fprintf(stderr, "DownlodRepo CreateSymlink error: %s", ec2.message().c_str());
        }
    }
}

} // namespace mls
