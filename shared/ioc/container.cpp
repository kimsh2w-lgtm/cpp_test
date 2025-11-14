// container.cpp
#include "container.hpp"
#include "component_collection.hpp"
#include "service_provider.hpp"

namespace ioc {

// 싱글톤 인스턴스: 함수 지역 static → 스레드 세이프 (C++11 이상)
Container& Container::instance() {
    static Container instance;
    return instance;
}

} // namespace ioc
