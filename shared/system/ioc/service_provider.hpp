#pragma once

#include <memory>

#include "base_factory.hpp"
#include "component_collection.hpp"


namespace ioc {

	// ServiceProvider는 관리되는 Component에 대한 접근을 제한한다.
	// 연결된 ComponentCollection에 의해 
	class ServiceProvider
	{

	public:
		ServiceProvider(std::shared_ptr<ComponentCollection> collections);
		~ServiceProvider();

		template<typename I>
		[[nodiscard]] std::weak_ptr<I> getService(intptr_t scope = 0) {
			return collections_->getService<I>(scope);
		}

		template<typename I>
		[[nodiscard]] std::weak_ptr<I> getService(const std::string& name, intptr_t scope = 0) {
			return collections_->getService<I>(name, scope);
		}
	private:
		std::shared_ptr<ComponentCollection> collections_;
	};

}; // namespace ioc
