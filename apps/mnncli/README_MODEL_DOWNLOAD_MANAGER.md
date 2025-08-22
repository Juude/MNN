# MNN Model Download Manager

This document describes the new Model Download Manager system that fixes the `.gitattributes` file download error and provides a unified interface for downloading models from multiple sources.

## Overview

The Model Download Manager is a C++ implementation that routes download requests to appropriate platform-specific downloaders, similar to the Java implementation in the Android app. It provides:

- **Unified Interface**: Single API for all model sources
- **Platform Routing**: Automatically routes to HuggingFace, ModelScope, or Modelers downloaders
- **Enhanced Metadata Handling**: Proper handling of HTTP redirects and HuggingFace-specific headers
- **Progress Tracking**: Real-time download progress and status updates
- **Error Handling**: Comprehensive error handling with retry mechanisms

## Architecture

### Core Components

1. **ModelDownloadManager**: Main orchestrator that routes requests to appropriate downloaders
2. **ModelRepoDownloader**: Base interface that all platform-specific downloaders implement
3. **HfModelDownloader**: HuggingFace-specific downloader with enhanced metadata handling
4. **MsModelDownloader**: ModelScope downloader
5. **MlModelDownloader**: Modelers downloader
6. **HfFileMetadataUtils**: Utilities for handling HuggingFace file metadata and redirects

### Key Features

- **Automatic Source Detection**: Detects model source from model ID format
- **Metadata Pre-fetching**: Fetches file metadata before downloading to handle special files
- **Redirect Handling**: Properly handles HTTP 307 redirects for `.gitattributes` and other files
- **Progress Callbacks**: Comprehensive progress and status notifications
- **Pause/Resume**: Support for pausing and resuming downloads

## Usage

### Basic Usage

```cpp
#include "model_download_manager.hpp"

// Get download manager instance
auto& download_manager = mnncli::ModelDownloadManager::getInstance("./models");

// Add progress listener
class MyListener : public mnncli::DownloadListener {
    void onDownloadProgress(const std::string& model_id, const mnncli::DownloadProgress& progress) override {
        std::cout << "Progress: " << (progress.progress * 100.0) << "%" << std::endl;
    }
    // ... implement other callback methods
};

auto listener = std::make_unique<MyListener>();
download_manager.addListener(listener.get());

// Start download
download_manager.startDownload("HuggingFace:taobao-mnn/chatglm3-6b");
```

### Model ID Format

The system supports multiple model ID formats:

- **HuggingFace**: `HuggingFace:owner/repo` or `hf:owner/repo`
- **ModelScope**: `ModelScope:owner/repo` or `ms:owner/repo`
- **Modelers**: `Modelers:owner/repo` or `ml:owner/repo`
- **Auto-detect**: `owner/repo` (defaults to HuggingFace)

### Download Control

```cpp
// Pause download
download_manager.pauseDownload("HuggingFace:taobao-mnn/chatglm3-6b");

// Resume download
download_manager.resumeDownload("HuggingFace:taobao-mnn/chatglm3-6b");

// Cancel download
download_manager.cancelDownload("HuggingFace:taobao-mnn/chatglm3-6b");

// Get download status
auto info = download_manager.getDownloadInfo("HuggingFace:taobao-mnn/chatglm3-6b");
std::cout << "Progress: " << (info.progress * 100.0) << "%" << std::endl;
```

## Fixing the .gitattributes Error

### Problem

The original error occurred when downloading `.gitattributes` files from HuggingFace:
```
DownloadFile error at file: .gitattributes error message: GetFileMetadata Failed to fetch metadata status 307, attempt: 1/3
```

### Root Cause

- HuggingFace returns HTTP 307 redirects for certain files like `.gitattributes`
- The original implementation didn't properly handle redirects
- Missing support for HuggingFace-specific headers (`x-linked-etag`, `x-linked-size`)

### Solution

1. **Enhanced Metadata Fetching**: Uses `HEAD` requests with proper redirect handling
2. **Special Header Support**: Parses HuggingFace-specific headers for accurate file information
3. **Redirect Resolution**: Manually handles redirects to get final file locations
4. **Fallback Mechanisms**: Falls back to standard HTTP headers when special ones aren't available

### Implementation Details

```cpp
// In HfFileMetadataUtils::getFileMetadata()
auto response = client_ptr->Head(path, headers);

// Handle redirects (301-308)
bool is_redirect = (response->status >= 301 && response->status <= 308);
if (is_redirect) {
    std::string location = response->get_header_value("Location");
    metadata.location = handleRedirects(url, location);
}

// Parse HuggingFace specific headers
parseHuggingFaceHeaders(response->headers, metadata);
```

## Building

### Dependencies

- C++17 or later
- httplib (HTTP client library)
- OpenSSL
- MNN framework

### CMake Integration

The new files are automatically included in the CMakeLists.txt:

```cmake
add_executable(mnncli
    # ... existing files ...
    ${CMAKE_CURRENT_LIST_DIR}/src/model_repo_downloader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/hf_file_metadata_utils.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/hf_model_downloader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/model_download_manager.cpp
)
```

## Testing

### Demo Program

A demo program is included (`src/model_download_demo.cpp`) that shows how to use the system:

```bash
# Build the demo
make model_download_demo

# Run the demo
./model_download_demo

# Example commands
> download HuggingFace:taobao-mnn/chatglm3-6b
> status HuggingFace:taobao-mnn/chatglm3-6b
> pause HuggingFace:taobao-mnn/chatglm3-6b
> list
```

### Testing .gitattributes Handling

To test the fix for `.gitattributes` files:

1. Start a download for a HuggingFace model that contains `.gitattributes`
2. Monitor the logs for successful metadata fetching
3. Verify that the file downloads without 307 errors

## Migration from Old System

### Before (Old HfApiClient::DownloadRepo)

```cpp
// Old approach - direct download without metadata
void HfApiClient::DownloadRepo(const RepoInfo& repo_info) {
    RemoteModelDownloader model_downloader{GetHost(), max_attempts_, retry_delay_seconds_};
    // ... download logic
}
```

### After (New ModelDownloadManager)

```cpp
// New approach - routed through download manager
auto& download_manager = ModelDownloadManager::getInstance(cache_path);
download_manager.startDownload("HuggingFace:owner/repo");
```

### Benefits of Migration

1. **Better Error Handling**: Proper handling of HTTP redirects and special files
2. **Unified Interface**: Single API for all model sources
3. **Progress Tracking**: Real-time progress updates and status monitoring
4. **Extensibility**: Easy to add new model sources
5. **Consistency**: Same interface across all platforms

## Troubleshooting

### Common Issues

1. **Compilation Errors**: Ensure C++17 is enabled and all dependencies are available
2. **HTTP Errors**: Check network connectivity and firewall settings
3. **Permission Errors**: Ensure write access to cache directory

### Debug Mode

Enable verbose logging:

```cpp
download_manager.setVerbose(true);
```

### Log Analysis

Look for these key messages:
- `✅ API response received successfully` - Metadata fetch successful
- `🔍 Making request to:` - HTTP request details
- `📊 Progress for:` - Download progress updates

## Future Enhancements

1. **Resume Downloads**: Support for resuming interrupted downloads
2. **Parallel Downloads**: Download multiple files simultaneously
3. **Cache Management**: Intelligent cache cleanup and management
4. **Rate Limiting**: Respect API rate limits
5. **Authentication**: Support for private repositories

## Contributing

When adding new model sources:

1. Implement the `ModelRepoDownloader` interface
2. Add the source to `ModelSources` enum
3. Update `ModelDownloadManager::getDownloaderForSource()`
4. Add appropriate tests

## License

Copyright (c) 2024 Alibaba Group Holding Limited All rights reserved.
