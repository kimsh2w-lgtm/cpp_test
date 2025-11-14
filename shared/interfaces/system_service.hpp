#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

#include "result.h"
#include "helpper.hpp"
#include "message.hpp"
#include "command_def.hpp"

namespace service {

    
#define REGISTER_COMMAND(name)                                   \
    do {                                                        \
        static_assert(#name[0]=='c' && #name[1]=='m' && #name[2]=='d', \
                      "Command handler must start with 'cmd'"); \
        commands_[ std::string(#name).substr(3) ] =             \
            [this](const message::Message& args){           \
                return this->name(args);                        \
            };                                                  \
    } while(0)



class SystemService {
public:
    virtual ~SystemService() {
        commands_.clear();
    };

    Result<void> invokeMethod(const std::string& name, const message::Message& args) {
        auto it = commands_.find(name);
        if (it != commands_.end()) {
            return it->second(args);
        }
        return Fail();
    }
protected:
    virtual void registerCommand() = 0;

    std::unordered_map<std::string, std::function<Result<void>(const message::Message& args)>> commands_;
    
}; // class SystemService



} // namespace service

