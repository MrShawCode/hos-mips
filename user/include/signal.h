#include <signum.h>

sighandler_t signal(int sign, sighandler_t handler);
int tkill(int pid, int sign);
int set_shellrun_pid();
