
#include "base.h"
#include "worker.hpp"


namespace worker {

using namespace logging;

Worker::~Worker() {
    stop();
}

Result<void> Worker::init(WorkerDescriptor desc)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != WorkerState::Init && state_ != WorkerState::Stopped)
            return Error("Worker already initialized");
        
        desc_ = desc;
        type_ = desc.type;
        state_ = WorkerState::Ready;
    }    
    resetFlags();
    return Ok();
}

Result<void> Worker::start()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ == WorkerState::Running)
            return Fail();
    }

    auto pre = onPreStart();
    if (pre.hasError())
        return pre;
    
    auto r = thread_.init();
    if (!r) return r;

    task::TaskDescriptor td;
    td.name     = desc_.name;
    td.affinity = desc_.affinity;
    td.policy   = desc_.policy;
    td.priority = desc_.priority;
    if(type_ == WorkerType::Loop)
        td.func     = [this]() { threadLoopEntry(); };
    else 
        td.func     = [this]() { threadSingleEntry(); };
    
    auto exec_result = thread_.execute(td);
    if (!exec_result) return exec_result;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = WorkerState::Running;
        cond_.notify_all();
    }
    onPostStart();

    return Ok();
}

Result<void> Worker::stop()
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (state_ != WorkerState::Running && state_ != WorkerState::Stopping)
        return Ok();

    lock.unlock();
    onPreStop();

    lock.lock();
    stop_requested_ = true;
    paused_ = false;
    sleeping_ = false;
    state_ = WorkerState::Stopping;
    cond_.notify_all();

    lock.unlock();
    thread_.stop();
    thread_.join();

    lock.lock();
    if (state_ != WorkerState::Stopped)
        state_ = WorkerState::Stopped;
    lock.unlock();
    resetFlags();
    onPostStop();

    return Ok();
}

// 
Result<void> Worker::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (desc_.type != WorkerType::Loop)
        return Error("pause() only available in Loop type Worker");
    paused_ = true;
    return Ok();
}

Result<void> Worker::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (desc_.type != WorkerType::Loop)
        return Error("resume() only available in Loop type Worker");
    paused_  = false;
    cond_.notify_all();
    return Ok();
}

Result<void> Worker::sleep(int msec)
{
    std::unique_lock<std::mutex> lock(mutex_);
    sleeping_ = true;
    cond_.wait_for(lock, std::chrono::milliseconds(msec), [this]() { return !sleeping_ || stop_requested_; });
    sleeping_  = false;
    return Ok();
}

Result<void> Worker::wakeup()
{
    std::lock_guard<std::mutex> lock(mutex_);
    sleeping_ = false;
    cond_.notify_all();
    return Ok();
}

void Worker::threadLoopEntry() {
    try {
        { // wait
            std::unique_lock<std::mutex> lock(mutex_);
            cond_.wait(lock, [this]() { return state_ == WorkerState::Running || stop_requested_; });
            if (stop_requested_) return; 
        }
        while (true) {
            { // pause wait
                std::unique_lock<std::mutex> lock(mutex_);
                cond_.wait(lock, [this]() { return !paused_ || stop_requested_; });
                if (stop_requested_ || state_ != WorkerState::Running) break;
            }
            run();
            if (stop_requested_) break;
            sleep(desc_.loop_sleep_ms);
        }
    } catch (const std::exception& e) {
        LOG_ERROR(logTag(), "Worker[{}] exception: {}", desc_.name, e.what());
    } catch (...) {
        LOG_ERROR(logTag(), "Worker[{}] unknown exception occurred", desc_.name);
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_requested_ = true;
        state_ = WorkerState::Stopped;
    }
}

void Worker::threadSingleEntry() {
    try {
        { // wait
            std::unique_lock<std::mutex> lock(mutex_);
            cond_.wait(lock, [this]() { return state_ == WorkerState::Running || stop_requested_; });
            if (stop_requested_) return; 
        }
        run();
    } catch (const std::exception& e) {
        LOG_ERROR(logTag(), "Worker[{}] exception: {}", desc_.name, e.what());
    } catch (...) {
        LOG_ERROR(logTag(), "Worker[{}] unknown exception occurred", desc_.name);
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_requested_ = true;
        state_ = WorkerState::Stopped;
    }
}

void Worker::resetFlags() {
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = false;
    sleeping_ = false;
    stop_requested_ = false;
}


} // namespace worker