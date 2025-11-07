#pragma once
#include <algorithm>
#include <memory>
#include <mutex>
#include <iostream>

#include "system_manifest.hpp"
#include "system_manifest_loader.hpp"


namespace manifest {

static const std::string TAG = "manifest";

class SystemManifestManager {
public:

    bool load(const std::string& path);

    bool isLoaded() const noexcept { return loaded_; }

    const SystemManifest& getManifest() const {
        std::lock_guard<std::mutex> lock(mutex_);
        ensureLoaded();
        return manifest_;
    }

    const std::vector<SubsystemInfo>& getSubsystems() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        ensureLoaded();
        return manifest_.subsystems; 
    }

    const SystemInfo& getSystemInfo() const {
        std::lock_guard<std::mutex> lock(mutex_);
        ensureLoaded();
        return manifest_.system;
    }

    const std::map<std::string, HostInfo>& getHosts() const {
        std::lock_guard<std::mutex> lock(mutex_);
        ensureLoaded();
        return manifest_.hosts;
    }

    const SubsystemInfo* findSubsystem(const std::string& name) const {
        for (const auto& s : manifest_.subsystems)
            if (s.name == name) return &s;
        return nullptr;
    }

    std::vector<SubsystemInfo> getAllowedSubsystems(const std::string& mode) const {
        std::vector<SubsystemInfo> result;
        for (const auto& s : manifest_.subsystems) {
            if (std::find(s.denied_modes.begin(), s.denied_modes.end(), mode) == s.denied_modes.end()) {
                result.push_back(s);
            }
        }
        return result;
    }


private:
    SystemManifestManager() = default;
    SystemManifestManager(const SystemManifestManager&) = delete;
    SystemManifestManager& operator=(const SystemManifestManager&) = delete;

    void ensureLoaded() const {
        if (!loaded_)
            throw std::runtime_error("Manifest not loaded");
    }
    
    void sortSubsystems();
    void printRestartPolicies() const;
    
    bool loaded_ = false;
    SystemManifest manifest_;
    mutable std::mutex mutex_;
};


} // namespace manifest