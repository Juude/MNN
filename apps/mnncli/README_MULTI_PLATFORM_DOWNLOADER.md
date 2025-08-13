# Multi-Platform Model Downloader

The `RemoteModelDownloader` class has been optimized to support multiple model sources, incorporating the logic from the Android Kotlin downloaders (`HfModelDownloader`, `MLModelDownloader`, and `MsModelDownloader`).

## Features

- **Multi-Platform Support**: HuggingFace, ModelScope, and Modelers
- **Unified Interface**: Single API for all platforms
- **Auto-Detection**: Automatically detects platform from model ID
- **Resume Downloads**: Supports resuming interrupted downloads
- **Progress Tracking**: Real-time download progress with percentage
- **Error Handling**: Comprehensive error handling and retry logic
- **Symlink Management**: Efficient storage with blob deduplication

## Supported Platforms

### 1. HuggingFace (hf)
- **Model ID Format**: `HuggingFace/owner/repo` or `hf/owner/repo`
- **API Endpoint**: `https://hf-mirror.com`
- **Features**: Full metadata support, commit hashes, file sizes

### 2. ModelScope (ms)
- **Model ID Format**: `ModelScope/owner/repo` or `ms/owner/repo`
- **API Endpoint**: `https://modelscope.cn`
- **Features**: Model repository browsing, file metadata

### 3. Modelers (ml)
- **Model ID Format**: `Modelers/owner/repo` or `ml/owner/repo`
- **API Endpoint**: `https://modelers.cn`
- **Features**: Code repository integration, media file support

## Architecture

### Core Classes

#### `IPlatformApiClient` (Interface)
Abstract base class for platform-specific implementations:
```cpp
class IPlatformApiClient {
public:
    virtual UnifiedRepoInfo GetRepoInfo(const std::string& model_id, bool calculate_size, std::string& error_info) = 0;
    virtual std::string GetDownloadUrl(const std::string& model_id, const std::string& file_path) = 0;
    virtual std::string GetPlatformName() const = 0;
};
```

#### `RemoteModelDownloader` (Main Class)
Unified downloader that manages all platforms:
```cpp
class RemoteModelDownloader {
public:
    // Unified download method
    std::string DownloadModel(const std::string& model_id, const fs::path& storage_folder, std::string& error_info);
    
    // Platform-specific methods
    std::string DownloadHfModel(const std::string& model_id, const fs::path& storage_folder, std::string& error_info);
    std::string DownloadMsModel(const std::string& model_id, const fs::path& storage_folder, std::string& error_info);
    std::string DownloadMlModel(const std::string& model_id, const fs::path& storage_folder, std::string& error_info);
    
    // Utility methods
    UnifiedRepoInfo GetRepoInfo(const std::string& model_id, bool calculate_size, std::string& error_info);
    size_t GetRepoSize(const std::string& model_id, std::string& error_info);
    bool CheckUpdate(const std::string& model_id, std::string& error_info);
    bool DeleteModel(const std::string& model_id, std::string& error_info);
};
```

### Data Structures

#### `UnifiedFileMetadata`
```cpp
struct UnifiedFileMetadata {
    std::string location;      // Download URL
    std::string etag;          // File identifier
    size_t size;               // File size in bytes
    std::string commit_hash;   // Git commit hash
    std::string relative_path; // Relative file path
    std::string platform;      // Platform identifier
};
```

#### `UnifiedRepoInfo`
```cpp
struct UnifiedRepoInfo {
    std::string model_id;                    // Model identifier
    std::string platform;                    // Platform name
    std::string sha;                         // Repository SHA
    std::string last_modified;               // Last modification time
    size_t total_size;                       // Total repository size
    std::vector<UnifiedFileMetadata> files; // List of files
};
```

## Usage Examples

### Basic Usage

```cpp
#include "remote_model_downloader.hpp"

// Create downloader instance
mnncli::RemoteModelDownloader downloader(3, 2); // 3 max attempts, 2 second delay

// Set storage folder
fs::path storage_folder = fs::current_path() / "models";

// Download model (auto-detects platform)
std::string error_info;
auto result = downloader.DownloadModel("bert-base-uncased", storage_folder, error_info);
if (error_info.empty()) {
    std::cout << "Download successful: " << result << std::endl;
} else {
    std::cout << "Download failed: " << error_info << std::endl;
}
```

### Platform-Specific Downloads

```cpp
// HuggingFace model
auto hf_result = downloader.DownloadHfModel("HuggingFace/bert-base-uncased", storage_folder, error_info);

// ModelScope model
auto ms_result = downloader.DownloadMsModel("ModelScope/damo/speech_model", storage_folder, error_info);

// Modelers model
auto ml_result = downloader.DownloadMlModel("Modelers/owner/repo", storage_folder, error_info);
```

### Getting Repository Information

```cpp
// Get repo info with size calculation
auto repo_info = downloader.GetRepoInfo("bert-base-uncased", true, error_info);
if (error_info.empty()) {
    std::cout << "Platform: " << repo_info.platform << std::endl;
    std::cout << "Total size: " << repo_info.total_size << " bytes" << std::endl;
    std::cout << "Files count: " << repo_info.files.size() << std::endl;
}

// Get repo size only
auto repo_size = downloader.GetRepoSize("bert-base-uncased", error_info);
```

### Update Checking

```cpp
// Check for updates
bool has_update = downloader.CheckUpdate("bert-base-uncased", error_info);
if (error_info.empty()) {
    std::cout << "Has update: " << (has_update ? "Yes" : "No") << std::endl;
}
```

### Model Management

```cpp
// Delete downloaded model
bool deleted = downloader.DeleteModel("bert-base-uncased", error_info);
if (deleted) {
    std::cout << "Model deleted successfully" << std::endl;
}
```

## Storage Structure

The downloader creates an efficient storage structure:

```
models/
├── bert-base-uncased -> models/bert-base-uncased_model/pointers/_sha_/
├── bert-base-uncased_model/
│   ├── blobs/
│   │   ├── abc123... (etag-based blob files)
│   │   └── def456... (etag-based blob files)
│   └── pointers/
│       └── _sha_/
│           ├── config.json -> ../../blobs/abc123...
│           └── pytorch_model.bin -> ../../blobs/def456...
```

### Benefits of This Structure

1. **Deduplication**: Multiple models can share the same blob files
2. **Efficient Updates**: Only changed files are re-downloaded
3. **Atomic Operations**: Downloads use temporary files and atomic moves
4. **Symlink Management**: Efficient file access through symbolic links

## Error Handling

The downloader provides comprehensive error handling:

- **Network Errors**: Automatic retry with exponential backoff
- **File System Errors**: Proper cleanup and error reporting
- **API Errors**: Platform-specific error messages
- **Validation Errors**: Model ID format validation

## Configuration

### Constructor Parameters

```cpp
RemoteModelDownloader(int max_attempts = 3, int retry_delay_seconds = 2)
```

- `max_attempts`: Maximum download attempts per file
- `retry_delay_seconds`: Delay between retry attempts

### Platform Detection

The downloader automatically detects platforms based on model ID prefixes:

- `HuggingFace/` or `hf/` → HuggingFace platform
- `ModelScope/` or `ms/` → ModelScope platform  
- `Modelers/` or `ml/` → Modelers platform
- No prefix → Defaults to HuggingFace

## Building

### Dependencies

- C++17 or later
- `httplib` for HTTP requests
- `std::filesystem` for file operations
- Standard C++ libraries

### CMake Integration

```cmake
# Add to your CMakeLists.txt
add_executable(multi_platform_demo src/multi_platform_downloader_demo.cpp)
target_link_libraries(multi_platform_demo remote_model_downloader httplib)
```

## Performance Considerations

1. **Parallel Downloads**: Files are downloaded sequentially (can be enhanced for parallel downloads)
2. **Memory Usage**: Downloads use streaming to minimize memory footprint
3. **Disk I/O**: Efficient blob storage with symlink-based access
4. **Network**: Resume support for large file downloads

## Future Enhancements

1. **Parallel Downloads**: Download multiple files simultaneously
2. **Authentication**: Support for authenticated API access
3. **Rate Limiting**: Platform-specific rate limiting
4. **Caching**: Intelligent caching of metadata and file lists
5. **Progress Callbacks**: Customizable progress reporting
6. **Background Downloads**: Asynchronous download support

## Troubleshooting

### Common Issues

1. **Network Timeouts**: Increase retry attempts and delay
2. **Permission Errors**: Ensure write access to storage folder
3. **Platform Detection**: Verify model ID format
4. **API Rate Limits**: Implement appropriate delays between requests

### Debug Information

Enable debug output by setting appropriate log levels and checking error_info strings for detailed error messages.

## Contributing

When adding new platforms:

1. Implement `IPlatformApiClient` interface
2. Add platform detection logic
3. Update platform-specific URL construction
4. Add appropriate error handling
5. Include unit tests for new functionality

## License

Copyright (c) 2024 Alibaba Group Holding Limited All rights reserved.
