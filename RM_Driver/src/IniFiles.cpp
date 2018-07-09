
#include "IniFiles.h"

#ifdef __linux__
	#include <errno.h>
#else
	#include <windows.h>
#endif



IniFile::IniFile(std::string fileName)
{
	this->file.open(fileName.c_str());
}



IniFile::~IniFile()
{
	this->file.close();
	this->configInfo.clear();
}



void IniFile::Parse()
{
	std::string line;
	std::string section;
	std::string key, value;

	section.reserve(20);
	key.reserve(20);
	value.reserve(20);
	section.assign("DEFAULT");

	bool alg_comment = 0, alg_section = 0;
	size_t s;
	while (getline(this->file, line)) {
		// -----------------------------------------------
		// defines symbol with which string is started
		for (s = 0; s < line.size(); s++) {
			if (line[s] != ' ' && line[s] != 9) {
				alg_comment = (line[s] == '#');
				alg_section = (line[s] == '[');
				break;
			}
		}
		// -----------------------------------------------
		// string is started with #
		if (alg_comment) {
			//
			alg_comment = false;
		// string is started with [
		} else if (alg_section) {
			section.clear();
			for (s++; s < line.size() && line[s] != ']' && line[s] != '\r'; s++) {
				section.push_back(line[s]);
			}
			alg_section = false;
		// string contains pair (key, value)
		} else if (line.size() > 2) { // min 'x=y'
			do { // key
				if (line[s] != ' ' && line[s] != '\t' && line[s] != '\r')
					key.push_back(line[s]);
			} while (++s < line.size() && line[s] != '=');
			do { // value
				if (line[s] == '#' || line[s] == ';') {
					break;
				}
				if (line[s] != ' ' && line[s] != '\t' && line[s] != '=' && line[s] != '\r')
					value.push_back(line[s]);
			} while (++s < line.size());
		}
		// -----------------------------------------------
		if (key.size() != 0 && value.size() != 0) {
			this->configInfo[section][key] = value;
		}
		key.clear();
		value.clear();
		// -----------------------------------------------
	}
}



int IniFile::GetLastSystemError() const
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



bool IniFile::isOpened() const
{
	return this->file.is_open();
}



const std::map<std::string, std::string>& IniFile::GetSection(std::string section_name) const
{
	return (this->configInfo.find(section_name))->second;
}



const std::map <std::string, std::map<std::string, std::string> >& IniFile::GetConfig() const
{
	return this->configInfo;
}
