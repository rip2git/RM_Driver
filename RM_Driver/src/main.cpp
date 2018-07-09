
/// linker, misc
/// -static-libgcc -static-libstdc++
#include "Driver.h"


int main()
{
	Driver drv;
	drv.Initialization();
	drv.StartProcess();
	return 0;
}
