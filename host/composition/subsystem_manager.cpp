#include <map>
#include <memory>
#include <vector>
#include <string>
#include <fmt/core.h>

#include "subsystem_manager.hpp"
#include "system_manifest.hpp"

namespace composition {

template<typename Fn>
void SubsystemManager::forEachController(Fn&& fn) {
    for (auto& [name, ctrl] : controllers_)
        fn(name, *ctrl);
}

template <typename Call>
bool SubsystemManager::callAllControllers(const char* action, Call&& fn) {
    bool all_ok = true;
    forEachController([&](const auto& name, auto& ctrl) {
        if (!(ctrl.*fn)()) {
            LOGE("Subsystem {} failed: {}", action, name);
            all_ok = false;
        }
    });
    return all_ok;
}

bool SubsystemManager::load(const manifest::SystemManifest& manifest) {
    for (const auto& info : manifest.subsystems) {
        const std::string so_path = fmt::format("lib{}.so", info.name);

        // TODO: param 정의후 manifest에서 정보 넣어서 만들어야함
        SubsystemParams params = {0, info.config.c_str(), 0, "system_manifest.yaml"};

        auto loaded = SubsystemLoader::load(so_path, params);
        if (!loaded) {
            if (info.optional) {
                LOGW("Failed to load subsystem(optional): {}", info.name);
                continue;
            }
            LOGE("Failed to load subsystem: {}", info.name);
            return false;
        }

        // SubsystemController 생성 및 등록
        auto controller = std::make_unique<SubsystemController>(info.name, std::move(loaded));
        controllers_[info.name] = std::move(controller);

        LOGI("Loaded subsystem controller: {}", info.name);
    }
    return true;
}

void SubsystemManager::unloadAll() {
    forEachController([&](const auto& name, auto& ctrl) {
        ctrl.unload();
    });
    controllers_.clear();
}

bool SubsystemManager::registryModuleAll() {
    return callAllControllers("registry", &SubsystemController::registry);
}

bool SubsystemManager::registryAll() {
    return callAllControllers("registry module", &SubsystemController::registryModule);
}

bool SubsystemManager::initializeAll() {
    return callAllControllers("initialize", &SubsystemController::initialize);
}

bool SubsystemManager::selfTestAll() {
    return callAllControllers("self test", &SubsystemController::selfTest);
}

bool SubsystemManager::configureAll() {
    return callAllControllers("configure", &SubsystemController::configure);
}

bool SubsystemManager::readyAll() {
    return callAllControllers("ready", &SubsystemController::ready);
}

bool SubsystemManager::startAll() {
    return callAllControllers("start", &SubsystemController::start);
}

bool SubsystemManager::pauseAll() {
    return callAllControllers("pause", &SubsystemController::pause);
}

bool SubsystemManager::stopAll() {
    return callAllControllers("stop", &SubsystemController::stop);
}

bool SubsystemManager::recoveryAll() {
    return callAllControllers("recovery", &SubsystemController::recovery);
}

bool SubsystemManager::safeAll() {
    return callAllControllers("safe", &SubsystemController::safe);
}

bool SubsystemManager::systemModeAll(SystemModeType mode) {
    bool all_ok = true;

    forEachController([&](const auto& name, auto& ctrl) {
        if(!ctrl.systemMode(mode)) {
            LOGE("Subsystem system mode failed: {}", action, name);
            all_ok = false;
        }
    });
    return all_ok;
}

SubsystemController* SubsystemManager::getController(const std::string& name) {
    auto it = controllers_.find(name);
    return (it != controllers_.end()) ? it->second.get() : nullptr;
}


}
