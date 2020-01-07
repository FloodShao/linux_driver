/*
* @Author: FloodShao
* @Date:   2020-01-06 14:54:57
* @Last Modified by:   FloodShao
* @Last Modified time: 2020-01-06 15:30:42
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>

static void signalio_handler(int signum){
	printf("receive a signal from globalfifo, signal num: %d\n", signum);
}

int main(void){
	int fd, oflags;
	fd = open("/dev/global_fifo", O_RDWR, S_IRUSR | S_IWUSR);
	if(fd != -1){
		signal(SIGIO, signalio_handler);
		fcntl(fd, F_SETOWN, getpid());
		oflags = fcntl(fd, F_GETFL);
		fcntl(fd, F_SETFL, oflags | FASYNC);
		while(1){
			sleep(1000);
		}
	} else{
		printf("device open failure.\n");
	}

	return 0;
}