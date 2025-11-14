#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <fmt/ostream.h>
#include <unordered_map>

#include "result.h"
#include "logging.hpp"
#include "thread_task.hpp"

namespace task {
    
enum class WorkerState{
    Init,
    Ready,
    Running,
    Stopping,
    Stopped,
};

enum class WorkerType{
    Single,
    Loop,
    Event,
};

struct WorkerDescriptor {
    std::string name;
    std::vector<int> affinity;
    int policy = 0;
    int priority = 0;
    WorkerType type = WorkerType::Single;
    int loop_sleep_ms = 1000;
};

struct WorkerStatus {
    WorkerState state;
    WorkerType type;
    bool paused;
    bool sleeping;
    bool stop_requested;
};



class Worker
{
public:
    Worker() : state_(WorkerState::Init), type_(WorkerType::Single) { }
    virtual ~Worker();

    // 복사 대입 금지 
    Worker(const Worker&)            = delete;
    Worker& operator=(const Worker&) = delete;
    Worker(Worker&& other) noexcept = delete;
    Worker& operator=(Worker&& other) noexcept = delete;

    Result<void> init(WorkerDescriptor desc);
    Result<void> start();
    Result<void> stop();

    // for loop type
    Result<void> pause();
    Result<void> resume();
    Result<void> wakeup();
    Result<void> sleep(int msec);

    // for event type
    Result<void> event();

    bool isInitialized() const noexcept {
        return status().state == WorkerState::Ready ||
            status().state == WorkerState::Running;
    }

    bool isStopRequested() const noexcept {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        return stop_requested_;
    }

    WorkerStatus status() const noexcept {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        return {state_, type_, paused_, sleeping_, stop_requested_};
    };

protected:
    virtual Result<void> run() = 0;
    virtual void onCompleted(Result<void> result) { }; 

    virtual Result<void> onPreStart() { return OK(); }
    virtual void onPostStart() { }

    virtual void onPreStop() { }
    virtual void onPostStop() { }


    static constexpr const char* LOG_TAG = "Worker";

private:
    Result<void> threadSingleEntry();
    Result<void> threadLoopEntry();
    Result<void> threadEventEntry();
    void resetFlags();
    const char* logTag() const {
        auto [it, inserted] = tag_cache_.try_emplace(
        this, fmt::format("{}#{:016x}", LOG_TAG, thread_.id()));
        return it->second.c_str();
    }
    void clearTagCache() const {
        tag_cache_.erase(this);
    }

private:
    static thread_local std::unordered_map<const Worker*, std::string> tag_cache_;

    mutable std::mutex worker_mutex_;
    mutable std::mutex event_mutex_;

    std::condition_variable cond_;
    std::condition_variable cond_event_;
    bool event_ = false;
    bool sleeping_ = false;
    bool paused_  = false;
    bool stop_requested_ = false;
    WorkerState   state_;
    WorkerType    type_;

    task::ThreadTask<void> thread_;
    WorkerDescriptor desc_;
};


}
