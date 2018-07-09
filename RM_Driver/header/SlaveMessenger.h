#ifndef SMSGR_H
#define SMSGR_H

#include "TON.h"
#include "CheckDelegate.h"
#include "Messenger.h"



/*! ------------------------------------------------------------------------------------
 * @brief: Represent high level interface between messenger and user level (as drone).
 * -------------------------------------------------------------------------------------
 * */
class SlaveMessenger : public Messenger {
public:
	/*! ------------------------------------------------------------------------------------
	 * @brief:
	 * -------------------------------------------------------------------------------------
	 * */
	enum class RESULT {
		ERROR = 0,
		SUCCESS
	};

	/*! ------------------------------------------------------------------------------------
	 * @brief: allows to send telemetry (or other data) to master after receiving command
	 * from Listening process. Configures data pack from user pack;
	 * send short pack or several packs with config pack (depends on this->bufferSize)
	 * tries to send resulting pack and receives answer from master 'repeats' times;
	 * sets this->ANSWERS which contains ID of responded devices
	 *
	 * return value is
	 * -------------------------------------------------------------------------------------
	 * */
	SlaveMessenger::RESULT SendData(const std::shared_ptr<Messenger::UserPack> &pack);

	/*! ------------------------------------------------------------------------------------
	 * @brief: listens to the transmission environment. reads first byte from port and
	 * defines type of received pack; sets returned pack with corresponding type of pack;
	 *
	 * NOTE: can be interrupted by external condition (if chDeleg was defined)
	 *
	 * return value is
	 * -------------------------------------------------------------------------------------
	 * */
	SlaveMessenger::RESULT Listening(std::shared_ptr<Messenger::UserPack> &packBack);

	/*! ------------------------------------------------------------------------------------
	 * @brief: leads to check external dependencies while Listening is processing
	 * -------------------------------------------------------------------------------------
	 * */
	void SetExChecker(CheckDelegate *chDeleg);

	/*! ------------------------------------------------------------------------------------
	 * @brief:
	 * -------------------------------------------------------------------------------------
	 * */
	SlaveMessenger(Logger &LOG);
	~SlaveMessenger();

private:
	/*! ------------------------------------------------------------------------------------
	 * @brief: timer to detect a duplicate command
	 * -------------------------------------------------------------------------------------
	 * */
	TON duplicateTimer;

	/*! ------------------------------------------------------------------------------------
	 * @brief: supporting pack to detect a duplicate command
	 * -------------------------------------------------------------------------------------
	 * */
	std::vector <uint8_t> lastPack;

	/*! ------------------------------------------------------------------------------------
	 * @brief:
	 * -------------------------------------------------------------------------------------
	 * */
	uint8_t lastRequestedID;

	/*! ------------------------------------------------------------------------------------
	 * @brief: a set of responded devices; specified when telemetry command is received
	 * -------------------------------------------------------------------------------------
	 * */
	std::map <uint8_t, bool> ANSWERS;

	/*! ------------------------------------------------------------------------------------
	 * @brief: delegator for external checking; allows to interrupt Listening
	 * (if pipes have broken)
	 * -------------------------------------------------------------------------------------
	 * */
	CheckDelegate *chDeleg;

	/*! ------------------------------------------------------------------------------------
	 * @brief: allows to detect a duplicate command
	 * -------------------------------------------------------------------------------------
	 * */
	bool isRepeat(const AnswerPack &pack, uint16_t FCS);

};

#endif
