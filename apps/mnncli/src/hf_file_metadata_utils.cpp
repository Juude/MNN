//
// Created by ruoyi.sjd on 2024/12/25.
// Copyright (c) 2024 Alibaba Group Holding Limited All rights reserved.
//

#include "hf_file_metadata_utils.hpp"
#include "httplib.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace mnncli {

HfFileMetadata HfFileMetadataUtils::getFileMetadata(const std::string& url, std::string& error_info) {
    // Create a default HTTP client for metadata requests
    auto client = std::make_shared<httplib::SSLClient>("huggingface.co");
    return getFileMetadata(url, client, error_info);
}

HfFileMetadata HfFileMetadataUtils::getFileMetadata(const std::string& url, 
                                                   std::shared_ptr<httplib::SSLClient> client,
                                                   std::string& error_info) {
    HfFileMetadata metadata;
    metadata.location = url;
    
    try {
        // Parse URL to get host and path
        auto [host, path] = HfApiClient::ParseUrl(url);
        if (host.empty() || path.empty()) {
            error_info = "Invalid URL format: " + url;
            return metadata;
        }
        
        // Create HTTP client if not provided
        std::unique_ptr<httplib::SSLClient> local_client;
        httplib::SSLClient* client_ptr = client.get();
        
        if (!client_ptr) {
            local_client = std::make_unique<httplib::SSLClient>(host, 443);
            client_ptr = local_client.get();
        }
        
        // Set up headers for HEAD request
        httplib::Headers headers;
        headers.emplace(HEADER_ACCEPT_ENCODING, "identity");
        
        // Make HEAD request to get metadata
        auto response = client_ptr->Head(path, headers);
        
        if (!response) {
            error_info = "No response received from server";
            return metadata;
        }
        
        // Check for success or redirect status codes (301, 302, 303, 307, 308)
        bool is_redirect = (response->status >= 301 && response->status <= 308);
        
        if (response->status < 200 || response->status >= 300 && !is_redirect) {
            error_info = "Failed to fetch metadata status " + std::to_string(response->status);
            return metadata;
        }
        
        // Handle redirects
        if (is_redirect) {
            std::string location = response->get_header_value(HEADER_LOCATION);
            if (!location.empty()) {
                metadata.location = handleRedirects(url, location);
            }
        }
        
        // Parse HuggingFace specific headers
        parseHuggingFaceHeaders(response->headers, metadata);
        
        // Validate metadata
        if (!metadata.isValid()) {
            error_info = "Invalid metadata received - missing required fields";
        }
        
    } catch (const std::exception& e) {
        error_info = "Exception during metadata fetch: " + std::string(e.what());
    }
    
    return metadata;
}

int64_t HfFileMetadataUtils::parseContentLength(const std::string& content_length) {
    if (content_length.empty()) {
        return 0;
    }
    
    try {
        return std::stoll(content_length);
    } catch (const std::exception&) {
        return 0;
    }
}

std::string HfFileMetadataUtils::normalizeETag(const std::string& etag) {
    if (etag.empty()) {
        return etag;
    }
    
    // Remove quotes if present
    if (etag.length() >= 2 && etag.front() == '"' && etag.back() == '"') {
        return etag.substr(1, etag.length() - 2);
    }
    
    return etag;
}

std::string HfFileMetadataUtils::handleRedirects(const std::string& original_url, 
                                                const std::string& location_header) {
    if (location_header.empty()) {
        return original_url;
    }
    
    // If location is absolute URL, use it directly
    if (location_header.find("http://") == 0 || location_header.find("https://") == 0) {
        return location_header;
    }
    
    // If location is relative, construct absolute URL
    if (location_header.front() == '/') {
        // Extract base URL (scheme + host + port) from original URL
        auto [host, path] = HfApiClient::ParseUrl(original_url);
        if (!host.empty()) {
            return "https://" + host + location_header;
        }
    }
    
    // For relative paths without leading slash, append to original path
    auto [host, path] = HfApiClient::ParseUrl(original_url);
    if (!host.empty()) {
        // Find the last slash in the path and truncate there
        size_t last_slash = path.find_last_of('/');
        if (last_slash != std::string::npos) {
            std::string base_path = path.substr(0, last_slash + 1);
            return "https://" + host + base_path + location_header;
        }
    }
    
    return original_url;
}

void HfFileMetadataUtils::parseHuggingFaceHeaders(const httplib::Headers& headers, 
                                                   HfFileMetadata& metadata) {
    // Parse HuggingFace specific headers first, fallback to standard headers
    
    // ETag handling
    std::string linked_etag = getHeaderValue(headers, HEADER_X_LINKED_ETAG);
    std::string standard_etag = getHeaderValue(headers, HEADER_ETAG);
    
    if (!linked_etag.empty()) {
        metadata.etag = normalizeETag(linked_etag);
    } else if (!standard_etag.empty()) {
        metadata.etag = normalizeETag(standard_etag);
    }
    
    // Size handling
    std::string linked_size = getHeaderValue(headers, HEADER_X_LINKED_SIZE);
    std::string content_length = getHeaderValue(headers, HEADER_CONTENT_LENGTH);
    
    if (!linked_size.empty()) {
        metadata.size = parseContentLength(linked_size);
    } else if (!content_length.empty()) {
        metadata.size = parseContentLength(content_length);
    }
    
    // Commit hash
    std::string commit_hash = getHeaderValue(headers, HEADER_X_REPO_COMMIT);
    if (!commit_hash.empty()) {
        metadata.commit_hash = commit_hash;
    }
}

// Helper method to get header value from multimap
std::string HfFileMetadataUtils::getHeaderValue(const httplib::Headers& headers, const std::string& key) {
    auto it = headers.find(key);
    if (it != headers.end()) {
        return it->second;
    }
    return "";
}

} // namespace mnncli
