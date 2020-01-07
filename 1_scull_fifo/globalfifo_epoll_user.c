/*
* @Author: FloodShao
* @Date:   2019-12-20 10:57:01
* @Last Modified by:   FloodShao
* @Last Modified time: 2019-12-20 13:28:47
*/

#include <sys/fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <strings.h>

#define FIFO_CLEAR	0x1
#define BUFFER_LEN	20

void main(void){

	int fd;

	fd = open("/dev/globalfifo_poll", O_RDONLY | O_NONBLOCK);

	if(fd != -1){
		struct epoll_event ev_globalfifo;
		int err;
		int epfd;

		if(ioctl(fd, FIFO_CLEAR, 0) < 0){
			printf("ioctl command failure\n");
		}
		epfd = epoll_create(1);
		if(epfd < 0){
			perror("epoll_create()");
			return;
		}

		bzero(&ev_globalfifo, sizeof(struct epoll_event));
		ev_globalfifo.events = EPOLLIN | EPOLLPRI;

		err = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev_globalfifo);
		if(err < 0){
			perror("epoll_ctl()");
			return;
		}
		err = epoll_wait(epfd, &ev_globalfifo, 1, 15000);
		if(err < 0){
			perror("epoll_wait()");
		} else if(err == 0){
			printf("No data input in FIFO within 15 seconds\n");
		} else{
			printf("FIFO was not empty\n");
		}

		err = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &ev_globalfifo);
		if(err < 0){
			perror("epoll_ctl()\n");
		}


	} else{
		printf("Device open failure\n");
	}

}