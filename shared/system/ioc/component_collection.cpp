#include "component_collection.hpp"

namespace ioc {


ComponentCollection::ComponentCollection() noexcept
    : destroying_(false)
{
}

ComponentCollection::~ComponentCollection()
{
    std::lock_guard<std::mutex> lock(mutex_);
    destroying_ = true;
    collection_.clear();
}
		
void ComponentCollection::expiredInstance(intptr_t key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (destroying_ || key == 0)
        return;

    for (auto& [type, named_components] : collection_) {
        if (named_components.empty())
            continue;

        for (auto& [name, component] : named_components) {
            try { 
                component->destroyInstance(key);
             } catch (...) 
             {
                /* log */ 
            }
        }
    }
}

} // namespace ioc
