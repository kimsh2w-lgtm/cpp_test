#pragma once
#include <map>
#include <memory>
#include <vector>
#include <string>

#include "host_base.hpp"
#include "system_manifest.hpp"
#include "subsystem_loader.hpp"

namespace composition {
    
class SubsystemController  {
public:
    explicit SubsystemController(const std::string name, 
        std::unique_ptr<SubsystemLoader::LoadedSubsystem> subsystem)
        : name_(name),
          subsystem_(std::move(subsystem)) {}

    bool initialize();
    bool selfTest();
    bool configure();
    bool ready();
    bool start();
    bool pause();
    bool stop();
    bool recovery();
    bool safe();
    bool systemMode(SystemModeType mode);

    bool query(uint32_t code, void* in, void* out);

    const std::string& name() const { return name_; }


    bool registry();   // IoC Container registry
    bool registryModule(); // Module IoC Container registry
    void unload();

private:
    const std::string name_;
    std::unique_ptr<SubsystemLoader::LoadedSubsystem> subsystem_;

    inline static constexpr const char* LOG_TAG = "SubsystemController";
};


} // namespace composition
