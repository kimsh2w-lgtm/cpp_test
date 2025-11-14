#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <any>

#include <yaml-cpp/yaml.h>

#include "command_def.hpp"   // CommandInfo

namespace command {

class CommandManifestLoader {
public:
    std::vector<CommandInfo> loadFromFile(const std::string& path) {
        YAML::Node doc = YAML::LoadFile(path);

        std::vector<CommandInfo> list;
        std::string subsystem = doc["subsystem"].as<std::string>();

        for (const auto& node : doc["commands"]) {
            CommandInfo info;

            info.name = node["name"].as<std::string>();
            info.service = subsystem;

            if (node["allowed_modes"])
                info.allowed_modes = node["allowed_modes"].as<std::unordered_set<std::string>>();

            // ---------- args ----------
            // args: type specification
            if (node["args"]) {
                for (const auto& it : node["args"]) {
                    std::string key = it.first.as<std::string>();
                    std::string typeStr = it.second.as<std::string>();
                    info.arg_types[key] = parseArgType(typeStr);
                }
            }

            if (node["emit"])
                info.emit = node["emit"].as<std::vector<std::string>>();
            else
                info.emit.clear();  // 없음 → empty

            if (node["description"])
                info.description = node["description"].as<std::string>();

            list.push_back(std::move(info));
        }

        return list;
    }

private:
    // convert string → enum ArgType
    ArgType parseArgType(const std::string& s) {
        if (s == "string" || s == "String")
            return ArgType::String;
        if (s == "int" || s == "Int" )
            return ArgType::Int;
        if (s == "float" || s == "Float" )
            return ArgType::Float;
        if (s == "bool" || s == "Bool")
            return ArgType::Bool;

        return ArgType::Unknown;
    }
};

} // namespace command
