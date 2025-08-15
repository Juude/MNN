//
// Created by MNN on 2024/12/18.
// Copyright (c) 2024 Alibaba Group Holding Limited All rights reserved.
//

#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <memory>

namespace mnncli {

// Terminal color constants for colored logging
namespace Colors {
    const std::string RESET = "\033[0m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
    const std::string BOLD = "\033[1m";
}

// Log levels enum
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

// Log utility class for centralized logging
class LogUtils {
public:
    // Set global verbose mode
    static void setVerbose(bool verbose);
    
    // Check if verbose mode is enabled
    static bool isVerbose();
    
    // Log methods with different levels
    static void debug(const std::string& message, const std::string& tag = "");
    static void info(const std::string& message, const std::string& tag = "");
    static void warning(const std::string& message, const std::string& tag = "");
    static void error(const std::string& message, const std::string& tag = "");
    
    // Conditional debug logging (only outputs when verbose is enabled)
    static void debugIfVerbose(const std::string& message, const std::string& tag = "");
    
    // Format file size for logging
    static std::string formatFileSize(int64_t bytes);
    
    // Format progress percentage
    static std::string formatProgress(double progress);
    
    // Get current timestamp string
    static std::string getTimestamp();

private:
    static bool verbose_enabled_;
    static std::string formatMessage(LogLevel level, const std::string& message, const std::string& tag);
};

// Convenience macros for logging
#define LOG_DEBUG(msg) mnncli::LogUtils::debugIfVerbose(msg)
#define LOG_DEBUG_TAG(msg, tag) mnncli::LogUtils::debugIfVerbose(msg, tag)
#define LOG_INFO(msg) mnncli::LogUtils::info(msg)
#define LOG_WARNING(msg) mnncli::LogUtils::warning(msg)
#define LOG_ERROR(msg) mnncli::LogUtils::error(msg)

// Conditional debug macro that only compiles when verbose is enabled
#ifdef DEBUG
#define VERBOSE_LOG(msg) mnncli::LogUtils::debug(msg)
#define VERBOSE_LOG_TAG(msg, tag) mnncli::LogUtils::debug(msg, tag)
#else
#define VERBOSE_LOG(msg) do {} while(0)
#define VERBOSE_LOG_TAG(msg, tag) do {} while(0)
#endif

} // namespace mnncli
