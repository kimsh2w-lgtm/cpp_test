#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "base.h"
#include "thread_unit.hpp"

namespace worker {
    
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

    // 이동 허용
    Worker(Worker&& other) noexcept = default;
    Worker& operator=(Worker&& other) noexcept = default;

    Result<void> init(WorkerDescriptor desc);
    Result<void> start();
    Result<void> stop();

    // for loop type
    Result<void> pause();
    Result<void> resume();

    WorkerStatus status() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return {state_, type_, paused_, sleeping_, stop_requested_};
    };

protected:
    virtual void run() = 0;

    virtual Result<void> onPreStart() { return Ok(); }
    virtual void onPostStart() { }

    virtual void onPreStop() { }
    virtual void onPostStop() { }

    Result<void> wakeup();
    Result<void> sleep(int msec);

    /// Each derived Worker should override logTag() to provide a unique static name.
    /// Never return desc_.name.c_str() here (temporary string risk).
    virtual const char* logTag() const { return "Worker"; }

private:
    void threadSingleEntry();
    void threadLoopEntry();
    void resetFlags();
    
private:
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    bool sleeping_ = false;
    bool paused_  = false;
    bool stop_requested_ = false;
    WorkerState   state_;
    WorkerType    type_;

    task::ThreadUnit thread_;
    
    WorkerDescriptor desc_;
};


}
