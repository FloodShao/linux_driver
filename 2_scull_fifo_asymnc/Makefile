KVERS = $(shell uname -r)

obj-m += global_fifo.o

build: kernel_modules user_test

kernel_modules:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) modules
user_test:
	gcc -o globalfifo_test globalfifo_test.c

clean:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) clean
	rm globalfifo_test globalfifo_test.o