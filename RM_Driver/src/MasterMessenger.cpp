#include "MasterMessenger.h"
#include "CrossSleep.h"


const std::vector <Messenger::UserPack>& MasterMessenger::Send(const Messenger::UserPack &pack)
{
	this->result.clear(); // free previous result
	this->ClearRecvdStuff();

	// set timeout for answer
	if (pack.DeviceID == this->broadcastID) {
		this->SetBroadcast();
	} else {
		switch (pack.Command) {
		case Command::SysCommand::GetGeolocation: {
			this->SetTelemetry();
		} break;
		default: {
			this->SetUnicast();
		} break;
		}
	}

	// request-response automats
	switch (pack.GetPackType()) {
	case Messenger::UserPack::PackType::Command: {
		Messenger::CommandPack cmdPack;
		cmdPack.Flags = pack.Command;
		cmdPack.Source = this->deviceID;
		cmdPack.Destination = pack.DeviceID;
		cmdPack.TransactionID = this->transacID;
		this->Send(cmdPack);
	} break;
	case Messenger::UserPack::PackType::Data: {
		if (pack.Data.size() == 0)
			break;
		if (pack.Data.size() <= this->bufferSize) {
			Messenger::ShortDataPack shrtDataPack;
			shrtDataPack.Flags = Messenger::Flags::Data | Messenger::Flags::Short;
			shrtDataPack.Source = this->deviceID;
			shrtDataPack.Destination = pack.DeviceID;
			shrtDataPack.TransactionID = this->transacID;
			shrtDataPack.DataSize = pack.Data.size();
			shrtDataPack.Data.assign(pack.Data.begin(), pack.Data.end());
			this->Send(shrtDataPack);
		} else {
			Messenger::ConfigPack cfgPack;
			cfgPack.Flags = Messenger::Flags::Config;
			cfgPack.Source = this->deviceID;
			cfgPack.Destination = pack.DeviceID;
			cfgPack.TransactionID = this->transacID;
			cfgPack.TotalSize = pack.Data.size();
			cfgPack.BufferSize = this->bufferSize;
			cfgPack.TrustPacks = this->trustPacks;
			this->Send(cfgPack, pack.Data);
		}
	} break;
	default: {
		// nop
	} break;
	}

	this->transacID++;
	return this->result;
}



void MasterMessenger::Send(const Messenger::CommandPack &cmdPack)
{
	std::shared_ptr <Messenger::SystemPack> msgrPack;
	Messenger::RESULT msgrRes;

	// for registering answers from slaves
	std::vector <bool> answers(this->nSlaves + 2);

	Command command = cmdPack.Flags; // current command
	for (int i = 0; i < this->repeats; ++i) {
		msgrRes = SendPack(std::static_pointer_cast<Messenger::SystemPack>(
				std::make_shared<Messenger::CommandPack>(cmdPack) ));
		if (msgrRes != Messenger::RESULT::SUCCESS)
			return;

		*this->LOG << this->LOG->Time << "R_cmd ";
		this->LOG->flush();

		if (cmdPack.Destination == this->broadcastID) { // without responses
			return; // return {cmd}{255}{0} to user
		} // ---------------------------------------------------------------

		uint8_t expectedResponses = cmdPack.Destination == this->broadcastID?
				this->nSlaves : 1;
		for (int j = 0; j < expectedResponses; ++j) {
			msgrRes = this->RecvPack(msgrPack);
			if (msgrRes != Messenger::RESULT::SUCCESS) {
				if (msgrRes == Messenger::RESULT::TIMEOUT) {
					*this->LOG << "T ";
					this->LOG->flush();
					break;	// for (j)
				} else {
					*this->LOG << "E ";
					this->LOG->flush();
					if (msgrRes == Messenger::RESULT::FCSErr) {
						continue; 	// for (j)
					} else if (msgrRes == Messenger::RESULT::SYSErr) {
						return; 	// method
					}
				}
			}

			// response handling
			switch (command) {
			case Command::SysCommand::GetGeolocation: {
				std::shared_ptr<Messenger::ShortDataPack> shrtDataPack =
						std::static_pointer_cast<Messenger::ShortDataPack>(msgrPack);
				if (shrtDataPack.use_count() != 0) {
					if (shrtDataPack.get()->Destination == cmdPack.Source &&
						shrtDataPack.get()->TransactionID == cmdPack.TransactionID)
					{
						if (answers[shrtDataPack.get()->Source] == false) {
							answers[shrtDataPack.get()->Source] = true;
							Messenger::UserPack pack(Messenger::UserPack::PackType::Data);
							pack.DeviceID = shrtDataPack.get()->Source;
							pack.Command = command;
							pack.Data.assign(shrtDataPack.get()->Data.begin(), shrtDataPack.get()->Data.end());
							this->result.push_back(pack);
						}
						*this->LOG << "A ";
						this->LOG->flush();
					}
				}
			} break;
			default: {
				std::shared_ptr<Messenger::AnswerPack> ansPack =
						std::static_pointer_cast<Messenger::AnswerPack>(msgrPack);
				if (ansPack.use_count() != 0) {
					if (ansPack.get()->Destination == cmdPack.Source &&
						ansPack.get()->TransactionID == cmdPack.TransactionID)
					{
						if (answers[ansPack.get()->Source] == false) {
							answers[ansPack.get()->Source] = true;
							Messenger::UserPack pack(Messenger::UserPack::PackType::Command);
							pack.DeviceID = ansPack.get()->Source;
							pack.Command = command;
							this->result.push_back(pack);
						}
						*this->LOG << "A ";
						this->LOG->flush();
					}
				}
			} break;
			}
		}
		if (this->result.size() == expectedResponses)
			break; // for (i)
	}
}



void MasterMessenger::Send(const Messenger::ShortDataPack &shrtDataPack)
{
	std::shared_ptr <Messenger::SystemPack> msgrPack;
	Messenger::RESULT msgrRes;

	// for registering answers from slaves
	std::vector <bool> answers(this->nSlaves + 2);

	for (int i = 0; i < this->repeats; ++i) {
		msgrRes = SendPack(std::static_pointer_cast<Messenger::SystemPack>(
				std::make_shared<Messenger::ShortDataPack>(shrtDataPack) ));
		if (msgrRes != Messenger::RESULT::SUCCESS)
			return;

		*this->LOG << this->LOG->Time << "R_d ";
		this->LOG->flush();

		if (shrtDataPack.Destination == this->broadcastID) { // without responses
			return; // return {1}{255}{0} to user
		} // --------------------------------------------------------------------

		uint8_t expectedResponses = shrtDataPack.Destination == this->broadcastID?
				this->nSlaves : 1;
		for (int j = 0; j < expectedResponses; ++j) {
			msgrRes = this->RecvPack(msgrPack);
			if (msgrRes != Messenger::RESULT::SUCCESS) {
				if (msgrRes == Messenger::RESULT::TIMEOUT) {
					*this->LOG << "T ";
					this->LOG->flush();
					break;	// for (j)
				} else {
					*this->LOG << "E ";
					this->LOG->flush();
					if (msgrRes == Messenger::RESULT::FCSErr) {
						continue; 	// for (j)
					} else if (msgrRes == Messenger::RESULT::SYSErr) {
						return; 	// method
					}
				}
			}

			{ // response handling
				std::shared_ptr<Messenger::AnswerPack> ansPack =
						std::static_pointer_cast<Messenger::AnswerPack>(msgrPack);
				if (ansPack.use_count() != 0) {
					if (ansPack.get()->Destination == shrtDataPack.Source &&
						ansPack.get()->TransactionID == shrtDataPack.TransactionID)
					{
						if (answers[ansPack.get()->Source] == false) {
							answers[ansPack.get()->Source] = true;
							Messenger::UserPack pack(Messenger::UserPack::PackType::Command);
							pack.DeviceID = ansPack.get()->Source;
							pack.Command = Command::SysCommand::SendData;
							this->result.push_back(pack);
						}
						*this->LOG << "A ";
						this->LOG->flush();
					}
				}
			}
		}
		if (this->result.size() == expectedResponses)
			break; // for (i)
	}
}



void MasterMessenger::Send(
		const Messenger::ConfigPack &cfgPack,
		const std::vector<uint8_t> &data)
{
	std::shared_ptr <Messenger::SystemPack> msgrPack;
	Messenger::RESULT msgrRes;

	// for registering answers from slaves
	std::vector <bool> answers(this->nSlaves + 2);
	uint8_t answers_cnt = 0;

	{ // sends config
		for (int i = 0; i < this->repeats; ++i) {
			msgrRes = SendPack(std::static_pointer_cast<Messenger::SystemPack>(
					std::make_shared<Messenger::ConfigPack>(cfgPack) ));
			if (msgrRes != Messenger::RESULT::SUCCESS)
				return;

			*this->LOG << this->LOG->Time << "R_cfg ";
			this->LOG->flush();

			uint8_t expectedResponses = cfgPack.Destination == this->broadcastID?
					this->nSlaves : 1;
			for (int j = 0; j < expectedResponses; ++j) {
				msgrRes = this->RecvPack(msgrPack);
				if (msgrRes != Messenger::RESULT::SUCCESS) {
					if (msgrRes == Messenger::RESULT::TIMEOUT) {
						*this->LOG << "T ";
						this->LOG->flush();
						break;	// for (j)
					} else {
						*this->LOG << "E ";
						this->LOG->flush();
						if (msgrRes == Messenger::RESULT::FCSErr) {
							continue; 	// for (j)
						} else if (msgrRes == Messenger::RESULT::SYSErr) {
							return; 	// method
						}
					}
				}

				{ // response handling
					std::shared_ptr<Messenger::AnswerPack> ansPack =
							std::static_pointer_cast<Messenger::AnswerPack>(msgrPack);
					if (ansPack.use_count() != 0) {
						if (ansPack.get()->Destination == cfgPack.Source &&
							ansPack.get()->TransactionID == cfgPack.TransactionID)
						{
							if (answers[ansPack.get()->Source] == false) {
								answers[ansPack.get()->Source] = true;
								answers_cnt++;
							}
							*this->LOG << "A ";
							this->LOG->flush();
						}
					}
				}
			}
			if (answers_cnt == expectedResponses)
				break; // for (i)
		}
		if (answers_cnt == 0) // nobody
			return;
	}

	{ // sends data
		Messenger::DataPack dataPack(this->bufferSize);
		dataPack.Flags = Messenger::Flags::Data;
		dataPack.TransactionID = this->transacID;
		dataPack.Data.resize(cfgPack.BufferSize);

		uint32_t iter = 0; // data-byte iterator
		uint8_t psnd, bsnd; // sent packs, sent bytes
		uint16_t part = 0; // current part of data pack
		uint16_t parts = (cfgPack.TotalSize / cfgPack.BufferSize); // total quantity of parts
		uint32_t remainder = (cfgPack.TotalSize - cfgPack.BufferSize * parts); // num of bytes in remainder
		parts += (remainder) ? 1 : 0; // +last part

		uint8_t expectedResponses = answers_cnt;

		while (iter < cfgPack.TotalSize) {
			for (int i = 0; i < this->repeats; ++i) {
				{ // sending
					for (psnd = 0; psnd <= this->trustPacks; ++psnd) {
						dataPack.Part = ++part;
						for (bsnd = 0; bsnd < cfgPack.BufferSize && iter < cfgPack.TotalSize; ++bsnd) {
							dataPack.Data[bsnd] = data[iter++];
						}
						if (dataPack.Data.size() != bsnd) {
							dataPack.Data.resize(bsnd); // saves content
						}
						msgrRes = SendPack(std::static_pointer_cast<Messenger::SystemPack>(
								std::make_shared<Messenger::DataPack>(dataPack) ));
						if (msgrRes != Messenger::RESULT::SUCCESS)
							return;

						*this->LOG << "D ";
						this->LOG->flush();

						if (part == parts) {
							psnd++;
							break; // for (psnd)
						}
						CrossSleep(this->dataDelay); // pause between parts
					}
				}

				{ // responses expecting
					answers_cnt = 0;
					std::fill(answers.begin(), answers.end(), false);
					for (int j = 0; j < expectedResponses; ++j) {
						msgrRes = this->RecvPack(msgrPack);
						if (msgrRes != Messenger::RESULT::SUCCESS) {
							if (msgrRes == Messenger::RESULT::TIMEOUT) {
								*this->LOG << "T ";
								this->LOG->flush();
								break; 		// for (j)
							} else {
								*this->LOG << "E ";
								this->LOG->flush();
								if (msgrRes == Messenger::RESULT::FCSErr) {
									continue; 	// for (j)
								} else if (msgrRes == Messenger::RESULT::SYSErr) {
									return; 	// method
								}
							}
						}

						{ // response handling
							std::shared_ptr<Messenger::AnswerPack> ansPack =
									std::static_pointer_cast<Messenger::AnswerPack>(msgrPack);
							if (ansPack.use_count() != 0) {
								if (ansPack.get()->Destination == cfgPack.Source &&
									ansPack.get()->TransactionID == cfgPack.TransactionID)
								{
									if (answers[ansPack.get()->Source] == false) {
										answers[ansPack.get()->Source] = true;
										answers_cnt++;
									}
									*this->LOG << "A ";
									this->LOG->flush();
								}
							}
						}
					}
				}
				if (answers_cnt >= expectedResponses) {
					break; // for (i)
				} else if (i != this->repeats - 1) { // should sends the data to the good slaves
					iter -= (part == parts && remainder)?
							(cfgPack.BufferSize * (psnd - 1) + remainder) :
							(cfgPack.BufferSize * psnd);
					part -= psnd;
				} else { // on last try
					expectedResponses = answers_cnt;
				}
			}
			if (expectedResponses == 0)
				break; // while (iter)
		}
	}

	// push results
	Messenger::UserPack pack(Messenger::UserPack::PackType::Command);
	pack.Command = Command::SysCommand::SendData;
	for (size_t i = 2; i < answers.size(); ++i) {
		if (answers[i]) {
			pack.DeviceID = i;
			this->result.push_back(pack);
		}
	}
	return;
}



MasterMessenger::MasterMessenger(Logger &LOG) : Messenger(LOG)
{}



MasterMessenger::~MasterMessenger()
{}


