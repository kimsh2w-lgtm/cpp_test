#pragma once
#include <string>
#include "result.h"
#include "logging_def.hpp"

namespace logging {


class LoggerBackend {
public:
    virtual ~LoggerBackend() = default;
    virtual Result<void> init() = 0;
    virtual Result<void> shutdown() = 0;
    virtual Result<void> registerLogger(const std::string& tag) = 0;
    virtual Result<void> setLevel(const std::string& tag, Level level) = 0;
    virtual void log(const std::string& tag, Level level, const std::string& msg) = 0;
    
    virtual Result<void> enableTag(const std::string& tag) = 0;
    virtual Result<void> disableTag(const std::string& tag) = 0;

    // Sink 추가
    virtual Result<void> setConsoleSink(const std::string& tag) = 0;
    virtual Result<void> setFileSink(const std::string& tag, const std::string& filename) = 0;
    virtual Result<void> setRotatingFileSink(const std::string& tag,
                            const std::string& filename,
                            size_t max_size,
                            size_t max_files) = 0;
    virtual Result<void> setSyslogSink(const std::string& tag, const std::string& ident) = 0;
    virtual Result<void> setUdpSink(const std::string& tag, const std::string& host, int port) = 0;
    virtual Result<void> setUdpJsonSink(const std::string& tag, const std::string& host, int port) = 0;
    virtual Result<void> setLokiSink(const std::string& tag,
                   const std::string& url,
                   const std::string& job) = 0;
};

} // namespace logging
