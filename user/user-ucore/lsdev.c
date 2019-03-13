#include <stdio.h>
#include <file.h>

#define printf(...)                     fprintf(1, __VA_ARGS__)

int main(int argc, char **argv)
{
  struct devinfo curdev, nextdev;
  struct devinfo *pcurdev = NULL;
  printf("DEVNAME\t\t\tMAJOR\t\t\tMINOR\r\n");
  while (getdevinfo(pcurdev, &nextdev) == 0) {
    printf("%s\t\t\t%d\t\t\t%d\r\n", nextdev.name, nextdev.major, nextdev.minor);
    curdev = nextdev;
    pcurdev = &curdev;
  }
  return 0;
}