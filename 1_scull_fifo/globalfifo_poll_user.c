/*
* @Author: FloodShao
* @Date:   2019-12-20 10:24:35
* @Last Modified by:   FloodShao
* @Last Modified time: 2019-12-20 10:56:08
*/

#include <sys/select.h> //run select and poll system call
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h> // ioctl syscall
#include <fcntl.h>
#include <stdio.h>


#define FIFO_CLEAR	0x01
#define BUFFER_LEN	20

void main(void){
	int fd, num;
	char rd_ch[BUFFER_LEN];

	fd_set rfds, wfds; //an array of long type, recording the file handle

	//non blocking open /dev/globalfifo_poll
	fd = open("/dev/globalfifo_poll", O_RDONLY | O_NONBLOCK); //syscall, return a file handle

	if(fd != -1){ //open success
		// clear fifo
		if(ioctl(fd, FIFO_CLEAR, 0) < 0){ //failed
			printf("ioctl command failed\n");
		}

		while(1){

			//clear the file set
			FD_ZERO(&rfds);
			FD_ZERO(&wfds);

			FD_SET(fd, &rfds);
			FD_SET(fd, &wfds);

			//int select(int nfds, fd_set *restrict readfds, fd_set *restrict writefds, fd_set *restrict errorfds,struct timeval *restrict timeout);
			select(fd+1, &rfds, &wfds, NULL, NULL); //check

			if(FD_ISSET(fd, &rfds))
				printf("Poll monitor: can be read\n");
			if(FD_ISSET(fd, &wfds))
				printf("Poll monitor: can be written\n");
		}
	} else{
		printf("Device open failure!\n");
	}

}