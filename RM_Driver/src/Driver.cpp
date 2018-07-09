
#include "Driver.h"
#include "SlaveMessenger.h"
#include "MasterMessenger.h"


using std::cout;
using std::cin;
using std::cerr;
using std::endl;
using std::vector;
using std::string;
using std::map;



void Driver::MasterMode()
{
	MasterMessenger *messenger = dynamic_cast <MasterMessenger*> (this->messenger);
	if (messenger) {
		std::shared_ptr <Messenger::UserPack> m_wrpack, m_rdpack;
		UserInterface::UserPack ui_wrpack, ui_rdpack;

		while (1) {
			if ( this->ui->Read(ui_rdpack) == UserInterface::RESULT::SUCCESS ) {
				ui_wrpack.Command = ui_rdpack.Command;
				ui_wrpack.DeviceID = ui_rdpack.DeviceID;
				ui_wrpack.TotalSize = 0;
				ui_wrpack.Data.clear();

				switch (ui_rdpack.Command) {
				case ::Command::SysCommand::SendData: {
					m_wrpack.reset(new Messenger::UserPack(Messenger::UserPack::PackType::Data));
					m_wrpack.get()->Command = ui_wrpack.Command;
					m_wrpack.get()->DeviceID = ui_rdpack.DeviceID;
					m_wrpack.get()->Data.assign(ui_rdpack.Data.begin(), ui_rdpack.Data.end());
					const std::vector <Messenger::UserPack> &results = messenger->Send( *m_wrpack.get() );

					ui_wrpack.TotalSize = results.size(); // slaves
					ui_wrpack.Data.reserve(ui_wrpack.TotalSize); // slaves ids
					for (size_t i = 0; i < ui_wrpack.TotalSize; ++i)
						ui_wrpack.Data.push_back( results[i].DeviceID );

					this->ui->Write(ui_wrpack);
				} break;
				case ::Command::SysCommand::GetGeolocation: {
					m_wrpack.reset(new Messenger::UserPack(Messenger::UserPack::PackType::Command));
					m_wrpack.get()->Command = ui_wrpack.Command;
					m_wrpack.get()->DeviceID = ui_rdpack.DeviceID;
					const std::vector <Messenger::UserPack> &results = messenger->Send( *m_wrpack.get() );

					ui_wrpack.TotalSize = results.size(); // slaves
					ui_wrpack.Data.reserve(ui_wrpack.TotalSize); // slaves ids
					for (size_t i = 0; i < ui_wrpack.TotalSize; ++i)
						ui_wrpack.Data.push_back( results[i].DeviceID );

					this->ui->Write(ui_wrpack);

					// data from slaves
					size_t slaves = ui_wrpack.TotalSize;
					for (size_t i = 0; i < slaves; ++i) {
						ui_wrpack.DeviceID = results[i].DeviceID;
						ui_wrpack.TotalSize = results[i].Data.size();
						ui_wrpack.Data.assign( results[i].Data.begin(), results[i].Data.end() );
						this->ui->Write(ui_wrpack);
					}
				} break;
				case ::Command::SysCommand::R3:
				case ::Command::SysCommand::R4:
				case ::Command::SysCommand::R5:
				case ::Command::SysCommand::R6:
				case ::Command::SysCommand::R7:
				case ::Command::SysCommand::R8:
				case ::Command::SysCommand::R9:
				case ::Command::SysCommand::R10: {
					this->ui->Write(ui_wrpack);
				} break;
				default: {
					if (ui_rdpack.Command < static_cast<uint8_t>(Messenger::Flags::Function)) {
						m_wrpack.reset(new Messenger::UserPack(Messenger::UserPack::PackType::Command));
						m_wrpack.get()->Command = ui_wrpack.Command;
						m_wrpack.get()->DeviceID = ui_rdpack.DeviceID;
						const std::vector <Messenger::UserPack> &results = messenger->Send( *m_wrpack.get() );

						ui_wrpack.TotalSize = results.size(); // slaves
						ui_wrpack.Data.reserve(ui_wrpack.TotalSize); // slaves ids
						for (size_t i = 0; i < ui_wrpack.TotalSize; ++i)
							ui_wrpack.Data.push_back( results[i].DeviceID );
					}
					this->ui->Write(ui_wrpack);
				} break;
				}
			}
		}
	}
}



void Driver::SlaveMode()
{
	SlaveMessenger *messenger = dynamic_cast <SlaveMessenger*> (this->messenger);
	if (messenger) {
		// for detecting pipes crashes
		CheckDelegate chDeleg = NewCheckDelegate(this->ui, &UserInterface::CheckWorkingCapacity);
		messenger->SetExChecker(&chDeleg);
		// ---------------------------

		TON timer;
		std::shared_ptr <Messenger::UserPack> m_wrpack, m_rdpack;
		UserInterface::UserPack ui_wrpack, ui_rdpack;
		ui_wrpack.DeviceID = this->deviceID;

		while (1) {
			if ( messenger->Listening(m_rdpack) == SlaveMessenger::RESULT::SUCCESS )
				switch (m_rdpack.get()->GetPackType()) {
				case Messenger::UserPack::PackType::Command: {
					ui_wrpack.Command = m_rdpack.get()->Command;
					ui_wrpack.TotalSize = 0;

					timer.reset();
					timer.start(this->maxPipeDelay);

//					this->ui->ClearRecvdStuff();
					if ( this->ui->Write(ui_wrpack) == UserInterface::RESULT::SUCCESS ) {
						switch (ui_wrpack.Command) {
						case ::Command::SysCommand::GetGeolocation: {
							if ( this->ui->Read(ui_rdpack) == UserInterface::RESULT::SUCCESS ) {
								if (!timer.check()) {
									m_wrpack.reset(new Messenger::UserPack(Messenger::UserPack::PackType::Data));
									m_wrpack.get()->DeviceID = m_rdpack.get()->DeviceID;
									m_wrpack.get()->Data.assign(ui_rdpack.Data.begin(), ui_rdpack.Data.end());
									messenger->SendData(m_wrpack);
								} else {
									*this->LOG << this->LOG->Time << "Pipe Time Out -> Skip" << endl;
								}
							}
						} break;
						default: {
							// nop
						} break;
						}
					}
				} break;
				case Messenger::UserPack::PackType::Data: {
					ui_wrpack.Command = m_rdpack.get()->Command;
					ui_wrpack.SetData(m_rdpack.get()->Data);
					this->ui->Write(ui_wrpack);
				} break;
				default: {
					// nop
				} break;
			}
		}
	}
}



void Driver::Initialization(void)
{
	Logger::MODE LM;
	// reads config ---------------------------------------------------------------------------
	try {
		std::map <std::string, std::string> ucnf;
		IniFile ini(CFG::FILE_NAME);

		if (ini.isOpened())
			ini.Parse();
		else
			throw std::exception();

		ucnf = ini.GetSection(CFG::MAIN::SECTION);

		// prevents segmentation fault from stoi
		if ( ucnf.find(CFG::MAIN::LOG_MODE) == ucnf.end() ) throw std::exception();
		if ( ucnf.find(CFG::MAIN::DEVICE_ID) == ucnf.end() ) throw std::exception();
		if ( ucnf.find(CFG::MAIN::PIPE_DELAY) == ucnf.end() ) throw std::exception();

		LM = static_cast <Logger::MODE> ( stoi(ucnf.find(CFG::MAIN::LOG_MODE)->second) );
		this->deviceID = static_cast <uint8_t> ( stoi(ucnf.find(CFG::MAIN::DEVICE_ID)->second) );
		this->maxPipeDelay = static_cast <uint16_t> ( stoi(ucnf.find(CFG::MAIN::PIPE_DELAY)->second) );

	} catch (std::exception &e) {
		std::cerr << "CONFIG error -> Exit" << std::endl;
		exit(0);
	}

	// switch logger --------------------------------------------------------------------------
	this->LOG->SwitchMode( LM );

	// creates messenger ----------------------------------------------------------------------
	switch (this->deviceID) {
	case 0: { // error
		std::cerr << "CONFIG error -> Exit" << std::endl;
		*this->LOG << this->LOG->Time << "CONFIG error -> Exit";
		exit(0);
	} break;
	case 1: { // master
		this->messenger = new MasterMessenger(*this->LOG);
	} break;
	default: { // slaves
		this->messenger = new SlaveMessenger(*this->LOG);
	} break;
	}

	this->messenger->Initialization();
	if (this->messenger->GetState() != Messenger::STATE::READY) {
		std::cerr << "CONFIG/PORT error -> Exit" << std::endl;
		*this->LOG << this->LOG->Time << "CONFIG/PORT error -> Exit";
		exit(0);
	}

	this->ui = new UserInterface(UserInterface::MODE::CREATE_NEW, UserInterface::CONNTYPE::DUPLEX);
	this->ui->Initialization();
	if (this->ui->GetState() == UserInterface::STATE::CLOSED) {
		std::cerr << "CONFIG/PIPES error -> Exit" << std::endl;
		*this->LOG << this->LOG->Time << "CONFIG/PIPES error -> Exit";
		exit(0);
	}

}



void Driver::StartProcess(void)
{
	switch (this->deviceID) {
	case 1: {
		this->MasterMode();
	} break;
	default: {
		this->SlaveMode();
	} break;
	}
}



Driver::Driver()
{
	this->messenger = nullptr;
	this->deviceID = 0;
	this->maxPipeDelay = 0;
	this->LOG = new Logger();
	this->receivingThread = nullptr;
	this->ui = nullptr;
}



Driver::~Driver()
{
	delete this->messenger;
	delete this->LOG;
	delete this->receivingThread;
	delete this->ui;
}
