#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "result.h"
#include "message.hpp"
#include "ioc.hpp"
#include "system_service.hpp"


namespace sample {

class SampleService : public service::SystemService {
private:
    

public:
    SampleService() {
        registerCommand();
    };

    ~SampleService();
    
    /**
     * @type: command
     * @command: Sample
     * @allowed_modes: [normal,low_power,diagnostics]
     * @args: [sample:"string", op:"int"] 
     * @emit: SampleAccepted, event.sample.completed
     * @description: sample command
    */
   Result<void> cmdSample(const message::Message& args);        

    /**
     * @type: command
     * @command: UploadLog
     * @allowed_modes: diagnostics, recovery
     * @args: [log:"string"] 
     * @emit: event.log.uploaded, event.log.failed
     * @description: Upload diagnostic logs to remote server
     */
    Result<void> cmdUploadLog(const message::Message& args);

    /**
     * @type: command
     * @command: GetStatus
     * @allowed_modes: normal, low_power, diagnostics, recovery
     * @emit:
     * @description: Get current system status
     */
    Result<void> cmdGetStatus(const message::Message& args);

protected:
    void registerCommand() {
        REGISTER_COMMAND(cmdSample);
        REGISTER_COMMAND(cmdUploadLog);
        REGISTER_COMMAND(cmdGetStatus);
    }

private:
    INJECT(SampleService)

}; // class SampleService

} // namespace sample

