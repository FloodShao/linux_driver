KVERS = $(shell uname -r)
CUR_DIR = $(shell pwd)
K_DIR = /lib/modules/$(KVERS)/build

obj-m += global_fifo.o
obj-m += global_fifo_poll.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(K_DIR) M=$(CUR_DIR) modules

clean:
	$(MAKE) -C $(K_DIR) M=$(CUR_DIR) clean

