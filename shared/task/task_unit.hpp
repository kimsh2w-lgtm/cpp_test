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
#include <future>

#include "result.h"
#include "logging.hpp"

namespace task {

enum class TaskExcutionMode
{
    Sync,
    Async,
    Thread,
};

enum class TaskDispatchPolicy
{
    Immediate,
    Throttled,
    Deferred,
};

template<typename T = void>
struct TaskDescriptor {
    std::string name;
    std::function<Result<T>()> func;
    std::function<void(Result<T>)> on_complete; // 콜백
    
    TaskDispatchPolicy dispatch;
    int throttle_time_ms;
    std::vector<int> affinity;
    int policy = 0;
    int priority = 0;
};

template<typename T = void>
class TaskBuilder {
public:
    TaskBuilder& name(const std::string& n) {
        desc_.name = n; return *this;
    }

    TaskBuilder& func(std::function<Result<T>()> f) {
        desc_.func = std::move(f); return *this;
    }

    TaskBuilder& onComplete(std::function<void(Result<T>)> cb) {
        desc_.on_complete = std::move(cb); return *this;
    }

    TaskBuilder& dispatch(TaskDispatchPolicy p) {
        desc_.dispatch = p; return *this;
    }

    TaskBuilder& throttle(int ms) {
        desc_.throttle_time_ms = ms; return *this;
    }

    TaskBuilder& affinity(std::vector<int> cores) {
        desc_.affinity = std::move(cores); return *this;
    }

    TaskBuilder& policy(int p) {
        desc_.policy = p; return *this;
    }

    TaskBuilder& priority(int p) {
        desc_.priority = p; return *this;
    }

    TaskDescriptor<T> build() {
        if (!desc_.func)
            throw std::runtime_error("TaskBuilder requires func()");
        return desc_;
    }

private:
    TaskDescriptor<T> desc_;
};



class TaskUnit {

public:
    virtual ~TaskUnit() = default;
    virtual Result<void> init() = 0;
    virtual Result<void> execute(TaskDescriptor<void> desc) = 0;
    virtual Result<void> stop() = 0;
    virtual bool isStop() const = 0;
    virtual bool isRunning() const = 0;
    virtual bool isIdle() const = 0;

    virtual Result<void> setAffinity(const std::vector<int>& cores) = 0;
    virtual std::vector<int> getAffinity() const;

    virtual std::size_t id() const = 0;
    virtual int getPolicy() const = 0;
    virtual int getPriority() const = 0;

    virtual Result<void> wait(int msec) = 0;
    virtual Result<void> join() = 0;
    virtual Result<void> detach() = 0;

    virtual TaskExcutionMode excutionMode() const noexcept = 0;
    
    // 복사 대입 이동 금지 
    TaskUnit(const TaskUnit&)            = delete;
    TaskUnit& operator=(const TaskUnit&) = delete;
    TaskUnit(TaskUnit&& other) noexcept = delete;
    TaskUnit& operator=(TaskUnit&& other) noexcept = delete;

protected:
    TaskUnit() = default;
};


template<typename T>
class ResultTaskUnit : virtual public TaskUnit {

public:
    virtual ~ResultTaskUnit() = default;
    virtual Result<void> execute(TaskDescriptor<T> desc) = 0;
    virtual Result<T> result() = 0;


protected:
    ResultTaskUnit() = default;
};

}
