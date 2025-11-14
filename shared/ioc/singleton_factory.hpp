#pragma once
#include <memory>

#include "base_factory.hpp"


namespace ioc {

    // SingletonFactory는 생성하는 서비스(객체)의 소유권을 공유한다. 
    // 단, 객체의 최종적인 소유권은 Facotry에서 소유한다.
    // @tparam I Interface type
    // @tparam T Implementation class type
    template<typename I, typename T>
    class SingletonFactory : virtual public BaseFactory<I>
    {

    public:
        SingletonFactory()
        {
            instance_ = std::make_shared<T>();
        }

        SingletonFactory(std::shared_ptr<I> instance)
            : instance_(instance)
        {
            
        }

        ~SingletonFactory() override = default;
        
        TypeId factoryType() const override
        {
            return getTypeId<SingletonFactory<I, T>>();
        }

        [[nodiscard]] std::shared_ptr<I> create(intptr_t _ = 0) override
        {
            return instance_;
        }

        // NOTE: singleton 객체도 재생성 해야 할지 고민.
        //       안정성을 위해서는 재생성을 안하는 방향이 맞을 것으로 보임
        Result<void> destroyInstance(intptr_t _ = 0) override
        {
            return OK();
        }

    private:
        std::shared_ptr<I> instance_;
        
    }; // class SingletonFactory

}; // namespace ioc
