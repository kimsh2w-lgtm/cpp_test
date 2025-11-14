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
#include <fmt/ostream.h>

#include "result.h"
#include "task_unit.hpp"

namespace task {
    
template<typename T>
class ThreadTask : virtual public ResultTaskUnit<T> {
public:
    ThreadTask() = default;

    ~ThreadTask() {
        stop();
        clearTagCache();
        if (thread_.joinable() && std::this_thread::get_id() != thread_.get_id()) {
            thread_.join();
        }
    }

    TaskExcutionMode excutionMode() const noexcept override { return TaskExcutionMode::Thread; }

    Result<void> init() override {
        if (thread_.joinable()) return Fail();

        stop_.store(false, std::memory_order_relaxed);
        has_task_.store(false, std::memory_order_relaxed);

        try {
            thread_ = std::thread([this]() { loop(); });
        } catch (const std::exception& e) {
            LOG_ERROR(logTag(), "init: unhandled exception: {}", e.what());
            return Fail();
        } catch (...) {
            LOG_ERROR(logTag(), "init: Unknown exception in thread");
            return Fail();
        }
        return OK();
    }

    Result<void> execute(TaskDescriptor<T> desc) override {
        LOG_DEBUG(logTag(), "execute");
        if (stop_.load(std::memory_order_relaxed) || !thread_.joinable()) return Fail();
        if(!desc.func) return Fail();

        {
            std::lock_guard<std::mutex> lock(task_mutex_);
            if (has_task_.load(std::memory_order_relaxed)) {
                return Fail();
            }

            desc_ = std::move(desc);
            has_task_.store(true, std::memory_order_relaxed);
            
            // 2) 변경 감지 (optional 비교)
            desired_name_     = desc_.name.empty() ? std::optional<std::string>{} : std::make_optional(desc_.name);
            desired_affinity_ = desc_.affinity.empty() ? std::optional<std::vector<int>>{} : std::make_optional(desc_.affinity);
            desired_policy_   = (desc_.policy   != 0) ? std::make_optional(desc_.policy)   : std::optional<int>{};
            desired_priority_ = (desc_.priority != 0) ? std::make_optional(desc_.priority) : std::optional<int>{};

            // 이름
            if (desired_name_ != cur_name_) {
                dirty_name_ = true;
            }
            // affinity (벡터 비교)
            if (desired_affinity_ != cur_affinity_) {
                dirty_affinity_ = true;
            }
            // 스케줄링(둘 중 하나라도 변경되면 세트로 적용)
            if (desired_policy_ != cur_policy_ || desired_priority_ != cur_priority_) {
                dirty_sched_ = true;
            }
        }
        
        applyThreadAttributesIfDirty();
        cond_.notify_one();
        return OK();
    }

    Result<void> stop() noexcept override { 
        LOG_DEBUG(logTag(), "stop");
        stop_.store(true, std::memory_order_seq_cst);
        cond_task_.notify_all();
        cond_.notify_all();
        clearTagCache();
        return OK();
    }

    Result<T> result() override {
        std::lock_guard<std::mutex> lock(result_mutex_);
        return last_result_;
    }

    bool isStop() const noexcept override { return stop_.load(std::memory_order_relaxed); }
    bool isRunning() const noexcept override { return running_.load(std::memory_order_relaxed); }
    bool isIdle() const noexcept override { return !has_task_.load(std::memory_order_relaxed); }

    Result<void> wait(int msec = -1) override {
        std::unique_lock<std::mutex> lock(task_mutex_);
        if (!task_running_.load(std::memory_order_relaxed)) return OK();

        bool ok = false;
        if(msec < 0) {
            cond_task_.wait(lock, [this]() {
                return !task_running_.load(std::memory_order_relaxed);
            });
            ok = true;
        } else {
            ok  = cond_task_.wait_for(lock, std::chrono::milliseconds(msec), [this]() {
            return !task_running_.load(std::memory_order_relaxed);
        });
        }

        return ok ? OK() : Error(ResultCode::Timeout, "thread wait timeout");
    }

    Result<void> join() override { if (thread_.joinable()) thread_.join(); return OK(); }
    Result<void> detach() override { if (thread_.joinable()) thread_.detach(); return OK(); }

    Result<void> setAffinity(const std::vector<int>& cores) override {;
        if (!thread_.joinable()) return Fail();

        pthread_t handle = thread_.native_handle();

        // 빈 벡터 → affinity 해제 요청으로 해석하거나 변경 없음 처리
        std::optional<std::vector<int>> new_affinity =
            cores.empty() ? std::optional<std::vector<int>>{} : std::make_optional(cores);

        // 변경되지 않았다면 시스템 호출 생략
        if (new_affinity == cur_affinity_) {
            return OK();  // 이미 동일하므로 재설정 불필요
        }

        // 새 affinity 적용 시도
        if (new_affinity.has_value()) {
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            for (int core : *new_affinity) {
                if (core >= 0)
                    CPU_SET((unsigned)core, &cpuset);
            }

            int rc = ::pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
            if (rc == 0) {
                cur_affinity_ = new_affinity;  // 성공 → 현재 값 갱신
                dirty_affinity_ = false;
                return OK();
            } else {
                // 실패 시 dirty 플래그 남김 (재시도 가능)
                dirty_affinity_ = true;
                desired_affinity_ = new_affinity;
                return Fail();
            }
        } else {
            // new_affinity가 nullopt라면 해제 요청 또는 유지 정책
            // (필요에 따라 기본 affinity 복원 로직 추가 가능)
            cur_affinity_.reset();
            return OK();
        }
    }

    std::size_t id() const override {
        return thread_.joinable()
            ? std::hash<std::thread::id>{}(thread_.get_id())
            : 0; // joinable 아니면 0 등 기본값
    }
    std::vector<int> getAffinity() const {
        if (!cur_affinity_.has_value()) return {};
        auto v = *cur_affinity_;
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
        return v;
    };
    int getPolicy() const override { return cur_policy_.value_or(0); }
    int getPriority() const override { return cur_priority_.value_or(0); }

protected:
    static constexpr const char* LOG_TAG = "ThreadTask";

private:
    void applyThreadAttributesIfDirty() {
        std::lock_guard<std::mutex> lock(task_mutex_);
        if (!thread_.joinable()) return;
        pthread_t handle = thread_.native_handle();

        // name
        if (dirty_name_) {
            if (desired_name_.has_value()) {
                std::string truncated = desired_name_->substr(0, 15);
                int rc = ::pthread_setname_np(handle, truncated.c_str());
                if (rc == 0) { cur_name_ = truncated; dirty_name_ = false; }
                else { LOG_WARN(logTag(), "pthread_setname_np failed: {}", strerror(errno)); }
                // 실패 시 dirty 유지 → 다음 기회에 재시도
            } else {
                // 미지정(nullopt)이면 변경 안 함 (필요 시 이름 초기화 정책 추가 가능)
                dirty_name_ = false;
            }
        }

        // affinity
        if (dirty_affinity_) {
            if (desired_affinity_.has_value()) {
                cpu_set_t cpuset; CPU_ZERO(&cpuset);
                for (int c : *desired_affinity_) if (c >= 0) CPU_SET((unsigned)c, &cpuset);
                int rc = ::pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
                if (rc == 0) { cur_affinity_ = desired_affinity_; dirty_affinity_ = false; }
                else { LOG_WARN(logTag(), "pthread_setaffinity_np failed: {}", strerror(errno)); }
            } else {
                // nullopt → 변경 없음 (혹은 기본으로 해제 로직을 원하면 여기 구현)
                dirty_affinity_ = false;
            }
        }

        // policy / priority 
        if (dirty_sched_) {
            if (desired_policy_.has_value() || desired_priority_.has_value()) {
                int policy = desired_policy_.value_or(SCHED_OTHER);
                int prio   = desired_priority_.value_or(0);
                sched_param sp{}; sp.sched_priority = prio;
                int rc = ::pthread_setschedparam(handle, policy, &sp);
                if (rc == 0) { cur_policy_ = policy; cur_priority_ = prio; dirty_sched_ = false; }
                else { LOG_WARN(logTag(), "pthread_setschedparam failed: {}", strerror(errno)); }
                // 권한 없으면 실패 → dirty 유지(다음에 재시도)
            } else {
                // nullopt → 변경 없음 (원한다면 기본값으로 되돌리기 구현)
                dirty_sched_ = false;
            }
        }
    }

    void loop() {
        running_.store(true, std::memory_order_relaxed);
        while (!stop_.load(std::memory_order_relaxed)) {
            TaskDescriptor<T> task;
            {
                std::unique_lock<std::mutex> lock(task_mutex_);
                cond_.wait(lock, [this]() {
                    return stop_.load(std::memory_order_relaxed) || has_task_.load(std::memory_order_relaxed);
                });
                if (stop_.load(std::memory_order_relaxed)) break;

                task = std::move(desc_);
                has_task_.store(false, std::memory_order_relaxed);
                task_running_.store(true, std::memory_order_relaxed); 
            }
            
            if (task.func) {
                Result<T> result;
                try {
                    result = task.func();
                } catch (const std::exception& e) {
                    LOG_ERROR(logTag(), "Unhandled exception: {}", e.what());
                    result = Fail();
                } catch (...) {
                    LOG_ERROR(logTag(), "Unknown exception in thread");
                    result = Fail();
                }
                {
                    std::lock_guard<std::mutex> lock(result_mutex_);
                    last_result_ = result;
                }

                if (task.on_complete)
                    task.on_complete(result);
            }
            {
                std::lock_guard<std::mutex> lock(task_mutex_);
                task_running_.store(false, std::memory_order_relaxed);
                has_task_.store(false, std::memory_order_relaxed);
                cond_task_.notify_all();
            }
        }
        running_.store(false, std::memory_order_relaxed);
        cond_task_.notify_all();
    }

    const char* logTag() const {
        auto [it, inserted] = tag_cache_.try_emplace(
        this, fmt::format("{}#{:016x}", LOG_TAG, id()));
        return it->second.c_str();
    }
    void clearTagCache() const {
        tag_cache_.erase(this);
    }

private:
    inline static thread_local std::unordered_map<const ThreadTask*, std::string> tag_cache_;
    std::mutex result_mutex_;
    Result<T> last_result_;

    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> has_task_{false};
    std::atomic<bool> task_running_{false}; 

    TaskDescriptor<T> desc_;

    std::mutex task_mutex_;
    std::condition_variable cond_;
    std::condition_variable cond_task_;

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
