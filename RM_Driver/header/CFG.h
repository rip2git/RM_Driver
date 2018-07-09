#ifndef CFG_H
#define CFG_H


namespace CFG {
	const char* const FILE_NAME 			= "config.ini";

	namespace MAIN {
		const char* const SECTION 			= "MAIN_SECTION";
		const char* const LOG_MODE 			= "log_mode";
		const char* const DEVICE_ID 		= "device_id";
		const char* const PIPE_DELAY 		= "max_pipe_delay";
	};

	namespace MSGR {
		const char* const SECTION			= "MSGR_SECTION";
		const char* const SLAVES 			= "slaves";
		const char* const HIGHER_SLAVE 		= "higher_slave_id";
		const char* const BRDCST_ID 		= "broadcast_id";
		const char* const ANS_DELAY 		= "ans_delay";
		const char* const DATA_DELAY 		= "data_delay";
		const char* const BUF_SIZE 			= "buffer_size";
		const char* const TRUST_PACKS 		= "trust_packs";
		const char* const RETRIES 			= "repeats";
	};

	namespace COM {
		const char* const SECTION 			= "COM_SECTION";
		const char* const COM_NAME 			= "com_name";
		const char* const BAUD_RATE 		= "baud_rate";
		const char* const AIR_BAUD_RATE 	= "air_baud_rate";
		const char* const TLMTR_TIME_OUT 	= "telemetry_time_out";
		const char* const BRCST_TIME_OUT 	= "broadcast_time_out";
		const char* const UNICST_TIME_OUT 	= "unicast_time_out";
	};

	namespace PIPE {
		const char* const SECTION			= "PIPE_SECTION";
		const char* const WRP_NAME 			= "wr_pipe_name";
		const char* const RDP_NAME 			= "rd_pipe_name";
	};
};


#endif
