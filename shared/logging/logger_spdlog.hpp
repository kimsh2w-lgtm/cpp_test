#pragma once
#include "base.h"
#include "logger_backend.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/syslog_sink.h>
#include <unordered_map>
#include <unordered_set>
#include <cstdlib>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <mutex>


namespace logging {


class SpdlogBackend : virtual public LoggerBackend {
public:
    SpdlogBackend() : global_level_(Level::Off) { };
    ~SpdlogBackend();

    Result<void> init() override;
    Result<void> shutdown() override;
    Result<void> registerLogger(const std::string& tag) override;
    Result<void> setLevel(const std::string& tag, Level lvl) override;
    Result<void> enableTag(const std::string& tag) override { 
        std::lock_guard<std::mutex> lock(sink_mutex_);
        disabled_tags_.erase(tag); 
    }
    Result<void> disableTag(const std::string& tag) override { 
        std::lock_guard<std::mutex> lock(sink_mutex_);
        disabled_tags_.insert(tag); 
    }

    Result<void> setConsoleSink(const std::string& tag) override;
    Result<void> setFileSink(const std::string& tag, const std::string& filename) override;
    Result<void> setRotatingFileSink(const std::string& tag, 
                                const std::string& filename, 
                                size_t max_size, size_t max_files) override;
    Result<void> setSyslogSink(const std::string& tag, const std::string& ident) override;
    Result<void> setUdpSink(const std::string& tag, const std::string& host, int port) override;
    Result<void> setUdpJsonSink(const std::string& tag, const std::string& host, int port) override;
    Result<void> setLokiSink(const std::string& tag, const std::string& url, const std::string& job) override;
    void log(const std::string& tag, Level level, const std::string& msg) override;

private:
    static spdlog::level::level_enum toSpd_(Level lvl) {
        switch (lvl) {
            case Level::Trace: return spdlog::level::trace;
            case Level::Debug: return spdlog::level::debug;
            case Level::Info:  return spdlog::level::info;
            case Level::Warn:  return spdlog::level::warn;
            case Level::Error: return spdlog::level::err;
            case Level::Fatal: return spdlog::level::critical;
            case Level::Off:   return spdlog::level::off;
        }
        return spdlog::level::off;
    }

private:
    bool initialized_ = false;
    std::unordered_set<std::string> disabled_tags_;
    std::unordered_map<std::string, std::vector<spdlog::sink_ptr>> tag_sinks_;
    std::mutex sink_mutex_;

    Level global_level_;

    
};

} // namespace logging
