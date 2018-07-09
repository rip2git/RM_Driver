#include "SlaveMessenger.h"
#include "CrossSleep.h"



SlaveMessenger::RESULT SlaveMessenger::SendData(const std::shared_ptr<Messenger::UserPack> &pack)
{
	Messenger::RESULT msgrRes;

	if (pack.get()->Data.size() == 0)
		return SlaveMessenger::RESULT::SUCCESS;
	else if (pack.get()->Data.size() > Messenger::ShortDataPack::MAX_DATA_SIZE)
		return SlaveMessenger::RESULT::ERROR;

	Messenger::ShortDataPack shrtDataPack;
	shrtDataPack.Flags = Messenger::Flags::Data | Flags::Short | Flags::Slave;
	shrtDataPack.Source = this->deviceID;
	shrtDataPack.Destination = pack.get()->DeviceID;
	shrtDataPack.TransactionID = this->transacID;
	shrtDataPack.DataSize = static_cast <uint8_t> ( pack.get()->Data.size() );
	shrtDataPack.Data.assign(pack.get()->Data.begin(), pack.get()->Data.end());

	double sleep_time = this->lastRequestedID == this->broadcastID?
			this->deviceID : 2.0;
	sleep_time = (sleep_time - MSGR_DELAY_BIAS) * static_cast <double> (this->dataDelay);
	CrossSleep( static_cast <uint32_t> (sleep_time));
	msgrRes = this->SendPack(std::static_pointer_cast<Messenger::SystemPack>(
			std::make_shared<Messenger::ShortDataPack>(shrtDataPack) ));
	if (msgrRes == Messenger::RESULT::SUCCESS) {
		*this->LOG << "D ";
		this->LOG->flush();
		return SlaveMessenger::RESULT::SUCCESS;
	} else {
		*this->LOG << "E ";
		this->LOG->flush();
		return SlaveMessenger::RESULT::ERROR;
	}
}



SlaveMessenger::RESULT SlaveMessenger::Listening(std::shared_ptr<Messenger::UserPack> &packBack)
{
	std::shared_ptr <SystemPack> msgrPack;
	Messenger::RESULT msgrRes;

	std::shared_ptr <Messenger::ConfigPack> confPack;
	packBack.reset();

	uint32_t iter = 0;		// data-byte iterator
	uint32_t parts = 0; 	// for receiving
	uint32_t part = 0;
	uint32_t remainder = 0;
	bool RepeatLongData = false;

	this->SetBroadcast();

	while (1) {
//		if (this->chDeleg != nullptr) { // pipe check
//			if (!this->chDeleg->Check()) { // catch thread for recovery
//				return SlaveMessenger::RESULT::ERROR;
//			}
//		}

		msgrRes = this->RecvPack(msgrPack);

		if (msgrRes == Messenger::RESULT::SYSErr)
			return SlaveMessenger::RESULT::ERROR;

		if (msgrRes == Messenger::RESULT::SUCCESS) {
			switch (msgrPack.get()->GetPackType()) {
			//--------------------------------------------------------------------------------------------------------------------------------------
			case Messenger::SystemPack::PackType::CommandPack: {
				std::shared_ptr<Messenger::CommandPack> cmdPack =
					std::static_pointer_cast<Messenger::CommandPack>(msgrPack);

				if (cmdPack.get()->Destination == this->deviceID ||
					cmdPack.get()->Destination == this->broadcastID)
				{
					*this->LOG << this->LOG->Time << "R_cmd ";
					this->LOG->flush();

					this->transacID = cmdPack.get()->TransactionID;
					this->lastRequestedID = cmdPack.get()->Destination;
					packBack.reset(new UserPack(Messenger::UserPack::PackType::Command));
					packBack.get()->Command = static_cast<Command>(cmdPack.get()->Flags);
					packBack.get()->DeviceID = cmdPack.get()->Source;

					// without answer ----------------------------------------
					if (cmdPack.get()->Destination == this->broadcastID)
						return SlaveMessenger::RESULT::SUCCESS;
					// -------------------------------------------------------

					switch ( Messenger::Convert(cmdPack.get()->Flags) ) {
					case Command::SysCommand::GetGeolocation: {
						// without answer
						return SlaveMessenger::RESULT::SUCCESS;
					} break;
					default: {
						Messenger::AnswerPack ansPack;
						ansPack.Flags = cmdPack.get()->Flags;
						ansPack.Source = this->deviceID;
						ansPack.Destination = cmdPack.get()->Source;
						ansPack.TransactionID = cmdPack.get()->TransactionID;

						double sleep_time = this->lastRequestedID == this->broadcastID?
								this->deviceID : 2.0;
						sleep_time = (sleep_time - MSGR_DELAY_BIAS) * static_cast <double> (this->dataDelay);
						CrossSleep( static_cast <uint32_t> (sleep_time));
						msgrRes = this->SendPack(std::static_pointer_cast<Messenger::SystemPack>(
								std::make_shared<Messenger::AnswerPack>(ansPack) ));
						if (msgrRes == Messenger::RESULT::SUCCESS) {
							*this->LOG << "A ";
							this->LOG->flush();
						} else {
							*this->LOG << "E ";
							this->LOG->flush();
							return SlaveMessenger::RESULT::ERROR;
						}

						if ( !this->isRepeat(ansPack, cmdPack.get()->FCS) ) { // else just responded on cmd (for repeats)
							return SlaveMessenger::RESULT::SUCCESS;
						}
					} break;
					}
				}
			} break;
			//--------------------------------------------------------------------------------------------------------------------------------------
			case Messenger::SystemPack::PackType::ConfigPack: {
				confPack.reset(new Messenger::ConfigPack( *dynamic_cast<Messenger::ConfigPack*>(msgrPack.get()) ));

				if (confPack.get()->Destination == this->deviceID ||
					confPack.get()->Destination == this->broadcastID)
				{
					*this->LOG << this->LOG->Time << "R_cfg ";
					this->LOG->flush();

					this->transacID = confPack.get()->TransactionID;
					this->lastRequestedID = confPack.get()->Destination;

					// data reset ---------------------------------------------------------------
					packBack.reset(new UserPack(Messenger::UserPack::PackType::Data));
					packBack.get()->Command = ::Command::SysCommand::SendData;
					packBack.get()->DeviceID = confPack.get()->Source;
					packBack.get()->Data.resize(confPack.get()->TotalSize);
					parts = confPack.get()->TotalSize / confPack.get()->BufferSize;
					remainder = confPack.get()->TotalSize - confPack.get()->BufferSize * parts;
					parts += (confPack.get()->BufferSize * parts < confPack.get()->TotalSize) ? 1 : 0;
					part = 0;
					this->expectedDataSize = confPack.get()->BufferSize;
					// ---------------------------------------------------------------------------

					Messenger::AnswerPack ansPack;
					ansPack.Flags = (Messenger::Flags::Config | Messenger::Flags::Ack | Flags::Slave);
					ansPack.Source = this->deviceID;
					ansPack.Destination = confPack.get()->Source;
					ansPack.TransactionID = confPack.get()->TransactionID;

					double sleep_time = this->lastRequestedID == this->broadcastID?
							this->deviceID : 2.0;
					sleep_time = (sleep_time - MSGR_DELAY_BIAS) * static_cast <double> (this->dataDelay);
					CrossSleep( static_cast <uint32_t> (sleep_time));
					msgrRes = this->SendPack(std::static_pointer_cast<Messenger::SystemPack>(
							std::make_shared<Messenger::AnswerPack>(ansPack) ));
					if (msgrRes == Messenger::RESULT::SUCCESS) {
						*this->LOG << "A ";
						this->LOG->flush();
					} else {
						*this->LOG << "E ";
						this->LOG->flush();
						return SlaveMessenger::RESULT::ERROR;
					}
					RepeatLongData = this->isRepeat(ansPack, confPack.get()->FCS);
				}
			} break;
			//--------------------------------------------------------------------------------------------------------------------------------------
			case Messenger::SystemPack::PackType::DataPack: {
				if (packBack.use_count() != 0) {
					std::shared_ptr<Messenger::DataPack> dataPack =
						std::static_pointer_cast<Messenger::DataPack>(msgrPack);

					// 'iter' depends on 'part' variable
					// changes part for first message and for retries
					if ((!confPack.get()->TrustPacks) || (dataPack.get()->Part % (confPack.get()->TrustPacks + 1) == 1))
						part = dataPack.get()->Part;
					else
						part++;

					if (dataPack.get()->TransactionID == confPack.get()->TransactionID &&
						dataPack.get()->Part == part)
					{
						iter = (part - 1) * confPack.get()->BufferSize;
						for (uint8_t i = 0; i < dataPack.get()->Data.size(); ++i)
							packBack.get()->Data[iter + i] = dataPack.get()->Data[i];

						if (!confPack.get()->TrustPacks ||
							!(part % (confPack.get()->TrustPacks + 1)) ||
							part == parts)
						{
							for (int k = 0; k < confPack.get()->TrustPacks + 1; ++k)
								*this->LOG << "D";
							*this->LOG << " ";
							this->LOG->flush();

							Messenger::AnswerPack ansPack;
							ansPack.Flags = (Messenger::Flags::Data | Messenger::Flags::Ack | Flags::Slave);
							ansPack.Source = this->deviceID;
							ansPack.Destination = confPack.get()->Source;
							ansPack.TransactionID = confPack.get()->TransactionID;

							double sleep_time = this->lastRequestedID == this->broadcastID?
									this->deviceID : 2.0;
							sleep_time = (sleep_time - MSGR_DELAY_BIAS) * static_cast <double> (this->dataDelay);
							CrossSleep( static_cast <uint32_t> (sleep_time));
							msgrRes = this->SendPack(std::static_pointer_cast<Messenger::SystemPack>(
									std::make_shared<Messenger::AnswerPack>(ansPack) ));
							if (msgrRes == Messenger::RESULT::SUCCESS) {
								*this->LOG << "A ";
								this->LOG->flush();
							} else {
								*this->LOG << "E ";
								this->LOG->flush();
								return SlaveMessenger::RESULT::ERROR;
							}
						}

						if (part == parts && !RepeatLongData) { // RepeatLongData up when previous conf is received
							return SlaveMessenger::RESULT::SUCCESS;
						}
					} else {
						part = 0; // blocks next error-packs
					}

					if (((part + 1) == parts) && remainder) { // for last pack
						this->expectedDataSize = remainder;
					} else {
						this->expectedDataSize = confPack.get()->BufferSize;
					}
				}
			} break;
			//--------------------------------------------------------------------------------------------------------------------------------------
			case Messenger::SystemPack::PackType::ShortDataPack: {
				std::shared_ptr<Messenger::ShortDataPack> shortDataPack =
					std::static_pointer_cast<Messenger::ShortDataPack>(msgrPack);

				if (shortDataPack.get()->Destination == this->deviceID ||
					shortDataPack.get()->Destination == this->broadcastID)
				{
					*this->LOG << this->LOG->Time << "R_d ";
					this->LOG->flush();

					this->transacID = shortDataPack.get()->TransactionID;
					this->lastRequestedID = shortDataPack.get()->Destination;

					packBack.reset(new UserPack(Messenger::UserPack::PackType::Data));
					packBack.get()->Command = ::Command::SysCommand::SendData;
					packBack.get()->DeviceID = shortDataPack.get()->Source;
					packBack.get()->Data.assign(shortDataPack.get()->Data.begin(), shortDataPack.get()->Data.end());

					// without answer ----------------------------------------
					if (shortDataPack.get()->Destination == this->broadcastID)
						return SlaveMessenger::RESULT::SUCCESS;
					// -------------------------------------------------------

					Messenger::AnswerPack ansPack;
					ansPack.Flags = (Messenger::Flags::Data | Messenger::Flags::Short | Messenger::Flags::Ack | Flags::Slave);
					ansPack.Source = this->deviceID;
					ansPack.Destination = shortDataPack.get()->Source;
					ansPack.TransactionID = shortDataPack.get()->TransactionID;

					double sleep_time = this->lastRequestedID == this->broadcastID?
							this->deviceID : 2.0;
					sleep_time = (sleep_time - MSGR_DELAY_BIAS) * static_cast <double> (this->dataDelay);
					CrossSleep( static_cast <uint32_t> (sleep_time));
					msgrRes = this->SendPack(std::static_pointer_cast<Messenger::SystemPack>(
							std::make_shared<Messenger::AnswerPack>(ansPack) ));
					if (msgrRes == Messenger::RESULT::SUCCESS) {
						*this->LOG << "A ";
						this->LOG->flush();
					} else {
						*this->LOG << "E ";
						this->LOG->flush();
						return SlaveMessenger::RESULT::ERROR;
					}

					if ( !this->isRepeat(ansPack, shortDataPack.get()->FCS) ) { // else just responded on cmd (for retries)
						return SlaveMessenger::RESULT::SUCCESS;
					}
				}
			} break;
			//--------------------------------------------------------------------------------------------------------------------------------------
			default: break;
			//--------------------------------------------------------------------------------------------------------------------------------------
			}
		}
	}
}



bool SlaveMessenger::isRepeat(const AnswerPack &pack, uint16_t FCS)
{
	bool res = false;

	if (this->lastPack[0] == pack.Flags &&
		this->lastPack[1] == pack.Source &&
		this->lastPack[2] == pack.Destination &&
		this->lastPack[3] == pack.TransactionID &&
		this->lastPack[4] == pack.FCS &&
		(this->lastPack[5] | this->lastPack[6] << 8) == FCS)
		res = true;
	this->lastPack[0] = pack.Flags;
	this->lastPack[1] = pack.Source;
	this->lastPack[2] = pack.Destination;
	this->lastPack[3] = pack.TransactionID;
	this->lastPack[4] = pack.FCS;
	this->lastPack[5] = static_cast<uint8_t>(FCS);
	this->lastPack[6] = static_cast<uint8_t>(FCS >> 8);
	res = res && (this->duplicateTimer.since() < ((this->ansDelay + this->dataDelay) * 32));
	this->duplicateTimer.start(0);

	return res;
}



SlaveMessenger::SlaveMessenger(Logger &LOG) : Messenger(LOG)
{
	this->lastPack.resize(7);
	this->chDeleg = nullptr;
}



void SlaveMessenger::SetExChecker(CheckDelegate *chDeleg)
{
	this->chDeleg = chDeleg;
}



SlaveMessenger::~SlaveMessenger()
{}


