
#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <mutex>
#include <cstdint>

#include "task_unit.hpp"


namespace task {

    
class WorkerPool {
public:
    using Ptr  = std::shared_ptr<TaskUnit>;
    using AffinityMask = uint64_t;
    
    enum class Match { Exact, Superset, Intersect, Any };


    void addWorker(const Ptr& worker) {
        if (!worker) return;
        std::lock_guard<std::mutex> lk(mutex_);
        workers_.push_back(worker);
        indexWorkerLocked(worker);
    }

    // If you need to remove a worker explicitly
    void removeWorker(const TaskUnit* wptr) {
        if (!wptr) return;
        std::lock_guard<std::mutex> lk(mutex_);
        deindexTaskLocked(wptr);
        workers_.erase(std::remove_if(workers_.begin(), workers_.end(),
                         [&](const Ptr& p){ return p.get() == wptr; }),
                       workers_.end());
    }

    // affinity 변경 후 재인덱싱 호출
    void onWorkerAffinityChanged(TaskUnit& w) {
        deindexWorker(w);
        indexWorker(std::static_pointer_cast<TaskUnit>(w.shared_from_this())); // or pass pointer you keep
    }

    // 찾기 API
    std::shared_ptr<TaskUnit>
    findByAffinity(const std::vector<int>& want, Match match, bool idleOnly=true) {
        const AffinityMask wm = make_mask(normalize(want));

        // 1) 정확 일치
        if (match == Match::Exact) {
            auto it = by_affinity_.find(wm);
            if (it != by_affinity_.end()) {
                for (auto& w : it->second) {
                    if (!idleOnly || w->isIdle()) return w;
                }
            }
            return nullptr;
        }

        // 2) 상위집합 (워커 마스크가 요청 마스크를 포함)
        if (match == Match::Superset) {
            for (auto& [mask, vec] : by_affinity_) {
                if ( (mask & wm) == wm ) {
                    for (auto& w : vec) {
                        if (!idleOnly || w->isIdle()) return w;
                    }
                }
            }
            // fall-through → Intersect/Any로 완화할 수도 있음
        }

        // 3) 교집합(겹치는 코어가 하나라도)
        if (match == Match::Intersect) {
            for (auto& [mask, vec] : by_affinity_) {
                if ( (mask & wm) != 0 ) {
                    for (auto& w : vec) {
                        if (!idleOnly || w->isIdle()) return w;
                    }
                }
            }
        }

        // 4) 아무거나 (idle 우선)
        if (match == Match::Any) {
            for (auto& w : workers_) {
                if (!idleOnly || w->isIdle()) return w;
            }
        }
        return nullptr;
    }

private:
    static std::vector<int> normalize(std::vector<int> v) {
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
        return v;
    }

    void indexWorker(std::shared_ptr<TaskUnit> w) {
        AffinityMask m = make_mask(w->getAffinity());
        by_affinity_[m].push_back(w);
    }
    void deindexWorker(TaskUnit& w) {
        AffinityMask m = make_mask(w.getAffinity());
        auto it = by_affinity_.find(m);
        if (it == by_affinity_.end()) return;
        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                   [&](const std::shared_ptr<TaskUnit>& p){ return p.get() == &w; }),
                  vec.end());
        if (vec.empty()) by_affinity_.erase(it);
    }

private:
    std::mutex mutex_;
    std::vector<Ptr> workers_;

    // Affinity mask -> workers having that exact mask
    std::unordered_map<AffinityMask, std::vector<Ptr>> by_affinity_;
    // Round-robin cursor per mask
    std::unordered_map<AffinityMask, size_t> rr_index_;

    std::vector<std::shared_ptr<TaskUnit>> workers_;
    
};


}
