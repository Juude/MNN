//
// Created by MNN on 2024/12/18.
// Copyright (c) 2024 Alibaba Group Holding Limited All rights reserved.
//

#include "log_utils.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace mnncli {

// Static member initialization
bool LogUtils::verbose_enabled_ = false;

void LogUtils::setVerbose(bool verbose) {
    verbose_enabled_ = verbose;
}

bool LogUtils::isVerbose() {
    return verbose_enabled_;
}

void LogUtils::debug(const std::string& message, const std::string& tag) {
    std::cout << formatMessage(LogLevel::DEBUG, message, tag) << std::endl;
}

void LogUtils::info(const std::string& message, const std::string& tag) {
    std::cout << formatMessage(LogLevel::INFO, message, tag) << std::endl;
}

void LogUtils::warning(const std::string& message, const std::string& tag) {
    std::cerr << formatMessage(LogLevel::WARNING, message, tag) << std::endl;
}

void LogUtils::error(const std::string& message, const std::string& tag) {
    std::cerr << Colors::RED << formatMessage(LogLevel::ERROR, message, tag) << Colors::RESET << std::endl;
}

void LogUtils::debugIfVerbose(const std::string& message, const std::string& tag) {
    if (verbose_enabled_) {
        debug(message, tag);
    }
}

std::string LogUtils::formatFileSize(int64_t bytes) {
    if (bytes < 1024) {
        return std::to_string(bytes) + " B";
    } else if (bytes < 1024 * 1024) {
        return std::to_string(bytes / 1024) + " KB";
    } else if (bytes < 1024 * 1024 * 1024) {
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    } else {
        return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
    }
}

std::string LogUtils::formatProgress(double progress) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (progress * 100.0) << "%";
    return oss.str();
}

std::string LogUtils::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string LogUtils::formatMessage(LogLevel level, const std::string& message, const std::string& tag) {
    std::ostringstream oss;
    
    // Add timestamp
    oss << "[" << getTimestamp() << "] ";
    
    // Add level indicator
    switch (level) {
        case LogLevel::DEBUG:
            oss << "[DEBUG] ";
            break;
        case LogLevel::INFO:
            oss << "[INFO]  ";
            break;
        case LogLevel::WARNING:
            oss << "[WARN]  ";
            break;
        case LogLevel::ERROR:
            oss << "[ERROR] ";
            break;
    }
    
    // Add tag if provided
    if (!tag.empty()) {
        oss << "[" << tag << "] ";
    }
    
    // Add message
    oss << message;
    
    return oss.str();
}

} // namespace mnncli
