#include "logger_spdlog.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/syslog_sink.h>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <cstdlib>
#include <arpa/inet.h>
#include <unistd.h>
#include <curl/curl.h>
#include <chrono>


namespace logging {

std::string levelToString(spdlog::level::level_enum level)
{
    switch (level)
    {
    case spdlog::level::trace:    return "trace";
    case spdlog::level::debug:    return "debug";
    case spdlog::level::info:     return "info";
    case spdlog::level::warn:     return "warn";
    case spdlog::level::err:      return "error";
    case spdlog::level::critical: return "critical";
    case spdlog::level::off:      return "off";
    default:                      return "unknown";
    }
}


std::string jsonEscape(const std::string& s) {
    std::string out; out.reserve(s.size()+8);
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                // 0x00~0x1F 제어문자 추가 방어
                if (static_cast<unsigned char>(c) <= 0x1F) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// ---------- UDP Sink ----------
class UdpSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    UdpSink(const std::string& host, int port) {
        sock_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_ < 0) throw std::runtime_error("socket() failed");
        server_.sin_family = AF_INET;
        server_.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &server_.sin_addr);
    }
    ~UdpSink() { if (sock_ >= 0) close(sock_); }
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        base_sink<std::mutex>::formatter_->format(msg, formatted);
        sendto(sock_, formatted.data(), formatted.size(), 0,
               (struct sockaddr*)&server_, sizeof(server_));
    }
    void flush_() override {}
private:
    int sock_;
    struct sockaddr_in server_;
};

// ---------- UDP JSON Sink ----------
class UdpJsonSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    UdpJsonSink(const std::string& host, int port) {
        sock_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_ < 0) throw std::runtime_error("socket() failed");
        server_.sin_family = AF_INET;
        server_.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &server_.sin_addr);
    }
    ~UdpJsonSink() { if (sock_ >= 0) close(sock_); }
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        base_sink<std::mutex>::formatter_->format(msg, formatted);
        std::string log_line(formatted.data(), formatted.size());
        std::string json = fmt::format(
            R"({{"tag":"{}","level":"{}","msg":"{}"}})",
            std::string(msg.logger_name.data(), msg.logger_name.size()),
            spdlog::level::to_string_view(msg.level),
            log_line
        );
        sendto(sock_, json.data(), json.size(), 0,
               (struct sockaddr*)&server_, sizeof(server_));
    }
    void flush_() override {}
private:
    int sock_;
    struct sockaddr_in server_;
};

// ---------- Loki Sink ----------
class LokiSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    LokiSink(const std::string& url, const std::string& job, const std::string& tag)
        : url_(url), job_(job), tag_(tag) {
        curl_global_init(CURL_GLOBAL_ALL);
    }
    ~LokiSink() { curl_global_cleanup(); }
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        base_sink<std::mutex>::formatter_->format(msg, formatted);
        std::string log_line(formatted.data(), formatted.size());
        auto ns_since_epoch = std::chrono::duration_cast<std::chrono::nanoseconds>(
            msg.time.time_since_epoch()).count();
        std::string log = jsonEscape(log_line);
        auto level = levelToString(msg.level);
        const char* json_fmt = R"({{"streams":[{{"stream":{{"job":"{}","tag":"{}","level":"{}"}},"values":[["{}","{}"]]}}]}})";
        std::string payload = fmt::format(json_fmt, job_, tag_, level, std::to_string(ns_since_epoch), log);
        CURL* curl = curl_easy_init();
        if (curl) {
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload.size());
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 200L);
            auto result = curl_easy_perform(curl);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }
    }
    void flush_() override {}
private:
    std::string url_;
    std::string job_;
    std::string tag_;

};


SpdlogBackend::~SpdlogBackend() {
    shutdown();
}

void SpdlogBackend::init() {
    spdlog::set_pattern("[%n] [%^%l%$] %v");
    initialized_ = true;
}

void SpdlogBackend::shutdown() {
    std::lock_guard<std::mutex> lock(sink_mutex_);
    spdlog::apply_all([](auto l){ l->flush(); });
    spdlog::shutdown();
}

void SpdlogBackend::registerLogger(const std::string& tag) {
    std::lock_guard<std::mutex> lock(sink_mutex_);
    if (!initialized_) init();
    if (!spdlog::get(tag)) {
        std::vector<spdlog::sink_ptr> sinks;
        if (tag_sinks_.count(tag)) sinks = tag_sinks_[tag];
        else sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        // attach global sink
        for (auto& g_sink : tag_sinks_[std::string(GLOBAL_TAG)]) {
            sinks.push_back(g_sink);
        }
        auto logger = std::make_shared<spdlog::logger>(tag, sinks.begin(), sinks.end());
        logger->set_level(toSpd_(global_level_));
        spdlog::register_logger(logger);
    }
}

void SpdlogBackend::setLevel(const std::string& tag, Level level) {
    std::lock_guard<std::mutex> lock(sink_mutex_);
    if (tag == GLOBAL_TAG) {
        global_level_ = level;
        return;
    }
    auto logger = spdlog::get(tag);
    if (logger) {
        logger->set_level(toSpd_(level));
    }
}

void SpdlogBackend::setConsoleSink(const std::string& tag) {
    std::lock_guard<std::mutex> lock(sink_mutex_);
    tag_sinks_[tag].push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
}

void SpdlogBackend::setFileSink(const std::string& tag, const std::string& filename) {
    std::lock_guard<std::mutex> lock(sink_mutex_);
    tag_sinks_[tag].push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true));
}

void SpdlogBackend::setRotatingFileSink(const std::string& tag, const std::string& filename, size_t max_size, size_t max_files) {
    std::lock_guard<std::mutex> lock(sink_mutex_);
    tag_sinks_[tag].push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(filename, max_size, max_files));
}

void SpdlogBackend::setSyslogSink(const std::string& tag, const std::string& ident) {
    std::lock_guard<std::mutex> lock(sink_mutex_);
    tag_sinks_[tag].push_back(std::make_shared<spdlog::sinks::syslog_sink_mt>(ident, LOG_PID, LOG_USER, true));
}

void SpdlogBackend::setUdpSink(const std::string& tag, const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(sink_mutex_);
    tag_sinks_[tag].push_back(std::make_shared<UdpSink>(host, port));
}

void SpdlogBackend::setUdpJsonSink(const std::string& tag, const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(sink_mutex_);
    tag_sinks_[tag].push_back(std::make_shared<UdpJsonSink>(host, port));
}

void SpdlogBackend::setLokiSink(const std::string& tag, const std::string& url, const std::string& job) {
    std::lock_guard<std::mutex> lock(sink_mutex_);
    tag_sinks_[tag].push_back(std::make_shared<LokiSink>(url, job, tag));
}

void SpdlogBackend::log(const std::string& tag, Level level, const std::string& msg) {
    std::shared_ptr<spdlog::logger> logger;
    {
        std::lock_guard<std::mutex> lock(sink_mutex_);
        if (disabled_tags_.count(tag)) return;
    }

    logger = spdlog::get(tag);
    if (!logger) {
        registerLogger(tag);
        logger = spdlog::get(tag);
    }
    logger->log(toSpd_(level), msg);
    if (level == Level::Fatal) {
        spdlog::apply_all([](std::shared_ptr<spdlog::logger> l){ l->flush(); });
    }
}



} // namespace logging
