#pragma once
#include <string>
#include <memory>
#include "basis_typeinfo.hpp"
#include "base_factory.hpp"

namespace ioc
{

    class IComponent
    {
    public:
        virtual ~IComponent() = default;
        virtual std::string name() const = 0;
        virtual TypeId interfaceType() const = 0;
        virtual TypeId callerType() const = 0;
        virtual TypeId factoryType() const = 0;
        virtual Result<void> destroyInstance(intptr_t key = 0) = 0;
    };

    // 지정된 Interface에 대한 Component를 제어하기 위한 가상 클래스
    template<typename I>
    class BaseComponent : public IComponent
    {
    public:
        virtual std::shared_ptr<BaseFactory<I>> factory() const = 0;
        virtual std::shared_ptr<I> createService(intptr_t key = 0) = 0;
    }; // class BaseComponent


} // namespace ioc
