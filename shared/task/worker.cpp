
#include "result.h"
#include "logging.hpp"
#include "worker.hpp"


namespace task {

using namespace logging;

thread_local std::unordered_map<const Worker*, std::string> Worker::tag_cache_;

Worker::~Worker() {
    stop();
    clearTagCache();
}

Result<void> Worker::init(WorkerDescriptor desc)
{
    LOG_DEBUG(logTag(), "init");
    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        if (state_ != WorkerState::Init && state_ != WorkerState::Stopped)
            return Error(ResultCode::AlreadyExists, "already initialized");
        
        desc_ = desc;
        type_ = desc.type;
        state_ = WorkerState::Ready;
    }    
    resetFlags();

    auto r = thread_.init();
    if (!r) {
        thread_.stop();
        return r;
    }
    clearTagCache();

    return OK();
}

Result<void> Worker::start()
{
    LOG_DEBUG(logTag(), "start");
    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        
        if (state_ == WorkerState::Running)
            return Fail();
    }

    auto pre = onPreStart();
    if (pre.hasError())
        return pre;
    
    task::TaskDescriptor td;
    td.name     = desc_.name;
    td.affinity = desc_.affinity;
    td.policy   = desc_.policy;
    td.priority = desc_.priority;
    if(type_ == WorkerType::Loop)
        td.func     = [this]() { return threadLoopEntry(); };
    else if(type_ == WorkerType::Event)
        td.func     = [this]() { return threadEventEntry(); };
    else 
        td.func     = [this]() { return threadSingleEntry(); };
    
    auto exec_result = thread_.execute(td);
    if (!exec_result) return exec_result;
    
    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        state_ = WorkerState::Running;
        cond_.notify_all();
    }
    onPostStart();

    return OK();
}

Result<void> Worker::stop()
{
    LOG_DEBUG(logTag(), "stop");
    {
        std::unique_lock<std::mutex> lock(worker_mutex_);
        if (state_ != WorkerState::Running && state_ != WorkerState::Stopping)
            return OK();

        state_ = WorkerState::Stopping;
        stop_requested_ = true;
        paused_ = false;
        sleeping_ = false;
        cond_.notify_all();
    }
    LOG_DEBUG(logTag(), "stopping...");
    onPreStop();
    try {
        thread_.stop();
        thread_.join();
    } catch (const std::exception& e) {
        LOG_ERROR(logTag(), "Exception during thread join: {}", e.what());
    }
    LOG_DEBUG(logTag(), "stopped.");
    {
        std::unique_lock<std::mutex> lock(worker_mutex_);
        state_ = WorkerState::Stopped;
    }
    
    resetFlags();
    onPostStop();
    clearTagCache();
    return OK();
}

// 
Result<void> Worker::pause() {
    LOG_DEBUG(logTag(), "pause");
    std::lock_guard<std::mutex> lock(worker_mutex_);
    if (desc_.type != WorkerType::Loop)
        return Error(ResultCode::NotSupported, "pause() only available in Loop type Worker");
    paused_ = true;
    return OK();
}

Result<void> Worker::resume() {
    LOG_DEBUG(logTag(), "resume");
    std::lock_guard<std::mutex> lock(worker_mutex_);
    if (desc_.type != WorkerType::Loop)
        return Error(ResultCode::NotSupported, "resume() only available in Loop type Worker");
    paused_  = false;
    cond_.notify_all();
    return OK();
}

Result<void> Worker::sleep(int msec)
{
    std::unique_lock<std::mutex> lock(worker_mutex_);
    sleeping_ = true;
    cond_.wait_for(lock, std::chrono::milliseconds(msec), [this]() { return !sleeping_ || stop_requested_; });
    sleeping_  = false;
    return OK();
}

Result<void> Worker::wakeup()
{
    std::lock_guard<std::mutex> lock(worker_mutex_);
    sleeping_ = false;
    cond_.notify_all();
    return OK();
}

Result<void> Worker::event()
{
    std::lock_guard<std::mutex> lock(event_mutex_);
    event_ = true;
    cond_event_.notify_all();
    return OK();
}

Result<void> Worker::threadLoopEntry() {
    LOG_INFO(logTag(), "Loop[{}] loop start", desc_.name);
    Result<void> result;
    try {
        { // start wait
            std::unique_lock<std::mutex> lock(worker_mutex_);
            cond_.wait(lock, [this]() { return state_ == WorkerState::Running || stop_requested_; });
            if (stop_requested_) return OK(); 
        }
        while (!stop_requested_) {
            { // pause wait
                std::unique_lock<std::mutex> lock(worker_mutex_);
                cond_.wait(lock, [this]() { return !paused_ || stop_requested_; });
                if (stop_requested_ || state_ != WorkerState::Running) break;
            }
            result = run();
            if(onCompleted) onCompleted(result);
            if (!result) {
                std::lock_guard<std::mutex> lock(worker_mutex_);
                state_ = WorkerState::Stopped;
                stop_requested_ = true; 
                cond_.notify_all();
                break;
            }
            if (stop_requested_) break;
            sleep(desc_.loop_sleep_ms);
        }
    } catch (const std::exception& e) {
        LOG_ERROR(logTag(), "loop[{}] exception: {}", desc_.name, e.what());
        result = Fail();
    } catch (...) {
        LOG_ERROR(logTag(), "loop[{}] unknown exception occurred", desc_.name);
        result = Fail();
    }
    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        state_ = WorkerState::Stopped;
    }
    return result;
}

Result<void> Worker::threadEventEntry() {
    LOG_INFO(logTag(), "event[{}] loop start", desc_.name);
    Result<void> result;
    try {
        { // start wait
            std::unique_lock<std::mutex> lock(worker_mutex_);
            cond_.wait(lock, [this]() { return state_ == WorkerState::Running || stop_requested_; });
            if (stop_requested_) return OK(); 
        }
        while (!stop_requested_) {
            { // event wait
                std::unique_lock<std::mutex> event_lock(event_mutex_);
                cond_event_.wait(event_lock, [this]() { return event_ || stop_requested_; });
                if (stop_requested_ || state_ != WorkerState::Running) break;
                event_ = false;
            }
            result = run();
            if(onCompleted) onCompleted(result);
            if (!result) {
                std::lock_guard<std::mutex> lock(worker_mutex_);
                state_ = WorkerState::Stopped;
                stop_requested_ = true; 
                cond_.notify_all();
                break;
            }
            if (stop_requested_) break;
            //sleep(desc_.loop_sleep_ms);
        }
    } catch (const std::exception& e) {
        LOG_ERROR(logTag(), "event[{}] exception: {}", desc_.name, e.what());
        result = Fail();
    } catch (...) {
        LOG_ERROR(logTag(), "event[{}] unknown exception occurred", desc_.name);
        result = Fail();
    }
    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        state_ = WorkerState::Stopped;
    }
    return result;
}

Result<void> Worker::threadSingleEntry() {
    Result<void> result;
    try {
        { // wait
            std::unique_lock<std::mutex> lock(worker_mutex_);
            cond_.wait(lock, [this]() { return state_ == WorkerState::Running || stop_requested_; });
            if (stop_requested_) return OK(); 
        }
        result = run();
        if(onCompleted) {
            onCompleted(result);
        }
    } catch (const std::exception& e) {
        LOG_ERROR(logTag(), "single[{}] exception: {}", desc_.name, e.what());
        result = Fail();
    } catch (...) {
        LOG_ERROR(logTag(), "single[{}] unknown exception occurred", desc_.name);
        result = Fail();
    }
    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        state_ = WorkerState::Stopped;
    }
    return result;
}



void Worker::resetFlags() {
    std::lock_guard<std::mutex> lock(worker_mutex_);
    paused_ = false;
    sleeping_ = false;
    stop_requested_ = false;
    event_ = false;
}


} // namespace worker