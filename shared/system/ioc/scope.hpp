#pragma once

#include "container.hpp"

namespace ioc {

	// Container로 부터 주입 받는 객체에서 Scope Service들에 대한 life time을 동기화 하기 위한 객체
	// 소멸자 호출될 때 Container에서 동기화 된 객체들을 만료시킨다.
	class Scope
	{
		using ScopeId = intptr_t;

	private:
		// moving forbidden
		Scope(Scope&&) = delete;
		Scope& operator=(Scope&&) = delete;
		
		// copying forbidden
		Scope(const Scope&) = delete;               
		void operator=(const Scope&) = delete;

	public:
		explicit operator ScopeId() const { return address_; };

		Scope() :
			address_(ScopeId(this))
		{
		}

		~Scope() 
		{
			ioc::Container::instance().expiredInstance(address_);
		}

		ScopeId get() const 
		{
			return address_;
		};

	private:
		ScopeId address_;
	}; // class Scope

}; // namespace ioc
