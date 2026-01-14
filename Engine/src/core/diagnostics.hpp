#pragma once

#include "core/logging/logger.hpp"
#include "core/platform.hpp"
#include "utils/timer.hpp"

#include <cassert>
#include <filesystem>
#include <format>
#include <iostream>

// clang-format off

// Platform-specific debug break implementation 
#if defined(PXT_PLATFORM_WINDOWS)
    // Use the Windows-specific intrinsic
    #define PXT_DEBUG_BREAK() __debugbreak()
#elif defined(PXT_PLATFORM_POSIX_LIKE)
    // Use the POSIX signal method for Apple, Android, Linux, Unix, and generic POSIX
    #include <signal.h>
    #define PXT_DEBUG_BREAK() raise(SIGTRAP)
#elif defined(__GNUC__) || defined(__clang__)
    // Use GCC/Clang built-in function
    #define PXT_DEBUG_BREAK() __builtin_trap()
#else
    #error "Unsupported platform for debug break"
#endif

// clang-format on

// The "EXPAND" helper is necessary for MSVC compatibility
#define PXT_EXPAND(x) x

// Assertion without custom message
#define PXT_ASSERT_NO_MSG(condition)                                                                                   \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            PXT_ERROR("Assertion `{}` failed at {}:{}", #condition,                                                    \
                      std::filesystem::path(__FILE__).filename().string(), __LINE__);                                  \
            PXT_DEBUG_BREAK();                                                                                         \
        }                                                                                                              \
    } while (0)

// Assertion with custom message
#define PXT_ASSERT_WITH_MSG(condition, msg)                                                                            \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            PXT_ERROR(msg);                                                                                            \
            PXT_DEBUG_BREAK();                                                                                         \
        }                                                                                                              \
    } while (0)

// Assertion with custom formatted message
#define PXT_ASSERT_WITH_FMT(condition, msg, ...)                                                                       \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            PXT_ERROR(msg, __VA_ARGS__);                                                                               \
            PXT_DEBUG_BREAK();                                                                                         \
        }                                                                                                              \
    } while (0)

// Macro chooser (1 arg = no msg, 2 = with msg, 3+ = with formatted msg)
#define PXT_ASSERT_MACRO_CHOOSER(_1, _2, _3, NAME, ...) NAME

// Dispatcher
#define PXT_ASSERT(...)                                                                                                \
    PXT_EXPAND(PXT_ASSERT_MACRO_CHOOSER(__VA_ARGS__, PXT_ASSERT_WITH_FMT, PXT_ASSERT_WITH_MSG,                         \
                                        PXT_ASSERT_NO_MSG)(__VA_ARGS__))

#define PXT_STATIC_ASSERT(condition, msg) static_assert(condition, msg)

#define PXT_ENABLE_PROFILING

// Profiling macros
#if defined(PXT_ENABLE_PROFILING)
#define PXT_PROFILE(name) utils::ProfilingTimer timer##__LINE__(name)
#define PXT_PROFILE_FN() PXT_PROFILE(__FUNCTION__)
#else
#define PXT_PROFILE(name)
#define PXT_PROFILE_FN()
#endif