#include <ulib.h>
#include <stdio.h>
#include <file.h>
#include <error.h>
#include <unistd.h>
#include <string.h>

#define printf(...)                     fprintf(1, __VA_ARGS__)

void usage(void)
{
  printf("usage: mknod path major minor\r\n");
}

int atoi(char* pstr)
{
	int result = 0;
	if(pstr == NULL) {
		return -1;
	}
	while(*pstr == ' ') {
    ++pstr;
  }
	while(*pstr >= '0' && *pstr <= '9') {
		result = result * 10 + *pstr - '0';
		++pstr;
	}
	return result;
}

int main(int argc, char **argv)
{
  if (argc != 4) {
    usage();
  } else {
    int ret;
    int major = atoi(argv[2]);
    int minor = atoi(argv[3]);
    int buf[FS_MAX_DNAME_LEN+1];
    if (major < 0 || minor < 0) {
      return -E_INVAL;
    }
    strncpy(buf, argv[1], FS_MAX_DNAME_LEN);
    if ((ret = mknod(buf, major, minor)) != 0) {
      return ret;
    }
  }
  return 0;
}
