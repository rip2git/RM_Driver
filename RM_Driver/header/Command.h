#ifndef COMMAND_H_
#define COMMAND_H_

#include <stdint.h>


/*! ------------------------------------------------------------------------------------
 * @brief:
 * -------------------------------------------------------------------------------------
 * */
struct Command {
	/*! ------------------------------------------------------------------------------------
	 * @brief:
	 * -------------------------------------------------------------------------------------
	 * */
	struct SysCommand {
		enum {
			Reserved = 0,
			SendData = 1,
			GetGeolocation = 2,
			R3, R4, R5, R6, R7, R8, R9, R10
		};
		operator Command() {
			return static_cast<Command>(*this);
		}
	};

	/*! ------------------------------------------------------------------------------------
	 * @brief:
	 * -------------------------------------------------------------------------------------
	 * */
	union {
		uint8_t _raw;
		SysCommand sysCmd;
		uint8_t userCmd;
	};

	Command& operator=(uint8_t ucmd) {
		this->userCmd = ucmd;
		return *this;
	}
	Command& operator=(SysCommand scmd) {
		this->sysCmd = scmd;
		return *this;
	}
	bool operator==(const Command& scmd) {
		return (this->_raw == scmd._raw);
	}
	operator uint8_t() const {
		return this->_raw;
	}
	operator SysCommand() const {
		return this->sysCmd;
	}
};

#endif /* COMMAND_H_ */
