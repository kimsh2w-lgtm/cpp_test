#pragma once

#include "device_container.hpp"

namespace ioc {

	// Container로 부터 주입 받는 객체에서 Scope Service들에 대한 life time을 동기화 하기 위한 객체
	// 소멸자 호출될 때 Container에서 동기화 된 객체들을 만료시킨다.
	class DeviceScope
	{
		using ScopeId = intptr_t;

	private:
		// moving forbidden
		DeviceScope(DeviceScope&&) = delete;
		DeviceScope& operator=(DeviceScope&&) = delete;
		
		// copying forbidden
		DeviceScope(const DeviceScope&) = delete;               
		void operator=(const DeviceScope&) = delete;

	public:
		explicit operator ScopeId() const { return address_; };

		DeviceScope() :
			address_(ScopeId(this))
		{
		}

		~DeviceScope() 
		{
			ioc::DeviceContainer::instance().expiredInstance(address_);
		}

		ScopeId get() const 
		{
			return address_;
		};

	private:
		ScopeId address_;
	}; // class Scope

}; // namespace ioc
