#pragma once
#include <memory>

#include "base_factory.hpp"

namespace ioc {

    // SingletonLazyFactory는 생성하는 서비스(객체)의 소유권을 공유한다. 
    // 단, 객체의 최종적인 소유권은 Facotry에서 소유한다.
    // SingletonFacotry와 동일한 동작을 하나, 처음 호출 될 때 서비스(객체)를 생성한다.
    // @tparam I Interface type
    // @tparam T Implementation class type
    template<typename I, typename T>
    class SingletonLazyFactory
        : virtual public BaseFactory<I>
    {

    public:
        SingletonLazyFactory()
        {
            instance_ = std::shared_ptr<I>(nullptr);
        }

        ~SingletonLazyFactory()
        {

        }

        TypeId factoryType() const override
        {
            return getTypeId<SingletonLazyFactory<I, T>>();
        }

        [[nodiscard]] std::shared_ptr<I> create(intptr_t _ = 0) override
        {
            if (instance_.get() == nullptr)
            {
                instance_ = std::make_shared<T>();
            }
            return instance_;
        }

        void destroyInstance(intptr_t _ = 0) override
        {
            
        }

    private:
        std::shared_ptr<I> instance_;
        
    }; // class SingletonLazyFactory

}; // namespace ioc
