#pragma once
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <any>
#include <vector>

#include "message.hpp"

namespace command {
    


struct CommandInfo {
    std::string name;                                // command name
    std::string service;                             // subsystem name
    std::unordered_set<std::string> allowed_modes;   // allowed modes
    
    std::unordered_map<std::string, message::ArgType> arg_types; // args: name → type
    std::vector<std::string> emit;                   // events to emit
    std::string description;                         // optional
};





}
