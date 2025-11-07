#pragma once
#include <algorithm>
#include <memory>
#include <mutex>
#include <iostream>

#include "system_manifest.hpp"

namespace manifest {
    
class SubSystemManifestLoader{
public:
    // 매니페스트 로드
    bool load(const std::string& path);
    

private:
    SubSystemManifestLoader() = default;
    SubSystemManifestLoader(const SubSystemManifestLoader&) = delete;
    SubSystemManifestLoader& operator=(const SubSystemManifestLoader&) = delete;

    mutable std::mutex mutex_;
};



} // namespace manifest