#pragma once
#include "result.h"
#include "logger_backend.hpp"
#include <iostream>
#include <mutex>

namespace logging {

class ConsoleBackend : public LoggerBackend {
public:
    Result<void> init() override {
        std::lock_guard<std::mutex> lock(mutex_);
        global_level_ = Level::Info;
        return OK();
    };
    Result<void> shutdown() override { return OK(); }

    Result<void> registerLogger(const std::string& tag) override {
        std::lock_guard<std::mutex> lock(mutex_);
        // 새 태그는 기본 레벨(global_level_)로 설정
        if (tag_levels_.find(tag) == tag_levels_.end()) {
            tag_levels_[tag] = global_level_;
        }
        return OK();
    }

    Result<void> setLevel(const std::string& tag, Level level) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (tag == GLOBAL_TAG)
            global_level_ = level;
        else
            tag_levels_[tag] = level;
        return OK();
    }

    Result<void> enableTag(const std::string& tag) override {
        std::lock_guard<std::mutex> lock(mutex_);
        disabled_tags_.erase(tag);
        return OK();
    }
    Result<void> disableTag(const std::string& tag) override {
        std::lock_guard<std::mutex> lock(mutex_);
        disabled_tags_.insert(tag);
        return OK();
    }

    Result<void> setConsoleSink(const std::string& tag) override { std::lock_guard<std::mutex> lock(mutex_);
        if (tag_levels_.find(tag) == tag_levels_.end()) {
            tag_levels_[tag] = global_level_;
        }
        disabled_tags_.erase(tag);  // 콘솔 sink 설정 시 자동 활성화
        return OK();
    }
    Result<void> setFileSink(const std::string&, const std::string&) override { return Error(ResultCode::NotSupported); }
    Result<void> setRotatingFileSink(const std::string&, const std::string&, size_t, size_t) override { return Error(ResultCode::NotSupported); }
    Result<void> setSyslogSink(const std::string&, const std::string&) override { return Error(ResultCode::NotSupported); }
    Result<void> setUdpSink(const std::string&, const std::string&, int) override { return Error(ResultCode::NotSupported); }
    Result<void> setUdpJsonSink(const std::string&, const std::string&, int) override { return (ResultCode::NotSupported); }
    Result<void> setLokiSink(const std::string&, const std::string&, const std::string&) override { return Error(ResultCode::NotSupported); }

    void log(const std::string& tag, Level level, const std::string& msg) override {
        std::lock_guard<std::mutex> lock(mutex_);

        // 비활성화된 태그는 무시
        if (disabled_tags_.count(tag))
            return;

        // 태그 레벨 조회 (없으면 글로벌 레벨 사용)
        Level current_level = global_level_;
        if (tag_levels_.find(tag) != tag_levels_.end())
            current_level = tag_levels_[tag];

        // 현재 태그 레벨보다 낮으면 출력 안 함
        if (level < current_level)
            return;

        const char* tagColor = "\033[94m";   // blue
        const char* reset    = "\033[0m";
        const char* levelColor = levelToColor(level);

        std::ostream& out = (level >= Level::Error) ? std::cerr : std::cout;

        out << "[" << tagColor << tag << reset << "] "
            << levelColor << levelToStr(level) << reset << ": "
            << msg << std::endl;
    }

private:
    static const char* levelToStr(Level level) {
        switch (level) {
            case Level::Trace: return "TRACE";
            case Level::Debug: return "DEBUG";
            case Level::Info:  return "INFO";
            case Level::Warn:  return "WARN";
            case Level::Error: return "ERROR";
            case Level::Fatal: return "FATAL";
            default:           return "OFF";
        }
    }

    static const char* levelToColor(Level lvl) {
        switch (lvl) {
            case Level::Trace: return "\033[90m"; // gray
            case Level::Debug: return "\033[36m"; // cyan
            case Level::Info:  return "\033[32m"; // green
            case Level::Warn:  return "\033[33m"; // yellow
            case Level::Error: return "\033[31m"; // red
            case Level::Fatal: return "\033[1;31m"; // bold red
            default:           return "\033[0m";
        }
    }

    std::mutex mutex_;
    Level global_level_ = Level::Info;
    std::unordered_map<std::string, Level> tag_levels_;
    std::unordered_set<std::string> disabled_tags_;
};

} // namespace logging
