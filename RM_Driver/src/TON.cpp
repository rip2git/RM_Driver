
#include "TON.h"


typedef std::chrono::milliseconds milliseconds;
typedef std::chrono::duration<std::chrono::milliseconds> HighResTime;
#define HighResTimeGet() std::chrono::high_resolution_clock::now().time_since_epoch()
#define HighResTimeToMilliseconds(x) std::chrono::duration_cast<std::chrono::milliseconds>(x)



TON::TON()
{
	this->_Q = false;
	this->_IN = false;
}



void TON::start(long long int _PT, bool _IN)
{
	this->_PT = milliseconds(_PT);
	if (!this->_IN && _IN) {
		this->_stT = HighResTimeToMilliseconds(HighResTimeGet());
	}
	this->_IN = _IN;
}



void TON::reset()
{
	this->_IN = false;
	this->_Q = false;
}



long long int TON::since() const
{
	this->_ET = HighResTimeToMilliseconds(HighResTimeGet());
	this->_ET -= this->_stT;
	return HighResTimeToMilliseconds(this->_ET).count();
}



bool TON::check()
{
	this->since();
	if (this->_ET >= this->_PT && this->_IN)
		this->_Q = true;
	return this->_Q;
}
