#include "device_container.hpp"

namespace ioc {

// 싱글톤 인스턴스: 함수 지역 static → 스레드 세이프 (C++11 이상)
DeviceContainer& DeviceContainer::instance() {
    static DeviceContainer instance;
    return instance;
}

} // namespace ioc
