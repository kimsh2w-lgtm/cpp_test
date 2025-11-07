
#include "base.h"
#include "thread_unit.hpp"


namespace task {

void ThreadUnit::applyThreadAttributesIfDirty() {
    if (!thread_.joinable()) return;
    pthread_t handle = thread_.native_handle();

    // name
    if (dirty_name_) {
        if (desired_name_.has_value()) {
            std::string truncated = desired_name_->substr(0, 15);
            int rc = ::pthread_setname_np(handle, truncated.c_str());
            if (rc == 0) { cur_name_ = truncated; dirty_name_ = false; }
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
            // 권한 없으면 실패 → dirty 유지(다음에 재시도)
        } else {
            // nullopt → 변경 없음 (원한다면 기본값으로 되돌리기 구현)
            dirty_sched_ = false;
        }
    }
}

Result<void> ThreadUnit::init() {
    if (thread_.joinable()) return Fail();

    stop_.store(false, std::memory_order_relaxed);
    has_task_.store(false, std::memory_order_relaxed);

    try {
        thread_ = std::thread([this]() { loop(); });
    } catch (...) {
        return Fail();
    }
    return Ok();
}

Result<void> ThreadUnit::execute(TaskDescriptor desc) {
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
    return Ok();
}

void ThreadUnit::loop() {
    running_.store(true, std::memory_order_relaxed);

    while (!stop_.load(std::memory_order_relaxed)) {
        TaskDescriptor task;
        {
            std::unique_lock<std::mutex> lock(task_mutex_);
            cond_.wait(lock, [this]() {
                return stop_.load(std::memory_order_relaxed) || has_task_.load(std::memory_order_relaxed);
            });
            if (stop_.load(std::memory_order_relaxed)) break;

            task = std::move(desc_);
            has_task_.store(false, std::memory_order_relaxed);

            lock.unlock(); // 실행 중에는 잠금 해제
        }
        
        if (task.func) {
            try {
                task.func();
            } catch (...) {
                // 예외 처리
            }
        }
    }
    running_.store(false, std::memory_order_relaxed);
}

Result<void> ThreadUnit::setAffinity(const std::vector<int>& cores) {
    if (!thread_.joinable()) return Fail();

    pthread_t handle = thread_.native_handle();

    // 빈 벡터 → affinity 해제 요청으로 해석하거나 변경 없음 처리
    std::optional<std::vector<int>> new_affinity =
        cores.empty() ? std::optional<std::vector<int>>{} : std::make_optional(cores);

    // 변경되지 않았다면 시스템 호출 생략
    if (new_affinity == cur_affinity_) {
        return Ok();  // 이미 동일하므로 재설정 불필요
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
            return Ok();
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
        return Ok();
    }

}



} // namespace task