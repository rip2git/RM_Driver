
#include "CrossSleep.h"

#ifdef __linux__
#include <unistd.h>
#else
#include <windows.h>
#endif



void CrossSleep(uint32_t ms)
{
#ifdef __linux__
		usleep(ms * 1000);
#else
		Sleep(ms);
#endif
}
