#pragma once
#include <queue>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

#include "result.h"
#include "logging.hpp"
#include "worker.hpp"
#include "async_task.hpp"
#include "task_unit.hpp"

namespace task {

// ------------------------------------------------------
// 설정 정보
// ------------------------------------------------------
struct AsyncPoolDescriptor {
    size_t async_count = std::thread::hardware_concurrency(); // 동시 수행 가능한 async 개수
    size_t max_queue   = 128;
};


// ------------------------------------------------------
// 통계 정보
// ------------------------------------------------------
struct AsyncPoolStats {
    std::atomic<size_t> executed{0};
    std::atomic<size_t> failed{0};
    std::atomic<size_t> dropped{0};
    std::atomic<double> avg_exec_ms{0.0};
};


class AsyncPool  : public Worker {
public:
    explicit AsyncPool(const AsyncPoolDescriptor& desc) : desc_(desc) {
        WorkerDescriptor wd;
        wd.name = "AsyncPool";
        wd.type = WorkerType::Event;
        wd.loop_sleep_ms = 1;                // ignore

        auto r = Worker::init(wd);
        if (!r) {
            LOGE("TaskPool init failed: {}", r.error());
        }
    }

    ~AsyncPool() override {
        stop();
    }

public:
    Result<void> submit(const TaskDescriptor<void>& desc, int priority = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (tasks_.size() >= desc_.max_queue) {
            stats_.dropped++;
            return Error(ResultCode::ResourceBusy, "Task queue full");
        }
        if(desc.dispatch == TaskDispatchPolicy::Throttled) {
            auto cur = std::chrono::steady_clock::now();
            if(expired_tasks_.find(desc.name) != expired_tasks_.end()) {
                auto throttle = expired_tasks_[desc.name]
                        + std::chrono::milliseconds(desc.throttle_time_ms);
                if(cur < throttle) return Error(ResultCode::RateLimit, "throttling error");
            }

            expired_tasks_[desc.name] = cur;
        }
        tasks_.push({desc, priority});
        event();
        return OK();
    }


protected:
    Result<void> run() override {
        constexpr int MAX_RETRY           = 3;
        constexpr int RETRY_PRIORITY_BOOST = 10;
        constexpr int LOOP_SLEEP_MS       = 2;

        while (!isStopRequested()) {
            TaskItem task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (tasks_.empty()) break;
                task = std::move(tasks_.top());
                tasks_.pop();
            }

            bool assigned = false;

            for (size_t id : all_async_ids_) {
                auto it = asyncs_.find(id);
                if (it == asyncs_.end()) continue;

                auto& async_unit = it->second.async_;
                if (async_unit->isIdle()) {
                    auto res = async_unit->execute(task.desc);
                    if (res) {
                        assigned = true;
                        stats_.executed++;
                        break;
                    } else {
                        stats_.failed++;
                    }
                }
            }

            if (!assigned) {
                bool pushed = false;
                TaskItem boosted = task;

                for (int retry = 1; retry <= MAX_RETRY; ++retry) {
                    boosted.priority = task.priority + RETRY_PRIORITY_BOOST * retry;
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        if (tasks_.size() < desc_.max_queue) {
                            tasks_.push(boosted);
                            pushed = true;
                            LOGW("AsyncPool: Requeued task '{}' (retry {}/{}, boosted priority {})",
                                 task.desc.name, retry, MAX_RETRY, boosted.priority);
                            break;
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(LOOP_SLEEP_MS));
                }

                if (!pushed) {
                    stats_.dropped++;
                    LOGE("AsyncPool: Dropped task '{}' after {} push retries",
                         task.desc.name, MAX_RETRY);
                }
            }

            {
                bool has_more = false;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    has_more = !tasks_.empty();
                }
                if (has_more)
                    std::this_thread::sleep_for(std::chrono::milliseconds(LOOP_SLEEP_MS));
            }
        }

        return OK();
    }


    Result<void> onPreStart() override {
        std::lock_guard<std::mutex> lock(mutex_);

        asyncs_.clear();
        all_async_ids_.clear();

        // 코어 개수 계산
        size_t total_async = desc_.async_count ? desc_.async_count
                                               : std::thread::hardware_concurrency();

        LOGI("AsyncPool config: total_async={}, max_queue={}",
             total_async, desc_.max_queue);

        for (size_t i = 0; i < total_async; ++i) {
            auto async_unit = std::make_unique<task::AsyncTask<void>>();
            auto res = async_unit->init();
            if (!res) {
                asyncs_.clear();
                all_async_ids_.clear();
                return res;
            }

            asyncs_[i] = {i, std::move(async_unit)};
            all_async_ids_.push_back(i);
        }

        return OK();
    }

    void onPostStop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        asyncs_.clear();
        all_async_ids_.clear();
        while (!tasks_.empty()) tasks_.pop();
    }

private:
    struct TaskItem {
        TaskDescriptor<void> desc;
        int priority;
        std::chrono::steady_clock::time_point enqueue_time{
            std::chrono::steady_clock::now()
        };

        bool operator<(const TaskItem& other) const {
            if (priority != other.priority) return priority < other.priority;
            return enqueue_time > other.enqueue_time; // 동일 우선순위 → FIFO
        }
    };

    struct AsyncItem {
        size_t id_;
        std::unique_ptr<task::AsyncTask<void>> async_;
    };


    AsyncPoolDescriptor desc_;

    mutable std::mutex mutex_;
    std::priority_queue<TaskItem> tasks_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> expired_tasks_;

    std::unordered_map<size_t, AsyncItem> asyncs_; // key - index, value - async task
    std::vector<size_t> all_async_ids_;

    AsyncPoolStats stats_;

    const char* LOG_TAG = "AsyncPool";
};



} // namespace task
