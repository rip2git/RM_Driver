
#include "NamedPipe.h"
#include "TON.h"

#ifdef __linux__
	#include <errno.h>
	#include <termios.h>
	#include <fcntl.h>
	#include <unistd.h>
	#include <signal.h>
	#include <sys/stat.h>
#else
	#include <windows.h>
	#define BUFFER_SIZE 	2048
	#define TIMEOUT			5000
#endif



NamedPipe::NamedPipe()
{
	this->state = NamedPipe::STATE::CLOSED;
}



NamedPipe::NamedPipe(const NamedPipe::InitializationStruct &initStr)
{
	this->Initialization(initStr);
}



void NamedPipe::Initialization(const NamedPipe::InitializationStruct &initStr)
{
	this->state = NamedPipe::STATE::CLOSED;
	this->mode = initStr.mode;

#ifdef __linux__
	this->pipeName = "/tmp/" + initStr.pipeName;
#else
	this->pipeName = "\\\\.\\pipe\\" + initStr.pipeName;
#endif

	this->state = NamedPipe::STATE::INITIALIZED;
	return;
}



NamedPipe::~NamedPipe()
{
	this->Close();
}



void NamedPipe::Create()
{
	this->instanceType = NamedPipe::INSTANCE_TYPE::CREATING;
#ifdef __linux__
	mkfifo(this->pipeName.c_str(), S_IRUSR | S_IWUSR);

	if (this->mode == NamedPipe::MODE::WRITE)
		this->fd = open(this->pipeName.c_str(), O_WRONLY);
	else
		this->fd = open(this->pipeName.c_str(), O_RDONLY);

	if (this->fd != -1)
		this->state = NamedPipe::STATE::OPENED;
#else
	std::wstring stemp = std::wstring(this->pipeName.begin(), this->pipeName.end());

	if (this->mode == NamedPipe::MODE::WRITE) {
		this->fd = CreateNamedPipeW(stemp.c_str(), PIPE_ACCESS_OUTBOUND,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			1, BUFFER_SIZE, BUFFER_SIZE, TIMEOUT, NULL);
	} else {
		this->fd = CreateNamedPipeW(stemp.c_str(), PIPE_ACCESS_INBOUND,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			1, BUFFER_SIZE, BUFFER_SIZE, TIMEOUT, NULL);
	}
	if (this->fd != INVALID_HANDLE_VALUE)
		if (ConnectNamedPipe(this->fd, NULL))
			this->state = NamedPipe::STATE::OPENED;
#endif
	return;
}



void NamedPipe::Open()
{
	this->instanceType = NamedPipe::INSTANCE_TYPE::OPENING;
#ifdef __linux__
	if (this->mode == NamedPipe::MODE::WRITE) {
		this->fd = open(this->pipeName.c_str(), O_WRONLY);
	} else {
		this->fd = open(this->pipeName.c_str(), O_RDONLY);
	}

	if (this->fd != -1)
		this->state = NamedPipe::STATE::OPENED;
#else
	std::wstring stemp = std::wstring(this->pipeName.begin(), this->pipeName.end());
	if (this->mode == NamedPipe::MODE::WRITE) {
		this->fd = CreateFileW(stemp.c_str(), GENERIC_WRITE,
			0, NULL, OPEN_EXISTING, 0, NULL);
	} else {
		this->fd = CreateFileW(stemp.c_str(), GENERIC_READ,
			0, NULL, OPEN_EXISTING, 0, NULL);
	}

	if (this->fd != INVALID_HANDLE_VALUE)
		this->state = NamedPipe::STATE::OPENED;
#endif
	return;
}



void NamedPipe::Close()
{
	if (this->state != NamedPipe::STATE::CLOSED) {
		this->state = NamedPipe::STATE::CLOSED;
#ifdef __linux__
		close(this->fd);
		if (this->instanceType == NamedPipe::INSTANCE_TYPE::CREATING)
			unlink(this->pipeName.c_str());
#else
		if (this->instanceType == NamedPipe::INSTANCE_TYPE::CREATING)
			DisconnectNamedPipe(this->fd);
		CloseHandle(this->fd);
#endif
	}
	return;
}



NamedPipe::STATE NamedPipe::GetState() const
{
	return this->state;
}



int NamedPipe::Read(uint8_t *buf, DWORD buf_size) const
{
	if (this->mode == NamedPipe::MODE::WRITE)
		return 0;
#ifdef __linux__
	int res = read(this->fd, buf, buf_size);
#else
	DWORD bytes_recvd;
	int res = ReadFile(this->fd, buf, buf_size, &bytes_recvd, 0);
	res = (res != 0)? static_cast <int> (bytes_recvd) : -1;
#endif
	return res;
}



int NamedPipe::Write(const uint8_t *buf, DWORD buf_size) const
{
	if (this->mode == NamedPipe::MODE::READ)
		return 0;
#ifdef __linux__
	signal(SIGPIPE, SIG_IGN);
	int res = write(this->fd, buf, buf_size);
#else
	DWORD bytes_written;
	int res = WriteFile(this->fd, buf, buf_size, &bytes_written, NULL);
	res = (res != 0)? static_cast <int> (bytes_written) : -1;
#endif
	return res;
}



bool NamedPipe::CheckWorkingCapacity() const
{
	DWORD lastErr;
#ifdef __linux__
	errno = 0;
	if (this->mode == NamedPipe::MODE::READ) {
		int res;
		timeval tv;
		TON timer;
		fd_set rfds;
		//
		tv.tv_sec = 0;
		tv.tv_usec = 1000;
		//
		FD_ZERO(&rfds);
		FD_SET(this->fd, &rfds);
		timer.reset();
		timer.start(tv.tv_usec / 1000, 1);
		res = select(this->fd + 1, &rfds, NULL, NULL, &tv);
		if (!timer.check() && res != 0) { //
			lastErr = EIO; // I/O err POSIX
		}
	}
	return (lastErr == 0);
#else
	SetLastError(0);
	GetNamedPipeHandleState(this->fd, 0, 0, 0, 0, 0, 0);
	lastErr = GetLastError();
	return (lastErr == NO_ERROR ||
			lastErr == ERROR_MORE_DATA ||
			lastErr == WAIT_TIMEOUT);
#endif
}



int NamedPipe::GetLastSystemError() const
{
	int lastErr = 0;
#ifdef __linux__
	if (errno != 0) {
		lastErr = errno;
	}
	errno = 0;
#else
	if (GetLastError() != 0) {
		lastErr = GetLastError();
	}
	SetLastError(0);
#endif
	return lastErr;
}



bool NamedPipe::FastFlush() const
{
#ifdef __linux__
	int res = tcflush(this->fd, TCIFLUSH);
	return (res != -1);
#else
	DWORD res = PurgeComm(this->fd, PURGE_RXCLEAR);
	return (res != 0);
#endif
}




