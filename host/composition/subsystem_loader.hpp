#pragma once
#include <string>
#include <dlfcn.h>
#include <memory>

#include "host_base.hpp"
#include "subsystem_abi.h"

namespace composition {
    
class SubsystemLoader {
public:
    inline static constexpr const char* LOG_TAG = "SubsystemLoader";

    struct LoadedSubsystem {
        void* handle = nullptr;
        const SubsystemDescriptor* descriptor = nullptr;
        SubsystemHandle* instance = nullptr;

        explicit operator bool() const noexcept {
           return descriptor && instance && descriptor->vtable;
        }
    };

    static std::unique_ptr<LoadedSubsystem> load(const std::string& so_path, const SubsystemParams& params) {
        void* handle = dlopen(so_path.c_str(), RTLD_NOW);
        if (!handle) {
            const char* err = dlerror();
            LOG_ERROR(LOG_TAG, "dlopen failed: {}", err ? err : "unknown");
            return nullptr;
        }

        auto fn = reinterpret_cast<fn_subsystem_descriptor_t>(
            dlsym(handle, SUBSYSTEM_DESCRIPTOR_SYMBOL));
        if (!fn) {
            LOG_ERROR(LOG_TAG, "Missing symbol: {}", SUBSYSTEM_DESCRIPTOR_SYMBOL);
            dlclose(handle);
            return nullptr;
        }

        const SubsystemDescriptor* desc = fn();
        if (!desc || desc->abi_version != SUBSYS_ABI_VERSION) {
            LOG_ERROR(LOG_TAG, "ABI version mismatch or invalid descriptor");
            dlclose(handle);
            return nullptr;
        }

        SubsystemHandle* instance = nullptr;
        if (desc->create && desc->create(&params, &instance) != SUBSYS_OK) {
            LOG_ERROR(LOG_TAG, "Subsystem create failed");
            dlclose(handle);
            return nullptr;
        }

        auto loaded = std::make_unique<LoadedSubsystem>();
        loaded->handle = handle;
        loaded->descriptor = desc;
        loaded->instance = instance;
        return loaded;
    }

    static void unload(std::unique_ptr<LoadedSubsystem>& loaded) {
        if (!loaded) return;

        if (loaded->descriptor && loaded->descriptor->destroy && loaded->instance) {
            int ret = loaded->descriptor->destroy(loaded->instance);
            if (ret != SUBSYS_OK)
                LOG_WARN(LOG_TAG, "Subsystem destroy returned non-zero: {}", ret);
        }

        if (loaded->handle)
            dlclose(loaded->handle);

        loaded->instance = nullptr;
        loaded->descriptor = nullptr;

        loaded.reset();
    }

};



}
