#pragma once
#include "base_factory.hpp"

namespace ioc {

    // TransientFactory는 생성하는 서비스(객체)의 소유권을 이양한다. 
    // @tparam I Interface type
    // @tparam T Implementation class type
    template<typename I, typename T>
    class TransientFactory : virtual public BaseFactory<I>
    {
    
    public:
        TransientFactory() { }

        ~TransientFactory() { }

        TypeId factoryType() const override
        {
            return getTypeId<TransientFactory<I, T>>();
        }

        [[nodiscard]] std::shared_ptr<I> create(intptr_t _ = 0) override
        {
            auto instance = std::make_shared<T>();
            return instance;
        }

        void destroyInstance(intptr_t key = 0) override
        {

        }

    private:
        
    }; // class TransientFactory


};  // namespace ioc

