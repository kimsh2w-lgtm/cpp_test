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

    class Container final : public BasicContainer<Container>
    {
    private:
        Container() = default;
        ~Container() = default;
        
    public:
        static Container& instance();

    }; // class Container


}; // namespace ioc
