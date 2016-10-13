#include <ulib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <file.h>

#define printf(...)                 fprintf(1, __VA_ARGS__)
#define BUFSIZE 4096
int max_num;

char *readline(const char *prompt)
{
	static char buffer[BUFSIZE];
	if (prompt != NULL) {
		printf("%s", prompt);
	}
	int ret, i = 0;
	while (1) {
		char c;
		if ((ret = read(0, &c, sizeof(char))) < 0) {
			return NULL;
		} else if (ret == 0) {
			if (i > 0) {
				buffer[i] = '\0';
				break;
			}
			return NULL;
		}

		if (c == 3) {
			return NULL;
		} else if (c >= ' ' && i < BUFSIZE - 1) {
			//putc(c);
			buffer[i++] = c;
		} else if (c == '\b' && i > 0) {
			//putc(c);
			i--;
		} else if (c == '\n' || c == '\r') {
			//putc(c);
			buffer[i] = '\0';
			break;
		}
	}
	return buffer;
}

int main(int argc, char *argv[]){
	max_num = 100;
	int num = rand()%100;
	int con = 0;
	int input = max_num;
        while(input != num){
	     input = 0;
	     char *s = readline("input a number > ");
             int i=0;
	     for(;s[i]!='\0';i++){ input = input*10+(s[i]-'0'); }
	     con++;
	     if(input > num) printf("\r\nsmaller\r\n");
	     if(input < num) printf("\r\nbiger\r\n");
        }
        printf("\r\nsuccessfully guess the num, use %d times\r\n", con);
	return 0;
}
