#pragma once
#include <iostream>
#include <yaml-cpp/yaml.h>

#include "system_manifest.hpp"

namespace manifest {
    
class SystemManifestLoader{
public:
    
     static SystemManifest load(const std::string& path) {
        YAML::Node root = YAML::LoadFile(path);
        SystemManifest m;

        // ---------------------------
        // 전역 설정
        // ---------------------------
        m.platforms = root["platforms"].as<std::vector<std::string>>();
        m.modes = root["modes"].as<std::vector<std::string>>();
        m.restart_policys = root["restart_policys"].as<std::vector<std::string>>();

        // ---------------------------
        // system 정보
        // ---------------------------
        if (root["system"]) {
            auto sys = root["system"];
            m.system.name = sys["name"].as<std::string>("");
            m.system.description = sys["description"].as<std::string>("");
            m.system.mode = sys["mode"].as<std::string>("");
        }

        // ---------------------------
        // hosts 정보
        // ---------------------------
        if (root["hosts"]) {
            for (auto it : root["hosts"]) {
                HostInfo host;
                host.entry = it.second["entry"].as<std::string>("");
                m.hosts[it.first.as<std::string>()] = host;
            }
        }

        // ---------------------------
        // subsystems 정보
        // ---------------------------
        if (root["subsystems"]) {
            for (auto sub : root["subsystems"]) {
                SubsystemInfo s;
                s.name = sub["name"].as<std::string>("");
                s.group = sub["group"].as<std::string>("");
                s.description = sub["description"].as<std::string>("");
                s.priority = sub["priority"].as<int>(0);
                s.config = sub["config"].as<std::string>("");
                s.auto_start = sub["auto_start"].as<bool>(false);
                s.allow_version = sub["allow_version"].as<std::string>("");
                if (sub["affinity"]) s.affinity = sub["affinity"].as<std::vector<int>>();
                s.restart_policy = sub["restart_policy"].as<std::string>("");
                s.restart_delay_ms = sub["restart_delay_ms"].as<int>(0);
                s.max_retries = sub["max_retries"].as<int>(0);
                s.optional = sub["optional"].as<bool>(false);
                if (sub["denied_modes"]) s.denied_modes = sub["denied_modes"].as<std::vector<std::string>>();
                if (sub["depends_on"]) s.depends_on = sub["depends_on"].as<std::vector<std::string>>();
                m.subsystems.push_back(std::move(s));
            }
        }

        return m;
    }
};



} // namespace manifest