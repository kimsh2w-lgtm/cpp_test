#pragma once
#include "logging_def.hpp"
#include "logger.hpp"
#include <string>
#include <fmt/core.h>
#include <fmt/format.h>
#include <string_view>
#include <type_traits>
#include <utility>



namespace logging {


inline void init(logging::Type logger_type, const std::string& filename) {
    Logger::instance().init(logger_type, filename);
    Logger::instance().apply();
}
    
inline void apply() {
    Logger::instance().apply();
}


template <typename... Args>
inline void log(const char* tag, logging::Level level,
                        std::string_view fmt_str, Args&&... args) {
    fmt::memory_buffer buf;
    fmt::format_to(std::back_inserter(buf), fmt_str, std::forward<Args>(args)...);
    Logger::instance().log(tag, level, std::string(buf.data(), buf.size()));
}


template <typename T, typename... Args>
inline void log_auto(T* self, logging::Level level,
                          std::string_view fmt_str, Args&&... args) {
    // 클래스 내부: T::LOG_TAG 사용
    log(T::LOG_TAG, level, fmt_str, std::forward<Args>(args)...);
}

template <typename... Args>
inline void log_auto(std::nullptr_t, logging::Level level,
                          std::string_view fmt_str, Args&&... args) {
    // 전역: TAG 사용
    log(DEFAULT_TAG, level, fmt_str, std::forward<Args>(args)...);
}


} // namespace logging


template <typename... Args>
inline void LOG_TRACE(const char* tag, std::string_view fmt_str, Args&&... args) {
    logging::log(tag, logging::Level::Trace, fmt_str, std::forward<Args>(args)...);
}

template <typename... Args>
inline void LOG_DEBUG(const char* tag, std::string_view fmt_str, Args&&... args) {
    logging::log(tag, logging::Level::Debug, fmt_str, std::forward<Args>(args)...);
}

template <typename... Args>
inline void LOG_INFO(const char* tag, std::string_view fmt_str, Args&&... args) {
    logging::log(tag, logging::Level::Info, fmt_str, std::forward<Args>(args)...);
}

template <typename... Args>
inline void LOG_WARN(const char* tag, std::string_view fmt_str, Args&&... args) {
    logging::log(tag, logging::Level::Warn, fmt_str, std::forward<Args>(args)...);
}

template <typename... Args>
inline void LOG_ERROR(const char* tag, std::string_view fmt_str, Args&&... args) {
    logging::log(tag, logging::Level::Error, fmt_str, std::forward<Args>(args)...);
}

template <typename... Args>
inline void LOG_FATAL(const char* tag, std::string_view fmt_str, Args&&... args) {
    logging::log(tag, logging::Level::Fatal, fmt_str, std::forward<Args>(args)...);
}


#define LOGT(...) logging::log(std::decay_t<decltype(*this)>::LOG_TAG, logging::Level::Trace, __VA_ARGS__)
#define LOGD(...) logging::log(std::decay_t<decltype(*this)>::LOG_TAG, logging::Level::Debug, __VA_ARGS__)
#define LOGI(...) logging::log(std::decay_t<decltype(*this)>::LOG_TAG, logging::Level::Info, __VA_ARGS__)
#define LOGW(...) logging::log(std::decay_t<decltype(*this)>::LOG_TAG, logging::Level::Warn, __VA_ARGS__)
#define LOGE(...) logging::log(std::decay_t<decltype(*this)>::LOG_TAG, logging::Level::Error, __VA_ARGS__)
#define LOGF(...) logging::log(std::decay_t<decltype(*this)>::LOG_TAG, logging::Level::Fatal, __VA_ARGS__)

