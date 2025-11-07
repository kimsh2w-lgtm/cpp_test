#pragma once
#include <thread>
#include <atomic>
#include <functional>
#include <utility>
#include <vector>
#include <string>
#include <stdexcept>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <optional>
#include <mutex>
#include <condition_variable>
#include <algorithm>

#include "base.h"
#include "task_unit.hpp"

namespace task {
    
class ThreadUnit : virtual public TaskUnit {
public:
    ThreadUnit() = default;

    ThreadUnit(ThreadUnit&& other) noexcept { moveFrom(std::move(other)); }
    ThreadUnit& operator=(ThreadUnit&& other) noexcept {
        if (this != &other) {
            if (thread_.joinable()) thread_.join();
            moveFrom(std::move(other));
        }
        return *this;
    }

    ~ThreadUnit() {
        stop();
        // RAII: join 안 하면 파괴 시 join (데드락 주의: 자신 스레드에서 소멸 금지)
        if (thread_.joinable() && std::this_thread::get_id() != thread_.get_id()) {
            thread_.join();
        }
    }

    TaskType taskType() const noexcept override { return TaskType::Thread; }

    Result<void> init() override; 
    Result<void> execute(TaskDescriptor desc) override;
    Result<void> stop() noexcept override { 
        stop_.store(true, std::memory_order_relaxed);
        cond_.notify_all();
        return Ok();
    }
    bool isStop() const noexcept override { return stop_.load(std::memory_order_relaxed); }
    bool isRunning() const noexcept override { return running_.load(std::memory_order_relaxed); }
    bool isIdle() const noexcept override { return !has_task_.load(std::memory_order_relaxed); }

    Result<void> join() override { if (thread_.joinable()) thread_.join(); return Ok(); }
    Result<void> detach() override { if (thread_.joinable()) thread_.detach(); return Ok(); }

    Result<void> setAffinity(const std::vector<int>& cores) override;

    std::vector<int> getAffinity() const {
        if (!cur_affinity_.has_value()) return {};
        auto v = *cur_affinity_;
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
        return v;
    };
    int getPolicy() const override { return cur_policy_.value_or(0); }
    int getPriority() const override { return cur_priority_.value_or(0); }

private:
    void moveFrom(ThreadUnit&& other) {
        thread_ = std::move(other.thread_);
        stop_.store(other.stop_.load(std::memory_order_relaxed));
        running_.store(other.running_.load(std::memory_order_relaxed));
        has_task_.store(other.has_task_.load(std::memory_order_relaxed));

        cur_affinity_   = std::move(other.cur_affinity_);
        cur_policy_     = std::move(other.cur_policy_);
        cur_priority_   = std::move(other.cur_priority_);
        cur_name_       = std::move(other.cur_name_);
        desired_affinity_ = std::move(other.desired_affinity_);
        desired_policy_   = std::move(other.desired_policy_);
        desired_priority_ = std::move(other.desired_priority_);
        desired_name_     = std::move(other.desired_name_);

         // moved-from 객체 비활성화
        other.running_.store(false, std::memory_order_relaxed);
        other.stop_.store(true, std::memory_order_relaxed); 
    }

    void applyThreadAttributesIfDirty();
    void loop();

private:
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> has_task_{false};

    TaskDescriptor desc_;

    std::mutex task_mutex_;
    std::condition_variable cond_;

    // 적용 상태(성공한 값)
    std::optional<std::vector<int>> cur_affinity_;
    std::optional<int>              cur_policy_;
    std::optional<int>              cur_priority_;
    std::optional<std::string>      cur_name_;

    // 원하는 상태(최근 desc)
    std::optional<std::vector<int>> desired_affinity_;
    std::optional<int>              desired_policy_;
    std::optional<int>              desired_priority_;
    std::optional<std::string>      desired_name_;

    // 바뀌었는지 표시 (적용 필요)
    bool dirty_affinity_ = false;
    bool dirty_sched_    = false; // policy/priority
    bool dirty_name_     = false;
};


}
