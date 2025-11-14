// container.cpp
#include "basic_container.hpp"
#include "component_collection.hpp"
#include "service_provider.hpp"

namespace ioc {


// private 생성자: 내부 구성요소 초기화
BasicContainer::BasicContainer() {
    component_collection_ = std::make_shared<ComponentCollection>();
    service_provider_     = std::make_shared<ServiceProvider>(component_collection_);
}

// 소멸자: 기본 파괴(스마트포인터로 정리)
BasicContainer::~BasicContainer() {
}

} // namespace ioc
