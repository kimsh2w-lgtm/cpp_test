#pragma once
#include "system_manifest_manager.hpp"

#include <algorithm>
#include <memory>
#include <mutex>
#include <iostream>

#include "host_base.hpp"
#include "system_manifest.hpp"
#include "system_manifest_loader.hpp"

namespace manifest {

bool SystemManifestManager::load(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (loaded_) {
        LOGE("Manifest already loaded.");
        return false;
    }

    try {
        manifest_ = SystemManifestLoader::load(path);
        loaded_ = true;
        LOGD("Manifest loaded successfully (read-only mode)");
        return true;
    } catch (const std::exception& e) {
        LOGE("Failed to load manifest: ", e.what());
        return false;
    }
}

void SystemManifestManager::sortSubsystems() {
    std::sort(manifest_.subsystems.begin(), manifest_.subsystems.end(),
        [](const SubsystemInfo& a, const SubsystemInfo& b) {
            return a.priority > b.priority;  // 큰 숫자가 먼저
    });
}

void SystemManifestManager::printRestartPolicies() const {
    for (const auto& s : manifest_.subsystems)
        LOGD(" restart_policy={}", s.restart_policy);
}

} // namespace manifest