
#include <iostream>
#include <ctime>
#include <iomanip>
#include "Logger.h"



Logger::Logger() : std::fstream()
{
	this->Time.start(0);
}



Logger::~Logger()
{
	if (this->is_open())
		this->close();
}



Logger& Logger::operator<<(TON &t)
{
	long long int ms = t.since() % 1000;
	*this << "\n[" << t.since() / 1000 << "." << std::setfill('0') << std::setw(3) << ms << "] ";
	return *this;
}



void Logger::SwitchMode(Logger::MODE mode)
{
	switch (mode) {
	case Logger::MODE::OFF:
		this->toNull();
		break;
	case Logger::MODE::CONSOLE:
		this->toConsole();
		break;
	case Logger::MODE::FILE:
		this->toFile();
		break;
	}
}



void Logger::toNull()
{
	this->basic_ios<char>::rdbuf( 0 );
}



void Logger::toConsole()
{
	this->basic_ios<char>::rdbuf( std::cout.rdbuf() );
}



void Logger::toFile()
{
	const size_t buf_size = 40;
	size_t iter = 0;
	char buf[buf_size];
	std::string format = "%Y_%b_%d_%H.%M.%S_log";
	time_t seconds = time(NULL);
	tm *timeinfo = localtime(&seconds);
	strftime(buf, buf_size, format.c_str(), timeinfo);
	std::string filename;
	filename.reserve(buf_size);
	while ( iter < buf_size && buf[iter] != '\0' ) {
		filename.push_back(buf[iter]);
		iter++;
	}
	filename.resize(iter);
	filename += ".txt";
	this->open(filename.c_str(), std::ios_base::out);
	if ( !this->is_open() )
		exit(0);
}



