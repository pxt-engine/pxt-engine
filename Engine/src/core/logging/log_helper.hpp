#pragma once

#include "logger_sink.hpp"
#include <spdlog/spdlog.h>

inline spdlog::level::level_enum pxtToSpdlogLevel(pxt::core::LogLevel level) {
    switch (level) {
    case pxt::core::LogLevel::Trace:
        return spdlog::level::trace;
    case pxt::core::LogLevel::Debug:
        return spdlog::level::debug;
    case pxt::core::LogLevel::Info:
        return spdlog::level::info;
    case pxt::core::LogLevel::Warn:
        return spdlog::level::warn;
    case pxt::core::LogLevel::Error:
        return spdlog::level::err;
    case pxt::core::LogLevel::Fatal:
        return spdlog::level::critical;
    default:
        return spdlog::level::off;
    }
}

inline std::string_view cleanFunctionName(std::string_view fullSig) {
    if (fullSig.empty())
        return "Unknown";

    // 1. Find the start of the parameters '('
    size_t parenPos = fullSig.find('(');
    std::string_view namePart = (parenPos == std::string_view::npos) ? fullSig : fullSig.substr(0, parenPos);

    // 2. Find the start of the function name by looking for the last space
    // This removes 'void', '__cdecl', 'static', etc.
    size_t lastSpace = namePart.find_last_of(' ');
    if (lastSpace != std::string_view::npos) {
        namePart.remove_prefix(lastSpace + 1);
    }

    return namePart;
}