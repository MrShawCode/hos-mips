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
    char buf[100];
	const char *path = "/dev/mychar";
	printf("Will open: %s\r\n", path);
    int fd = open(path, O_RDWR);
	if (fd < 0) {
		printf("failed to open file.\r\n");
		return fd;
    }
	int wtnum = write(fd, "hello!", 7);
	printf("before buf : %s, wtnum : %d \r\n", buf, wtnum);
	int rdnum = read(fd,buf,7);
	printf("after buf : %s, rdnum : %d \r\n", buf, rdnum);
	return 0;
}
