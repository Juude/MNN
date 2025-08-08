#include "ms_remote_model_downloader.hpp"
#include "file_utils.hpp"
#include <fstream>
#include <thread>

namespace mls {

MsRemoteModelDownloader::MsRemoteModelDownloader(std::string host, int max_attempts, int retry_delay_seconds)
    : max_attempts_(max_attempts), retry_delay_seconds_(retry_delay_seconds), host_(std::move(host)) {}

std::string MsRemoteModelDownloader::DownloadWithRetries(const fs::path& storage_folder,
                                                         const std::string& repo,
                                                         const MsFileInfo& file_info,
                                                         std::string& error_info,
                                                         int max_retries) {
    int attempt = 0;
    while (attempt < max_retries) {
        error_info.clear();
        auto result = DownloadFile(storage_folder, repo, file_info, error_info);
        if (error_info.empty()) {
            return result;
        }
        attempt++;
        fprintf(stderr, "DownloadFile error at file: %s error message: %s, attempt: %d\n",
                file_info.path.c_str(), error_info.c_str(), attempt);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return "";
}

std::string MsRemoteModelDownloader::DownloadFile(const fs::path& storage_folder,
                                                  const std::string& repo,
                                                  const MsFileInfo& file_info,
                                                  std::string& error_info) {
    std::string url = "https://" + this->host_ + "/api/v1/models/" + repo + "/repo?FilePath=" + file_info.path;
    fs::path blob_path = storage_folder / "blobs" / file_info.sha256;
    fs::path blob_path_incomplete = storage_folder / "blobs" / (file_info.sha256 + ".incomplete");
    fs::path pointer_path = FileUtils::GetPointerPath(storage_folder, "_no_sha_", file_info.path);
    fs::create_directories(blob_path.parent_path());
    fs::create_directories(pointer_path.parent_path());

    if (fs::exists(pointer_path)) {
        return pointer_path.string();
    } else if (fs::exists(blob_path)) {
        std::error_code ec;
        FileUtils::CreateSymlink(blob_path, pointer_path, ec);
        if (ec) {
            error_info = ec.message();
            return "";
        }
        printf("DownloadFile %s already exists just create symlink\n", file_info.path.c_str());
        return pointer_path.string();
    }

    std::mutex lock;
    {
        std::lock_guard guard(lock);
        httplib::Headers headers;
        DownloadToTmpAndMove(blob_path_incomplete, blob_path, url, headers, file_info.size, file_info.path, false, error_info);
        if (error_info.empty()) {
            std::error_code ec;
            FileUtils::CreateSymlink(blob_path, pointer_path, ec);
            if (ec) {
                error_info = "create link error" + ec.message();
            }
        }
    }
    return pointer_path.string();
}

void MsRemoteModelDownloader::DownloadToTmpAndMove(const fs::path& incomplete_path,
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
    if (fs::exists(incomplete_path) && force_download) {
        fs::remove(incomplete_path);
    }
    size_t resume_size = fs::exists(incomplete_path) ? fs::file_size(incomplete_path) : 0;
    DownloadFileInner(url_to_download, incomplete_path, {}, resume_size, headers, expected_size, file_name, error_info);
    if (error_info.empty()) {
        printf("DownloadFile %s success\n", file_name.c_str());
        MoveWithPermissions(incomplete_path, destination_path, error_info);
    } else {
        printf("DownloadFile %s failed\n", file_name.c_str());
    }
}

void MsRemoteModelDownloader::DownloadFileInner(const std::string& url,
                                                const fs::path& temp_file,
                                                const std::unordered_map<std::string, std::string>& proxies,
                                                size_t resume_size,
                                                const httplib::Headers& headers,
                                                size_t expected_size,
                                                const std::string& displayed_filename,
                                                std::string& error_info) {
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
        if ((res->status >= 200 && res->status < 300) || res->status == 416) {
            progress.success = true;
            printf("\n");
        } else {
            error_info = "HTTP error: " + std::to_string(res->status);
        }
    } else {
        error_info = "Connection error: " + std::string(httplib::to_string(res.error()));
    }
    if (!error_info.empty()) {
        printf("HTTP Get Error: %s\n", error_info.c_str());
    }
}

bool MsRemoteModelDownloader::CheckDiskSpace(size_t required_size, const fs::path& path) {
    auto space = fs::space(path);
    return space.available >= required_size;
}

void MsRemoteModelDownloader::MoveWithPermissions(const fs::path& src, const fs::path& dest, std::string& error_info) {
    if (FileUtils::Move(src, dest, error_info)) {
        fs::permissions(dest, fs::perms::owner_all);
    }
}

} // namespace mls
