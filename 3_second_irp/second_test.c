/*
* @Author: FloodShao
* @Date:   2020-01-07 10:47:56
* @Last Modified by:   FloodShao
* @Last Modified time: 2020-01-07 11:01:09
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>

int main(){

	int fd;
	int counter = 0;
	int old_counter = 0;

	fd = open("/dev/second", O_RDONLY);
	if(fd != -1){
		while(1){
			read(fd, &counter, sizeof(unsigned int)); //kernel pass the count to user 
			if(counter != old_counter){
				printf("seconds after open /dev/second: %d\n", counter);
				old_counter = counter;
			}
		}
	} else{
		printf("Device open failure\n");
	}

	return 0;
}