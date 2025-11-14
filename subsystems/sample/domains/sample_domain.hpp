#pragma once

#include <memory>
#include <string>

#include "base.h"
#include "ioc.hpp"

namespace sample {

class SampleDomain {
private:
    

public:
    SampleDomain();
    ~SampleDomain();

    Result<int> sample();

private:
    INJECT(SampleDomain)

    int value_;
};

} // namespace sample

