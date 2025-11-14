#pragma once

#include <string>
#include <map>
#include <functional>
#include <mutex>

#include "base_factory.hpp"

namespace ioc {
    
    // The Scoped Factory shares ownership of the services (objects) it creates.
    // @tparam I Interface type
    // @tparam T Implementation class type
    template<typename I, typename T>
    class ScopedFactory : virtual public BaseFactory<I>
    {

    public:
        ScopedFactory() { }

        ~ScopedFactory() {
            // _instance_list 해제 확인.
        }

        TypeId factoryType() const
        {
            return getTypeId<ScopedFactory<I, T>>();
        }

        // “key=0은 non-scoped transient 동작, key≠0은 scoped singleton”
        [[nodiscard]] std::shared_ptr<I> create(intptr_t key = 0) override
        {
            std::lock_guard<std::mutex> lock(scoped_mutex_);
            if(key == 0)
            {
                return std::make_shared<T>();
            }
            auto scoped_instance = instance_list_.find(key);
            if (scoped_instance != instance_list_.end())
            {
                auto instance = scoped_instance->second;
                return instance;
            }
            auto instance = std::make_shared<T>();
            instance_list_.insert({ key, instance });
            return instance;
        }

        //key 에 할당된 객체들을 제거한다.
        Result<void> destroyInstance(intptr_t key = 0) override
        {
            if (key == 0) return;

            // NOTE: map::at()으로의 접근은 out_of_range exception 이 발생할 수 있다.
            //       일반적인 경우에는 문제가 발생하지 않음.
            //       다만, try catch 처리로 인한 의도한 오류의 명확함을 제공하기 어려울 수 있다.
            /*
            auto instance_pair = _instance_list.find(key);
            if (instance_pair != _instance_list.end())
            {
                _instance_list.erase(instance_pair->first);
                //auto instance = scoped_instance->second;
                //instance.reset();
            }
            
            for (auto it = instance_list_.lower_bound(key); it != instance_list_.upper_bound(key);)
            {
                instance_list_.erase(it++);
            }
            */
            auto it = instance_list_.find(key);
            if (it != instance_list_.end()) instance_list_.erase(it);
            return OK();
        }

    private:
        mutable std::mutex scoped_mutex_;
        std::map<intptr_t, std::shared_ptr<T>> instance_list_;

    }; // class ScopedFactory

}; // namespace ioc
