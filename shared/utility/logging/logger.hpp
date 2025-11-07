#pragma once
#include <string>
#include "logging_def.hpp"
#include "logger_backend.hpp"
#include <yaml-cpp/yaml.h>

namespace logging {

class Logger {
public:
    static Logger& instance();

    void init(logging::Type logger, const std::string& filename);
    void apply();
    void log(const std::string& tag, Level level, const std::string& msg) { if(logger_) logger_->log(tag, level, msg); };

    void setLevel(const std::string& tag, Level level) { if(logger_) logger_->setLevel(tag, level); };
    void enableTag(const std::string& tag) { if(logger_) logger_->enableTag(tag); }
    void disableTag(const std::string& tag) { if(logger_) logger_->disableTag(tag); }

private:
    Logger();
    ~Logger();

    // 복사/이동 금지
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    YAML::Node config_;
    std::shared_ptr<LoggerBackend> logger_;
    logging::Level toLevel(const std::string& s);
    void configureSink(const std::string& tag, const YAML::Node& sink);
    
};

} // namespace logging