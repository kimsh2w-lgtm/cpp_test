#pragma once

#include <stdlib.h>
#include <typeinfo>
#include <map>
#include <string>
#include <memory>
#include <typeindex>

#include "common/macros.h"
#include "basis_typeinfo.hpp"
#include "singleton_factory.hpp"
#include "singletonlazy_factory.hpp"
#include "scoped_factory.hpp"
#include "transient_factory.hpp"
#include "component.hpp"
#include "component_collection.hpp"
#include "service_provider.hpp"


namespace ioc {

    template<typename Derived>
    class BasicContainer
    {
    protected:
        // TODO: 
        //  다양한 방식으로의 Component를 관리하기 위해서
        //  Interface 형태로 Component Collection과 Service Provider가 생성될 필요가 있다.
        //  현재는 동적으로 객체를 생성하고 이후 Interface 형태로 생성되도록 작업 필요
        BasicContainer();
        ~BasicContainer();
        
        // Copying forbidden
        BasicContainer(const BasicContainer&);  
        void operator=(const BasicContainer&);
    public:
        std::shared_ptr<ServiceProvider> getServiceProvider() const
        { 
            return service_provider_;
        }

        template<typename I, typename T>
        BasicContainer& registerTransient(const std::string& name)
        {
            using FactoryType = TransientFactory<I, T>;
            using ComponentType = Component<I, T, FactoryType>;
            std::shared_ptr<ComponentType> component = std::make_shared<ComponentType>(name);
            return registerComponent<ComponentType>(component);
        }

        template<typename I, typename T>
        BasicContainer& registerTransient() {
            auto name = static_cast<std::string>(getTypeId<I>());
            return registerTransient<I, T>(name);
        }

        template<typename I, typename T>
        BasicContainer& registerScoped(const std::string& name)
        {
            using FactoryType = ScopedFactory<I, T>;
            using ComponentType = Component<I, T, FactoryType>;
            std::shared_ptr<ComponentType> component = std::make_shared<ComponentType>(name);
            return registerComponent<ComponentType>(component);
        }

        template<typename I, typename T>
        BasicContainer& registerScoped()
        {
            auto name = static_cast<std::string>(getTypeId<I>());
            return registerScoped<I, T>(name);
        }

        template<typename T>
        BasicContainer& registerSingleton()
        {
            return registerSingleton<T, T>();
        }
        
        template<typename I, typename T>
        BasicContainer& registerSingleton()
        {
            auto name = static_cast<std::string>(getTypeId<I>());
            return registerSingleton<I, T>(name);
        }

        template<typename T>
        BasicContainer& registerSingleton(const std::string& name)
        {
            return registerSingleton<T, T>(name);
        }
        
        template<typename I, typename T>
        BasicContainer& registerSingleton(const std::string& name)
        {
            using factory_type = SingletonFactory<I, T>;
            using component_type = Component<I, T, factory_type>;
            std::shared_ptr<component_type> component = std::make_shared<component_type>(name);
            return registerComponent<component_type>(component);
        }

        template<typename I, typename T>
        BasicContainer& registerInstance(std::shared_ptr<T> instance)
        {
            auto name = static_cast<std::string>(getTypeId<I>());
            return registerInstance<I, T>(instance, name);
        }

        template<typename T>
        BasicContainer& registerInstance(std::shared_ptr<T> instance)
        {
            auto name = static_cast<std::string>(getTypeId<T>());
            return registerInstance<T, T>(instance, name);
        }

        template<typename I, typename T>
        BasicContainer& registerInstance(std::shared_ptr<T> instance, const std::string& name)
        {
            using factory_type = SingletonFactory<I, T>;
            using component_type = Component<I, T, factory_type>;

            auto factory = std::make_shared<factory_type>(instance);
            std::shared_ptr<component_type> component =
                std::make_shared<component_type>(factory, name);
            return registerComponent<component_type>(component);
        }
        

        template<typename T>
        BasicContainer& registerSingletonLazy()
        {
            return registerSingletonLazy<T, T>();
        }

        template<typename I, typename T>
        BasicContainer& registerSingletonLazy()
        {
            auto name = static_cast<std::string>(getTypeId<I>());
            return registerSingletonLazy<I, T>(name);
        }

        template<typename T>
        BasicContainer& registerSingletonLazy(const std::string& name)
        {
            return registerSingletonLazy<T, T>(name);
        }

        template<typename I, typename T>
        BasicContainer& registerSingletonLazy(const std::string& name)
        {
            using factory_type = SingletonLazyFactory<I, T>;
            using component_type = Component<I, T, factory_type>;
            std::shared_ptr<component_type> component = std::make_shared<component_type>(name);
            return registerComponent<component_type>(component);
        }

        template<typename I>
        Derived& deregister()
        {
            component_collection_->remove<I>();
            return static_cast<Derived&>(*this);
        }

        template<typename I>
        Derived& deregister(const std::string& name)
        {
            component_collection_->remove<I>(name);
            return static_cast<Derived&>(*this);
        }

        //
        // 관리되는 객체를 만료(폐기)시킨다.
        // key 는 ServiceProvider로부터 가
        // 
        // TODO: 
        //   현재 Scope 일 경우에만 활용가치가 있는 기능으로
        //   Scope 객체에만 friend로 허용할지 검토.
        // 
        void expiredInstance(intptr_t key = 0)
        {
            component_collection_->expiredInstance(key);
        }

    private:
        template<typename C>
        Derived& registerComponent(std::shared_ptr<C>&& component)
        {
            component_collection_->add(std::move(component)); // rvalue
            return static_cast<Derived&>(*this);
        }

    private:
        std::shared_ptr<ServiceProvider> service_provider_;
        std::shared_ptr<ComponentCollection> component_collection_;

    }; // class Container


}; // namespace ioc
