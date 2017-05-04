#include <syscall.h>
#include "signal.h"

sighandler_t signal(int sign, sighandler_t handler)
{
	struct sigaction act = { handler, NULL, 1 << (sign - 1), 0 };
	sys_linux_sigaction(sign, &act, NULL);
	return handler;
}

int tkill(int pid, int sign)
{
	return sys_linux_tkill(pid, sign);
}
