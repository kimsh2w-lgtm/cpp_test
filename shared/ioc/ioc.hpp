#pragma once

#include <functional>

#include "service_provider.hpp"
#include "container.hpp"
#include "device_container.hpp"
#include "scope.hpp"

namespace ioc {
    
// build option -rdynamic(CMake: target_link_options(main PRIVATE -Wl,-export-dynamic))

// 단일 생성
// ioc::Container& ServiceBuild() {
//    return ioc::Container::instance();
// }
// extern "C" ioc::Container& ServiceBuild();
Container& ServiceBuild();


template <typename I>
inline std::shared_ptr<I> getService()                                                                      \
{                                                                                                
    auto service = ServiceBuild().getServiceProvider();
    return service->getService<I>();
}

template <typename I>
inline std::shared_ptr<I> getService(const std::string& name)
{                                                            
    auto service = ServiceBuild().getServiceProvider();
    return service->getService<I>(name);
}



// 단일 생성
// ioc::Container& DeviceAccessBuild() {
//    return ioc::DeviceContainer::instance();
// }
// extern "C" ioc::Container& DeviceAccessBuild();
DeviceContainer& DeviceAccessBuild();

template <typename I>
inline std::shared_ptr<I> getDeviceAccess()                                                                      \
{                                                                                                
    auto service = DeviceAccessBuild().getServiceProvider();
    return service->getService<I>();
}

template <typename I>
inline std::shared_ptr<I> getDeviceAccess(const std::string& name)
{                                                            
    auto service = DeviceAccessBuild().getServiceProvider();
    return service->getService<I>(name);
}


} // namespace ioc

#define INJECT(Signature)                                         \
  ioc::Scope scope;                                               \
  template <typename I>                                           \
  std::shared_ptr<I> Resolve()                                    \
  {                                                               \
    auto service = ServiceBuild().getServiceProvider();           \
    return service->getService<I>(intptr_t(scope));               \
  }                                                               \
  template <typename I>                                           \
  std::shared_ptr<I> Resolve(const std::string& name)             \
  {                                                               \
    auto service = ServiceBuild().getServiceProvider();           \
    return service->getService<I>(name, intptr_t(scope));         \
  }                                                               \
  inline static constexpr const char* LOG_TAG = #Signature;

