#pragma once
#include <future>
#include <atomic>
#include <unordered_map>
#include <string>
#include <optional>
#include <fmt/ostream.h>
#include <cxxabi.h>
#include <random>
#include <sstream>
#include <iomanip>

#include "result.h"
#include "helper.hpp"
#include "task_unit.hpp"

namespace task {

template<typename T>
class AsyncTask : virtual public ResultTaskUnit<T> {
public:
    AsyncTask() { 
        id_ = global_id_counter_.fetch_add(1, std::memory_order_relaxed);
        if (id_ == UINT64_MAX) global_id_counter_.store(0);
    }
    ~AsyncTask() {
        stop();
        if (future_.valid()) {
            future_.wait();
        }
        clearTagCache();
    }

    TaskExcutionMode excutionMode() const noexcept override { return TaskExcutionMode::Async; }

    Result<void> init() override {
        stop_.store(false, std::memory_order_relaxed);
        running_.store(false, std::memory_order_relaxed);
        has_task_.store(false, std::memory_order_relaxed);
        if (future_.valid()) future_.wait(); // async 종료 보장
        clearTagCache();
        return OK();
    }

    Result<T> result() override {
        if (future_.valid())
            return future_.get();
        return Error(ResultCode::InvalidState, "no future result");
    }

    Result<void> execute(TaskDescriptor<T> desc) override {
        if (stop_.load(std::memory_order_relaxed)) return Fail();
        if (!desc.func) return Error(ResultCode::InvalidArgument, "Invalid func");

        if (has_task_.exchange(true) || (future_.valid() && future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)) {
            return Error(ResultCode::ResourceBusy, "Already running async task");
        }
        
        desc_ = std::move(desc);
        running_.store(true, std::memory_order_relaxed);

        try {
            future_ = std::async(std::launch::async, [this]() -> Result<T> {
                Result<T> res;
                try {
                    res = desc_.func();
                } catch (const std::exception& e) {
                    LOG_ERROR(logTag(), "Unhandled exception: {}", e.what());
                    res = Fail();
                } catch (...) {
                    LOG_ERROR(logTag(), "Unknown exception in async");
                    res = Fail();
                }

                if (desc_.on_complete)
                    desc_.on_complete(res);

                running_.store(false, std::memory_order_relaxed);
                has_task_.store(false, std::memory_order_relaxed);
                return res;
            });
        } catch (const std::exception& e) {
            LOG_ERROR(logTag(), "std::async failed: {}", e.what());
            running_.store(false, std::memory_order_relaxed);
            has_task_.store(false, std::memory_order_relaxed);
            return Fail();
        }

        return OK();
    }


    Result<void> stop() noexcept override {
        stop_.store(true, std::memory_order_seq_cst);
        clearTagCache();
        return OK();
    }

    bool isStop() const noexcept override { return stop_.load(std::memory_order_relaxed); }
    bool isRunning() const noexcept override { return running_.load(std::memory_order_relaxed); }
    bool isIdle() const noexcept override { return !has_task_.load(std::memory_order_relaxed); }

    Result<void> wait(int msec = -1) override {
        if (!future_.valid()) return OK();
        if (msec < 0) {
            future_.wait();
            return OK();
        }
        auto st = future_.wait_for(std::chrono::milliseconds(msec));
        return (st == std::future_status::ready) ? OK()
                                             : Error(ResultCode::Timeout, "async wait timeout");
    }

    Result<void> join() override {
        if (future_.valid()) future_.wait();
        return OK();
    }

    Result<void> detach() override {
        // std::future는 detach 지원 안함 → no-op
        return OK();
    }

    Result<void> setAffinity(const std::vector<int>&) override {
        // std::async는 pthread 핸들 접근 불가 → no-op
        return Error(ResultCode::NotSupported, "AsyncTask does not support affinity");
    }

    std::size_t id() const override {
        return id_;
    }

    int getPolicy() const override { return 0; }
    int getPriority() const override { return 0; }

protected:
    static constexpr const char* LOG_TAG = "AsyncTask";

private:
    const char* logTag() const {
        auto [it, inserted] = tag_cache_.try_emplace(
        this, fmt::format("{}<{}>#{:016x}", LOG_TAG, 
            demangle(typeid(T).name()),
            static_cast<unsigned long long>(id_)));
        return it->second.c_str();
    }

    void clearTagCache() const {
        tag_cache_.erase(this);
    }

private:
    inline static thread_local std::unordered_map<const AsyncTask<T>*, std::string> tag_cache_;
    inline static std::atomic<uint64_t> global_id_counter_{0};
    uint64_t id_;

    std::atomic<bool> stop_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> has_task_{false};

    TaskDescriptor<T> desc_;
    std::future<Result<T>> future_;
};

} // namespace task
