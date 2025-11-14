
#include "sample_service.hpp"
#include "sample_domain.hpp"

#include "message.hpp"

namespace sample {

SampleService::SampleService() = default;
SampleService::~SampleService() = default;


Result<void> SampleService::cmdSample(const message::Message& args)
{
    auto domain = Resolve<SampleDomain>();
    return OK();
}    


Result<void> SampleService::cmdUploadLog(const message::Message& args)
{
    return OK();
}


Result<void> SampleService::cmdGetStatus(const message::Message& args)
{
    return OK();
}

} // namespace sample
