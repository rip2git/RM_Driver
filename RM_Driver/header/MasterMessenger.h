#ifndef MMSGR_H
#define MMSGR_H

#include "Messenger.h"
#include <map>


/*! ------------------------------------------------------------------------------------
 * @brief: Represent high level interface between messenger and user level
 * (as control station)
 * -------------------------------------------------------------------------------------
 * */
class MasterMessenger : public Messenger {
public:
	/*! ------------------------------------------------------------------------------------
	 * @brief:
	 *
	 * return value is RESULT that can be changed after this method was called
	 * -------------------------------------------------------------------------------------
	 * */
	const std::vector <Messenger::UserPack>& Send(const Messenger::UserPack &pack);

	/*! ------------------------------------------------------------------------------------
	 * @brief:
	 * -------------------------------------------------------------------------------------
	 * */
	MasterMessenger(Logger &LOG);
	~MasterMessenger();

private:
	/*! ------------------------------------------------------------------------------------
	 * @brief:
	 *
	 * NOTE: size eq 0 means no responses
	 * -------------------------------------------------------------------------------------
	 * */
	std::vector <Messenger::UserPack> result;

	/*! ------------------------------------------------------------------------------------
	 * @brief:
	 * -------------------------------------------------------------------------------------
	 * */
	void Send(const Messenger::CommandPack &cmdPack);
	void Send(
			const Messenger::ConfigPack &cfgPack,
			const std::vector<uint8_t> &data);
	void Send(const Messenger::ShortDataPack &shrtDataPack);

};



#endif
