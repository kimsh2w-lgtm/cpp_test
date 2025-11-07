#pragma once

#include <memory>
#include <string>
#include "ISystemService.hpp"

namespace sample {

class SampleService : public ISystemService {
public: // command

    /**
     * @type: command
     * @command: SampleCommandA
     * @allowed_modes: []
     * @emit: []
     * @description: 
     */
    void SampleCommandA();
public:
    SampleService();
    ~SampleService();

private:
    int value_;
};

} // namespace sample

