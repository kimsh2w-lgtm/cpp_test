#pragma once
#include <map>
#include <memory>
#include <vector>
#include <string>

#include "host_base.hpp"
#include "system_manifest.hpp"
#include "subsystem_loader.hpp"
#include "subsystem_controller.hpp"

namespace composition {
    
class SubsystemManager {
public:
    bool load(const manifest::SystemManifest& manifest);

    bool initializeAll();
    bool selfTestAll();
    bool configureAll();
    bool readyAll();
    bool startAll();
    bool pauseAll();
    bool stopAll();
    bool recoveryAll();
    bool safeAll();
    bool systemModeAll(SystemModeType mode);

    bool registryModuleAll();
    bool registryAll();
    void unloadAll();

    SubsystemController* getController(const std::string& name);

private:
    template<typename Fn>
    void forEachController(Fn&& fn);
    
    template <typename Call>
    bool callAllControllers(const char* action, Call&& fn);

    std::map<std::string, std::unique_ptr<SubsystemController>> controllers_;

    inline static constexpr const char* LOG_TAG = "SubsystemManager";
};


}
