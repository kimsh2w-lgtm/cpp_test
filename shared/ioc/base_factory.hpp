#pragma once

#include <memory>
#include "base.h"
#include "basis_typeinfo.hpp"

namespace ioc
{
    
    class IFactory
    {
    public:
        virtual ~IFactory() = default;
        virtual TypeId factoryType() const = 0;
        virtual Result<void> destroyInstance(intptr_t key = 0) = 0;
        virtual std::string name() const = 0;
    }; // interface IFactory


    template<typename I>
    class BaseFactory : virtual public IFactory
    {
    public:
        virtual ~BaseFactory() override = default;
        [[nodiscard]] virtual std::shared_ptr<I> create(intptr_t key = 0) = 0;
    }; // class BaseFactory


} // namespace ioc
