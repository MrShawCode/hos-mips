#include <ulib.h>
#include <stdio.h>
#include <string.h>
#include <dir.h>
#include <file.h>
#include <stat.h>
#include <dirent.h>
#include <unistd.h>

#define printf(...)                     fprintf(1, __VA_ARGS__)


int main(int argc, char **argv)
{
	char c;
    int fd = open("/dev/bluetooth", O_RDONLY);
	if (fd < 0) {
		printf("failed to open file.\n");
		return fd;
	}
	read(fd, &c, 1);
	printf("receive from bt: %c \n", c);
    return 0;
}
