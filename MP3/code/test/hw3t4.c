#include "syscall.h"

int
main()
{
	int n, i;
	for (n = 1; n < 10; ++n) {
		PrintInt(4);
		for (i=0; i<300; ++i);
	}
	Exit(4);
}
