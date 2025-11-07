

#ifndef SAMPLE_SERVICE_H_
#define SAMPLE_SERVICE_H_


/**
 * @type: command
 * @command: SampleCommand
 * @allowed_modes: [normal,low_power,diagnostics]
 * @emit: event.sample.accepted, ev.sample.completed
 * @description: sample command
*/


namespace sample
{


	class SampleService
	{
	public:
		SampleService();

		~SampleService();
		
        /**
         * @type: command
         * @command: SampleCommand
         * @allowed_modes: [normal,low_power,diagnostics]
         * @emit: event.sample.accepted, ev.sample.completed
         * @description: sample command
        */
        void SampleCommand(SampleModel model);        

        /**
         * @type: command
         * @command: UploadLog
         * @allowed_modes: diagnostics, recovery
         * @emit: event.log.uploaded, event.log.failed
         * @description: Upload diagnostic logs to remote server
         */
        void UploadLog(LogBundle bundle);

        /**
         * @type: command
         * @command: GetStatus
         * @allowed_modes: normal, low_power, diagnostics, recovery
         * @emit:
         * @description: Get current system status
         */
        StatusModel GetStatus();

		
	private:
		
	}; // class SampleService


} // namespace sample
