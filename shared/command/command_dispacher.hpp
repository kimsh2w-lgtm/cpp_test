#pragma once

#include "result.h"
#include "ioc.hpp"
#include "command_def.hpp"
#include "command_registry.hpp"
#include "command_helper.hpp"
#include "system_service.hpp"
#include "system_config.hpp"

#include "message.hpp"

namespace command {


class CommandDispatcher {
public:
    CommandDispatcher() {
        registry_ = Resolve<CommandRegistry>();
    }

    Result<void> dispatch(const std::string& command, const message::Message& args) {
        if(!registry_) return Error(ResultCode::NotFound, "command repo not found");
        auto info = registry_->find(command);
        if (!info) {
            return Error(ResultCode::NotFound, "Unknown command");
        }

        std::string mode = getCurrentMode();
        
        if (!isModeAllowed(*info, mode)) {
            LOGW("Ignoring command {}: mode '{}' is not allowed", 
                     command, mode);
            return Error(ResultCode::PermissionDenied, "command ignored"); // ignore
        }

        if (!invokeServiceCommand(info->service, command, args))
            return Error(ResultCode::InvalidArgument, "Invalid method");

        return OK();
    }

private:
    INJECT(CommandDispatcher)

    bool isModeAllowed(const CommandInfo& info, const std::string& mode) {
        return info.allowed_modes.count(mode) > 0;
    }

    std::string getCurrentMode() {
        auto config = Resolve<config::SystemConfig>();
        if (!config)
            return "normal"; // default mode
        return config->getMode();
    }

    Result<void> invokeServiceCommand(const std::string& service,
                                                const std::string& command,
                                                const message::Message& args) {
        auto meta = registry_->find(command);
        if (meta) {
            std::string errMsg;
            if (!CommandHelper::validate(*meta, args, errMsg)) {
                return Error(ResultCode::InvalidArgument, errMsg);
            }
        }

        auto system = Resolve<service::SystemService>(service);
        if (!system)
            return Error(ResultCode::InvalidState, "Service not found");

        return system->invokeMethod(command, args);
    }


    std::shared_ptr<CommandRegistry> registry_;

    
};



}
