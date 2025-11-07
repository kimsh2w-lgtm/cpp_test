#pragma once

#include <functional>

#include "service_provider.hpp"
#include "container.hpp"
#include "device_container.hpp"
#include "scope.hpp"

namespace ioc {
    
// main 문에 생성
// ioc::Container& ServiceBuild() {
//    return ioc::Container::instance();
// }
// extern "C" ioc::Container& ServiceBuild();
Container& ServiceBuild();


template <typename I>
inline std::weak_ptr<I> getService()                                                                      \
{                                                                                                
    auto service = ServiceBuild().getServiceProvider();
    return service->getService<I>();
}

template <typename I>
inline std::weak_ptr<I> getService(const std::string& name)
{                                                            
    auto service = ServiceBuild().getServiceProvider();
    return service->getService<I>(name);
}



// main 문에 생성
// ioc::Container& DeviceAccessBuild() {
//    return ioc::DeviceContainer::instance();
// }
// extern "C" ioc::Container& DeviceAccessBuild();
DeviceContainer& DeviceAccessBuild();

template <typename I>
inline std::weak_ptr<I> getDeviceAccess()                                                                      \
{                                                                                                
    auto service = DeviceAccessBuild().getServiceProvider();
    return service->getService<I>();
}

template <typename I>
inline std::weak_ptr<I> getDeviceAccess(const std::string& name)
{                                                            
    auto service = DeviceAccessBuild().getServiceProvider();
    return service->getService<I>(name);
}


} // namespace ioc

#define INJECT(Signature)                                         \
  inline static constexpr const char* LOG_TAG = Signature;        \
  ioc::Scope scope;                                               \
  template <typename I>                                           \
  std::weak_ptr<I> Resolve()                                    \
  {                                                               \
    auto service = ServiceBuild().getServiceProvider();           \
    return service->getService<I>(intptr_t(scope));               \
  }                                                               \
  template <typename I>                                           \
  std::weak_ptr<I> Resolve(const std::string& name)             \
  {                                                               \
    auto service = ServiceBuild().getServiceProvider();           \
    return service->getService<I>(name, intptr_t(scope));         \
  }

