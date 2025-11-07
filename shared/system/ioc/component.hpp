#pragma once

#include <iostream>
#include <memory>

#include "basis_typeinfo.hpp"
#include "base_factory.hpp"
#include "base_component.hpp"

namespace ioc
{
    template<typename I, typename T, typename F>
    class Component : public BaseComponent<I>
    {
        
    public:
        Component() : Component(static_cast<std::string>(getTypeId<I>())) {  }
        
        Component(const std::string& name) :
            interface_type_(getTypeId<I>()),
            caller_type_(getTypeId<T>()),
            factory_type_(getTypeId<F>()),
            factory_(std::make_shared<F>()),
            name_(name)
        {
            
        }

        Component(std::shared_ptr<BaseFactory<I>> factory, const std::string& name) :
            interface_type_(getTypeId<I>()),
            caller_type_(getTypeId<T>()),
            factory_type_(getTypeId<F>()),
            factory_(factory),
            name_(name)
        {

        }

        ~Component() override = default;

        std::string name() const override
        {
            return name_;
        }

        std::shared_ptr<BaseFactory<I>> factory() const override
        {
            return factory_;
        }

        [[nodiscard]] std::shared_ptr<I> createService(intptr_t key = 0) override
        {
            return factory_->create(key);
        }

        TypeId interfaceType() const override
        {
            return interface_type_;
        }
        
        TypeId callerType() const override
        {
            return caller_type_;
        }

        TypeId factoryType() const override
        {
            return factory_type_;
        }

        // Factory 에서 관리하는 Instance를 폐기한다
        // @param key Facotory에서 관리되는 Instance를 지칭하기 위한 값
        void destroyInstance(intptr_t key = 0) override
        {
            factory_->destroyInstance(key);
        }

    private:
        TypeId interface_type_;
        TypeId caller_type_;
        TypeId factory_type_;
        std::string name_;
        std::shared_ptr<BaseFactory<I>> factory_;
    }; // class Component

    
}; // namespace ioc
