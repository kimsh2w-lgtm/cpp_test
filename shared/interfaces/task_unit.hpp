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

#include "result.h"

namespace task {

enum class TaskType
{
    Sync,
    Async,
    Thread,
};

struct TaskDescriptor {
    std::function<void()> func;
    std::string name;

    std::vector<int> affinity;
    int policy = 0;
    int priority = 0;
};

class TaskUnit {

public:
    virtual ~TaskUnit() = default;
    virtual Result<void> init() = 0;
    virtual Result<void> execute(TaskDescriptor desc) = 0;
    virtual Result<void> stop() = 0;
    virtual bool isStop() const = 0;
    virtual bool isRunning() const = 0;
    virtual bool isIdle() const = 0;

    virtual Result<void> setAffinity(const std::vector<int>& cores) = 0;
    virtual std::vector<int> getAffinity() const;

    virtual int getPolicy() const = 0;
    virtual int getPriority() const = 0;

    virtual Result<void> join() = 0;
    virtual Result<void> detach() = 0;

    virtual TaskType taskType() const noexcept = 0;
    
    // 복사 대입 금지 
    TaskUnit(const TaskUnit&)            = delete;
    TaskUnit& operator=(const TaskUnit&) = delete;

    // 이동 허용
    TaskUnit(TaskUnit&& other) noexcept = delete;
    TaskUnit& operator=(TaskUnit&& other) noexcept = delete;

protected:
    TaskUnit() = default;
};



}
