#include "service_provider.hpp"

namespace ioc {

ServiceProvider::ServiceProvider(std::shared_ptr<ComponentCollection> collections)
    : collections_(std::move(collections))
{
}

ServiceProvider::~ServiceProvider() = default;

} // namespace ioc
