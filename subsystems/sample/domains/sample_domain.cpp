#include "sample_domain.hpp"

namespace sample {

SampleDomain::SampleDomain() = default;
SampleDomain::~SampleDomain() = default;

Result<int> SampleDomain::sample() {
    LOGI("sample {}", ++value_);
    return Result<int>::OK(value_);
}

} // namespace sample
