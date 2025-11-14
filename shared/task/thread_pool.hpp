#pragma once
#include <queue>
#include <unordered_map>
#include <condition_variable>
#include <atomic>

#include "result.h"
#include "logging.hpp"
#include "worker.hpp"
#include "thread_task.hpp"
#include "task_unit.hpp"

// NOTE
// TaskPool little_pool({4, {0,1,2,3}});   // A76 cluster cores
// TaskPool big_pool({4, {4,5,6,7}}); // A55 cluster cores


namespace task {

struct ThreadPoolDescriptor {
    size_t thread_count = std::thread::hardware_concurrency();
    std::vector<int> core_affinity;
    size_t max_queue = 128;
};

// ------------------------------------------------------
// 통계 정보
// ------------------------------------------------------
struct TaskPoolStats {
    std::atomic<size_t> executed{0};
    std::atomic<size_t> failed{0};
    std::atomic<size_t> dropped{0};
    std::atomic<double> avg_exec_ms{0.0};
};


class ThreadPool : public Worker {
public:
    explicit ThreadPool(const ThreadPoolDescriptor& desc) : desc_(desc) {
        WorkerDescriptor wd;
        wd.name = "ThreadPool";
        wd.type = WorkerType::Event;
        wd.loop_sleep_ms = 1;                // ignore

        auto r = Worker::init(wd);
        if (!r) {
            LOGE("ThreadPool init failed: {}", r.error());
        }
    }

    ~ThreadPool() override {
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
        constexpr int MAX_RETRY = 3;
        constexpr int RETRY_PRIORITY_BOOST = 10;
        constexpr int LOOP_SLEEP_MS = 2;

        while (!isStopRequested()) {     
            TaskItem task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (tasks_.empty()) break;
                task = std::move(tasks_.top());
                tasks_.pop();
            }

            bool assigned = false;
            std::vector<size_t> candidates;

            // 1. affinity 검사
            if (!task.desc.affinity.empty()) {
                selectCandidates(task.desc.affinity, candidates);
            } else {
                candidates = all_thread_ids_; // 전체 스레드 후보
            }

            // 2. idle 상태 스레드 찾기
            for (size_t id : candidates) {
                auto it = threads_.find(id);
                if (it == threads_.end()) continue;

                auto& thread = it->second.thread_;
                if (thread->isIdle()) {
                    auto res = thread->execute(task.desc);
                    if (res) {
                        assigned = true;
                        stats_.executed++;
                        
                        break;
                    } else {
                        stats_.failed++;
                    }
                }
            }

            // 4️ 배정 실패 시 재시도 or drop
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
                            LOGW("TaskPool: Requeued task '{}' (retry {}/{}, boosted priority {})",
                                task.desc.name, retry, MAX_RETRY, boosted.priority);
                            break;
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(LOOP_SLEEP_MS));
                }

                if (!pushed) {
                    stats_.dropped++;
                    LOGE("TaskPool: Dropped task '{}' after {} push retries", task.desc.name, MAX_RETRY);
                }
            }

            // 5 남은 작업 있으면 잠깐 sleep
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

    // --------------------------
    // 스레드 생성 / affinity pinning
    // --------------------------
    Result<void> onPreStart() override {
        std::lock_guard<std::mutex> lock(mutex_);

        threads_.clear();
        core_to_threads_.clear();
        all_thread_ids_.clear();  

        // 코어 개수 계산
        size_t core_count = desc_.core_affinity.empty()
            ? std::thread::hardware_concurrency()
            : desc_.core_affinity.size();

        // 의도: 사용자가 명시한 thread_count를 "총 스레드 수"로 신뢰
        // 단, 0이면 최소 core_count로 보정
        size_t total_threads = desc_.thread_count ? desc_.thread_count : core_count;

        LOGI("Thread config: requested_total={}, core_count={}, affinity_listed={}",
                 total_threads, core_count, desc_.core_affinity.empty() ? 0 : desc_.core_affinity.size());

        for (size_t i = 0; i < total_threads; ++i) {
            auto thread_unit = std::make_unique<task::ThreadTask<void>>();
            auto res = thread_unit->init();
            if (!res) {
                threads_.clear();
                core_to_threads_.clear();
                all_thread_ids_.clear();
                return res;
            }

            // affinity round-robin pinning (선택적)
            int pinned_core = 0;
            if (!desc_.core_affinity.empty()) {
                int core = desc_.core_affinity[i % desc_.core_affinity.size()];
                auto set_res = thread_unit->setAffinity({core});
                if (!set_res) {
                    LOGW("Failed to set affinity for thread core{}({}) msg={}",
                         core, i, set_res.error());
                } else {
                    pinned_core = core;
                    core_to_threads_[core].insert(i);
                }
            }
            threads_[i] = {pinned_core, i, std::move(thread_unit)};
            all_thread_ids_.push_back(i);
        }

        return OK();
    }

    void onPostStop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        threads_.clear();
        core_to_threads_.clear();
        all_thread_ids_.clear();
        while (!tasks_.empty()) tasks_.pop();
    }
private:
    void selectCandidates(const std::vector<int>& affinity, std::vector<size_t>& out) {
        out.clear();
        if (affinity.empty()) {
            out = all_thread_ids_;
            return;
        }
        std::set<size_t> uniq;
        for (int core : affinity) {
            auto it = core_to_threads_.find(core);
            if (it == core_to_threads_.end()) continue;
            for (const auto& id : it->second) uniq.insert(id);
        }
        out.assign(uniq.begin(), uniq.end());
        if (out.empty()) {
            out = all_thread_ids_;
        }
    }

private:
    struct TaskItem {
        TaskDescriptor<void> desc;
        int priority;
        std::chrono::steady_clock::time_point enqueue_time{std::chrono::steady_clock::now()};

        bool operator<(const TaskItem& other) const {
            if (priority != other.priority) return priority < other.priority;
            return enqueue_time > other.enqueue_time; // FIFO 유지
        }
    };

    struct ThreadItem {
        int core_;
        size_t id_;
        std::unique_ptr<task::ThreadTask<void>> thread_;
    };

    ThreadPoolDescriptor desc_;

    mutable std::mutex mutex_;
    std::priority_queue<TaskItem> tasks_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> expired_tasks_;

    std::unordered_map<int, std::set<size_t>> core_to_threads_; // core별  thread index 모음
    std::unordered_map<size_t, ThreadItem> threads_; // key - index, value - thread

    std::vector<size_t> all_thread_ids_;
    TaskPoolStats stats_;

    const char* LOG_TAG = "TaskPool";
};



} // namespace task
