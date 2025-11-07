#include "logger.hpp"
#include "logger_spdlog.hpp"
#include <iostream>

namespace logging {

Logger& Logger::instance() {
    static Logger instance;  // C++11 이후 thread-safe 보장
    return instance;
}

Logger::Logger() {

}

Logger::~Logger() {
    if(logger_) logger_->shutdown();
}

void Logger::init(logging::Type logger_type, const std::string& filename) {
    try {
        config_ = YAML::LoadFile(filename);
    } catch (const std::exception& e) {
        std::cerr << "YAML load error: " << e.what() << std::endl;
        throw;
    }

    switch (logger_type)
    {
    case logging::Type::SpdLog:
        logger_ = std::make_shared<SpdlogBackend>();
        break;
    default:
        throw std::runtime_error("unknown logger type");
    }
    
    logger_->init();
}

logging::Level Logger::toLevel(const std::string& s) {
    if (s == "trace") return logging::Level::Trace;
    if (s == "debug") return logging::Level::Debug;
    if (s == "info")  return logging::Level::Info;
    if (s == "warn")  return logging::Level::Warn;
    if (s == "error") return logging::Level::Error;
    if (s == "fatal") return logging::Level::Fatal;
    return logging::Level::Off;
}

void Logger::apply() {
    if (!config_["log"]) return;
    if (!logger_) return;

    auto g_tag = std::string(GLOBAL_TAG);
    if (config_["log"][g_tag]) {
        auto node = config_["log"][g_tag];

        // Global level 설정
        if (node["level"]) {
            logger_->setLevel(g_tag, toLevel(node["level"].as<std::string>()));
        }

        // Global sink 설정
        if (node["sinks"]) {
            for (auto sink : node["sinks"]) {
                configureSink(g_tag, sink);
            }
        }
    }

    // 
    for (auto it : config_["log"]) {
        std::string tag = it.first.as<std::string>();
        if(tag == GLOBAL_TAG) continue;
        auto node = it.second;

        if (node["sinks"]) {
            for (auto sink : node["sinks"]) {
                configureSink(tag, sink);;
            }
        }
        logger_->registerLogger(tag);

        // 레벨 적용
        if (node["level"]) {
            logger_->set_level(tag, toLevel(node["level"].as<std::string>()));
        }
    }
}

void Logger::configureSink(const std::string& tag, const YAML::Node& sink) {
    if (!logger_) return;

    std::string type = sink["type"].as<std::string>();

    if (type == "console") {
        // 기본 콘솔은 register_logger에서 자동 추가됨
        logger_->set_console_sink(tag);
    } else if (type == "file") {
        logger_->set_file_sink(tag, sink["filename"].as<std::string>());
    } else if (type == "rotating_file") {
        logger_->set_rotating_file_sink(tag,
            sink["filename"].as<std::string>(),
            sink["max_size"].as<size_t>(),
            sink["max_files"].as<size_t>());
    } else if (type == "syslog") {
        logger_->set_syslog_sink(tag,
            sink["ident"] ? sink["ident"].as<std::string>() : tag);
    } else if (type == "udp") {
        logger_->set_udp_sink(tag,
            sink["host"].as<std::string>(),
            sink["port"].as<int>());
    } else if (type == "udp_json") {
        logger_->set_udp_json_sink(tag,
            sink["host"].as<std::string>(),
            sink["port"].as<int>());
    } else if (type == "loki") {
        logger_->set_loki_sink(tag,
            sink["url"].as<std::string>(),
            sink["job"] ? sink["job"].as<std::string>() : "myapp");
    } else {
        std::cerr << "Unknown sink type: " << type << std::endl;
    }
}

} // namespace logging