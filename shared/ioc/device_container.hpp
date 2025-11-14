#pragma once

#include <stdlib.h>
#include <typeinfo>
#include <map>
#include <string>
#include <memory>
#include <typeindex>


#include "common/macros.h"
#include "basic_container.hpp"


namespace ioc {

    class DeviceContainer final : public BasicContainer<DeviceContainer>
    {
    private:
        DeviceContainer() = default;
        ~DeviceContainer() = default;
        
    public:
        static DeviceContainer& instance();

    }; // class Container


}; // namespace ioc
