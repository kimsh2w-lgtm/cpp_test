#pragma once

#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "ioc.hpp"
#include "command_def.hpp"

namespace command {

class CommandRegistry {
public:
    void registerCommands(const std::vector<CommandInfo>& list) {
        for (const auto& cmd : list) {
            if (commands_.count(cmd.name) > 0) {
                LOGW("CommandRegistry: duplicate command '{}' ignored", cmd.name);
                continue;
            }
            commands_.emplace(cmd.name, cmd);
        }
        }

    const CommandInfo* find(const std::string& name) const {
        auto it = commands_.find(name);
        return (it == commands_.end()) ? nullptr : &it->second;
    }

private:
    INJECT(CommandRegistry);
    std::unordered_map<std::string, CommandInfo> commands_;
};


} // namespace command

