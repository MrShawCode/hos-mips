#include <ulib.h>
#include <stdio.h>
#include <string.h>
#include <dir.h>
#include <file.h>
#include <stat.h>
#include <dirent.h>
#include <unistd.h>

#define printf(...)                     fprintf(1, __VA_ARGS__)

#define STP '0'	//stop
#define FRN '1'	//go front
#define BAK '2'	//go back
#define ACL '3'	//accelerate
#define SUB '4'	//decelerate
#define TRI	'5'	//turn right
#define TLE '6'	//turn left
#define ORI '7'	//stop right 
#define OLE '8'	//stop left

void delay(void)
{
    volatile unsigned int j;
    for (j = 0; j < (500); j++) ; // delay
}

int main(int argc, char **argv)
{
    int spd=9;
    int stat=0x2e;
    char c;
    char buf[100];
	const char *bt_path = "/dev/bluetooth";
    const char *car_path = "/dev/car";
    //open dev : bluetooth && car
	printf("Will open: %s\r\n", bt_path);
    int fd_bt = open(bt_path, O_RDONLY);
	if (fd_bt < 0) {
		printf("failed to open bt_path.\r\n");
		return fd_bt;
    }
    int fd_car = open(car_path, O_WRONLY);
    if (fd_car < 0) {
        printf("failed to open car_path.\r\n");
        return fd_car;
    }
    //operate car
    while(1){
        int rdnum = read(fd_bt,&c,1);
        printf("read from bt : %c, rdnum : %d \r\n", c, rdnum);
        delay();
        switch(c){
            case FRN://1
                stat=0x2e;
                memcpy(buf, &spd, 4);
                memcpy(buf+4, &stat, 4);
                write(fd_car, buf, 10);
                break;
            case BAK://2
                stat=0xd1;
                memcpy(buf, &spd, 4);
                memcpy(buf+4, &stat, 4);
                write(fd_car, buf, 10);
                break;
            case ACL://3
                if(spd<9) spd++;
                memcpy(buf, &spd, 4);
                memcpy(buf+4, &stat, 4);
                write(fd_car, buf, 10);
                break;
            case SUB://4
                if(spd>3) spd--;
                memcpy(buf, &spd, 4);
                memcpy(buf+4, &stat, 4);
                write(fd_car, buf, 10);
                break;
            case TRI://5
                stat = 0x9c;
                memcpy(buf, &spd, 4);
                memcpy(buf+4, &stat, 4);
                write(fd_car, buf, 10);
                break;
            case TLE://6
                stat = 0x63;
                memcpy(buf, &spd, 4);
                memcpy(buf+4, &stat, 4);
                write(fd_car, buf, 10);
                break;
            // case '7'://6
            //     writeCar(spd,spd,spd,spd,0x40);
            //     break;
            // case '8'://6
            //     writeCar(spd,spd,spd,spd,0x80);
            //     break;
            case STP://0
                stat = spd = 0x0;
                memcpy(buf, &spd, 4);
                memcpy(buf+4, &stat, 4);
                write(fd_car, buf, 10);
                return 0;       
            default:
                stat = spd = 0x0;
                memcpy(buf, &spd, 4);
                memcpy(buf+4, &stat, 4);
                write(fd_car, buf, 10);
                break;
        }
    }
	return 0;
}
