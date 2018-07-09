#include "Messenger.h"



/*! ------------------------------------------------------------------------------------
 * @brief:
 * -------------------------------------------------------------------------------------
 * */
static const CheckSum checkSum;



Messenger::RESULT Messenger::SendPack(std::shared_ptr<Messenger::SystemPack> pack) const
{
	std::vector <uint8_t> buffer;
	pack.get()->ToBytes(buffer);
	return (this->port->Write(&(buffer[0]), buffer.size()) != -1)?
			Messenger::RESULT::SUCCESS : Messenger::RESULT::SYSErr;
}



Messenger::RESULT Messenger::SendPack(std::shared_ptr<Messenger::SystemPack> &pack) const
{
	std::vector <uint8_t> buffer;
	pack.get()->ToBytes(buffer);
	return (this->port->Write(&(buffer[0]), buffer.size()) != -1)?
			Messenger::RESULT::SUCCESS : Messenger::RESULT::SYSErr;
}



Messenger::RESULT Messenger::RecvPack(std::shared_ptr<Messenger::SystemPack> &pack) const
{
	std::vector <uint8_t> buffer(Messenger::SystemPack::MAX_PACK_SIZE);

	pack.reset();

	/*
	 * Reads first byte from port and defines type of received pack, reads remaining data;
	 * Checks FCS error
	 * */
	int res;
	if ( (res = this->port->ReadByte( &(buffer[0]) )) > 0) {
		if (buffer[0] & Flags::Function) {
			if ( (res = this->port->Read(&(buffer[1]), this->CmdSz)) == this->CmdSz) {
				pack.reset( (new CommandPack())->FromBytes(buffer) );
				uint8_t crc = checkSum.GetCRC8(&(buffer[0]), Messenger::CommandPack::PACK_SIZE - Messenger::CommandPack::FCS_SIZE);
				if (crc == std::static_pointer_cast<Messenger::CommandPack>(pack).get()->FCS) {
					return Messenger::RESULT::SUCCESS;
				} else {
					this->ClearRecvdStuff();
					return Messenger::RESULT::FCSErr;
				}
			}
		} else {
			switch (buffer[0]) {
			// -------------------------------------------------------------------------------------------------------------------
			case Flags::Config | Flags::Ack | Flags::Slave:
			case Flags::Data | Flags::Ack | Flags::Slave:
			case Flags::Data | Flags::Short | Flags::Ack | Flags::Slave:
			case Flags::Config | Flags::Ack:
			case Flags::Data | Flags::Ack:
			{
				if ( (res = this->port->Read(&(buffer[1]), this->AnsSz)) == this->AnsSz) {
					pack.reset( (new AnswerPack())->FromBytes(buffer) );
					uint8_t crc = checkSum.GetCRC8(&(buffer[0]), Messenger::AnswerPack::PACK_SIZE - Messenger::AnswerPack::FCS_SIZE);
					if (crc == std::static_pointer_cast<Messenger::AnswerPack>(pack).get()->FCS) {
						return Messenger::RESULT::SUCCESS;
					} else {
						this->ClearRecvdStuff();
						return Messenger::RESULT::FCSErr;
					}
				}
			} break;
			// -------------------------------------------------------------------------------------------------------------------
			case Flags::Config:
			case Flags::Config | Flags::Slave:
			{
				if ( (res = this->port->Read(&(buffer[1]), this->CfgSz)) == this->CfgSz) {
					pack.reset( (new ConfigPack())->FromBytes(buffer) );
					uint8_t crc = checkSum.GetCRC8(&(buffer[0]), Messenger::ConfigPack::PACK_SIZE - Messenger::ConfigPack::FCS_SIZE);
					if (crc == std::static_pointer_cast<Messenger::ConfigPack>(pack).get()->FCS) {
						return Messenger::RESULT::SUCCESS;
					} else {
						this->ClearRecvdStuff();
						return Messenger::RESULT::FCSErr;
					}
				}
			} break;
			// -------------------------------------------------------------------------------------------------------------------
			case Flags::Data:
			case Flags::Data | Flags::Slave:
			{
				if ( (res = this->port->Read(&(buffer[1]), this->DataSz)) == this->DataSz) {
					uint8_t size = this->expectedDataSize + Messenger::DataPack::FCS_SIZE;
					if ( (res = this->port->Read(&(buffer[this->DataSz + 1]), size)) == size) {
						pack.reset( (new DataPack(this->expectedDataSize))->FromBytes(buffer) );
						uint16_t crc = checkSum.GetCRC16(&(buffer[0]), this->DataSz + 1 + size - Messenger::DataPack::FCS_SIZE);
						if (crc == std::static_pointer_cast<Messenger::DataPack>(pack).get()->FCS) {
							return Messenger::RESULT::SUCCESS;
						} else {
							this->ClearRecvdStuff();
							return Messenger::RESULT::FCSErr;
						}
					}
				}
			} break;
			// -------------------------------------------------------------------------------------------------------------------
			case Flags::Data | Flags::Short:
			case Flags::Data | Flags::Short | Flags::Slave:
			{
				if ( (res = this->port->Read(&(buffer[1]), this->ShrtDataSz)) == this->ShrtDataSz) {
					pack.reset( (new ShortDataPack())->FromBytes(buffer) );
					uint8_t size = std::static_pointer_cast<Messenger::ShortDataPack>(pack).get()->DataSize + Messenger::ShortDataPack::FCS_SIZE;
					if ( (res = this->port->Read(&(buffer[this->ShrtDataSz + 1]), size)) == size) {
						pack.get()->FromBytes(buffer);
						uint16_t crc = checkSum.GetCRC16(&(buffer[0]), this->ShrtDataSz + 1 + size - Messenger::ShortDataPack::FCS_SIZE);
						if (crc == std::static_pointer_cast<Messenger::ShortDataPack>(pack).get()->FCS) {
							return Messenger::RESULT::SUCCESS;
						} else {
							this->ClearRecvdStuff();
							return Messenger::RESULT::FCSErr;
						}
					}
				}
			} break;
			// -------------------------------------------------------------------------------------------------------------------
			default: {
				this->ClearRecvdStuff();
			} break;
			// -------------------------------------------------------------------------------------------------------------------
			}
		}
	}
	if (res >= 0)
		return Messenger::RESULT::TIMEOUT;
	else
		return Messenger::RESULT::SYSErr;
}



void Messenger::Initialization(void)
{
	this->state = Messenger::STATE::STOPPED;

	try {
		std::string pn;
		std::map <std::string, std::string> ucnf;
		COMPort::InitializationStruct init_str;
		IniFile ini(CFG::FILE_NAME);

		if (ini.isOpened())
			ini.Parse();
		else
			throw std::exception();

		ucnf = ini.GetSection(CFG::COM::SECTION);

		// prevents segmentation fault from stoi
		if ( ucnf.find(CFG::COM::COM_NAME) == ucnf.end() ) throw std::exception();
		if ( ucnf.find(CFG::COM::BAUD_RATE) == ucnf.end() ) throw std::exception();
		if ( ucnf.find(CFG::COM::AIR_BAUD_RATE) == ucnf.end() ) throw std::exception();
		if ( ucnf.find(CFG::COM::TLMTR_TIME_OUT) == ucnf.end() ) throw std::exception();
		if ( ucnf.find(CFG::COM::BRCST_TIME_OUT) == ucnf.end() ) throw std::exception();
		if ( ucnf.find(CFG::COM::UNICST_TIME_OUT) == ucnf.end() ) throw std::exception();

		this->telemetryTO = std::stoi( ucnf.find(CFG::COM::TLMTR_TIME_OUT)->second );
		this->broadcastTO = std::stoi( ucnf.find(CFG::COM::BRCST_TIME_OUT)->second );
		this->unicastTO = std::stoi( ucnf.find(CFG::COM::UNICST_TIME_OUT)->second );

		pn = ucnf.find(CFG::COM::COM_NAME)->second;
		init_str.portName = &( pn[0] );
		init_str.wireBaudRate = std::stoi( ucnf.find(CFG::COM::BAUD_RATE)->second );
		init_str.additionalBaudRate = std::stoi( ucnf.find(CFG::COM::AIR_BAUD_RATE)->second );
		init_str.timeOut.Ms = 20;
		init_str.timeOut.nChars = 0;

		this->port->Initialization(init_str);

		ucnf = ini.GetSection(CFG::MSGR::SECTION);

		if ( ucnf.find(CFG::MSGR::SLAVES) == ucnf.end() ) throw std::exception();
		if ( ucnf.find(CFG::MSGR::BRDCST_ID) == ucnf.end() ) throw std::exception();
		if ( ucnf.find(CFG::MSGR::ANS_DELAY) == ucnf.end() ) throw std::exception();
		if ( ucnf.find(CFG::MSGR::DATA_DELAY) == ucnf.end() ) throw std::exception();
		if ( ucnf.find(CFG::MSGR::BUF_SIZE) == ucnf.end() ) throw std::exception();
		if ( ucnf.find(CFG::MSGR::TRUST_PACKS) == ucnf.end() ) throw std::exception();
		if ( ucnf.find(CFG::MSGR::RETRIES) == ucnf.end() ) throw std::exception();

		this->nSlaves = std::stoi( ucnf.find(CFG::MSGR::SLAVES)->second );
		this->broadcastID = std::stoi( ucnf.find(CFG::MSGR::BRDCST_ID)->second );
		this->ansDelay = std::stoi( ucnf.find(CFG::MSGR::ANS_DELAY)->second );
		this->dataDelay = std::stoi( ucnf.find(CFG::MSGR::DATA_DELAY)->second );
		this->bufferSize = std::stoi( ucnf.find(CFG::MSGR::BUF_SIZE)->second );
		this->trustPacks = std::stoi( ucnf.find(CFG::MSGR::TRUST_PACKS)->second );
		this->repeats = std::stoi( ucnf.find(CFG::MSGR::RETRIES)->second );

		ucnf = ini.GetSection(CFG::MAIN::SECTION);

		if ( ucnf.find(CFG::MAIN::DEVICE_ID) == ucnf.end() ) throw std::exception();

		this->deviceID = std::stoi( ucnf.find(CFG::MAIN::DEVICE_ID)->second );

		if (this->port->GetState() == COMPort::STATE::OPENED) {
			this->state = Messenger::STATE::READY;
		}

	} catch (std::exception &e) {
		;
	}
}



Messenger::Messenger(Logger &LOG)
{
	this->port = new COMPort();
	this->LOG = &LOG;
	this->Reset();
}



Messenger::~Messenger()
{
	this->Reset();
}



Messenger::STATE Messenger::GetState() const
{
	return this->state;
}



uint8_t Messenger::GetBroadcastId() const
{
	return this->broadcastID;
}



uint8_t Messenger::GetDeviceId() const
{
	return this->deviceID;
}



void Messenger::Reset()
{
	this->transacID = 1;
}




Command Messenger::Convert(const Messenger::Flags &flag)
{
	Command cmd;
	cmd._raw = flag & 0x7F;
	return cmd;
}



Messenger::Flags Messenger::Convert(const Command &command)
{
	Messenger::Flags flag;
	flag = static_cast <uint8_t> (command) | 0x80;
	return flag;
}



void Messenger::SetBroadcast()
{
	COMPort::TimeOutStruct toStr;
	toStr.Ms = this->broadcastTO;
	toStr.nChars = 0;
	this->port->SetTimeOut(toStr);
}



void Messenger::SetUnicast()
{
	COMPort::TimeOutStruct toStr;
	toStr.Ms = this->unicastTO;
	toStr.nChars = 0;
	this->port->SetTimeOut(toStr);
}



void Messenger::SetTelemetry()
{
	COMPort::TimeOutStruct toStr;
	toStr.Ms = this->telemetryTO;
	toStr.nChars = 0;
	this->port->SetTimeOut(toStr);
}



void Messenger::SetAnyTO(DWORD time_ms, DWORD chars = 0)
{
	COMPort::TimeOutStruct toStr;
	toStr.Ms = time_ms;
	toStr.nChars = chars;
	this->port->SetTimeOut(toStr);
}



void Messenger::ClearRecvdStuff() const
{
	this->port->Flush();
}



Messenger::UserPack::PackType Messenger::UserPack::GetPackType() const
{
	return this->packType;
}



Messenger::UserPack::UserPack(UserPack::PackType packType) : packType(packType)
{}



Messenger::UserPack::~UserPack()
{}



Messenger::SystemPack::PackType Messenger::SystemPack::GetPackType() const
{
	return this->packType;
}



Messenger::SystemPack::SystemPack(Messenger::SystemPack::PackType packType) : packType(packType)
{}



Messenger::SystemPack::~SystemPack()
{}



Messenger::ConfigPack::ConfigPack() :
		Messenger::SystemPack::SystemPack(Messenger::SystemPack::PackType::ConfigPack)
{}



Messenger::ConfigPack::~ConfigPack()
{}



Messenger::CommandPack::CommandPack() :
		Messenger::SystemPack::SystemPack(Messenger::SystemPack::PackType::CommandPack)
{}



Messenger::CommandPack::~CommandPack()
{}



Messenger::AnswerPack::AnswerPack() :
		Messenger::SystemPack::SystemPack(Messenger::SystemPack::PackType::AnswerPack)
{}



Messenger::AnswerPack::~AnswerPack()
{}



Messenger::DataPack::DataPack() :
		Messenger::SystemPack::SystemPack(Messenger::SystemPack::PackType::DataPack)
{}



Messenger::DataPack::DataPack(uint8_t dataSize) :
		Messenger::SystemPack::SystemPack(Messenger::SystemPack::PackType::DataPack)
{
	this->Data.resize(dataSize);
}



Messenger::DataPack::~DataPack()
{}



Messenger::ShortDataPack::ShortDataPack() :
		Messenger::SystemPack::SystemPack(Messenger::SystemPack::PackType::ShortDataPack)
{}



Messenger::ShortDataPack::ShortDataPack(uint8_t dataSize) :
		Messenger::SystemPack::SystemPack(Messenger::SystemPack::PackType::ShortDataPack)
{
	this->Data.resize(dataSize);
}



Messenger::ShortDataPack::~ShortDataPack()
{}



Messenger::DataPack* Messenger::DataPack::ToBytes(std::vector <uint8_t> &buffer)
{
	buffer.clear();
	buffer.reserve(this->PACK_SERVICE_SIZE + this->Data.size());
	buffer.push_back(static_cast <uint8_t> (this->Flags));
	buffer.push_back(static_cast <uint8_t> (this->TransactionID));
	buffer.push_back(static_cast <uint8_t> (this->Part));
	buffer.push_back(static_cast <uint8_t> (this->Part >> 8));
	buffer.insert(buffer.end(), this->Data.begin(), this->Data.end());
	this->FCS = checkSum.GetCRC16( &(buffer[0]), this->PACK_SERVICE_SIZE + this->Data.size() - this->FCS_SIZE );
	buffer.push_back(static_cast <uint8_t> (this->FCS));
	buffer.push_back(static_cast <uint8_t> (this->FCS >> 8));
	return this;
}



Messenger::DataPack* Messenger::DataPack::FromBytes(const std::vector <uint8_t> &buffer)
{
	if (buffer.size() < Messenger::DataPack::PACK_SERVICE_SIZE)
		return nullptr;

	uint8_t i = 0;
	this->Flags = buffer[i++];
	this->TransactionID = buffer[i++];
	this->Part = buffer[i++];
	this->Part |= buffer[i++] << 8;
	this->Data.assign( &buffer[i], &buffer[i] + this->Data.size());
	i += this->Data.size();
	this->FCS = buffer[i++];
	this->FCS |= buffer[i++] << 8;
	return this;
}



Messenger::ShortDataPack* Messenger::ShortDataPack::ToBytes(std::vector <uint8_t> &buffer)
{
	buffer.clear();
	buffer.reserve(this->PACK_SERVICE_SIZE + this->Data.size());
	buffer.push_back(static_cast <uint8_t> (this->Flags));
	buffer.push_back(static_cast <uint8_t> (this->Source));
	buffer.push_back(static_cast <uint8_t> (this->Destination));
	buffer.push_back(static_cast <uint8_t> (this->TransactionID));
	buffer.push_back(static_cast <uint8_t> (this->DataSize));
	buffer.insert(buffer.end(), this->Data.begin(), this->Data.end());
	this->FCS = checkSum.GetCRC16( &(buffer[0]), this->PACK_SERVICE_SIZE + this->Data.size() - this->FCS_SIZE );
	buffer.push_back(static_cast <uint8_t> (this->FCS));
	buffer.push_back(static_cast <uint8_t> (this->FCS >> 8));
	return this;
}



Messenger::ShortDataPack* Messenger::ShortDataPack::FromBytes(const std::vector <uint8_t> &buffer)
{
	if (buffer.size() < Messenger::ShortDataPack::PACK_SERVICE_SIZE)
		return nullptr;

	uint8_t i = 0;
	this->Flags = buffer[i++];
	this->Source = buffer[i++];
	this->Destination = buffer[i++];
	this->TransactionID = buffer[i++];
	this->DataSize = buffer[i++];
	this->Data.assign( &buffer[i], &buffer[i] + this->DataSize);
	i += this->DataSize;
	this->FCS = buffer[i++];
	this->FCS |= buffer[i++] << 8;
	return this;
}



Messenger::AnswerPack* Messenger::AnswerPack::ToBytes(std::vector <uint8_t> &buffer)
{
	buffer.clear();
	buffer.reserve(this->PACK_SIZE);
	buffer.push_back(static_cast <uint8_t> (this->Flags));
	buffer.push_back(static_cast <uint8_t> (this->Source));
	buffer.push_back(static_cast <uint8_t> (this->Destination));
	buffer.push_back(static_cast <uint8_t> (this->TransactionID));
	this->FCS = checkSum.GetCRC8( &(buffer[0]), this->PACK_SIZE - this->FCS_SIZE );
	buffer.push_back(static_cast <uint8_t> (this->FCS));
	return this;
}



Messenger::AnswerPack* Messenger::AnswerPack::FromBytes(const std::vector <uint8_t> &buffer)
{
	if (buffer.size() < Messenger::AnswerPack::PACK_SIZE)
		return nullptr;

	uint8_t i = 0;
	this->Flags = buffer[i++];
	this->Source = buffer[i++];
	this->Destination = buffer[i++];
	this->TransactionID = buffer[i++];
	this->FCS = buffer[i++];
	return this;
}



Messenger::CommandPack* Messenger::CommandPack::ToBytes(std::vector <uint8_t> &buffer)
{
	buffer.clear();
	buffer.reserve(this->PACK_SIZE);
	buffer.push_back(static_cast <uint8_t> (this->Flags));
	buffer.push_back(static_cast <uint8_t> (this->Source));
	buffer.push_back(static_cast <uint8_t> (this->Destination));
	buffer.push_back(static_cast <uint8_t> (this->TransactionID));
	this->FCS = checkSum.GetCRC8( &(buffer[0]), this->PACK_SIZE - this->FCS_SIZE );
	buffer.push_back(static_cast <uint8_t> (this->FCS));
	return this;
}



Messenger::CommandPack* Messenger::CommandPack::FromBytes(const std::vector <uint8_t> &buffer)
{
	if (buffer.size() < Messenger::CommandPack::PACK_SIZE)
		return nullptr;

	uint8_t i = 0;
	this->Flags = buffer[i++];
	this->Source = buffer[i++];
	this->Destination = buffer[i++];
	this->TransactionID = buffer[i++];
	this->FCS = buffer[i++];
	return this;
}



Messenger::ConfigPack* Messenger::ConfigPack::ToBytes(std::vector <uint8_t> &buffer)
{
	buffer.clear();
	buffer.reserve(this->PACK_SIZE);
	buffer.push_back(static_cast <uint8_t> (this->Flags));
	buffer.push_back(static_cast <uint8_t> (this->Source));
	buffer.push_back(static_cast <uint8_t> (this->Destination));
	buffer.push_back(static_cast <uint8_t> (this->TransactionID));
	buffer.push_back(static_cast <uint8_t> (this->TotalSize));
	buffer.push_back(static_cast <uint8_t> (this->TotalSize >> 8));
	buffer.push_back(static_cast <uint8_t> (this->TotalSize >> 16));
	buffer.push_back(static_cast <uint8_t> (this->BufferSize));
	buffer.push_back(static_cast <uint8_t> (this->TrustPacks));
	this->FCS = checkSum.GetCRC8( &(buffer[0]), this->PACK_SIZE - this->FCS_SIZE );
	buffer.push_back(static_cast <uint8_t> (this->FCS));
	return this;
}



Messenger::ConfigPack* Messenger::ConfigPack::FromBytes(const std::vector <uint8_t> &buffer)
{
	if (buffer.size() < Messenger::ConfigPack::PACK_SIZE)
		return nullptr;

	uint8_t i = 0;
	this->Flags = buffer[i++];
	this->Source = buffer[i++];
	this->Destination = buffer[i++];
	this->TransactionID = buffer[i++];
	this->TotalSize = buffer[i++];
	this->TotalSize |= (buffer[i++] << 8);
	this->TotalSize |= (buffer[i++] << 16);
	this->BufferSize = buffer[i++];
	this->TrustPacks = buffer[i++];
	this->FCS = buffer[i++];
	return this;
}


