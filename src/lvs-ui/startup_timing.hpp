#pragma once

#include <SDL_log.h>

#include <chrono>
#include <cstdarg>
#include <cstdio>

inline auto StartupTimerBase() -> std::chrono::steady_clock::time_point
{
    static const auto base = std::chrono::steady_clock::now();
    return base;
}

inline auto StartupElapsedMs() -> uint64_t
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - StartupTimerBase())
                                     .count());
}

inline auto ElapsedMsSince(const std::chrono::steady_clock::time_point& start) -> uint64_t
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - start)
                                     .count());
}

inline void StartupLog(const char* fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "[startup +%llums] %s",
                static_cast<unsigned long long>(StartupElapsedMs()),
                buffer);
}
