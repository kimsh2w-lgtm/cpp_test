#pragma once

#include <memory>
#include <string>
#include "ISystemService.hpp"

namespace sample {

class SampleService : public ISystemService {
public:
    SampleService();
    ~SampleService();

    void SampleCommand(SampleModel model);
private:
    int value_;
};

} // namespace sample

