#pragma once
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>

#include "basis_typeinfo.hpp"

#include "base_factory.hpp"
#include "base_component.hpp"
#include "component.hpp"


namespace ioc {

	class ComponentCollection
	{
		// NamedComponents는 등록되는 service의 이름으로 component를 묶음 
        using NamedComponents = std::unordered_map<std::string, std::shared_ptr<IComponent>>;

		// 이름이 지정된 component를 interface type별 묶음 
		using ComponentMap = std::map<TypeId, NamedComponents>;

		// ComponentMap
		//  -----------------------------------------------
		// |            |   NamedComponents                |
		// |  TypeId    |   [  Name       , Component   ]  |
		// |------------------------------------------------
		// |  Foo       |   NamedComponents of Foo         |
 		// |            |   [ "Name foo " , Bar         ]  |
		// |            |   [ "Type<Foo>" , Bar2        ]  |
		//  -----------------------------------------------
		// |  ABC       |   NamedComponents of ABC         |
		// |            |   [ "Type<ABC>" , abc         ]  |
		// |            |                                  |
		//                     .....
		// -------------------------------------------------

	public:
		ComponentCollection() noexcept : destroying_ (false) {}

		~ComponentCollection();
		
		// Component를 등록하는 함수
		// 
		// TODO: 현재 시나리오상은 동일한 interface type과 component name이 존재할 경우
		//       특별한 오류 없이 등록을 무시한채로 넘어간다.
		//       - 등록 과정의 결과를 반환하는 TryAdd 함수 작업이 필요할 수 있음.
		//       - 중복 등록을 허용하도록 할 수 있으나 어떤 객체를 반환 할지 불분명해진다.
		//        사용 방법에 따라 논의 필요.
        void add(const std::shared_ptr<IComponent>& component)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto interface_type = component->interfaceType();

            auto [it, inserted] = collection_.try_emplace(interface_type, NamedComponents{});
            auto& interface_components = it->second;

            const auto& name = component->name();
            if (interface_components.find(name) != interface_components.end()) {
                return;
            }

            interface_components.emplace(name, component);
        }

        bool add(std::shared_ptr<IComponent>&& component)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto interface_type = component->interfaceType();

            auto [it, inserted] = collection_.try_emplace(interface_type, NamedComponents{});
            auto& interface_components = it->second;

            const auto& name = component->name();
            if (interface_components.find(name) != interface_components.end()) {
                // 중복
                return false;
            }

            interface_components.emplace(name, component);
            return true;
        }

		template<typename I>
		void remove(const std::string& name)
		{
            std::lock_guard<std::mutex> lock(mutex_);
			auto interface_type = getTypeId<I>();

            auto it = collection_.find(interface_type);
            if (it == collection_.end()) return;

			auto& components = it->second;
			auto itc = components.find(name);
			if (itc != components.end())
			{
				components.erase(itc);

				if(components.empty())
					collection_.erase(it);
			}
		}

		template<typename I>
		void remove()
		{
			auto name = std::string(getTypeId<I>());
			return remove<I>(name);
		}


		template<typename I>
		bool isComponent() const
		{
			auto name = std::string(getTypeId<I>());
			return isComponent<I>(name);
		}

		template<typename I>
		bool isComponent(const std::string& name) const
		{
            std::lock_guard<std::mutex> lock(mutex_);
			auto interface_type = getTypeId<I>();
			auto it = collection_.find(interface_type);
            if (it == collection_.end())
                return false;

            const auto& components = it->second;
            return components.find(name) != components.end();
		}
		 
		template<typename I>
		std::weak_ptr<I> getService(intptr_t scoped_key = 0) const
		{
			auto name = std::string(getTypeId<I>());
			return getService<I>(name, scoped_key);
		}

		template<typename I>
		std::weak_ptr<I> getService(const std::string& name, intptr_t scoped_key = 0) const
		{	
            std::lock_guard<std::mutex> lock(mutex_);
			auto interface_type = getTypeId<I>();
			auto it = collection_.find(interface_type);
            if (it == collection_.end())
                return {};

			auto& components = it->second;
			auto itr = components.find(name);
            if (itr == components.end())
                return {};

            auto* base_component = dynamic_cast<BaseComponent<I>*>(itr->second.get());
			if (!base_component) return {};
			return base_component->createService(scoped_key);
		}

		// key로 관리되는 전체 component의 instance들을 만료시킨다.(폐기한다).
		// 실제 동작은 Component의 Factory에서 관리하는 instance를 폐기한다.
		void expiredInstance(intptr_t key = 0);
		
	private:
		ComponentMap collection_;

        mutable std::mutex mutex_;

		// 순환 참조 방지용 플래그
		bool destroying_;
	}; // class ComponentCollection


}; // namespace ioc
